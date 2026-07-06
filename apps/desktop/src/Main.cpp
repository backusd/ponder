#include <ponder/core/Library.hpp>
#include <ponder/core/Log.hpp>
#include <ponder/core/Result.hpp>

namespace
{
pond::core::VoidResult RunDesktopStub()
{
    LOG_INFO("ponder-desktop scaffold started");
    LOG_INFO("Linked core library: {}", pond::core::GetLibraryName());
    return {};
}
} // namespace

int main()
{
    const auto result = RunDesktopStub();
    if (!result)
    {
        LOG_ERROR("ponder-desktop failed: {}", result.error().GetMessage());
        return 1;
    }

    return 0;
}