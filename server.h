#pragma once

#include <functional>
#include <memory>
#include <unordered_set>

#include "request.h"

namespace firecgi {

class Server {
  public:
	Server(int port, const std::function<void(Request*)>& callback, int threads=1, const std::unordered_set<std::string_view>& headers={}, int max_request_len=(16*1024));
	void Serve();
	void Shutdown();
	void RegisterSignalHandlers();

  private:
	Connection *NewConn(int listen_sock, int epoll_fd);
	int NewListenSock();
	void ServeInt();

	const int port_;
	const std::function<void(Request*)> callback_;
	const int threads_;
	const std::unordered_set<std::string_view> headers_;
	const int max_request_len_;

	int close_fd_;
};

} // firecgi
