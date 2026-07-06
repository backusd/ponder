#include <ponder/core/Uuid.hpp>

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <unordered_set>

namespace
{
[[nodiscard]] constexpr pond::core::Uuid::Bytes MakeSequentialBytes() noexcept
{
    pond::core::Uuid::Bytes bytes{};

    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        bytes[index] = static_cast<pond::core::Uuid::Byte>(index);
    }

    return bytes;
}

[[nodiscard]] bool FillDeterministicEntropy(
    std::span<pond::core::Uuid::Byte, pond::core::Uuid::kByteCount> bytes) noexcept
{
    pond::core::Uuid::Bytes sourceBytes = MakeSequentialBytes();
    std::ranges::copy(sourceBytes, bytes.begin());
    return true;
}

[[nodiscard]] bool FailEntropy(
    std::span<pond::core::Uuid::Byte, pond::core::Uuid::kByteCount>) noexcept
{
    return false;
}

TEST(UuidTests, DefaultsToNil)
{
    pond::core::Uuid uuid;

    EXPECT_TRUE(uuid.IsNil());
    EXPECT_EQ(uuid, pond::core::Uuid::Nil());
    EXPECT_EQ(uuid.ToString(), "00000000-0000-0000-0000-000000000000");
}

TEST(UuidTests, StoresBytesAndComparesByValue)
{
    pond::core::Uuid::Bytes bytes = MakeSequentialBytes();
    pond::core::Uuid uuid{bytes};
    pond::core::Uuid sameUuid{bytes};

    bytes[15] = 42;
    pond::core::Uuid differentUuid{bytes};

    EXPECT_EQ(uuid.GetBytes(), MakeSequentialBytes());
    EXPECT_EQ(uuid, sameUuid);
    EXPECT_NE(uuid, differentUuid);
    EXPECT_LT(uuid, differentUuid);
}

TEST(UuidTests, SupportsHashing)
{
    std::unordered_set<pond::core::Uuid> ids;

    ids.insert(pond::core::Uuid{MakeSequentialBytes()});

    EXPECT_TRUE(ids.contains(pond::core::Uuid{MakeSequentialBytes()}));
}

TEST(UuidTests, FormatsCanonicalLowercaseText)
{
    pond::core::Uuid uuid{MakeSequentialBytes()};

    EXPECT_EQ(uuid.ToString(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST(UuidTests, ParsesCanonicalLowercaseText)
{
    pond::core::Result<pond::core::Uuid> result =
        pond::core::ParseUuid("00010203-0405-0607-0809-0a0b0c0d0e0f");

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue(), pond::core::Uuid{MakeSequentialBytes()});
}

TEST(UuidTests, ParsesCanonicalUppercaseText)
{
    pond::core::Result<pond::core::Uuid> result =
        pond::core::ParseUuid("00010203-0405-0607-0809-0A0B0C0D0E0F");

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue().ToString(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST(UuidTests, RejectsInvalidText)
{
    EXPECT_FALSE(pond::core::ParseUuid("").HasValue());
    EXPECT_FALSE(pond::core::ParseUuid("000102030405060708090a0b0c0d0e0f").HasValue());
    EXPECT_FALSE(pond::core::ParseUuid("00010203_0405-0607-0809-0a0b0c0d0e0f").HasValue());
    EXPECT_FALSE(pond::core::ParseUuid("00010203-0405-0607-0809-0a0b0c0d0e0x").HasValue());
}

TEST(UuidTests, BuildsDeterministicVersion4UuidFromBytes)
{
    pond::core::Uuid uuid = pond::core::MakeUuidV4(MakeSequentialBytes());

    EXPECT_EQ(uuid.ToString(), "00010203-0405-4607-8809-0a0b0c0d0e0f");
    EXPECT_EQ(uuid.GetVersion(), 4U);
    EXPECT_TRUE(uuid.HasRfc4122Variant());
}

TEST(UuidTests, GeneratesVersion4UuidFromInjectedEntropy)
{
    pond::core::Result<pond::core::Uuid> result =
        pond::core::GenerateUuidV4(FillDeterministicEntropy);

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue().ToString(), "00010203-0405-4607-8809-0a0b0c0d0e0f");
}

TEST(UuidTests, ReportsEntropyFailures)
{
    EXPECT_FALSE(pond::core::GenerateUuidV4(nullptr).HasValue());
    EXPECT_FALSE(pond::core::GenerateUuidV4(FailEntropy).HasValue());
}

TEST(UuidTests, DefaultGenerationIsWellFormedWhenAvailable)
{
    pond::core::Result<pond::core::Uuid> result = pond::core::GenerateUuidV4();

    if (result.HasValue())
    {
        EXPECT_EQ(result.GetValue().GetVersion(), 4U);
        EXPECT_TRUE(result.GetValue().HasRfc4122Variant());
    }
}
} // namespace