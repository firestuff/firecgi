#pragma once

#include <functional>
#include <sys/uio.h>
#include <unordered_map>
#include <unordered_set>

#include "firebuf/stream_buffer.h"

#include "request.h"

namespace firecgi {

class Connection {
  public:
	Connection(int sock, const sockaddr_in6& client_addr, const std::function<void(std::unique_ptr<Request>)>& callback, const std::unordered_set<std::string_view>& headers);
	~Connection();

	[[nodiscard]] int Read();
	[[nodiscard]] bool Write(const std::vector<iovec>& vecs);

  private:
  	const int sock_;
	const std::function<void(std::unique_ptr<Request>)>& callback_;
	const std::unordered_set<std::string_view>& headers_;

	uint64_t requests_ = 0;

	firebuf::StreamBuffer buf_;

	std::unique_ptr<Request> request_;
};

} // namespace firecgi
