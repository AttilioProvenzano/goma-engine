#include "goma_tests.hpp"

#include "common/gen_vector.hpp"

using namespace goma;

namespace {

SCENARIO("common STL algorithms can be applied to a gen_vector",
         "[infrastructure]") {
    GIVEN("a gen_vector<int>") {
        gen_vector<int> vec;

        WHEN("values are added") {
            vec.reserve(10);

            auto id0 = vec.insert(0);
            int n = 0;
            std::generate_n(std::back_inserter(vec), 8, [&n] {
                ++n;
                return n * n;
            });
            auto id9 = vec.insert(81);

            REQUIRE(!vec.empty());
            REQUIRE(vec.size() == 10);
            REQUIRE(vec.valid_size() == 10);

            THEN("they can be accessed as expected") {
                CHECK(id0 == gen_id{0, 0});
                CHECK(id9 == gen_id{9, 0});

                CHECK(vec.is_valid({3, 0}));
                CHECK(vec[{3, 0}] == 9);

                CHECK(vec.at({5, 0}) == 25);
                CHECK(!vec.is_valid({5, 1}));
                CHECK_THROWS(vec.at({5, 1}));

                CHECK(vec.front() == 0);
                CHECK(vec.back() == 81);

                CHECK(*vec.begin() == 0);
                CHECK(std::distance(vec.begin(), vec.end()) == 10);

                CHECK(*std::next(vec.begin(), 6) == 36);
            }

            THEN("a range-based for loop can be performed on them") {
                int n = 0;
                for (auto&& val : vec) {
                    n += val;
                }

                CHECK(n == std::accumulate(vec.begin(), vec.end(), 0));
            }

            AND_WHEN("some elements are removed") {
                vec.erase(vec.begin());
                vec.erase({2, 0});
                vec.erase(decltype(vec)::iterator(&vec, 5),
                          decltype(vec)::iterator(&vec, 7));

                // Erasing across already removed values
                vec.erase({6, 0});
                vec.erase(decltype(vec)::iterator(&vec, 6),
                          decltype(vec)::iterator(&vec, 9));

                THEN("the remaining values are the expected ones") {
                    CHECK(vec.size() == 10);
                    CHECK(vec.valid_size() == 4);

                    std::vector<int> expected_values = {1, 9, 16, 81};
                    std::vector<int> actual_values;
                    std::copy(vec.begin(), vec.end(),
                              std::back_inserter(actual_values));
                    CHECK(actual_values == expected_values);
                }

                AND_WHEN("the last element is erased as well") {
                    // Erasing the last element should cause last_valid_id_ to
                    // move back a few steps
                    vec.erase({9, 0});

                    THEN("valid ids are still handled correctly") {
                        CHECK(vec.valid_size() == 3);
                        CHECK(vec.back() == 16);

                        std::vector<int> expected_values = {1, 9, 16};
                        std::vector<int> actual_values;
                        std::copy(vec.begin(), vec.end(),
                                  std::back_inserter(actual_values));
                        CHECK(actual_values == expected_values);
                    }
                }

                AND_WHEN("an element is inserted back") {
                    auto answer = vec.insert(42);

                    THEN("it gets the expected gen_id") {
                        CHECK(vec.size() == 10);
                        CHECK(vec.valid_size() == 5);

                        // Note: 8 is the last index previously removed
                        CHECK(answer == gen_id{8, 1});
                    }
                }
            }

            AND_WHEN("it is resized to a larger value") {
                vec.resize(20, 1000);

                THEN("values are as expected") {
                    CHECK(vec.size() == 20);
                    CHECK(vec.valid_size() == 20);

                    CHECK(vec.at({5, 0}) == 25);
                    CHECK(vec.at({15, 0}) == 1000);
                }
            }

            AND_WHEN("it is resized to a smaller value") {
                vec.resize(4);

                THEN("values are as expected") {
                    CHECK(vec.size() == 4);
                    CHECK(vec.valid_size() == 4);

                    CHECK(vec.at({3, 0}) == 9);
                    CHECK_THROWS(vec.at({6, 0}));
                }
            }
        }
    }
}

}  // namespace
