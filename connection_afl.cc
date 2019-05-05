#include "connection.h"

int main(int argc, char* argv[]) {
	FLAGS_logtostderr = 1;
	FLAGS_minloglevel = 3;
	google::InitGoogleLogging(argv[0]);
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	{
		firecgi::Connection conn(STDIN_FILENO, {}, [](std::unique_ptr<firecgi::Request> req) { req->End(); }, {});
		static_cast<void>(conn.Read());
	}

	gflags::ShutDownCommandLineFlags();
	google::ShutdownGoogleLogging();

	return 0;
}
