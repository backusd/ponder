#include "RuntimeChildRegistry.hpp"

#include <ponder/core/Assert.hpp>

namespace ponder::platform::detail
{
void RuntimeChildRegistry::RegisterChild(const void* child)
{
    PONDER_VERIFY(child != nullptr, "Cannot register a null platform child");
    const auto [iterator, inserted] = m_children.insert(child);
    static_cast<void>(iterator);
    PONDER_VERIFY(inserted, "Platform child is already registered");
}

void RuntimeChildRegistry::UnregisterChild(const void* child)
{
    PONDER_VERIFY(child != nullptr, "Cannot unregister a null platform child");
    const std::size_t removed = m_children.erase(child);
    PONDER_VERIFY(removed == 1, "Platform child is not registered");
}

bool RuntimeChildRegistry::IsEmpty() const noexcept
{
    return m_children.empty();
}

std::size_t RuntimeChildRegistry::GetChildCount() const noexcept
{
    return m_children.size();
}
} // namespace ponder::platform::detail
