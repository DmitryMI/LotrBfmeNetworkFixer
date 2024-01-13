#pragma once

#include <windows.h>
#include <detours.h>
#include <iostream>
#include <sstream>
#include <iphlpapi.h>
#include <vector>
#include <algorithm>
#include <Winsock.h>
#include <spdlog/spdlog.h>

#if _WIN64
#define WSOCK_MODULE_PATH "C:\\Windows\\System32\\WS2_32.dll"
#else
#define WSOCK_MODULE_PATH "C:\\Windows\\System32\\WSOCK32.dll"
#endif

typedef hostent FAR* (PASCAL FAR* gethostbyname_t)(const char* name);
typedef int (PASCAL FAR* gethostname_t)(FAR char* name, int name_len);

gethostbyname_t gethostbyname_ptr = NULL;
gethostname_t gethostname_ptr = NULL;

std::string target_ip = "";

bool request_target_ip()
{
    LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\LOTR_BFME_2_NETWORK_FIXER_PIPE");
    LPCTSTR lpvMessage = TEXT("GET IP");
    const int bufsize = 32;
    TCHAR chBuf[bufsize];

    HANDLE hPipe = CreateFile(
        lpszPipename,   // pipe name 
        GENERIC_READ |  // read and write access 
        GENERIC_WRITE,
        0,              // no sharing 
        NULL,           // default security attributes
        OPEN_EXISTING,  // opens existing pipe 
        0,              // default attributes 
        NULL);          // no template file 

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        spdlog::error(__FUNCTION__": hPip == INVALID_HANDLE_VALUE");
        return false;
    }

    DWORD dwMode = PIPE_READMODE_MESSAGE;
    BOOL fSuccess = SetNamedPipeHandleState(
        hPipe,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        spdlog::error(__FUNCTION__": SetNamedPipeHandleState failed. GLE={}", GetLastError());
        CloseHandle(hPipe);
        return false;
    }

    // Send a message to the pipe server. 
    DWORD cbWritten;
    DWORD cbRead;
    DWORD cbToWrite = (lstrlen(lpvMessage) + 1) * sizeof(TCHAR);

    fSuccess = WriteFile(
        hPipe,                  // pipe handle 
        lpvMessage,             // message 
        cbToWrite,              // message length 
        &cbWritten,             // bytes written 
        NULL);                  // not overlapped 

    if (!fSuccess)
    {
        spdlog::error(__FUNCTION__": WriteFile failed. GLE={}", GetLastError());
        CloseHandle(hPipe);
        return false;
    }

    do
    {
        // Read from the pipe. 

        fSuccess = ReadFile(
            hPipe,    // pipe handle 
            chBuf,    // buffer to receive reply 
            bufsize * sizeof(TCHAR),  // size of buffer 
            &cbRead,  // number of bytes read 
            NULL);    // not overlapped 

        if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
            break;
    }
    while (!fSuccess);  // repeat loop if ERROR_MORE_DATA 

    target_ip = chBuf;

    if (!fSuccess)
    {
        spdlog::error(__FUNCTION__": ReadFile from pipe failed. GLE={}", GetLastError());
        CloseHandle(hPipe);
        return false;
    }

    CloseHandle(hPipe);
    return true;
}

int PASCAL FAR hook_gethostname(char* name, int name_len)
{
    spdlog::info(__FUNCTION__ " hook called");
    return gethostname_ptr(name, name_len);
}

hostent FAR* PASCAL FAR hook_gethostbyname(const char* name)
{
    spdlog::debug(__FUNCTION__ " hook called for hostname {}", name);
    hostent* host_ent = gethostbyname_ptr(name);

    if (!request_target_ip())
    {
        if (target_ip.empty())
        {
            spdlog::error("Failed to get target ip from pipe server and cached value is empty!");
            return host_ent;
        }
        else
        {
            spdlog::warn("Pipe server is unavailable, using cached target ip: {}", target_ip);
        }
    }
    else
    {
        spdlog::info("Target ip fetched: {}", target_ip);
    }

    if (host_ent == nullptr)
    {
        spdlog::error(__FUNCTION__ " hook: gethostbyname_ptr returned NULL");
        return host_ent;
    }

    int target_index = -1;

    for (int i = 0; host_ent->h_addr_list[i] != 0; ++i)
    {
        struct in_addr addr;
        memcpy(&addr, host_ent->h_addr_list[i], sizeof(struct in_addr));

        char* ip_str = inet_ntoa(addr);

        if (strcmp(target_ip.c_str(), ip_str) == 0)
        {
            spdlog::debug(__FUNCTION__ " hook: Target Address {} found on index {}", ip_str, i);
            target_index = i;
        }
    }

    if (target_index == -1)
    {
        OutputDebugStringA(__FUNCTION__ " target interface not found!\n");
        spdlog::error(__FUNCTION__ " hook: Target address not found!");
        return host_ent;
    }


    char* temp = host_ent->h_addr_list[0];
    host_ent->h_addr_list[0] = host_ent->h_addr_list[target_index];
    host_ent->h_addr_list[1] = NULL;

    spdlog::debug(__FUNCTION__ " hook finished");
    return host_ent;
}

gethostname_t find_gethostname()
{
    PVOID func = DetourFindFunction(WSOCK_MODULE_PATH, "gethostname");
    if (func != NULL)
    {
        return (gethostname_t)func;
    }

    OutputDebugStringA("Failed to find gethostname function\n");

    return ::gethostname;
}

gethostbyname_t find_gethostbyname()
{
    PVOID func = DetourFindFunction(WSOCK_MODULE_PATH, "gethostbyname");
    if (func != NULL)
    {
        return (gethostbyname_t)func;
    }

    OutputDebugStringA("Failed to find gethostbyname function\n");

    return ::gethostbyname;
}

void install_gethostbyname_hook()
{
    spdlog::info("Installing gethostbyname hook...");

    gethostbyname_ptr = find_gethostbyname();

    auto error = DetourTransactionBegin();
    if (error != NO_ERROR)
    {
        spdlog::error("DetourTransactionBegin failed");
        return;
    }

    error = DetourUpdateThread(GetCurrentThread());
    if (error != NO_ERROR)
    {
        spdlog::error("DetourUpdateThread failed");
        return;
    }
    error = DetourAttach(&(PVOID&)gethostbyname_ptr, hook_gethostbyname);
    if (error != NO_ERROR)
    {
        spdlog::error("DetourAttach failed");
        return;
    }
    else
    {
        OutputDebugStringA("DetourAttach HookGetHostName ok\n");
        spdlog::info("gethostbyname hook installed");
    }

    error = DetourTransactionCommit();
    if (error != NO_ERROR)
    {
        spdlog::error("DetourTransactionCommit failed");
    }
    else
    {
        spdlog::info("DetourTransactionCommit ok");
    }
}

void uninstall_gethostbyname_hook()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    spdlog::info("Uninstalling hook: gethostbyname\n");
    DetourDetach(&(PVOID&)gethostbyname_ptr, hook_gethostbyname);

    DetourTransactionCommit();
}