#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <iomanip>
#include <thread>

#include "fireusage/usage.h"

#include "connection.h"
#include "server.h"

namespace firecgi {

Server::Server(int port, const std::function<void(Request*)>& callback,
               int threads, int max_request_len)
    : port_(port),
      callback_(callback),
      threads_(threads),
      max_request_len_(max_request_len),
      close_fd_(eventfd(0, 0)) {
  CHECK_GE(close_fd_, 0);

  LOG(INFO) << "listening on [::1]:" << port_;
  signal(SIGPIPE, SIG_IGN);
}

Server::~Server() { PCHECK(close(close_fd_) == 0); }

void Server::Serve() {
  std::vector<std::thread> threads;
  for (int i = 0; i < threads_ - 1; ++i) {
    threads.emplace_back([this]() { ServeInt(); });
  }
  ServeInt();
  for (auto& thread : threads) {
    thread.join();
  }
  LOG(INFO) << "all threads shut down";
}

void Server::Shutdown() {
  uint64_t shutdown = 1;
  PCHECK(write(close_fd_, &shutdown, sizeof(shutdown)) == sizeof(shutdown));
}

namespace {
Server* shutdown_server = nullptr;
}  // namespace

void Server::RegisterSignalHandlers() {
  shutdown_server = this;
  for (auto sig : {SIGINT, SIGTERM}) {
    signal(sig, [](int signum) {
      LOG(INFO) << "received " << strsignal(signum);
      shutdown_server->Shutdown();
    });
  }
}

void Server::ServeInt() {
  auto epoll_fd = epoll_create1(0);
  PCHECK(epoll_fd >= 0) << "epoll_create()";

  auto listen_sock = NewListenSock();

  char new_conn;
  {
    struct epoll_event ev {
      .events = EPOLLIN,
      .data = {
          .ptr = &new_conn,
      },
    };
    PCHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &ev) == 0);
  }

  char shutdown;
  {
    struct epoll_event ev {
      .events = EPOLLIN,
      .data = {
          .ptr = &shutdown,
      },
    };
    PCHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, close_fd_, &ev) == 0);
  }

  std::unordered_set<Connection*> connections;

  fireusage::UsageTracker usage_tracker;
  usage_tracker.Start();

  while (true) {
    constexpr auto max_events = 256;
    struct epoll_event events[max_events];
    auto num_fd = epoll_wait(epoll_fd, events, max_events, -1);
    if (num_fd == -1 && errno == EINTR) {
      continue;
    }
    PCHECK(num_fd > 0) << "epoll_wait()";

    for (auto i = 0; i < num_fd; ++i) {
      if (events[i].data.ptr == &new_conn) {
        connections.insert(CHECK_NOTNULL(NewConn(listen_sock, epoll_fd)));
      } else if (events[i].data.ptr == &shutdown) {
        for (auto& conn : connections) {
          usage_tracker.AddEvents(conn->Requests());
          delete conn;
        }
        usage_tracker.Stop();
        PCHECK(close(listen_sock) == 0);
        PCHECK(close(epoll_fd) == 0);
        usage_tracker.Log("requests");
        return;
      } else {
        auto conn = static_cast<Connection*>(events[i].data.ptr);
        auto fd = conn->Read();
        if (fd != -1) {
          PCHECK(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == 0);
          usage_tracker.AddEvents(conn->Requests());
          connections.erase(conn);
          delete conn;
        }
      }
    }
  }
}

Connection* Server::NewConn(int listen_sock, int epoll_fd) {
  sockaddr_in6 client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  auto client_sock =
      accept(listen_sock, (sockaddr*)&client_addr, &client_addr_len);
  PCHECK(client_sock >= 0) << "accept()";
  CHECK_EQ(client_addr.sin6_family, AF_INET6);

  int flags = 1;
  PCHECK(setsockopt(client_sock, SOL_TCP, TCP_NODELAY, &flags, sizeof(flags)) ==
         0);

  auto* conn =
      new Connection(client_sock, client_addr, callback_, max_request_len_);
  {
    struct epoll_event ev {
      .events = EPOLLIN,
      .data = {
          .ptr = conn,
      },
    };
    PCHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &ev) == 0);
  }

  return conn;
}

int Server::NewListenSock() {
  auto sock = socket(AF_INET6, SOCK_STREAM, 0);
  PCHECK(sock >= 0) << "socket()";

  {
    int optval = 1;
    PCHECK(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval,
                      sizeof(optval)) == 0);
  }

  {
    sockaddr_in6 bind_addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(port_),
        .sin6_addr = IN6ADDR_LOOPBACK_INIT,
    };
    PCHECK(bind(sock, (sockaddr*)&bind_addr, sizeof(bind_addr)) == 0);
  }

  PCHECK(listen(sock, 128) == 0);
  return sock;
}

}  // namespace firecgi
