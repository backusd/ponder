#include <ponder/platform/PlatformEvent.hpp>

#include <concepts>
#include <variant>

static_assert(std::same_as<std::variant_alternative_t<0, ponder::platform::DialogOutcome>, ponder::platform::DialogSelection>);
static_assert(std::same_as<std::variant_alternative_t<1, ponder::platform::DialogOutcome>, ponder::platform::DialogCancellation>);
static_assert(std::same_as<std::variant_alternative_t<2, ponder::platform::DialogOutcome>, ponder::platform::DialogFailure>);
