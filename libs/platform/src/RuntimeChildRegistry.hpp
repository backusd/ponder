#pragma once

#include <cstddef>
#include <unordered_set>

namespace ponder::platform::detail
{
class RuntimeChildRegistry final
{
public:
    void RegisterChild(const void* child);
    void UnregisterChild(const void* child);

    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] std::size_t GetChildCount() const noexcept;

private:
    std::unordered_set<const void*> m_children;
};
} // namespace ponder::platform::detail
