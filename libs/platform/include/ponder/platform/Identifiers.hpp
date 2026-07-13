#pragma once

#include <ponder/core/Identifier.hpp>

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