#pragma once

#include <functional>
#include <memory>
#include <unordered_set>

#include "request.h"

namespace firecgi {

class Server {
  public:
	Server(int port, const std::function<void(Request*)>& callback, int threads=1, const std::unordered_set<std::string_view>& headers={});
	void Serve();

  private:
	void NewConn(int listen_sock, int epoll_fd);
	int NewListenSock();
	void ServeInt();

	const int port_;
	const std::function<void(Request*)> callback_;
	const int threads_;
	const std::unordered_set<std::string_view> headers_;
};

} // firecgi
