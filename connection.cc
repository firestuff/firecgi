#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/uio.h>

#include "connection.h"

#include "parse.h"
#include "request.h"

namespace firecgi {

Connection::Connection(int sock, const sockaddr_in6& client_addr, const std::function<void(Request*)>& callback, const std::unordered_set<std::string_view>& headers, int max_request_len)
		: sock_(sock),
		  callback_(callback),
		  headers_(headers),
		  buf_(sock, max_request_len),
		  request_(this) {
	char client_addr_str[INET6_ADDRSTRLEN];
	PCHECK(inet_ntop(AF_INET6, &client_addr.sin6_addr, client_addr_str, sizeof(client_addr_str)));

	LOG(INFO) << "new connection: [" << client_addr_str << "]:" << ntohs(client_addr.sin6_port);
}

Connection::~Connection() {
	PCHECK(close(sock_) == 0);
	LOG(INFO) << "connection closed (handled " << requests_ << " requests)";
}

bool Connection::Write(const std::vector<iovec>& vecs) {
	ssize_t total_size = 0;
	for (const auto& vec : vecs) {
		total_size += vec.iov_len;
	}
	return writev(sock_, vecs.data(), vecs.size()) == total_size;
}

int Connection::Read() {
	if (!buf_.Refill()) {
		return sock_;
	}

	while (true) {
		buf_.ResetRead();

		const auto *header = buf_.ReadObj<Header>();
		if (!header) {
			break;
		}

		if (header->version != 1) {
			LOG(ERROR) << "invalid FastCGI protocol version: " << header->version;
			return sock_;
		}

		if (buf_.ReadMaxLen() < header->ContentLength()) {
			break;
		}

		switch (header->type) {
		  case 1:
		  	{
				if (header->ContentLength() != sizeof(BeginRequest)) {
					LOG(ERROR) << "FCGI_BeginRequestBody is the wrong length: " << header->ContentLength();
					return sock_;
				}

				const auto *begin_request = CHECK_NOTNULL(buf_.ReadObj<BeginRequest>());

				if (begin_request->Role() != 1) {
					LOG(ERROR) << "unsupported FastCGI role: " << begin_request->Role();
					return sock_;
				}

				request_.NewRequest(header->RequestId());
			}
			break;

		  case 4:
		    {
				if (header->RequestId() != request_.RequestId()) {
					LOG(ERROR) << "out of order FCGI_PARAMS record, or client is multiplexing requests (which we don't support)";
					return sock_;
				}

				firebuf::ConstBuffer param_buf(buf_.Read(header->ContentLength()), header->ContentLength());
				while (param_buf.ReadMaxLen() > 0) {
					const auto *param_header = param_buf.ReadObj<ParamHeader>();
					if (!param_header) {
						LOG(ERROR) << "FCGI_PARAMS missing header";
						return sock_;
					}

					const auto *key_buf = param_buf.Read(param_header->key_length);
					if (!key_buf) {
						LOG(ERROR) << "FCGI_PARAMS missing key";
						return sock_;
					}
					std::string_view key(key_buf, param_header->key_length);

					const auto *value_buf = param_buf.Read(param_header->value_length);
					if (!value_buf) {
						LOG(ERROR) << "FCGI_PARAMS missing value";
						return sock_;
					}
					std::string_view value(value_buf, param_header->value_length);

					if (headers_.find(key) != headers_.end()) {
						request_.AddParam(key, value);
					}
				}
			}
			break;

		  case 5:
		  	{
				if (header->RequestId() != request_.RequestId()) {
					LOG(ERROR) << "out of order FCGI_STDIN record, or client is multiplexing requests (which we don't support)";
					return sock_;
				}

				if (header->ContentLength() == 0) {
					// Magic signal for completed request (mirrors the HTTP/1.1 protocol)
					requests_++;
					callback_(&request_);
					buf_.Consume(); // discard data and invalidate pointers
				} else {
					std::string_view in(buf_.Read(header->ContentLength()), header->ContentLength());
					request_.AddIn(in);
				}
			}
			break;

		  default:
			LOG(ERROR) << "unknown record type: " << header->type;
			return sock_;
		}

		if (!buf_.Discard(header->padding_length)) {
			break;
		}

		buf_.Commit(); // we've acted on the bytes read so far
	}

	return -1;
}

} // namespace firecgi
