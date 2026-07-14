#include <ponder/render/RenderError.hpp>

#include <array>
#include <gtest/gtest.h>

namespace
{
struct ErrorMapping final
{
    pond::render::RenderErrorCode renderCode;
    pond::core::ErrorCategory category;
    pond::core::ErrorCodeValue value;
};

constexpr std::array kErrorMappings{
    ErrorMapping{pond::render::RenderErrorCode::InvalidArgument,
                 pond::core::ErrorCategory::InvalidArgument, 0x0003'0001},
    ErrorMapping{pond::render::RenderErrorCode::InvalidState,
                 pond::core::ErrorCategory::InvalidArgument, 0x0003'0002},
    ErrorMapping{pond::render::RenderErrorCode::UnsupportedBackend,
                 pond::core::ErrorCategory::Unsupported, 0x0003'0003},
    ErrorMapping{pond::render::RenderErrorCode::UnsupportedCapability,
                 pond::core::ErrorCategory::Unsupported, 0x0003'0004},
    ErrorMapping{pond::render::RenderErrorCode::UnsupportedSurface,
                 pond::core::ErrorCategory::Unsupported, 0x0003'0005},
    ErrorMapping{pond::render::RenderErrorCode::LoaderUnavailable,
                 pond::core::ErrorCategory::NotFound, 0x0003'0006},
    ErrorMapping{pond::render::RenderErrorCode::NoCompatibleAdapter,
                 pond::core::ErrorCategory::NotFound, 0x0003'0007},
    ErrorMapping{pond::render::RenderErrorCode::OutOfMemory,
                 pond::core::ErrorCategory::General, 0x0003'0008},
    ErrorMapping{pond::render::RenderErrorCode::SurfaceLost,
                 pond::core::ErrorCategory::General, 0x0003'0009},
    ErrorMapping{pond::render::RenderErrorCode::DeviceLost,
                 pond::core::ErrorCategory::General, 0x0003'000A},
    ErrorMapping{pond::render::RenderErrorCode::BackendFailure,
                 pond::core::ErrorCategory::General, 0x0003'000B},
};

constexpr bool EveryErrorMappingIsConstexpr()
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const pond::core::ErrorCode coreCode = pond::render::ToErrorCode(mapping.renderCode);
        if (coreCode.GetCategory() != mapping.category || coreCode.GetValue() != mapping.value)
        {
            return false;
        }
    }

    return true;
}

static_assert(EveryErrorMappingIsConstexpr());

TEST(RenderErrorTests, MapsEveryPublishedCodeToItsStableCoreCode)
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const pond::core::ErrorCode coreCode = pond::render::ToErrorCode(mapping.renderCode);

        EXPECT_EQ(coreCode.GetCategory(), mapping.category);
        EXPECT_EQ(coreCode.GetValue(), mapping.value);
    }
}

TEST(RenderErrorTests, MapsUnknownValuesToInternalWithoutChangingTheValue)
{
    constexpr pond::core::ErrorCodeValue kUnknownValue{0x0003'00FF};
    constexpr auto kUnknownCode = static_cast<pond::render::RenderErrorCode>(kUnknownValue);
    constexpr pond::core::ErrorCode kCoreCode = pond::render::ToErrorCode(kUnknownCode);

    EXPECT_EQ(kCoreCode.GetCategory(), pond::core::ErrorCategory::Internal);
    EXPECT_EQ(kCoreCode.GetValue(), kUnknownValue);
}
} // namespace