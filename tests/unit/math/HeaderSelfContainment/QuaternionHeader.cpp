#include <ponder/math/Quaternion.hpp>

#include <type_traits>
#include <utility>

namespace
{
static_assert(sizeof(pond::math::Quaternion) == sizeof(float) * 4);
static_assert(alignof(pond::math::Quaternion) == alignof(float));
static_assert(std::is_standard_layout_v<pond::math::Quaternion>);
static_assert(std::is_trivially_copyable_v<pond::math::Quaternion>);
static_assert(std::is_trivially_copy_constructible_v<pond::math::Quaternion>);
static_assert(std::is_trivially_move_constructible_v<pond::math::Quaternion>);
static_assert(std::is_trivially_copy_assignable_v<pond::math::Quaternion>);
static_assert(std::is_trivially_move_assignable_v<pond::math::Quaternion>);
static_assert(std::is_trivially_destructible_v<pond::math::Quaternion>);
static_assert(!std::is_aggregate_v<pond::math::Quaternion>);
static_assert(!std::is_constructible_v<pond::math::Quaternion, float, float, float, float>);
static_assert(
    std::is_same_v<decltype(std::declval<const pond::math::Quaternion&>().GetX()), float>);
static_assert(pond::math::Quaternion{}.GetX() == 0.0F);
static_assert(pond::math::Quaternion{}.GetY() == 0.0F);
static_assert(pond::math::Quaternion{}.GetZ() == 0.0F);
static_assert(pond::math::Quaternion{}.GetW() == 1.0F);
static_assert(pond::math::Quaternion{} == pond::math::Quaternion::Identity());
static_assert(pond::math::Conjugate(pond::math::Quaternion::Identity()) ==
              pond::math::Quaternion::Identity());
static_assert(pond::math::Inverse(pond::math::Quaternion::Identity()) ==
              pond::math::Quaternion::Identity());
static_assert(pond::math::Rotate(pond::math::Quaternion::Identity(),
                                 pond::math::Vector3{1.0F, 2.0F, 3.0F}) ==
              pond::math::Vector3{1.0F, 2.0F, 3.0F});
static_assert(pond::math::ToMatrix3x3(pond::math::Quaternion::Identity()) ==
              pond::math::Matrix3x3::Identity());
static_assert(pond::math::ToMatrix4x4(pond::math::Quaternion::Identity()) ==
              pond::math::Matrix4x4::Identity());
static_assert(pond::math::Matrix4x4::Rotation(pond::math::Quaternion::Identity()) ==
              pond::math::Matrix4x4::Identity());
} // namespace