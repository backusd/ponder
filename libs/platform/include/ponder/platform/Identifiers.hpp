#pragma once

#include <ponder/core/Identifier.hpp>

#include <format>
#include <ostream>
#include <string>

namespace pond::platform
{
namespace detail
{
struct WindowIdTag final
{
};
struct DisplayIdTag final
{
};
struct DialogRequestIdTag final
{
};
} // namespace detail

using WindowId = core::Identifier<detail::WindowIdTag>;
using DisplayId = core::Identifier<detail::DisplayIdTag>;
using DialogRequestId = core::Identifier<detail::DialogRequestIdTag>;
} // namespace pond::platform

namespace std
{
template <>
struct formatter<pond::platform::WindowId> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::WindowId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<pond::platform::DisplayId> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::DisplayId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<pond::platform::DialogRequestId> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::platform::DialogRequestId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};
} // namespace std

namespace pond::platform::detail
{
inline std::ostream& operator<<(std::ostream& output, WindowId id)
{
    return output << std::format("{}", id);
}

inline std::ostream& operator<<(std::ostream& output, DisplayId id)
{
    return output << std::format("{}", id);
}

inline std::ostream& operator<<(std::ostream& output, DialogRequestId id)
{
    return output << std::format("{}", id);
}
} // namespace pond::platform::detail