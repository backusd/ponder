#pragma once

#include <ponder/render/Bootstrap.hpp>

#include <utility>

namespace pond::render::detail
{
// Captures backend context where a Vulkan failure is translated. Public operations attach the
// structured record to their owning diagnostics instead of reparsing core::Error display text.
class VulkanDiagnosticScope final
{
public:
    VulkanDiagnosticScope() noexcept : m_previous{std::exchange(s_current, this)} {}

    VulkanDiagnosticScope(const VulkanDiagnosticScope&) = delete;
    VulkanDiagnosticScope& operator=(const VulkanDiagnosticScope&) = delete;
    VulkanDiagnosticScope(VulkanDiagnosticScope&&) = delete;
    VulkanDiagnosticScope& operator=(VulkanDiagnosticScope&&) = delete;

    ~VulkanDiagnosticScope()
    {
        s_current = m_previous;
    }

    [[nodiscard]] OptionalBackendDiagnostic TakeLastFailure() noexcept
    {
        OptionalBackendDiagnostic failure = std::move(m_lastFailure);
        m_lastFailure.reset();
        return failure;
    }

    [[nodiscard]] static OptionalBackendDiagnostic TakeCurrentLastFailure() noexcept
    {
        return s_current == nullptr ? OptionalBackendDiagnostic{} : s_current->TakeLastFailure();
    }

    [[nodiscard]] static OptionalBackendDiagnostic CopyCurrentLastFailure()
    {
        return s_current == nullptr ? OptionalBackendDiagnostic{} : s_current->m_lastFailure;
    }

    static void Record(BackendDiagnostic diagnostic)
    {
        if (s_current != nullptr && !s_current->m_lastFailure.has_value())
        {
            s_current->m_lastFailure.emplace(std::move(diagnostic));
        }
    }

private:
    inline static thread_local VulkanDiagnosticScope* s_current{};

    VulkanDiagnosticScope* m_previous{};
    OptionalBackendDiagnostic m_lastFailure{};
};
} // namespace pond::render::detail
