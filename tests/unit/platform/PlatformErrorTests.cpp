#include <ponder/platform/PlatformError.hpp>

#include <array>
#include <concepts>
#include <format>
#include <gtest/gtest.h>
#include <string_view>

namespace
{
struct ErrorMapping final
{
    ponder::platform::PlatformErrorCode platformCode;
    ponder::core::ErrorCategory category;
    ponder::core::ErrorCodeValue value;
};

constexpr std::array kErrorMappings{
    ErrorMapping{ponder::platform::PlatformErrorCode::InvalidArgument, ponder::core::ErrorCategory::InvalidArgument, 0x0001'0001},
    ErrorMapping{ponder::platform::PlatformErrorCode::RuntimeAlreadyActive, ponder::core::ErrorCategory::General, 0x0001'0002},
    ErrorMapping{ponder::platform::PlatformErrorCode::BackendFailure, ponder::core::ErrorCategory::General, 0x0001'0003},
    ErrorMapping{ponder::platform::PlatformErrorCode::NotFound, ponder::core::ErrorCategory::NotFound, 0x0001'0004},
    ErrorMapping{ponder::platform::PlatformErrorCode::Unsupported, ponder::core::ErrorCategory::Unsupported, 0x0001'0005},
    ErrorMapping{ponder::platform::PlatformErrorCode::WrongThread, ponder::core::ErrorCategory::General, 0x0001'0006},
};

constexpr bool EveryErrorMappingIsConstexpr()
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const ponder::core::ErrorCode coreCode = ponder::platform::ToErrorCode(mapping.platformCode);
        if (coreCode.GetCategory() != mapping.category || coreCode.GetValue() != mapping.value)
        {
            return false;
        }
    }

    return true;
}

static_assert(EveryErrorMappingIsConstexpr());
static_assert(
    std::same_as<decltype(PLATFORM_EXCEPTION(ponder::platform::PlatformErrorCode::InvalidArgument, "value {}", 7)), ponder::core::Exception>);

TEST(PlatformErrorTests, MapsEveryPublishedCodeToItsStableCoreCode)
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const ponder::core::ErrorCode coreCode = ponder::platform::ToErrorCode(mapping.platformCode);

        EXPECT_EQ(coreCode.GetCategory(), mapping.category);
        EXPECT_EQ(coreCode.GetValue(), mapping.value);
    }
}

TEST(PlatformErrorTests, MapsUnknownValuesToInternalWithoutChangingTheValue)
{
    constexpr ponder::core::ErrorCodeValue kUnknownValue{0x0001'00FF};
    constexpr auto kUnknownCode = static_cast<ponder::platform::PlatformErrorCode>(kUnknownValue);
    constexpr ponder::core::ErrorCode kCoreCode = ponder::platform::ToErrorCode(kUnknownCode);

    EXPECT_EQ(kCoreCode.GetCategory(), ponder::core::ErrorCategory::Internal);
    EXPECT_EQ(kCoreCode.GetValue(), kUnknownValue);
}

TEST(PlatformErrorTests, FormatsWrongThreadCode)
{
    EXPECT_EQ(std::format("{}", ponder::platform::PlatformErrorCode::WrongThread), "wrong_thread");
}

TEST(PlatformErrorTests, PlatformExceptionEmbedsFormattedCodeAndMessageAndPreservesLocation)
{
    const auto expectedLine = __LINE__ + 2;
    const ponder::core::Exception exception =
        PLATFORM_EXCEPTION(ponder::platform::PlatformErrorCode::InvalidArgument, "Invalid value {} for {}.", 17, "sample");

    EXPECT_EQ(exception.GetMessage(), std::string_view{"Platform error [invalid_argument]: Invalid value 17 for sample."});
    EXPECT_STREQ(exception.GetLocation().file_name(), __FILE__);
    EXPECT_EQ(exception.GetLocation().line(), expectedLine);
}

TEST(PlatformErrorTests, CoreErrorComparesDirectlyWithPlatformErrorCode)
{
    struct NotAnErrorCode final
    {
    };

    static_assert(ponder::core::ConvertToErrorCode<ponder::platform::PlatformErrorCode>);
    static_assert(!ponder::core::ConvertToErrorCode<NotAnErrorCode>);

    const ponder::core::Error error{ponder::platform::ToErrorCode(ponder::platform::PlatformErrorCode::Unsupported), "unsupported"};

    EXPECT_TRUE(error == ponder::platform::PlatformErrorCode::Unsupported);
    EXPECT_TRUE(ponder::platform::PlatformErrorCode::Unsupported == error);
    EXPECT_FALSE(error == ponder::platform::PlatformErrorCode::BackendFailure);
    EXPECT_TRUE(error != ponder::platform::PlatformErrorCode::BackendFailure);
}
} // namespace
