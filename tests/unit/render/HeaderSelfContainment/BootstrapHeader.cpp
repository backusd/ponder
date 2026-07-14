#include <ponder/render/Bootstrap.hpp>

namespace
{
static_assert(pond::render::IsValid(pond::render::RenderBootstrapDesc{}));
[[maybe_unused]] pond::render::RenderBootstrap bootstrap;
} // namespace