#pragma once

#include <ponder/math/MathError.hpp>

#include <cstddef>
#include <functional>

namespace pond::math
{
struct Vector2 final
{
    float x{0.0F};
    float y{0.0F};

    constexpr Vector2() noexcept = default;

    explicit constexpr Vector2(float xValue, float yValue) noexcept
        : x(xValue), y(yValue)
    {
    }

    [[nodiscard]] static constexpr Vector2 Zero() noexcept
    {
        return Vector2{};
    }

    [[nodiscard]] core::Result<std::reference_wrapper<float>> At(std::size_t index)
    {
        switch (index)
        {
        case 0:
            return std::ref(x);
        case 1:
            return std::ref(y);
        default:
            return core::Result<std::reference_wrapper<float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument),
                "Vector2 index is out of range."});
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
        default:
            return core::Result<std::reference_wrapper<const float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument),
                "Vector2 index is out of range."});
        }
    }

    [[nodiscard]] friend constexpr bool operator==(const Vector2& lhs,
                                                   const Vector2& rhs) noexcept = default;
};
} // namespace pond::math
