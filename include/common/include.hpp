#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_XYZW_ONLY
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <outcome.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;
using outcome::result;

#include <mapbox/variant.hpp>
using mapbox::util::variant;

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#define _USE_MATH_DEFINES
#include <math.h>
