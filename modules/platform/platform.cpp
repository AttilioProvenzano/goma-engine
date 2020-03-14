#include "platform/platform.hpp"
#include "platform/win32_platform.hpp"

namespace goma {

result<std::string> Platform::ReadFile(const std::string& filename,
                                       bool binary) {
#ifdef WIN32
    return Win32Platform::ReadFile(filename, binary);
#endif
}

result<void> Platform::WriteFile(const std::string& filename, size_t size,
                                 const char* data, bool binary) {
#ifdef WIN32
    return Win32Platform::WriteFile(filename, size, data, binary);
#endif
}

}  // namespace goma
