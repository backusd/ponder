#include <ponder/platform/TextInput.hpp>

#include <gtest/gtest.h>
#include <limits>

namespace
{
static_assert(ponder::platform::TextCompositionRange{} == ponder::platform::TextCompositionRange{0, 0});
static_assert(ponder::platform::IsValid(ponder::platform::TextInputArea{}));

TEST(TextInputTests, DistinguishesZeroLengthAndNonzeroCompositionRanges)
{
    const ponder::platform::TextCompositionRange insertion{4, 0};
    const ponder::platform::TextCompositionRange replacement{4, 3};

    EXPECT_EQ(insertion.start, 4U);
    EXPECT_EQ(insertion.length, 0U);
    EXPECT_NE(insertion, replacement);
}

TEST(TextInputTests, ValidatesLogicalAreaAndCursorValues)
{
    const ponder::platform::TextInputArea valid{.rectangle = {{-12.5F, 4.25F}, {180.0F, 24.0F}}, .cursorOffset = 30.5F};
    EXPECT_TRUE(ponder::platform::IsValid(valid));

    ponder::platform::TextInputArea invalid = valid;
    invalid.rectangle.extent.width = -1.0F;
    EXPECT_FALSE(ponder::platform::IsValid(invalid));

    invalid = valid;
    invalid.rectangle.origin.y = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(ponder::platform::IsValid(invalid));

    invalid = valid;
    invalid.cursorOffset = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(ponder::platform::IsValid(invalid));
}
} // namespace
