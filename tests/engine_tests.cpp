#include "engine/engine.hpp"

#include "goma_tests.hpp"

using namespace goma;

namespace {

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
constexpr int kTimeoutSeconds = 2;

SCENARIO("the renderer can render a model", "[engine][renderer]") {
    GIVEN("a valid engine and a model") {
        Engine e;

        GOMA_TEST_TRYV(
            e.LoadScene(GOMA_ASSETS_DIR "models/Lantern/glTF/Lantern.gltf"));
        REQUIRE(e.scene() != nullptr);

        THEN("the renderer can render the model") {
            auto start_time = std::chrono::steady_clock::now();
            auto elapsed_time = std::chrono::steady_clock::now() - start_time;
            int frame = 0;

            GOMA_TEST_TRYV(e.platform().MainLoop([&]() -> result<bool> {
                frame++;
                elapsed_time = std::chrono::steady_clock::now() - start_time;
                if (elapsed_time > std::chrono::seconds(kTimeoutSeconds)) {
                    return true;
                }

                OUTCOME_TRY(e.renderer().Render());
                return false;
            }));

            char* test_name = "Rendering a model (renderer only)";
            SPDLOG_INFO("{} - Average frame time: {:.2f} ms", test_name,
                        elapsed_time.count() / (1e6 * frame));
        }
    }
}
}  // namespace
