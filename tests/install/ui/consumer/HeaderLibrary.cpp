#include <ponder/ui/Library.hpp>

#include <string_view>
#include <type_traits>

static_assert(noexcept(pond::ui::GetLibraryName()));
static_assert(std::is_same_v<decltype(pond::ui::GetLibraryName()), std::string_view>);
