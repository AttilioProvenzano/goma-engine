#pragma once

#include <catch.hpp>

#ifndef GOMA_ASSETS_DIR
#define GOMA_ASSETS_DIR "assets/"
#endif

#ifndef GOMA_TEST_TRYV
#define GOMA_TEST_TRYV(fn)                                         \
    {                                                              \
        auto res = fn;                                             \
        if (res.has_error()) UNSCOPED_INFO(res.error().message()); \
        REQUIRE(!res.has_error());                                 \
    }
#endif

#ifndef GOMA_TEST_TRY
#define GOMA_TEST_TRY(handle, fn)                      \
    auto handle##_res = fn;                            \
    if (handle##_res.has_error())                      \
        UNSCOPED_INFO(handle##_res.error().message()); \
    REQUIRE(!handle##_res.has_error());                \
    auto& handle = handle##_res.value();
#endif
