#include <ponder/application/Application.hpp>

#include <concepts>
#include <utility>

static_assert(std::same_as<decltype(std::declval<ponder::application::Application&>().Run()), int>);
