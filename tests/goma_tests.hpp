#pragma once

#include <catch.hpp>

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
