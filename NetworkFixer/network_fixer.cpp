#include <windows.h>
#include <detours.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <memory>
#include "pipe_sink.hpp"

const std::string pipe_name = "LOTR_BFME_2_NETWORK_FIXER_PIPE";

void setup_logging()
{
    OutputDebugStringA("Setting up logger\n");

    // auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);
    auto pipe_sink = std::make_shared<pipe_sink_mt>(pipe_name);
#ifdef DEBUG
    OutputDebugStringA("Debug log level enabled\n");
    msvc_sink->set_level(spdlog::level::debug);
    pipe_sink->set_level(spdlog::level::debug);
#else
    OutputDebugStringA("Debug log level disabled\n");
    // msvc_sink->set_level(spdlog::level::info);
    pipe_sink->set_level(spdlog::level::info);
#endif
    // msvc_sink->set_pattern("[%c] [%n] [proc %P (%t)] [%^%l%$] %v");
    pipe_sink->set_pattern("[%n] [P-%P (T-%t)] [%^%l%$] %v");

    // spdlog::logger logger("network_fixer", { msvc_sink, pipe_sink });
    // spdlog::logger logger("network_fixer", { msvc_sink });
    spdlog::logger logger("network_fixer", { pipe_sink });
    OutputDebugStringA("The next log message should be 'Logging set up'\n");
    logger.info("Logging set up");
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(logger));
    spdlog::flush_every(std::chrono::seconds(3));
}

void install_hook()
{

}

void uninstall_hook()
{

}

BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringA("DLL_PROCESS_ATTACH\n");
        setup_logging();
        spdlog::info("Dll attached");
        install_hook();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("DLL_PROCESS_DETACH\n");
        uninstall_hook();
        break;
    }
    return TRUE;
}