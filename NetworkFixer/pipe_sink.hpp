#pragma once

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/common.h>
#include <windows.h>
#include <winbase.h>
#include <cstring>

#include <mutex>
#include <string>

template<class Mutex>
class pipe_sink : public spdlog::sinks::base_sink<Mutex>
{
private:
    LPCTSTR lpszPipename;
    bool is_connected = false;
    // std::vector<spdlog::details::log_msg> log_queue;
    HANDLE hPipe = NULL;

    bool connect()
    {
        if (is_connected)
        {
            return true;
        }

        hPipe = CreateFile(
            lpszPipename,   // pipe name 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 

        // Break if the pipe handle is valid. 

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            is_connected = true;
            return true;
        }

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 

        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            std::stringstream ss;
            ss << TEXT("Could not open pipe. GLE=");
            ss << GetLastError() << "\n";
            OutputDebugStringA(ss.str().c_str());
            return false;
        }

        OutputDebugStringA("All pipe instances are busy");
        return false;
    }

public:
    ~pipe_sink()
    {
        if (lpszPipename != NULL)
        {
            delete[] lpszPipename;
        }

        if (hPipe != NULL)
        {
            CloseHandle(hPipe);
        }
    }

    explicit pipe_sink(std::string pipe_name)
    {
        auto full_name = std::format("\\\\.\\pipe\\{}", pipe_name);
        lpszPipename = new TCHAR[full_name.size() + 1];
        strncpy_s((char*)lpszPipename, full_name.size() + 1, full_name.c_str(), _TRUNCATE);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        send_message(msg);
    }

    bool send_message(const spdlog::details::log_msg& msg)
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        char* formatted_data = formatted.data();
        auto formatted_size = formatted.size();

        char* debug_message = new char[formatted_size] {0};
        strncpy_s(debug_message, formatted_size - 1, formatted_data, _TRUNCATE);
        OutputDebugStringA(debug_message);
        OutputDebugStringA("\n");
        delete[] debug_message;

        std::stringstream ss;

        if (!connect())
        {
            // Simply drop messages
            OutputDebugStringA("pipe_sink: log messages dropped");
            return false;
        }

        const size_t message_max_size = 256;
        TCHAR* lpvMessage = new TCHAR[message_max_size]{ '\0'};

        const char* signature = "LOG ";

        if (formatted_size > message_max_size - 4)
        {
            formatted_size = message_max_size - 4;
        }

        strcpy_s(lpvMessage, 5, signature);
        strncpy_s(lpvMessage + 4, formatted_size, formatted_data, _TRUNCATE);
        lpvMessage[4 + formatted_size - 1] = '\0';

        DWORD dwMode = PIPE_READMODE_MESSAGE;
        BOOL fSuccess = SetNamedPipeHandleState(
            hPipe,    // pipe handle 
            &dwMode,  // new pipe mode 
            NULL,     // don't set maximum bytes 
            NULL);    // don't set maximum time 

        if (!fSuccess)
        {
            ss.clear();
            ss << TEXT("SetNamedPipeHandleState failed. GLE=");
            ss << GetLastError() << "\n";
            OutputDebugStringA(ss.str().c_str());
            is_connected = false;
            delete[] lpvMessage;
            return false;
        }

        int symb_num = lstrlen(lpvMessage);
        DWORD cbToWrite = (symb_num + 1) * sizeof(TCHAR);

        DWORD cbWritten;

        fSuccess = WriteFile(
            hPipe,                  // pipe handle 
            lpvMessage,             // message 
            cbToWrite,              // message length 
            &cbWritten,             // bytes written 
            NULL);                  // not overlapped 

        if (!fSuccess)
        {
            ss.clear();
            ss << "WriteFile to pipe failed. GLE=" << GetLastError() << "\n";
            is_connected = false;
            OutputDebugStringA(ss.str().c_str());
            delete[] lpvMessage;
            return false;
        }

        delete[] lpvMessage;

        return true;
    }

    void flush_() override
    {

    }
};

using pipe_sink_mt = pipe_sink<std::mutex>;
using pipe_sink_st = pipe_sink<spdlog::details::null_mutex>;