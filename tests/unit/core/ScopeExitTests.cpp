#include <ponder/core/ScopeExit.hpp>

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace
{
struct NoThrowCallback
{
    void operator()() noexcept {}
};

struct ThrowingCallback
{
    void operator()() {}
};

struct ThrowingMoveCallback
{
    ThrowingMoveCallback() noexcept = default;
    ThrowingMoveCallback(const ThrowingMoveCallback&) noexcept = default;
    ThrowingMoveCallback(ThrowingMoveCallback&&) noexcept(false) {}

    void operator()() noexcept {}
};

using TestHandler = void (*)(int&) noexcept;

void IncrementHandler(int& value) noexcept
{
    ++value;
}

void AddTenHandler(int& value) noexcept
{
    value += 10;
}

TestHandler gHandler = IncrementHandler;

void RunEarlyReturnStyleCleanup(int& value)
{
    auto cleanup = pond::core::MakeScopeExit(
        [&value]() noexcept
        {
            value = 7;
        });

    if (value == 0)
    {
        return;
    }

    value = 1;
}

void RunWithTemporaryHandler(TestHandler handler, int& value)
{
    TestHandler previousHandler = std::exchange(gHandler, handler);
    auto restoreHandler = pond::core::MakeScopeExit(
        [previousHandler]() noexcept
        {
            gHandler = previousHandler;
        });

    gHandler(value);
}

TEST(ScopeExitTests, InvokesCleanupOnScopeExit)
{
    bool called = false;

    {
        auto cleanup = pond::core::MakeScopeExit(
            [&called]() noexcept
            {
                called = true;
            });

        EXPECT_TRUE(cleanup.IsActive());
    }

    EXPECT_TRUE(called);
}

TEST(ScopeExitTests, RunsDuringEarlyReturnStyleUsage)
{
    int value = 0;

    RunEarlyReturnStyleCleanup(value);

    EXPECT_EQ(value, 7);
}

TEST(ScopeExitTests, DismissPreventsCleanup)
{
    bool called = false;

    {
        auto cleanup = pond::core::MakeScopeExit(
            [&called]() noexcept
            {
                called = true;
            });

        cleanup.Dismiss();
    }

    EXPECT_FALSE(called);
}

TEST(ScopeExitTests, MoveTransfersCleanupOwnership)
{
    int calls = 0;

    {
        auto cleanup = pond::core::MakeScopeExit(
            [&calls]() noexcept
            {
                ++calls;
            });
        auto movedCleanup = std::move(cleanup);

        EXPECT_TRUE(movedCleanup.IsActive());
    }

    EXPECT_EQ(calls, 1);
}

TEST(ScopeExitTests, RestoresStateForHandlerStyleOverrides)
{
    gHandler = IncrementHandler;
    int value = 0;

    RunWithTemporaryHandler(AddTenHandler, value);

    EXPECT_EQ(value, 10);

    gHandler(value);

    EXPECT_EQ(value, 11);
}

TEST(ScopeExitTests, HasNoThrowCleanupContract)
{
    static_assert(pond::core::ScopeExitCallback<NoThrowCallback>);
    static_assert(!pond::core::ScopeExitCallback<ThrowingCallback>);
    static_assert(!pond::core::ScopeExitCallback<ThrowingMoveCallback>);

    auto cleanup = pond::core::MakeScopeExit(NoThrowCallback{});

    static_assert(std::is_nothrow_destructible_v<decltype(cleanup)>);
    static_assert(std::is_move_constructible_v<decltype(cleanup)>);
    static_assert(!std::is_copy_constructible_v<decltype(cleanup)>);
    static_assert(!std::is_move_assignable_v<decltype(cleanup)>);
    static_assert(noexcept(pond::core::MakeScopeExit(NoThrowCallback{})));

    EXPECT_TRUE(cleanup.IsActive());
}
} // namespace