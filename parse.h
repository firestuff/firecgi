#pragma once

#include <arpa/inet.h>

namespace firecgi {

struct Header {
	Header(uint8_t type_in, uint16_t request_id, uint16_t content_length);

	uint8_t version = 1;
	uint8_t type;
  private:
	uint16_t request_id_; // network byte order
	uint16_t content_length_; // network byte order
  public:
	uint8_t padding_length = 0;
	uint8_t reserved = 0;

	uint16_t RequestId() const { return ntohs(request_id_); }
	uint16_t ContentLength() const { return ntohs(content_length_); }

	void SetRequestId(uint16_t request_id) { request_id_ = htons(request_id); }
	void SetContentLength(uint16_t content_length) { content_length_ = htons(content_length); }
};

struct BeginRequest {
  private:
	uint16_t role_; // network byte order
  public:
	uint8_t flags;
	uint8_t reserved[5];

	uint16_t Role() const { return ntohs(role_); }
};

struct EndRequest {
	uint32_t app_status = htonl(0); // network byte order
	uint8_t protocol_status;
	uint8_t reserved[3] = {};
};

struct ParamHeader {
	uint8_t key_length;
	uint8_t value_length;
};

} // namespace firecgi
