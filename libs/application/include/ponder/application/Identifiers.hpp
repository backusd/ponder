#pragma once

#include <ponder/core/Identifier.hpp>

#include <format>
#include <ostream>
#include <string>

namespace ponder::application
{
namespace detail
{
struct BackgroundProcessIdTag final
{
};
} // namespace detail

using BackgroundProcessId = ponder::core::Identifier<detail::BackgroundProcessIdTag>;
} // namespace ponder::application

namespace std
{
template <>
struct formatter<ponder::application::BackgroundProcessId> : formatter<string>
{
    template <typename FormatContext>
    auto format(ponder::application::BackgroundProcessId id, FormatContext& context) const
    {
        const string text = id.IsValid() ? std::format("{}", id.GetValue()) : "invalid";
        return formatter<string>::format(text, context);
    }
};
} // namespace std

namespace ponder::application::detail
{
inline std::ostream& operator<<(std::ostream& output, BackgroundProcessId id)
{
    return output << std::format("{}", id);
}
} // namespace ponder::application::detail
