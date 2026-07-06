#include <ponder/core/Log.hpp>
#include <ponder/core/Result.hpp>

#include <string_view>

namespace
{
pond::core::VoidResult RunProjectInspectStub(int argc, char** argv)
{
    LOG_INFO("ponder-project-inspect scaffold started");

    if (argc > 1)
    {
        const std::string_view projectPath{argv[1]};
        LOG_INFO("Project loading is not implemented yet. Requested path: {}", projectPath);
    }
    else
    {
        LOG_INFO("Project loading is not implemented yet. Pass a .ponder file later.");
    }

    return {};
}
} // namespace

int main(int argc, char** argv)
{
    const auto result = RunProjectInspectStub(argc, argv);
    if (!result)
    {
        LOG_ERROR("ponder-project-inspect failed: {}", result.error().GetMessage());
        return 1;
    }

    return 0;
}