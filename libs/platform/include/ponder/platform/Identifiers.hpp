#pragma once

#include <ponder/core/Identifier.hpp>

#include <format>
#include <ostream>
#include <string>

namespace ponder::platform
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

using WindowId = ponder::core::Identifier<detail::WindowIdTag>;
using DisplayId = ponder::core::Identifier<detail::DisplayIdTag>;
using DialogRequestId = ponder::core::Identifier<detail::DialogRequestIdTag>;
} // namespace ponder::platform

namespace std
{
template <>
struct formatter<ponder::platform::WindowId> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::WindowId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::DisplayId> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::DisplayId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};

template <>
struct formatter<ponder::platform::DialogRequestId> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::platform::DialogRequestId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};
} // namespace std

namespace ponder::platform::detail
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
} // namespace ponder::platform::detail
