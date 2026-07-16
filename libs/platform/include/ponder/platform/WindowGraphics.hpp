#pragma once

#include <cstdint>
#include <format>
#include <ostream>
#include <string_view>

namespace pond::platform
{
enum class WindowGraphicsCompatibility : std::uint8_t
{
    Default = 0,
    Vulkan = 1,
    Metal = 2
};

[[nodiscard]] constexpr std::string_view GetWindowGraphicsCompatibilityName(
    WindowGraphicsCompatibility compatibility) noexcept
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
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::WindowGraphicsCompatibility> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowGraphicsCompatibility compatibility,
                FormatContext& context) const
    {
        return formatter<string_view>::format(
            pond::platform::GetWindowGraphicsCompatibilityName(compatibility), context);
    }
};
} // namespace std