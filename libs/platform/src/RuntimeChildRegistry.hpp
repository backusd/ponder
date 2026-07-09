#pragma once

#include <cstddef>
#include <unordered_set>

namespace pond::platform::detail
{
class RuntimeChildRegistry final
{
public:
    void RegisterChild(const void* child);
    void UnregisterChild(const void* child);
    void RegisterRequest(const void* request);
    void UnregisterRequest(const void* request);

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] std::size_t GetChildCount() const noexcept;
    [[nodiscard]] std::size_t GetRequestCount() const noexcept;

private:
    std::unordered_set<const void*> m_children;
    std::unordered_set<const void*> m_requests;
};
} // namespace pond::platform::detail
