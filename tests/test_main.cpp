#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

#include "goma_tests.hpp"

// Default values
int TestOptions::timeout = 2;

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);

    Catch::Session session;  // There must be exactly one instance

    // Build a new parser on top of Catch's
    using namespace Catch::clara;
    auto cli = session.cli()  // Get Catch's composite command line parser
               | Opt(TestOptions::timeout, "timeout")["--timeout"](
                     "timeout in seconds for each test");

    // Now pass the new composite back to Catch so it uses that
    session.cli(cli);

    // Let Catch (using Clara) parse the command line
    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0)  // Indicates a command line error
        return returnCode;

    return session.run();
}