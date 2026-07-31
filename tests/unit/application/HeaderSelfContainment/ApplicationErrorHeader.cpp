#include <ponder/application/ApplicationError.hpp>

#include <concepts>

static_assert(
    std::same_as<decltype(ponder::application::ToErrorCode(ponder::application::ApplicationErrorCode::InvalidState)), ponder::core::ErrorCode>);
static_assert(ponder::application::ToErrorCode(ponder::application::ApplicationErrorCode::InternalFailure).GetCategory() ==
              ponder::core::ErrorCategory::Internal);
