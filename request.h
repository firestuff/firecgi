#pragma once

#include <mutex>
#include <unordered_map>

#include "firebuf/buffer.h"

#include "parse.h"

namespace firecgi {

class Connection;

class Request {
  public:
	Request(Connection *conn);

	void NewRequest(uint16_t request_id);

	uint16_t RequestId();

	void AddParam(const std::string_view& key, const std::string_view& value);
	void SetBody(const std::string_view& in);

	const std::string_view& GetParam(const std::string& key);
	const std::string_view& GetBody();

	void WriteHeader(const std::string_view& name, const std::string_view& value);
	void WriteBody(const std::string_view& body);
	[[nodiscard]] bool Flush();
	bool End();

  private:
	Header OutputHeader();
	iovec OutputVec();

	Connection *conn_;
	uint16_t request_id_ = 0;

	std::unordered_map<std::string_view, std::string_view> params_;
	std::string_view body_;

	firebuf::Buffer out_buf_;
	bool body_written_;
	std::mutex output_mu_;
};

} // namespace firecgi
