#include <ponder/platform/Window.hpp>

#include <concepts>
#include <utility>

static_assert(
    std::same_as<decltype(std::declval<const ponder::platform::Window&>().GetDisplayId()), ponder::core::Result<ponder::platform::DisplayId>>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Window&>().GetNativeHandle()),
                           ponder::core::Result<ponder::platform::NativeWindowHandle>>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Window&>().GetPixelDensity()), float>);
static_assert(std::same_as<decltype(std::declval<const ponder::platform::Window&>().GetDisplayScale()), float>);
