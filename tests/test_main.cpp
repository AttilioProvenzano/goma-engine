#define CATCH_CONFIG_RUNNER
#include <catch.hpp>

#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::debug);
    int result = Catch::Session().run(argc, argv);
    return result;
}