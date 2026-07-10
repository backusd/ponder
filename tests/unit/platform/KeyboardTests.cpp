#include <ponder/platform/Keyboard.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

namespace
{
constexpr pond::platform::LogicalKey kUnknownLogicalKey;
constexpr pond::platform::LogicalKey kCharacterLogicalKey =
    pond::platform::LogicalKey::FromCharacter(U'\u03bb');
constexpr pond::platform::LogicalKey kNamedLogicalKey =
    pond::platform::LogicalKey::FromNamed(pond::platform::NamedKey::Enter);

static_assert(kUnknownLogicalKey.GetKind() == pond::platform::LogicalKey::Kind::Unknown);
static_assert(kCharacterLogicalKey.GetCharacter() == U'\u03bb');
static_assert(kNamedLogicalKey.GetNamedKey() == pond::platform::NamedKey::Enter);
static_assert(pond::platform::LogicalKey::FromCharacter(U'\0').GetKind() ==
              pond::platform::LogicalKey::Kind::Unknown);
static_assert(pond::platform::LogicalKey::FromCharacter(static_cast<char32_t>(0xD800U)).GetKind() ==
              pond::platform::LogicalKey::Kind::Unknown);
static_assert(pond::platform::LogicalKey::FromCharacter(static_cast<char32_t>(0x110000U))
                  .GetKind() == pond::platform::LogicalKey::Kind::Unknown);

TEST(KeyboardTests, RepresentsKnownAndUnknownPhysicalAndLogicalKeys)
{
    EXPECT_EQ(pond::platform::PhysicalKey{}, pond::platform::PhysicalKey::Unknown);
    EXPECT_EQ(pond::platform::NamedKey{}, pond::platform::NamedKey::Unknown);
    EXPECT_EQ(kUnknownLogicalKey, pond::platform::LogicalKey::Unknown());
    EXPECT_FALSE(kUnknownLogicalKey.GetCharacter().has_value());
    EXPECT_FALSE(kUnknownLogicalKey.GetNamedKey().has_value());

    ASSERT_TRUE(kCharacterLogicalKey.GetCharacter().has_value());
    EXPECT_EQ(*kCharacterLogicalKey.GetCharacter(), U'\u03bb');
    EXPECT_FALSE(kCharacterLogicalKey.GetNamedKey().has_value());

    ASSERT_TRUE(kNamedLogicalKey.GetNamedKey().has_value());
    EXPECT_EQ(*kNamedLogicalKey.GetNamedKey(), pond::platform::NamedKey::Enter);
    EXPECT_FALSE(kNamedLogicalKey.GetCharacter().has_value());

    const auto forged =
        static_cast<pond::platform::NamedKey>(std::numeric_limits<std::uint16_t>::max());
    EXPECT_EQ(pond::platform::LogicalKey::FromNamed(forged), pond::platform::LogicalKey::Unknown());
}

TEST(KeyboardTests, CombinesAndQueriesIndependentModifierBits)
{
    using pond::platform::KeyModifiers;

    KeyModifiers modifiers = KeyModifiers::LeftControl | KeyModifiers::RightShift;
    modifiers |= KeyModifiers::CapsLock;

    EXPECT_TRUE(pond::platform::HasAnyKeyModifier(modifiers, KeyModifiers::Control));
    EXPECT_TRUE(pond::platform::HasAnyKeyModifier(modifiers, KeyModifiers::Shift));
    EXPECT_TRUE(pond::platform::HasAllKeyModifiers(modifiers, KeyModifiers::LeftControl |
                                                                  KeyModifiers::RightShift));
    EXPECT_FALSE(pond::platform::HasAnyKeyModifier(modifiers, KeyModifiers::Alt));
    EXPECT_FALSE(pond::platform::HasAllKeyModifiers(modifiers, KeyModifiers::Shift));

    modifiers &= KeyModifiers::Control;
    EXPECT_EQ(modifiers, KeyModifiers::LeftControl);
}
} // namespace
