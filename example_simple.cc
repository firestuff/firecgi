#include <gflags/gflags.h>
#include <glog/logging.h>

#include "server.h"

DEFINE_int32(port, 9000, "TCP port to bind");
DEFINE_int32(threads, 1, "Number of server threads");

int main(int argc, char *argv[]) {
	google::InitGoogleLogging(argv[0]);
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	firecgi::Server server(FLAGS_port, [](firecgi::Request* request) {
		request->WriteHeader("Content-Type", "text/plain");
		request->WriteBody("Hello world");
		request->End();
	}, FLAGS_threads);
	server.RegisterSignalHandlers();
	server.Serve();

	gflags::ShutDownCommandLineFlags();
	google::ShutdownGoogleLogging();
}
