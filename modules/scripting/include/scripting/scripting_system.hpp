#pragma once

#include "scripting/script.hpp"

#include "common/include.hpp"

namespace goma {

class Engine;

class ScriptingSystem {
  public:
    ScriptingSystem(Engine& engine) : engine_(engine) {}

    template <typename T>
    void RegisterScript(T&& script) {
        scripts_.push_back(std::make_unique<T>(std::forward<T>(script)));
    }

    void Update(float delta_time) {
        for (auto& script : scripts_) {
            script->Update(engine_, delta_time);
        }
    };

  private:
    Engine& engine_;

    std::vector<std::unique_ptr<Script>> scripts_{};
};

}  // namespace goma
