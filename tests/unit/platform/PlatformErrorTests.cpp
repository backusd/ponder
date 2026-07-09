#include <ponder/platform/PlatformError.hpp>

#include <gtest/gtest.h>

#include <array>

namespace
{
struct ErrorMapping final
{
    pond::platform::PlatformErrorCode platformCode;
    pond::core::ErrorCategory category;
    pond::core::ErrorCodeValue value;
};

constexpr std::array kErrorMappings{
    ErrorMapping{pond::platform::PlatformErrorCode::InvalidArgument,
                 pond::core::ErrorCategory::InvalidArgument, 0x0001'0001},
    ErrorMapping{pond::platform::PlatformErrorCode::RuntimeAlreadyActive,
                 pond::core::ErrorCategory::General, 0x0001'0002},
    ErrorMapping{pond::platform::PlatformErrorCode::BackendFailure,
                 pond::core::ErrorCategory::General, 0x0001'0003},
    ErrorMapping{pond::platform::PlatformErrorCode::NotFound,
                 pond::core::ErrorCategory::NotFound, 0x0001'0004},
    ErrorMapping{pond::platform::PlatformErrorCode::Unsupported,
                 pond::core::ErrorCategory::Unsupported, 0x0001'0005},
};

constexpr bool EveryErrorMappingIsConstexpr()
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const pond::core::ErrorCode coreCode = pond::platform::ToErrorCode(mapping.platformCode);
        if (coreCode.GetCategory() != mapping.category || coreCode.GetValue() != mapping.value)
        {
            return false;
        }
    }

    return true;
}

static_assert(EveryErrorMappingIsConstexpr());

TEST(PlatformErrorTests, MapsEveryPublishedCodeToItsStableCoreCode)
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const pond::core::ErrorCode coreCode = pond::platform::ToErrorCode(mapping.platformCode);

        EXPECT_EQ(coreCode.GetCategory(), mapping.category);
        EXPECT_EQ(coreCode.GetValue(), mapping.value);
    }
}

TEST(PlatformErrorTests, MapsUnknownValuesToInternalWithoutChangingTheValue)
{
    constexpr pond::core::ErrorCodeValue kUnknownValue{0x0001'00FF};
    constexpr auto kUnknownCode =
        static_cast<pond::platform::PlatformErrorCode>(kUnknownValue);
    constexpr pond::core::ErrorCode kCoreCode = pond::platform::ToErrorCode(kUnknownCode);

    EXPECT_EQ(kCoreCode.GetCategory(), pond::core::ErrorCategory::Internal);
    EXPECT_EQ(kCoreCode.GetValue(), kUnknownValue);
}
} // namespace
