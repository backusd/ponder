#pragma once

#include <cstdint>
#include <string_view>

namespace pond::core
{
class BuildInfo final
{
public:
    constexpr BuildInfo(std::string_view projectName, std::string_view projectVersion,
                        std::string_view gitCommit, std::string_view buildType,
                        std::string_view compilerName, std::string_view compilerVersion,
                        std::string_view targetSystemName, std::string_view targetSystemProcessor,
                        std::uint32_t pointerWidthBits, std::string_view cmakeGenerator,
                        std::string_view cmakeVersion) noexcept
        : m_projectName(projectName), m_projectVersion(projectVersion), m_gitCommit(gitCommit),
          m_buildType(buildType), m_compilerName(compilerName), m_compilerVersion(compilerVersion),
          m_targetSystemName(targetSystemName), m_targetSystemProcessor(targetSystemProcessor),
          m_pointerWidthBits(pointerWidthBits), m_cmakeGenerator(cmakeGenerator),
          m_cmakeVersion(cmakeVersion)
    {
    }

    [[nodiscard]] constexpr std::string_view GetProjectName() const noexcept
    {
        return m_projectName;
    }

    [[nodiscard]] constexpr std::string_view GetProjectVersion() const noexcept
    {
        return m_projectVersion;
    }

    [[nodiscard]] constexpr std::string_view GetGitCommit() const noexcept
    {
        return m_gitCommit;
    }

    [[nodiscard]] constexpr bool HasGitCommit() const noexcept
    {
        return !m_gitCommit.empty();
    }

    [[nodiscard]] constexpr std::string_view GetBuildType() const noexcept
    {
        return m_buildType;
    }

    [[nodiscard]] constexpr std::string_view GetCompilerName() const noexcept
    {
        return m_compilerName;
    }

    [[nodiscard]] constexpr std::string_view GetCompilerVersion() const noexcept
    {
        return m_compilerVersion;
    }

    [[nodiscard]] constexpr std::string_view GetTargetSystemName() const noexcept
    {
        return m_targetSystemName;
    }

    [[nodiscard]] constexpr std::string_view GetTargetSystemProcessor() const noexcept
    {
        return m_targetSystemProcessor;
    }

    [[nodiscard]] constexpr std::uint32_t GetPointerWidthBits() const noexcept
    {
        return m_pointerWidthBits;
    }

    [[nodiscard]] constexpr std::string_view GetCMakeGenerator() const noexcept
    {
        return m_cmakeGenerator;
    }

    [[nodiscard]] constexpr std::string_view GetCMakeVersion() const noexcept
    {
        return m_cmakeVersion;
    }

private:
    std::string_view m_projectName;
    std::string_view m_projectVersion;
    std::string_view m_gitCommit;
    std::string_view m_buildType;
    std::string_view m_compilerName;
    std::string_view m_compilerVersion;
    std::string_view m_targetSystemName;
    std::string_view m_targetSystemProcessor;
    std::uint32_t m_pointerWidthBits{0};
    std::string_view m_cmakeGenerator;
    std::string_view m_cmakeVersion;
};

[[nodiscard]] BuildInfo GetBuildInfo() noexcept;
} // namespace pond::core