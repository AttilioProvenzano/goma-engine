#pragma once

#include <catch.hpp>
#include "spdlog/spdlog.h"

struct TestOptions {
    static int timeout;
};

#ifndef GOMA_ASSETS_DIR
#define GOMA_ASSETS_DIR "assets/"
#endif

#ifndef GOMA_TEST_TRYV
#define GOMA_TEST_TRYV(fn)                                        \
    {                                                             \
        auto res = fn;                                            \
        if (res.has_error()) {                                    \
            auto err = res.error();                               \
            UNSCOPED_INFO("In " << #fn << ": " << err.message()); \
        }                                                         \
        REQUIRE(!res.has_error());                                \
    }
#endif

#ifndef GOMA_TEST_TRY
#define GOMA_TEST_TRY(handle, fn)                                          \
    auto handle##_res = fn;                                                \
    {                                                                      \
        bool handle##_has_err = handle##_res.has_error();                  \
        if (handle##_has_err) {                                            \
            auto handle##_err = handle##_res.error();                      \
            UNSCOPED_INFO("In " << #fn << ": " << handle##_err.message()); \
        }                                                                  \
        REQUIRE(!handle##_has_err);                                        \
    }                                                                      \
    auto& handle = handle##_res.value();
#endif

class RenderingBenchmark {
  public:
    void run(const char* test_name, std::function<void(int&)> fn) {
        start_time = std::chrono::steady_clock::now();

        int frame = 0;
        fn(frame);

        if (!test_name) {
            test_name = "Benchmark";
        }

        if (frame > 0) {
            SPDLOG_INFO("{} - Average frame time: {:.2f} ms", test_name,
                        elapsed_time().count() / (1e6 * frame));
        } else {
            SPDLOG_INFO("{} - No frames rendered", test_name);
        }
    }

    std::chrono::nanoseconds elapsed_time() const {
        return std::chrono::steady_clock::now() - start_time;
    }

  protected:
    std::chrono::steady_clock::time_point start_time =
        std::chrono::steady_clock::now();
};
