#include <sys/uio.h>

#include "request.h"

#include "connection.h"

namespace firecgi {
namespace {

template<class T> void AppendVec(const T& obj, std::vector<iovec>* vec) {
	vec->push_back(iovec{
		.iov_base = (void*)(&obj),
		.iov_len = sizeof(obj),
	});
}

} // namespace

Request::Request(Connection* conn)
		: conn_(conn),
		  out_buf_(64*1024) {}

void Request::NewRequest(uint16_t request_id) {
	request_id_ = request_id;
	params_.clear();
	body_ = {};
	out_buf_.Reset();
	body_written_ = false;
}

uint16_t Request::RequestId() {
	return request_id_;
}

void Request::AddParam(const std::string_view& key, const std::string_view& value) {
	params_.try_emplace(std::string(key), std::string(value));
}

void Request::SetBody(const std::string_view& body) {
	body_ = body;
}

const std::string_view& Request::GetParam(const std::string& key) {
	auto iter = params_.find(key);
	if (iter == params_.end()) {
		static const std::string_view none;
		return none;
	}
	return iter->second;
}

const std::string_view& Request::GetBody() {
	return body_;
}

void Request::WriteHeader(const std::string_view& name, const std::string_view& value) {
	std::lock_guard<std::mutex> l(output_mu_);

	CHECK(!body_written_);
	CHECK(out_buf_.Write(name));
	CHECK(out_buf_.Write(": "));
	CHECK(out_buf_.Write(value));
	CHECK(out_buf_.Write("\n"));
}

void Request::WriteBody(const std::string_view& body) {
	std::lock_guard<std::mutex> l(output_mu_);
	WriteBodyLocked(body);
}

bool Request::Flush() {
	std::lock_guard<std::mutex> l(output_mu_);

	std::vector<iovec> vecs;

	auto header = OutputHeader();
	AppendVec(header, &vecs);

	vecs.push_back(OutputVec());

	if (!conn_->Write(vecs)) {
		return false;
	}
	out_buf_.Commit();
	out_buf_.Consume();
	return true;
}

bool Request::End() {
	std::lock_guard<std::mutex> l(output_mu_);

	WriteBodyLocked("");

	std::vector<iovec> vecs;

	// Must be outside if block, so it lives through Write() below
	auto output_header = OutputHeader();
	if (output_header.ContentLength()) {
		AppendVec(output_header, &vecs);
		vecs.push_back(OutputVec());
	}

	EndRequest end;
	Header end_header(3, request_id_, sizeof(end));
	AppendVec(end_header, &vecs);
	AppendVec(end, &vecs);

	return conn_->Write(vecs);
}

iovec Request::OutputVec() {
	const auto output_len = out_buf_.ReadMaxLen();
	return iovec{
		.iov_base = (void *)(CHECK_NOTNULL(out_buf_.Read(output_len))),
		.iov_len = output_len,
	};
}

Header Request::OutputHeader() {
	return Header(6, request_id_, out_buf_.ReadMaxLen());
}

void Request::WriteBodyLocked(const std::string_view& body) {
	CHECK(!output_mu_.try_lock());

	if (!body_written_) {
		CHECK(out_buf_.Write("\n"));
		body_written_ = true;
	}
	// TODO: make this able to span multiple packets
	CHECK(out_buf_.Write(body));
}

} // namespace firecgi
