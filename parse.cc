#include "parse.h"

namespace firecgi {

Header::Header(uint8_t type_in, uint16_t request_id, uint16_t content_length)
		: type(type_in) {
	SetRequestId(request_id);
	SetContentLength(content_length);
}

} // namespace firecgi
