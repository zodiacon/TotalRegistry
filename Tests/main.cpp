#include "pch.h"
#include <catch2/catch_session.hpp>
#include "AppSettings.h"
#include "TestHelpers.h"

// globals the app's sources expect
CAppModule _Module;
AppSettings _Settings;

int main(int argc, char* argv[]) {
	auto result = Catch::Session().run(argc, argv);
	TestHelpers::Cleanup();
	return result;
}
