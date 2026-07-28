#include <ponder/platform/Keyboard.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

namespace
{
constexpr ponder::platform::LogicalKey kUnknownLogicalKey;
constexpr ponder::platform::LogicalKey kCharacterLogicalKey = ponder::platform::LogicalKey::FromCharacter(U'\u03bb');
constexpr ponder::platform::LogicalKey kNamedLogicalKey = ponder::platform::LogicalKey::FromNamed(ponder::platform::NamedKey::Enter);

static_assert(kUnknownLogicalKey.GetKind() == ponder::platform::LogicalKey::Kind::Unknown);
static_assert(kCharacterLogicalKey.GetCharacter() == U'\u03bb');
static_assert(kNamedLogicalKey.GetNamedKey() == ponder::platform::NamedKey::Enter);
static_assert(ponder::platform::LogicalKey::FromCharacter(U'\0').GetKind() == ponder::platform::LogicalKey::Kind::Unknown);
static_assert(ponder::platform::LogicalKey::FromCharacter(static_cast<char32_t>(0xD800U)).GetKind() == ponder::platform::LogicalKey::Kind::Unknown);
static_assert(ponder::platform::LogicalKey::FromCharacter(static_cast<char32_t>(0x110000U)).GetKind() == ponder::platform::LogicalKey::Kind::Unknown);

TEST(KeyboardTests, RepresentsKnownAndUnknownPhysicalAndLogicalKeys)
{
    EXPECT_EQ(ponder::platform::PhysicalKey{}, ponder::platform::PhysicalKey::Unknown);
    EXPECT_EQ(ponder::platform::NamedKey{}, ponder::platform::NamedKey::Unknown);
    EXPECT_EQ(kUnknownLogicalKey, ponder::platform::LogicalKey::Unknown());
    EXPECT_FALSE(kUnknownLogicalKey.GetCharacter().has_value());
    EXPECT_FALSE(kUnknownLogicalKey.GetNamedKey().has_value());

    ASSERT_TRUE(kCharacterLogicalKey.GetCharacter().has_value());
    EXPECT_EQ(*kCharacterLogicalKey.GetCharacter(), U'\u03bb');
    EXPECT_FALSE(kCharacterLogicalKey.GetNamedKey().has_value());

    ASSERT_TRUE(kNamedLogicalKey.GetNamedKey().has_value());
    EXPECT_EQ(*kNamedLogicalKey.GetNamedKey(), ponder::platform::NamedKey::Enter);
    EXPECT_FALSE(kNamedLogicalKey.GetCharacter().has_value());

    const auto forged = static_cast<ponder::platform::NamedKey>(std::numeric_limits<std::uint16_t>::max());
    EXPECT_EQ(ponder::platform::LogicalKey::FromNamed(forged), ponder::platform::LogicalKey::Unknown());
}

TEST(KeyboardTests, CombinesAndQueriesIndependentModifierBits)
{
    using ponder::platform::KeyModifiers;

    KeyModifiers modifiers = KeyModifiers::LeftControl | KeyModifiers::RightShift;
    modifiers |= KeyModifiers::CapsLock;

    EXPECT_TRUE(ponder::platform::HasAnyKeyModifier(modifiers, KeyModifiers::Control));
    EXPECT_TRUE(ponder::platform::HasAnyKeyModifier(modifiers, KeyModifiers::Shift));
    EXPECT_TRUE(ponder::platform::HasAllKeyModifiers(modifiers, KeyModifiers::LeftControl | KeyModifiers::RightShift));
    EXPECT_FALSE(ponder::platform::HasAnyKeyModifier(modifiers, KeyModifiers::Alt));
    EXPECT_FALSE(ponder::platform::HasAllKeyModifiers(modifiers, KeyModifiers::Shift));

    modifiers &= KeyModifiers::Control;
    EXPECT_EQ(modifiers, KeyModifiers::LeftControl);
}
} // namespace
