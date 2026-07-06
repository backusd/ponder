#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace pond::core
{
template <typename Callback>
concept ScopeExitCallback =
    std::is_nothrow_move_constructible_v<Callback> && std::is_nothrow_destructible_v<Callback> &&
    std::is_nothrow_invocable_v<Callback&>;

template <ScopeExitCallback Callback>
class [[nodiscard]] ScopeExit final
{
public:
    explicit constexpr ScopeExit(Callback callback) noexcept : m_callback(std::move(callback)) {}

    constexpr ScopeExit(ScopeExit&& other) noexcept
        : m_callback(std::move(other.m_callback)), m_active(std::exchange(other.m_active, false))
    {
    }

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;

    constexpr ~ScopeExit() noexcept
    {
        if (m_active)
        {
            std::invoke(m_callback);
        }
    }

    constexpr void Dismiss() noexcept
    {
        m_active = false;
    }

    [[nodiscard]] constexpr bool IsActive() const noexcept
    {
        return m_active;
    }

private:
    Callback m_callback;
    bool m_active{true};
};

template <typename Callback>
    requires ScopeExitCallback<std::decay_t<Callback>> &&
             std::is_nothrow_constructible_v<std::decay_t<Callback>, Callback&&>
[[nodiscard]] constexpr auto MakeScopeExit(Callback&& callback) noexcept
{
    using StoredCallback = std::decay_t<Callback>;
    return ScopeExit<StoredCallback>{std::forward<Callback>(callback)};
}
} // namespace pond::core
