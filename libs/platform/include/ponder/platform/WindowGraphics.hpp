#pragma once

#include <cstdint>
#include <format>
#include <ostream>
#include <string_view>

namespace ponder::platform
{
enum class WindowGraphicsCompatibility : std::uint8_t
{
    Default = 0,
    Vulkan = 1,
    Metal = 2
};

[[nodiscard]] constexpr std::string_view GetWindowGraphicsCompatibilityName(WindowGraphicsCompatibility compatibility) noexcept
{
    switch (compatibility)
    {
    case WindowGraphicsCompatibility::Default:
        return "default";
    case WindowGraphicsCompatibility::Vulkan:
        return "vulkan";
    case WindowGraphicsCompatibility::Metal:
        return "metal";
    }

    return "unrecognized";
}

inline std::ostream& operator<<(std::ostream& output, WindowGraphicsCompatibility compatibility)
{
    return output << GetWindowGraphicsCompatibilityName(compatibility);
}
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::WindowGraphicsCompatibility> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(ponder::platform::WindowGraphicsCompatibility compatibility, FormatContext& context) const
    {
        return formatter<string_view>::format(ponder::platform::GetWindowGraphicsCompatibilityName(compatibility), context);
    }
};
} // namespace std