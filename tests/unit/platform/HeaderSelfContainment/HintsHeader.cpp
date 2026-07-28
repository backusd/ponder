#include <ponder/platform/Hints.hpp>

#include <concepts>
#include <format>
#include <ostream>
#include <utility>

namespace
{
template <typename Hint>
concept HintValueContract = std::equality_comparable<Hint> && std::formattable<Hint, char> && requires(std::ostream& output, const Hint& hint) {
    hint.value;
    { output << hint } -> std::same_as<std::ostream&>;
};

static_assert(HintValueContract<ponder::platform::hints::MouseFocusClickThrough>);
static_assert(HintValueContract<ponder::platform::hints::EventLogging>);
static_assert(HintValueContract<ponder::platform::hints::VideoDriver>);
} // namespace
