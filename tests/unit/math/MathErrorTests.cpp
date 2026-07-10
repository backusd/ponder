#include <ponder/math/MathError.hpp>

#include <gtest/gtest.h>

#include <array>
#include <type_traits>

namespace
{
struct ErrorMapping final
{
    pond::math::MathErrorCode mathCode;
    pond::core::ErrorCategory category;
    pond::core::ErrorCodeValue value;
};

constexpr std::array kErrorMappings{
    ErrorMapping{pond::math::MathErrorCode::InvalidArgument,
                 pond::core::ErrorCategory::InvalidArgument, 0x0002'0001},
    ErrorMapping{pond::math::MathErrorCode::NonFiniteInput,
                 pond::core::ErrorCategory::InvalidArgument, 0x0002'0002},
    ErrorMapping{pond::math::MathErrorCode::DegenerateInput,
                 pond::core::ErrorCategory::General, 0x0002'0003},
    ErrorMapping{pond::math::MathErrorCode::SingularMatrix,
                 pond::core::ErrorCategory::General, 0x0002'0004},
    ErrorMapping{pond::math::MathErrorCode::UndefinedHomogeneousCoordinate,
                 pond::core::ErrorCategory::General, 0x0002'0005},
};

constexpr bool EveryErrorMappingIsConstexpr()
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const pond::core::ErrorCode coreCode = pond::math::ToErrorCode(mapping.mathCode);
        if (coreCode.GetCategory() != mapping.category || coreCode.GetValue() != mapping.value)
        {
            return false;
        }
    }

    return true;
}

static_assert(EveryErrorMappingIsConstexpr());
static_assert(pond::math::kMathErrorCodeFirst == 0x0002'0000);
static_assert(pond::math::kMathErrorCodeLast == 0x0002'FFFF);
static_assert(std::is_same_v<std::underlying_type_t<pond::math::MathErrorCode>,
                             pond::core::ErrorCodeValue>);

TEST(MathErrorTests, ReservesStableMathErrorBlock)
{
    EXPECT_EQ(pond::math::kMathErrorCodeFirst, 0x0002'0000);
    EXPECT_EQ(pond::math::kMathErrorCodeLast, 0x0002'FFFF);
}

TEST(MathErrorTests, MapsEveryPublishedCodeToItsStableCoreCode)
{
    for (const ErrorMapping& mapping : kErrorMappings)
    {
        const pond::core::ErrorCode coreCode = pond::math::ToErrorCode(mapping.mathCode);

        EXPECT_EQ(coreCode.GetCategory(), mapping.category);
        EXPECT_EQ(coreCode.GetValue(), mapping.value);
    }
}

TEST(MathErrorTests, MapsUnknownValuesToInternalWithoutChangingTheValue)
{
    constexpr pond::core::ErrorCodeValue kUnknownValue{0x0002'00FF};
    constexpr auto kUnknownCode = static_cast<pond::math::MathErrorCode>(kUnknownValue);
    constexpr pond::core::ErrorCode kCoreCode = pond::math::ToErrorCode(kUnknownCode);

    EXPECT_EQ(kCoreCode.GetCategory(), pond::core::ErrorCategory::Internal);
    EXPECT_EQ(kCoreCode.GetValue(), kUnknownValue);
}
} // namespace
