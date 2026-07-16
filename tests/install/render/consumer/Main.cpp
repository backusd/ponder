#include <ponder/core/BuildInfo.hpp>
#include <ponder/core/Result.hpp>
#include <ponder/platform/Geometry.hpp>
#include <ponder/platform/Identifiers.hpp>
#include <ponder/platform/Process.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>

int main()
{
    static_assert(pond::render::IsValid(pond::render::RenderBootstrapDesc{}));
    static_assert(pond::render::IsValid(pond::render::RenderValidationMode::Standard));
    static_assert(pond::render::IsValid(pond::render::ClearColor{.red = 0.1F,
                                                                  .green = 0.2F,
                                                                  .blue = 0.3F,
                                                                  .alpha = 1.0F}));

    const pond::platform::WindowId windowId{42U};
    const pond::platform::PixelSize pixelSize{640U, 480U};
    const pond::platform::LogicalSize logicalSize{640U, 480U};
    const pond::render::RenderTargetSnapshot snapshot{
        windowId, pixelSize, logicalSize, true, pond::platform::WindowState::Normal,
        pond::render::PresentationEnvironmentRevision{1U}, 1U};
    if (!pond::render::IsValid(snapshot))
    {
        return 1;
    }

    const pond::render::RenderTargetDesc targetDesc{.targetSnapshot = snapshot};
    if (!pond::render::IsValid(targetDesc))
    {
        return 2;
    }

    const pond::core::ErrorCode invalidArgumentCode =
        pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidArgument);
    if (invalidArgumentCode.GetValue() == 0)
    {
        return 3;
    }

    const pond::core::BuildInfo buildInfo = pond::core::GetBuildInfo();
    if (buildInfo.GetProjectName() != "ponder")
    {
        return 4;
    }

    const auto processResult = pond::platform::LaunchProcess({});
    if (processResult.HasValue())
    {
        return 5;
    }

    if (pond::render::GetRequiredWindowGraphicsCompatibility() !=
        pond::platform::WindowGraphicsCompatibility::Vulkan)
    {
        return 6;
    }

    pond::render::RenderBootstrap bootstrap;
    if (bootstrap.IsValid())
    {
        return 7;
    }

    return 0;
}
