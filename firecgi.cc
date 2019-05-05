#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>

#include "firecgi.h"
#include "connection.h"

namespace firecgi {

Server::Server(int port, const std::function<void(std::unique_ptr<Request>)>& callback, int threads, const std::unordered_set<std::string_view>& headers)
		: port_(port),
		  callback_(callback),
		  threads_(threads),
		  headers_(headers) {
	LOG(INFO) << "listening on [::1]:" << port_;

	signal(SIGPIPE, SIG_IGN);
}

void Server::Serve() {
	std::vector<std::thread> threads;
	for (int i = 0; i < threads_ - 1; ++i) {
		threads.emplace_back([this]() { ServeInt(); });
	}
	ServeInt();
}

void Server::ServeInt() {
	auto epoll_fd = epoll_create1(0);
	PCHECK(epoll_fd >= 0) << "epoll_create()";

	auto listen_sock = NewListenSock();

	{
		struct epoll_event ev{
			.events = EPOLLIN,
			.data = {
				.ptr = nullptr,
			},
		};
		PCHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &ev) == 0);
	}

	while (true) {
		constexpr auto max_events = 256;
		struct epoll_event events[max_events];
		auto num_fd = epoll_wait(epoll_fd, events, max_events, -1);
		if (num_fd == -1 && errno == EINTR) {
			continue;
		}
		PCHECK(num_fd > 0) << "epoll_wait()";

		for (auto i = 0; i < num_fd; ++i) {
			if (events[i].data.ptr == nullptr) {
				NewConn(listen_sock, epoll_fd);
			} else {
				auto conn = static_cast<Connection*>(events[i].data.ptr);
				auto fd = conn->Read();
				if (fd != -1) {
					PCHECK(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == 0);
					delete conn;
				}
			}
		}
	}
}

void Server::NewConn(int listen_sock, int epoll_fd) {
	sockaddr_in6 client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	auto client_sock = accept(listen_sock, (sockaddr*) &client_addr, &client_addr_len);
	PCHECK(client_sock >= 0) << "accept()";
	CHECK_EQ(client_addr.sin6_family, AF_INET6);

	int flags = 1;
	PCHECK(setsockopt(client_sock, SOL_TCP, TCP_NODELAY, &flags, sizeof(flags)) == 0);

	{
		auto *conn = new Connection(client_sock, client_addr, callback_, headers_);
		struct epoll_event ev{
			.events = EPOLLIN,
			.data = {
				.ptr = conn,
			},
		};
		PCHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev) == 0);
	}
}

int Server::NewListenSock() {
	auto sock = socket(AF_INET6, SOCK_STREAM, 0);
	PCHECK(sock >= 0) << "socket()";

	{
		int optval = 1;
		PCHECK(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == 0);
	}

	{
		sockaddr_in6 bind_addr = {
			.sin6_family = AF_INET6,
			.sin6_port = htons(port_),
			.sin6_addr = IN6ADDR_LOOPBACK_INIT,
		};
		PCHECK(bind(sock, (sockaddr*) &bind_addr, sizeof(bind_addr)) == 0);
	}

	PCHECK(listen(sock, 128) == 0);
	return sock;
}

} // namespace firecgi
