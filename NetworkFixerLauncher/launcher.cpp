#include <boost/program_options.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>
#include <string>
#include <windows.h>
#include <Tlhelp32.h>
#include <filesystem>
#include <Winsock.h>
#include "pipe_server.hpp"
#include <string.h>

namespace po = boost::program_options;

std::vector<DWORD> injected_processes;
std::string target_ip;
std::unique_ptr<pipe_server> pipe_server_instance;
const DWORD pipe_server_buffer_size = 1024;

#if _WIN64
const std::string dll_name_default = "NetworkFixer.dll";
#else
const std::string dll_name_default = "NetworkFixer (x86).dll";
#endif

po::options_description create_options_description()
{
	po::options_description desc("Allowed options");

	desc.add_options()
		("help", "produce help message")
		("ip", po::value<std::string>(), "IP address for the game to use")
        ("dll", po::value<std::string>()->default_value(dll_name_default), "Path to .dll to be injected")
        ("process", po::value<std::string>()->default_value("game.dat"), "Name of target process")
        ("pipe", po::value<std::string>()->default_value("LOTR_BFME_2_NETWORK_FIXER_PIPE"), "Name of pipe")
        ("period", po::value<int>()->default_value(5000), "Monitor period in ms")
		("verbosity,v", po::value<std::string>()->default_value("INFO"), "logging level")
		;

	return desc;
}

void print_error(DWORD winapi_error)
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


std::vector<PROCESSENTRY32> find_processes(std::string name)
{
    std::vector<PROCESSENTRY32> processHandles;
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hTool32 = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    BOOL bProcess = Process32First(hTool32, &pe32);
    if (bProcess == TRUE)
    {
        while ((Process32Next(hTool32, &pe32)) == TRUE)
        {
            if (strcmp(pe32.szExeFile, name.c_str()) == 0)
            {
                processHandles.push_back(pe32);
            }
        }
    }
    CloseHandle(hTool32);

    return processHandles;
}

bool inject_dll(PROCESSENTRY32 pe32, std::filesystem::path path_to_dll)
{
    if (!std::filesystem::exists(path_to_dll))
    {
        spdlog::error("File {} does not exist!", path_to_dll.string());
        return false;
    }

    char* DirPath = new char[MAX_PATH];
    char* FullPath = new char[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, DirPath);
    strcpy_s(FullPath, MAX_PATH, path_to_dll.string().c_str());

    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE, FALSE, pe32.th32ProcessID);

    if (hProcess == nullptr)
    {
        spdlog::error("OpenProcess failed for process {}", pe32.th32ProcessID);
        return false;
    }

    HMODULE kernel32_module = GetModuleHandle("kernel32.dll");
    if (kernel32_module == nullptr)
    {
        spdlog::error("GetModuleHandle failed for process {}", pe32.th32ProcessID);
        return false;
    }

    LPVOID LoadLibraryAddr = (LPVOID)GetProcAddress(kernel32_module, "LoadLibraryA");

    if (LoadLibraryAddr == nullptr)
    {
        spdlog::error("GetProcAddress(kernel32_module, LoadLibraryA) failed for process {}", pe32.th32ProcessID);
        return false;
    }

    LPVOID LLParam = (LPVOID)VirtualAllocEx(hProcess, NULL, strlen(FullPath),
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (LLParam == nullptr)
    {
        spdlog::error("VirtualAllocEx failed for process {}", pe32.th32ProcessID);
        return false;
    }

    bool write_ok = WriteProcessMemory(hProcess, LLParam, FullPath, strlen(FullPath), NULL);
    if (!write_ok)
    {
        auto err = GetLastError();
        print_error(err);
        return false;
    }

    HANDLE threadHandle = CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibraryAddr,
        LLParam, NULL, NULL);

    if (!threadHandle)
    {
        auto err = GetLastError();
        print_error(err);
        return false;
    }

    CloseHandle(hProcess);
    delete[] DirPath;
    delete[] FullPath;

    spdlog::info("{} injected to {}", path_to_dll.string(), pe32.th32ProcessID);
    return true;
}

bool inject_dll(std::string process_name, std::filesystem::path path_to_dll)
{
    if (!std::filesystem::exists(path_to_dll))
    {
        spdlog::error("File {} does not exist!", path_to_dll.string());
        return false;
    }

    auto process_handles = find_processes(process_name);
    if (process_handles.size() == 0)
    {
        spdlog::error("Process with name {} not found!", process_name);
        return false;
    }

    bool has_injected = false;

    for (const auto& pe32 : process_handles)
    {
        has_injected = inject_dll(process_name, path_to_dll);
    }

    return has_injected;
}

void monitor_and_inject(std::string process_name, std::filesystem::path path_to_dll, int period, bool exit_on_success, int max_retries = 0)
{
    int iteration_num = 0;

    spdlog::info("Looking for processes...");
    while (max_retries == 0 || iteration_num < max_retries)
    {
        spdlog::debug("Scanning...");

        auto processes = find_processes(process_name);

        std::vector<DWORD> injected_processes_current;
        bool has_injected = false;

        for (const auto& process : processes)
        {
            bool is_injected = std::any_of(injected_processes.begin(), injected_processes.end(),
                [process](DWORD proc_id) {return process.th32ProcessID == proc_id;});

            if (is_injected)
            {
                injected_processes_current.push_back(process.th32ProcessID);
                continue;
            }

            spdlog::info("Found uninjected process {} ({})", process_name, process.th32ProcessID);
            if (!inject_dll(process, path_to_dll))
            {
                spdlog::error("Failed to inject process {} ({})", process_name, process.th32ProcessID);
            }
            else
            {
                injected_processes_current.push_back(process.th32ProcessID);
                has_injected = true;
            }
        }

        injected_processes = injected_processes_current;

        if (has_injected && exit_on_success)
        {
            break;
        }

        iteration_num++;
        Sleep(period);
    }
}

std::string select_ip()
{
    const int size = 512;
    char name[size];

    int error = gethostname(name, size);

    if (error != 0)
    {
        print_error(WSAGetLastError());
        return "";
    }

    std::cout << "Hostname: " << name << "\n";

    struct hostent* phe = gethostbyname(name);
    if (phe == NULL)
    {
        print_error(WSAGetLastError());
        return "";
    }

    std::vector<std::string> ip_list;

    for (int i = 0; phe->h_addr_list[i] != NULL; ++i)
    {
        struct in_addr addr;
        memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
        char* ip_str = inet_ntoa(addr);
        ip_list.push_back(ip_str);
    }

    for (size_t i = 0; i < ip_list.size(); i++)
    {
        std::cout << i << ": " << ip_list[i] << "\n";
    }

    std::cout << "Select ip address by entering the number: ";
    size_t index;
    std::cin >> index;

    if (index >= ip_list.size())
    {
        spdlog::error("User input invalid");
        return "";
    }

    return ip_list[index];
}

bool reply_to_pipe_request(std::string request, std::string& reply)
{
    spdlog::debug("Client Request String:\"{}\"", request);

    if (request.starts_with("LOG "))
    {
        spdlog::info(request);
        return true;
    }

    if (request != "GET IP")
    {
        return false;
    }

    reply.clear();

    for (size_t i = 0; i < target_ip.size(); i++)
    {
        reply.push_back(target_ip[i]);
    }

    return true;
}

int main(int argc, char** argv)
{
	po::options_description po_desc = create_options_description();
	po::variables_map vm;

	po::store(po::parse_command_line(argc, argv, po_desc), vm);
	po::notify(vm);

	if (vm.count("help"))
	{
		std::cout << po_desc << "\n";
		return 0;
	}

	if (vm.count("verbosity"))
	{
		std::string level = vm["verbosity"].as<std::string>();
		if (!level.empty())
		{
			std::transform(level.begin(), level.end(), level.begin(),
				[](unsigned char c) { return std::tolower(c); }
			);

			spdlog::level::level_enum level_value = spdlog::level::from_str(level);
			spdlog::set_level(level_value);
		}
	}

    std::string ip_address;
    if (vm.count("ip"))
    {
        ip_address = vm["ip"].as<std::string>();
    }
    else
    {
        WSADATA wsaData;
        auto wVersionRequested = MAKEWORD(2, 2);
        int error = WSAStartup(wVersionRequested, &wsaData);

        if (error != 0)
        {
            print_error(WSAGetLastError());
            return -1;
        }

        ip_address = select_ip();

        WSACleanup();
    }

    if (ip_address.empty())
    {
        return -1;
    }

    target_ip = ip_address;
	
	spdlog::info("IP Address will be set to {}", ip_address);
    std::string process_name = vm["process"].as<std::string>();
    spdlog::info("Target process name: {}", process_name);
    std::string dll_path_str = vm["dll"].as<std::string>();

    auto dll_path = std::filesystem::absolute(dll_path_str);

    if (std::filesystem::exists(dll_path))
    {
        spdlog::info("Path to dll: {}", dll_path.string());
    }
    else
    {
        spdlog::error("Dll {} does not exist!", dll_path.string());
        return 1;
    }

    pipe_server_instance = std::make_unique<pipe_server>(vm["pipe"].as<std::string>(), pipe_server_buffer_size);
    pipe_server_instance->register_callback(reply_to_pipe_request);
    pipe_server_instance->create_pipe_and_listen();
    
    monitor_and_inject(process_name, dll_path, vm["period"].as<int>(), false);

	return 0;
}
