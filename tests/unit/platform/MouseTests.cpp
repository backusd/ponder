#include <ponder/platform/Mouse.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
static_assert(std::is_scoped_enum_v<pond::platform::MouseButton>);
static_assert(std::is_same_v<std::underlying_type_t<pond::platform::MouseButton>, std::uint8_t>);
static_assert(std::is_scoped_enum_v<pond::platform::SystemCursorShape>);
static_assert(
    std::is_same_v<std::underlying_type_t<pond::platform::SystemCursorShape>, std::uint8_t>);
static_assert(pond::platform::MouseButton{} == pond::platform::MouseButton::Unknown);
static_assert(pond::platform::SystemCursorShape{} == pond::platform::SystemCursorShape::Default);

template <typename Value, std::size_t Size>
void ExpectUnique(const std::array<Value, Size>& values)
{
    for (std::size_t lhsIndex = 0; lhsIndex < values.size(); ++lhsIndex)
    {
        for (std::size_t rhsIndex = lhsIndex + 1; rhsIndex < values.size(); ++rhsIndex)
        {
            EXPECT_NE(values[lhsIndex], values[rhsIndex]);
        }
    }
}

TEST(MouseTests, DefinesDistinctSupportedButtons)
{
    constexpr std::array buttons{
        pond::platform::MouseButton::Unknown, pond::platform::MouseButton::Left,
        pond::platform::MouseButton::Right,   pond::platform::MouseButton::Middle,
        pond::platform::MouseButton::X1,      pond::platform::MouseButton::X2};

    ExpectUnique(buttons);
}

TEST(MouseTests, DefinesDistinctSupportedSystemCursorShapes)
{
    constexpr std::array shapes{pond::platform::SystemCursorShape::Default,
                                pond::platform::SystemCursorShape::TextInput,
                                pond::platform::SystemCursorShape::Move,
                                pond::platform::SystemCursorShape::ResizeNorthSouth,
                                pond::platform::SystemCursorShape::ResizeEastWest,
                                pond::platform::SystemCursorShape::ResizeNortheastSouthwest,
                                pond::platform::SystemCursorShape::ResizeNorthwestSoutheast,
                                pond::platform::SystemCursorShape::Pointer,
                                pond::platform::SystemCursorShape::Wait,
                                pond::platform::SystemCursorShape::Progress,
                                pond::platform::SystemCursorShape::NotAllowed};

    ExpectUnique(shapes);
}
} // namespace
