#include "engine/engine.hpp"

#include "goma_tests.hpp"

using namespace goma;

namespace {

constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;

SCENARIO("the renderer can render a model", "[engine][renderer]") {
    GIVEN("a valid engine and a model") {
        Engine e;

        GOMA_TEST_TRYV(
            e.LoadScene(GOMA_ASSETS_DIR "models/Lantern/glTF/Lantern.gltf"));
        REQUIRE(e.scene() != nullptr);

        THEN("the renderer can render the model") {
            RenderingBenchmark rb;

            rb.run("Rendering a model (renderer only)", [&](int& frame) {
                GOMA_TEST_TRYV(e.platform().MainLoop([&]() -> result<bool> {
                    if (rb.elapsed_time() >
                        std::chrono::seconds(TestOptions::timeout)) {
                        return true;
                    }

                    OUTCOME_TRY(e.renderer().Render());
                    frame++;
                    return false;
                }));
            });
        }
    }
}
}  // namespace
