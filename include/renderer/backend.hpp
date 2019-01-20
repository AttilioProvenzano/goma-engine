#pragma once

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;
#include <vector>

namespace goma {

class Engine;

class Backend {
  public:
    Backend(Engine* engine) : engine_(engine) {}
    virtual ~Backend() = default;

    virtual result<void> InitContext() = 0;

  protected:
    Engine* engine_ = nullptr;
};

}  // namespace goma
