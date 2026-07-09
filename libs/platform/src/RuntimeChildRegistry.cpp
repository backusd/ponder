#include "RuntimeChildRegistry.hpp"

#include <ponder/core/Assert.hpp>

namespace pond::platform::detail
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

void RuntimeChildRegistry::RegisterRequest(const void* request)
{
    PONDER_VERIFY(request != nullptr, "Cannot register a null platform request");
    const auto [iterator, inserted] = m_requests.insert(request);
    static_cast<void>(iterator);
    PONDER_VERIFY(inserted, "Platform request is already registered");
}

void RuntimeChildRegistry::UnregisterRequest(const void* request)
{
    PONDER_VERIFY(request != nullptr, "Cannot unregister a null platform request");
    const std::size_t removed = m_requests.erase(request);
    PONDER_VERIFY(removed == 1, "Platform request is not registered");
}

bool RuntimeChildRegistry::IsEmpty() const noexcept
{
    return m_children.empty() && m_requests.empty();
}

std::size_t RuntimeChildRegistry::GetChildCount() const noexcept
{
    return m_children.size();
}

std::size_t RuntimeChildRegistry::GetRequestCount() const noexcept
{
    return m_requests.size();
}
} // namespace pond::platform::detail
