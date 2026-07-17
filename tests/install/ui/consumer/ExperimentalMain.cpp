#include <ponder/core/BuildInfo.hpp>
#include <ponder/platform/Process.hpp>
#include <ponder/render/Bootstrap.hpp>
#include <ponder/render/RenderError.hpp>
#include <ponder/ui/Error.hpp>
#include <ponder/ui/Library.hpp>
#include <ponder/ui/experimental/RectangleRenderer.hpp>

#include <string_view>
#include <type_traits>

int main()
{
    static_assert(!std::is_copy_constructible_v<pond::ui::experimental::RectangleRenderer>);
    static_assert(std::is_nothrow_move_constructible_v<pond::ui::experimental::RectangleRenderer>);

    if (pond::core::GetBuildInfo().GetProjectName() != "ponder" ||
        pond::ui::GetLibraryName() != std::string_view{"ui"})
    {
        return 1;
    }

    if (pond::platform::LaunchProcess({}).HasValue())
    {
        return 2;
    }
    if (pond::render::GetRequiredWindowGraphicsCompatibility() !=
        pond::platform::WindowGraphicsCompatibility::Vulkan)
    {
        return 3;
    }

    pond::render::RenderFrame emptyFrame;
    const auto metrics = pond::ui::experimental::MakeUiTargetMetricsForFrame(emptyFrame);
    if (metrics.HasValue() ||
        metrics.GetError().GetCode() !=
            pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState))
    {
        return 4;
    }

    pond::render::RenderDevice emptyDevice;
    const auto created = pond::ui::experimental::RectangleRenderer::Create(emptyDevice);
    if (created.HasValue() ||
        created.GetError().GetCode() !=
            pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidState))
    {
        return 5;
    }

    pond::ui::experimental::RectangleRenderer emptyRenderer;
    if (emptyRenderer.IsValid() ||
        pond::ui::experimental::kRectangleFacadeExperimentalNotice.empty())
    {
        return 6;
    }

    return 0;
}
