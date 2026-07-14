#include <ponder/render/RenderError.hpp>

namespace
{
static_assert(pond::render::ToErrorCode(pond::render::RenderErrorCode::InvalidArgument)
                  .GetCategory() == pond::core::ErrorCategory::InvalidArgument);
} // namespace