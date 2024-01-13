#pragma once

#include <windows.h> 
#include <stdio.h> 
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <set>

using pipe_callback = bool(std::string, std::string&);

class pipe_server
{
private:
	BOOL connected = false;
	DWORD dwThreadId = 0;
	HANDLE hPipe = INVALID_HANDLE_VALUE;
	HANDLE hThread = NULL;
	LPCTSTR lpszPipename;
	DWORD buffer_size;
	bool is_listening = false;

	std::thread listener_thread;
	std::vector<std::thread> pipe_threads;
	std::vector<std::function<pipe_callback>> callbacks;

	void pipe_thread_worker(HANDLE pipeHandle);

	bool get_answer_to_request(std::string request, std::string& reply);
	void create_pipe_and_listen_worker();

	static void print_error(DWORD err);
public:
	pipe_server(std::string name, DWORD buffer_size = 512);
	~pipe_server();

	pipe_server& operator=(const pipe_server&) = delete;
	pipe_server(const pipe_server&) = delete;
	pipe_server() = default;

	bool create_pipe_and_listen();
	void stop_listening();

	void register_callback(std::function<pipe_callback> callback);
	void unregister_callback(std::function<pipe_callback> callback);

	uint64_t get_buffer_size() const;
};