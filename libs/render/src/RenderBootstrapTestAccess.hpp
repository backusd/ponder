#pragma once

#include <ponder/render/Bootstrap.hpp>

namespace pond::render::detail
{
class RenderBootstrapTestAccess final
{
public:
    [[nodiscard]] static core::Result<PreparedSurface> CreatePreparedSurface(
        RenderBootstrap& bootstrap, const SurfacePreparationDesc& desc);
};
} // namespace pond::render::detail
