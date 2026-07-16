#include <ponder/core/Result.hpp>

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{
struct ConstexprWidget final
{
    int value{0};

    [[nodiscard]] constexpr int GetValue() const noexcept
    {
        return value;
    }
};

constexpr bool ResultValueObserversAreConstexpr()
{
    pond::core::Result<int> result = 42;
    return result.HasValue() && static_cast<bool>(result) && result.GetValue() == 42 &&
           *result == 42;
}

constexpr bool ResultFactoryAndArrowAreConstexpr()
{
    const auto result = pond::core::Result<ConstexprWidget>::FromValue(ConstexprWidget{7});
    return result.HasValue() && result->GetValue() == 7;
}

constexpr bool VoidResultObserversAreConstexpr()
{
    const pond::core::VoidResult result = pond::core::VoidResult::Success();
    return result.HasValue() && static_cast<bool>(result);
}

constexpr pond::core::Result<int> ResultWithDormantRuntimeFailurePath(bool succeed)
{
    if (succeed)
    {
        return 7;
    }

    return pond::core::MakeUnexpected("runtime-only failure");
}
static_assert(ResultValueObserversAreConstexpr());
static_assert(ResultFactoryAndArrowAreConstexpr());
static_assert(VoidResultObserversAreConstexpr());
static_assert(ResultWithDormantRuntimeFailurePath(true).GetValue() == 7);
static_assert(noexcept(std::declval<const pond::core::Result<int>&>().HasValue()));
static_assert(noexcept(static_cast<bool>(std::declval<const pond::core::Result<int>&>())));

constexpr pond::core::ErrorCode kMacroErrorCode{pond::core::ErrorCategory::Unsupported, 21};

pond::core::Result<int> MakeMacroResult(bool succeed)
{
    if (succeed)
    {
        return 9;
    }

    return pond::core::Result<int>::FromError(pond::core::Error{kMacroErrorCode, "macro failed"});
}

pond::core::Result<int> MakeCountingMacroResult(int& callCount, bool succeed)
{
    ++callCount;
    return MakeMacroResult(succeed);
}

pond::core::Result<int> ReturnErrorIfFailedMacro(bool succeed)
{
    auto result = MakeMacroResult(succeed);
    RETURN_ERROR_IF_FAILED(result);

    return result.GetValue() + 1;
}

pond::core::Result<int> ReturnErrorIfFailedMacroFromExpression(int& callCount, bool succeed)
{
    RETURN_ERROR_IF_FAILED(MakeCountingMacroResult(callCount, succeed));

    return 10;
}

pond::core::Result<int> ReturnErrorIfFailedMacroWithCallback(std::string& observedMessage)
{
    auto result = MakeMacroResult(false);
    RETURN_ERROR_IF_FAILED_FN(
        result,
        [&observedMessage, prefix = std::string{"observed"}](const pond::core::Error& error)
        {
            observedMessage = prefix + ": " + std::string{error.GetMessage()};
        });

    return 10;
}

void ReturnVoidIfFailedMacro(bool succeed, bool& reachedSuccessPath)
{
    auto result = MakeMacroResult(succeed);
    RETURN_VOID_IF_FAILED(result);

    reachedSuccessPath = true;
}

void ReturnVoidIfFailedMacroWithCallback(std::string& observedMessage, bool& reachedSuccessPath)
{
    auto result = MakeMacroResult(false);
    RETURN_VOID_IF_FAILED_FN(
        result,
        [&observedMessage, prefix = std::string{"observed"}](const pond::core::Error& error)
        {
            observedMessage = prefix + ": " + std::string{error.GetMessage()};
        });

    reachedSuccessPath = true;
}
TEST(ResultTests, StoresValue)
{
    pond::core::Result<int> result = 42;

    ASSERT_TRUE(result);
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue(), 42);
    EXPECT_EQ(*result, 42);
}

TEST(ResultTests, ConstructsValueInPlace)
{
    const auto result = pond::core::Result<std::string>::FromValue(3, 'x');

    ASSERT_TRUE(result);
    EXPECT_EQ(result.GetValue(), "xxx");
    EXPECT_EQ(result->size(), 3U);
}

TEST(ResultTests, StoresError)
{
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::InvalidArgument, 12};

    pond::core::Result<int> result =
        pond::core::Result<int>::FromError(pond::core::Error{kCode, "bad value"});

    ASSERT_FALSE(result);
    EXPECT_TRUE(result.GetError().GetCode() == kCode);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"bad value"});
}

TEST(ResultTests, AcceptsUnexpectedHelper)
{
    pond::core::Result<std::string> result = pond::core::MakeUnexpected(
        pond::core::ErrorCode{pond::core::ErrorCategory::NotFound, 7}, "missing");

    ASSERT_FALSE(result.HasValue());
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"missing"});
}

TEST(ResultTests, SupportsMoveOnlyValues)
{
    auto value = std::make_unique<int>(5);

    pond::core::Result<std::unique_ptr<int>> result = std::move(value);

    ASSERT_TRUE(result);
    ASSERT_NE(result.GetValue(), nullptr);
    EXPECT_EQ(*result.GetValue(), 5);
}

TEST(ResultTests, MovesValueFromRvalueResult)
{
    auto result = pond::core::Result<std::string>::FromValue("move me");

    std::string value = std::move(result).GetValue();

    EXPECT_EQ(value, "move me");
}

TEST(ResultTests, MovesErrorFromRvalueResult)
{
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::Parse, 4};
    auto result = pond::core::Result<int>::FromError(pond::core::Error{kCode, "bad parse"});

    pond::core::Error error = std::move(result).GetError();

    EXPECT_TRUE(error.GetCode() == kCode);
    EXPECT_EQ(error.GetMessage(), std::string_view{"bad parse"});
}

TEST(VoidResultTests, DefaultsToSuccess)
{
    pond::core::VoidResult result;

    EXPECT_TRUE(result);
    EXPECT_TRUE(result.HasValue());
}

TEST(VoidResultTests, ExplicitSuccessCanBeConsumed)
{
    pond::core::VoidResult result = pond::core::VoidResult::Success();

    ASSERT_TRUE(result);
    EXPECT_NO_THROW(result.GetValue());
}

TEST(VoidResultTests, StoresError)
{
    pond::core::VoidResult result = pond::core::MakeUnexpected("void failed");

    ASSERT_FALSE(result);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"void failed"});
}

TEST(ResultMacroTests, ReturnErrorIfFailedMacroContinuesOnSuccess)
{
    auto result = ReturnErrorIfFailedMacro(true);

    ASSERT_TRUE(result);
    EXPECT_EQ(result.GetValue(), 10);
}

TEST(ResultMacroTests, ReturnErrorIfFailedMacroPropagatesFailure)
{
    auto result = ReturnErrorIfFailedMacro(false);

    ASSERT_FALSE(result);
    EXPECT_TRUE(result.GetError() == kMacroErrorCode);
    EXPECT_EQ(result.GetError().GetMessage(), std::string_view{"macro failed"});
}

TEST(ResultMacroTests, ReturnErrorIfFailedMacroEvaluatesExpressionOnce)
{
    int callCount{};

    auto result = ReturnErrorIfFailedMacroFromExpression(callCount, false);

    ASSERT_FALSE(result);
    EXPECT_EQ(callCount, 1);
    EXPECT_TRUE(result.GetError() == kMacroErrorCode);
}

TEST(ResultMacroTests, ReturnErrorIfFailedCallbackReceivesErrorBeforePropagation)
{
    std::string observedMessage;

    auto result = ReturnErrorIfFailedMacroWithCallback(observedMessage);

    ASSERT_FALSE(result);
    EXPECT_EQ(observedMessage, "observed: macro failed");
    EXPECT_TRUE(result.GetError() == kMacroErrorCode);
}

TEST(ResultMacroTests, ReturnVoidIfFailedMacroReturnsOnlyOnFailure)
{
    bool reachedSuccessPath{};

    ReturnVoidIfFailedMacro(true, reachedSuccessPath);
    EXPECT_TRUE(reachedSuccessPath);

    reachedSuccessPath = false;
    ReturnVoidIfFailedMacro(false, reachedSuccessPath);
    EXPECT_FALSE(reachedSuccessPath);
}

TEST(ResultMacroTests, ReturnVoidIfFailedCallbackReceivesErrorBeforeReturn)
{
    std::string observedMessage;
    bool reachedSuccessPath{};

    ReturnVoidIfFailedMacroWithCallback(observedMessage, reachedSuccessPath);

    EXPECT_EQ(observedMessage, "observed: macro failed");
    EXPECT_FALSE(reachedSuccessPath);
}
} // namespace
