#include <ponder/core/Uuid.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <gtest/gtest.h>
#include <span>
#include <type_traits>
#include <unordered_set>

namespace
{
[[nodiscard]] constexpr ponder::core::Uuid::Bytes MakeSequentialBytes() noexcept
{
    ponder::core::Uuid::Bytes bytes{};

    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        bytes[index] = static_cast<ponder::core::Uuid::Byte>(index);
    }

    return bytes;
}

void FillDeterministicEntropy(std::span<ponder::core::Uuid::Byte, ponder::core::Uuid::kByteCount> bytes) noexcept
{
    ponder::core::Uuid::Bytes sourceBytes = MakeSequentialBytes();
    std::ranges::copy(sourceBytes, bytes.begin());
}

constexpr bool UuidValueObserversAreConstexpr()
{
    constexpr ponder::core::Uuid::Bytes kBytes = MakeSequentialBytes();
    constexpr ponder::core::Uuid kUuid{kBytes};
    constexpr ponder::core::Uuid kSameUuid{kBytes};

    auto differentBytes = kBytes;
    differentBytes[15] = 42;
    const ponder::core::Uuid differentUuid{differentBytes};

    return ponder::core::Uuid{}.IsNil() && ponder::core::Uuid::Nil().IsNil() && kUuid.GetBytes() == kBytes && !kUuid.IsNil() && kUuid == kSameUuid &&
           kUuid < differentUuid;
}

constexpr bool UuidVersionAndVariantAreConstexpr()
{
    const ponder::core::Uuid uuid = ponder::core::MakeUuidV4(MakeSequentialBytes());

    return uuid.GetVersion() == 4U && uuid.HasRfc4122Variant() && uuid.GetBytes()[6] == 0x46U && uuid.GetBytes()[8] == 0x88U;
}

constexpr bool UuidFormattingIsConstexpr()
{
    const ponder::core::Uuid nilUuid;
    const ponder::core::Uuid uuid{MakeSequentialBytes()};

    return nilUuid.ToString() == "00000000-0000-0000-0000-000000000000" && uuid.ToString() == "00010203-0405-0607-0809-0a0b0c0d0e0f";
}

constexpr bool UuidHashingIsConstexpr()
{
    const std::hash<ponder::core::Uuid> hash;

    return hash(ponder::core::Uuid{}) == hash(ponder::core::Uuid::Nil()) &&
           hash(ponder::core::Uuid{}) != hash(ponder::core::Uuid{MakeSequentialBytes()});
}

static_assert(UuidValueObserversAreConstexpr());
static_assert(UuidVersionAndVariantAreConstexpr());
static_assert(UuidFormattingIsConstexpr());
static_assert(UuidHashingIsConstexpr());
static_assert(std::is_same_v<ponder::core::UuidEntropySource, void (*)(std::span<ponder::core::Uuid::Byte, ponder::core::Uuid::kByteCount>)>);
static_assert(std::is_same_v<decltype(ponder::core::GenerateUuidV4()), ponder::core::Uuid>);
static_assert(noexcept(ponder::core::GenerateUuidV4(FillDeterministicEntropy)));
static_assert(noexcept(ponder::core::GenerateUuidV4()));

TEST(UuidTests, DefaultsToNil)
{
    ponder::core::Uuid uuid;

    EXPECT_TRUE(uuid.IsNil());
    EXPECT_EQ(uuid, ponder::core::Uuid::Nil());
    EXPECT_EQ(uuid.ToString(), "00000000-0000-0000-0000-000000000000");
}

TEST(UuidTests, StoresBytesAndComparesByValue)
{
    ponder::core::Uuid::Bytes bytes = MakeSequentialBytes();
    ponder::core::Uuid uuid{bytes};
    ponder::core::Uuid sameUuid{bytes};

    bytes[15] = 42;
    ponder::core::Uuid differentUuid{bytes};

    EXPECT_EQ(uuid.GetBytes(), MakeSequentialBytes());
    EXPECT_EQ(uuid, sameUuid);
    EXPECT_NE(uuid, differentUuid);
    EXPECT_LT(uuid, differentUuid);
}

TEST(UuidTests, SupportsHashing)
{
    std::unordered_set<ponder::core::Uuid> ids;

    ids.insert(ponder::core::Uuid{MakeSequentialBytes()});

    EXPECT_TRUE(ids.contains(ponder::core::Uuid{MakeSequentialBytes()}));
}

TEST(UuidTests, FormatsCanonicalLowercaseText)
{
    ponder::core::Uuid uuid{MakeSequentialBytes()};

    EXPECT_EQ(uuid.ToString(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST(UuidTests, ParsesCanonicalLowercaseText)
{
    ponder::core::Result<ponder::core::Uuid> result = ponder::core::ParseUuid("00010203-0405-0607-0809-0a0b0c0d0e0f");

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue(), ponder::core::Uuid{MakeSequentialBytes()});
}

TEST(UuidTests, ParsesCanonicalUppercaseText)
{
    ponder::core::Result<ponder::core::Uuid> result = ponder::core::ParseUuid("00010203-0405-0607-0809-0A0B0C0D0E0F");

    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue().ToString(), "00010203-0405-0607-0809-0a0b0c0d0e0f");
}

TEST(UuidTests, RejectsInvalidText)
{
    EXPECT_FALSE(ponder::core::ParseUuid("").HasValue());
    EXPECT_FALSE(ponder::core::ParseUuid("000102030405060708090a0b0c0d0e0f").HasValue());
    EXPECT_FALSE(ponder::core::ParseUuid("00010203_0405-0607-0809-0a0b0c0d0e0f").HasValue());
    EXPECT_FALSE(ponder::core::ParseUuid("00010203-0405-0607-0809-0a0b0c0d0e0x").HasValue());
}

TEST(UuidTests, BuildsDeterministicVersion4UuidFromBytes)
{
    ponder::core::Uuid uuid = ponder::core::MakeUuidV4(MakeSequentialBytes());

    EXPECT_EQ(uuid.ToString(), "00010203-0405-4607-8809-0a0b0c0d0e0f");
    EXPECT_EQ(uuid.GetVersion(), 4U);
    EXPECT_TRUE(uuid.HasRfc4122Variant());
}

TEST(UuidTests, GeneratesVersion4UuidFromInjectedEntropy)
{
    const ponder::core::Uuid uuid = ponder::core::GenerateUuidV4(FillDeterministicEntropy);

    EXPECT_EQ(uuid.ToString(), "00010203-0405-4607-8809-0a0b0c0d0e0f");
}

TEST(UuidTests, DefaultGenerationIsWellFormed)
{
    const ponder::core::Uuid uuid = ponder::core::GenerateUuidV4();

    EXPECT_EQ(uuid.GetVersion(), 4U);
    EXPECT_TRUE(uuid.HasRfc4122Variant());
}
} // namespace
