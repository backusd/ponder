#pragma once

#include <ponder/math/MathError.hpp>

#include <cstddef>
#include <functional>

namespace pond::math
{
struct Vector4 final
{
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{0.0F};

    constexpr Vector4() noexcept = default;

    explicit constexpr Vector4(float xValue, float yValue, float zValue, float wValue) noexcept
        : x(xValue), y(yValue), z(zValue), w(wValue)
    {
    }

    [[nodiscard]] static constexpr Vector4 Zero() noexcept
    {
        return Vector4{};
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
        case 3:
            return std::ref(w);
        default:
            return core::Result<std::reference_wrapper<float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument),
                "Vector4 index is out of range."});
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
        case 3:
            return std::cref(w);
        default:
            return core::Result<std::reference_wrapper<const float>>::FromError(core::Error{
                ToErrorCode(MathErrorCode::InvalidArgument),
                "Vector4 index is out of range."});
        }
    }

    [[nodiscard]] friend constexpr bool operator==(const Vector4& lhs,
                                                   const Vector4& rhs) noexcept = default;
};
} // namespace pond::math
