#include <ponder/platform/Process.hpp>

#include <concepts>
#include <utility>

static_assert(std::same_as<decltype(ponder::platform::LaunchProcess(std::declval<const ponder::platform::ProcessDesc&>())),
                           ponder::core::Result<ponder::platform::Process>>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Process&>().Wait()), ponder::core::Result<ponder::platform::ProcessExitStatus>>);
static_assert(std::same_as<decltype(std::declval<ponder::platform::Process&>().Terminate(ponder::platform::ProcessTerminationMode::Force)),
                           ponder::core::VoidResult>);
