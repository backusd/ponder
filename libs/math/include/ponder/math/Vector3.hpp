#pragma once

#include <ponder/math/MathError.hpp>

#include <cstddef>
#include <functional>

namespace pond::math
{
struct Vector3 final
{
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    constexpr Vector3() noexcept = default;

    explicit constexpr Vector3(float xValue, float yValue, float zValue) noexcept
        : x(xValue), y(yValue), z(zValue)
    {
    }

    [[nodiscard]] static constexpr Vector3 Zero() noexcept
    {
        return Vector3{};
    }

    [[nodiscard]] core::Result<std::reference_wrapper<float>> At(std::size_t index)
    {
        switch (index)
        {
        case 0:
            return std::ref(x);
        case 1:
            return std::ref(y);
        case 2:
            return std::ref(z);
        default:
            return core::Result<std::reference_wrapper<float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument),
                "Vector3 index is out of range."});
        }
    }

    [[nodiscard]] core::Result<std::reference_wrapper<const float>> At(std::size_t index) const
    {
        switch (index)
        {
        case 0:
            return std::cref(x);
        case 1:
            return std::cref(y);
        case 2:
            return std::cref(z);
        default:
            return core::Result<std::reference_wrapper<const float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument),
                "Vector3 index is out of range."});
        }
    }

    [[nodiscard]] friend constexpr bool operator==(const Vector3& lhs,
                                                   const Vector3& rhs) noexcept = default;
};
} // namespace pond::math
