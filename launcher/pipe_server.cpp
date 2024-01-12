#include "pipe_server.hpp"
#include <format>
#include <spdlog/spdlog.h>
#include <stdio.h>

pipe_server::pipe_server(std::string name, DWORD buffer_size)
{
	auto full_name = std::format("\\\\.\\pipe\\{}", name);
    lpszPipename = new TCHAR[full_name.size() + 1];
    strncpy_s((char*)lpszPipename, full_name.size() + 1, full_name.c_str(), _TRUNCATE);

    spdlog::info("Pipe full name: {}", lpszPipename);
    this->buffer_size = buffer_size;
}

pipe_server::~pipe_server()
{
    delete[] lpszPipename;

    stop_listening();

    if (listener_thread.joinable())
    {
        listener_thread.join();
    }

    for (std::thread& th : pipe_threads)
    {
        if (!th.joinable())
        {
            continue;
        }

        th.join();
    }

    if (hPipe != NULL)
    {
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

bool pipe_server::get_answer_to_request(std::string request, std::string& reply)
{
    for (const auto& callback : callbacks)
    {
        if (callback(request, reply))
        {
            return true;
        }
    }

    return false;
}

void pipe_server::pipe_thread_worker(HANDLE pipeHandle)
{
    char* request_buffer = new char[buffer_size];
    char* reply_buffer = new char[buffer_size];
    DWORD bytes_read = 0, written_bytes = 0;

    while (1)
    {
        // Read client requests from the pipe. This simplistic code only allows messages
        // up to BUFSIZE characters in length.
        bool read_success = ReadFile(
            pipeHandle,        // handle to pipe 
            request_buffer,    // buffer to receive data 
            buffer_size * sizeof(TCHAR), // size of buffer 
            &bytes_read, // number of bytes read 
            NULL);        // not overlapped I/O 

        if (!read_success || bytes_read == 0)
        {
            auto err = GetLastError();
            if (err == ERROR_BROKEN_PIPE)
            {
                spdlog::info("pipe_thread_worker: client disconnected.");
            }
            else if (err == ERROR_PIPE_LISTENING)
            {
                spdlog::error("pipe_thread_worker ReadFile failed, GLE=ERROR_PIPE_LISTENING.");
                print_error(err);
            }
            else if (err == ERROR_NO_DATA)
            {
                spdlog::error("pipe_thread_worker ReadFile failed, GLE=ERROR_NO_DATA.");
                print_error(err);
            }
            else
            {
                spdlog::error("pipe_thread_worker ReadFile failed, GLE={}.", err);
                print_error(err);
            }
            break;
        }

        // Process the incoming message.
        std::string request_str = request_buffer;
        std::string reply_str = "";
        bool has_message = get_answer_to_request(request_buffer, reply_str);

        spdlog::info("Replying with {}...", reply_str);
        strcpy_s(reply_buffer, buffer_size, reply_str.c_str());

        DWORD reply_bytes = static_cast<DWORD>(min(buffer_size, reply_str.size() + 1));

        if (has_message)
        {

            // Write the reply to the pipe. 
            bool write_success = WriteFile(
                pipeHandle,        // handle to pipe 
                reply_buffer,     // buffer to write from 
                reply_bytes, // number of bytes to write 
                &written_bytes,   // number of bytes written 
                NULL);        // not overlapped I/O 
            if (!write_success || reply_bytes != written_bytes)
            {
                spdlog::error("pipe_thread_worker WriteFile failed, GLE={}.", GetLastError());
                break;
            }
        }
        else
        {
            spdlog::error("get_answer_to_request returned false!");
            break;
        }
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 

    FlushFileBuffers(pipeHandle);
    DisconnectNamedPipe(pipeHandle);
    CloseHandle(pipeHandle);
    pipeHandle = NULL;

    delete[] request_buffer;
    delete[] reply_buffer;
}

void pipe_server::create_pipe_and_listen_worker()
{
    while (is_listening)
    {
        hPipe = CreateNamedPipe(
            lpszPipename,             // pipe name 
            PIPE_ACCESS_DUPLEX,       // read/write access 
            PIPE_TYPE_MESSAGE |       // message type pipe 
            PIPE_READMODE_MESSAGE |   // message-read mode 
            PIPE_WAIT,                // blocking mode 
            PIPE_UNLIMITED_INSTANCES, // max. instances  
            buffer_size,                  // output buffer size 
            buffer_size,                  // input buffer size 
            0,                        // client time-out 
            NULL);                    // default security attribute 

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            auto err = GetLastError();
            spdlog::error("CreateNamedPipe failed, GLE={}.\n", err);
            print_error(err);
            return;
        }

        spdlog::info("Pipe created, waiting for clients...");

        // Wait for the client to connect; if it succeeds, 
          // the function returns a nonzero value. If the function
          // returns zero, GetLastError returns ERROR_PIPE_CONNECTED. 

        connected = ConnectNamedPipe(hPipe, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected)
        {
            spdlog::info("Client connected, creating a processing thread.");

            std::thread thread(&pipe_server::pipe_thread_worker, this, hPipe);
            pipe_threads.push_back(std::move(thread));
        }
        else
        {
            CloseHandle(hPipe);
        }
    }
}

void pipe_server::print_error(DWORD winapi_error)
{
    wchar_t* s = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, winapi_error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&s, 0, NULL);
    // spdlog::error("{}", s);
    fprintf(stderr, "%ws\n", s);
    LocalFree(s);
}


void pipe_server::register_callback(std::function<pipe_callback> callback)
{
    callbacks.push_back(callback);
}

void pipe_server::stop_listening()
{
    is_listening = false;
    if (listener_thread.joinable())
    {
        listener_thread.join();
    }
}

void pipe_server::unregister_callback(std::function<pipe_callback> callback)
{
    // callbacks.erase(callback);
}

bool pipe_server::create_pipe_and_listen()
{
    if (is_listening)
    {
        spdlog::error("Already listening!");
        return false;
    }

    is_listening = true;

    listener_thread = std::thread(&pipe_server::create_pipe_and_listen_worker, this);

    return true;
}

uint64_t pipe_server::get_buffer_size() const
{
    return buffer_size;
}
