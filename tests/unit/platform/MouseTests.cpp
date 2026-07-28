#include <ponder/platform/Mouse.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

namespace
{
static_assert(std::is_scoped_enum_v<ponder::platform::MouseButton>);
static_assert(std::is_same_v<std::underlying_type_t<ponder::platform::MouseButton>, std::uint8_t>);
static_assert(std::is_scoped_enum_v<ponder::platform::SystemCursorShape>);
static_assert(std::is_same_v<std::underlying_type_t<ponder::platform::SystemCursorShape>, std::uint8_t>);
static_assert(ponder::platform::MouseButton{} == ponder::platform::MouseButton::Unknown);
static_assert(ponder::platform::SystemCursorShape{} == ponder::platform::SystemCursorShape::Default);
static_assert(std::is_aggregate_v<ponder::platform::MousePosition>);
static_assert(ponder::platform::MousePosition{} == ponder::platform::MousePosition{0.0F, 0.0F});

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
    constexpr std::array buttons{ponder::platform::MouseButton::Unknown, ponder::platform::MouseButton::Left, ponder::platform::MouseButton::Right,
                                 ponder::platform::MouseButton::Middle,  ponder::platform::MouseButton::X1,   ponder::platform::MouseButton::X2};

    ExpectUnique(buttons);
}

TEST(MouseTests, DefinesDistinctSupportedSystemCursorShapes)
{
    constexpr std::array shapes{ponder::platform::SystemCursorShape::Default,
                                ponder::platform::SystemCursorShape::TextInput,
                                ponder::platform::SystemCursorShape::Move,
                                ponder::platform::SystemCursorShape::ResizeNorthSouth,
                                ponder::platform::SystemCursorShape::ResizeEastWest,
                                ponder::platform::SystemCursorShape::ResizeNortheastSouthwest,
                                ponder::platform::SystemCursorShape::ResizeNorthwestSoutheast,
                                ponder::platform::SystemCursorShape::Pointer,
                                ponder::platform::SystemCursorShape::Wait,
                                ponder::platform::SystemCursorShape::Progress,
                                ponder::platform::SystemCursorShape::NotAllowed};

    ExpectUnique(shapes);
}
TEST(MouseTests, StoresBackendMouseCoordinatesAsAValue)
{
    constexpr ponder::platform::MousePosition position{-12.5F, 300.25F};
    EXPECT_FLOAT_EQ(position.x, -12.5F);
    EXPECT_FLOAT_EQ(position.y, 300.25F);
}
} // namespace
