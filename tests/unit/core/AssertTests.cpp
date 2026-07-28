#include <ponder/core/Assert.hpp>
#include <ponder/core/Exception.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
struct CapturedFailure
{
    ponder::core::AssertionFailureKind kind{ponder::core::AssertionFailureKind::Assertion};
    std::string expression;
    std::string message;
    std::source_location location;
    int count{0};
};

CapturedFailure assertionCapture;
CapturedFailure verifyCapture;

void ResetCapture(CapturedFailure& capture)
{
    capture = CapturedFailure{};
}

void Capture(CapturedFailure& capture, const ponder::core::AssertionFailure& failure)
{
    capture.kind = failure.GetKind();
    capture.expression = std::string{failure.GetExpression()};
    capture.message = std::string{failure.GetMessage()};
    capture.location = failure.GetLocation();
    ++capture.count;
}

void ThrowingAssertionHandler(const ponder::core::AssertionFailure& failure)
{
    Capture(assertionCapture, failure);
    throw PONDER_EXCEPTION("assertion captured");
}

void AlternateThrowingAssertionHandler(const ponder::core::AssertionFailure& failure)
{
    Capture(assertionCapture, failure);
    throw PONDER_EXCEPTION("alternate assertion captured");
}

#if defined(NDEBUG)
void CapturingAssertionHandler(const ponder::core::AssertionFailure& failure)
{
    Capture(assertionCapture, failure);
}
#endif

void CapturingVerifyHandler(const ponder::core::AssertionFailure& failure)
{
    Capture(verifyCapture, failure);
}

constexpr bool AssertionFailureStoresDataAtCompileTime()
{
    const auto location = std::source_location::current();
    const ponder::core::AssertionFailure failure{ponder::core::AssertionFailureKind::Verify, "value != 0", "bad value", location};

    return failure.GetKind() == ponder::core::AssertionFailureKind::Verify && failure.GetExpression() == std::string_view{"value != 0"} &&
           failure.GetMessage() == std::string_view{"bad value"} && failure.GetLocation().line() == location.line();
}

static_assert(AssertionFailureStoresDataAtCompileTime());
static_assert(noexcept(std::declval<const ponder::core::AssertionFailure&>().GetKind()));
static_assert(noexcept(std::declval<const ponder::core::AssertionFailure&>().GetExpression()));
static_assert(noexcept(std::declval<const ponder::core::AssertionFailure&>().GetMessage()));
static_assert(noexcept(std::declval<const ponder::core::AssertionFailure&>().GetLocation()));

TEST(AssertionFailureTests, StoresFailureData)
{
    const auto location = std::source_location::current();
    const ponder::core::AssertionFailure failure{ponder::core::AssertionFailureKind::Verify, "value != 0", "bad value", location};

    EXPECT_EQ(failure.GetKind(), ponder::core::AssertionFailureKind::Verify);
    EXPECT_EQ(failure.GetExpression(), std::string_view{"value != 0"});
    EXPECT_EQ(failure.GetMessage(), std::string_view{"bad value"});
    EXPECT_STREQ(failure.GetLocation().file_name(), location.file_name());
    EXPECT_EQ(failure.GetLocation().line(), location.line());
}

TEST(AssertionFailureTests, FormatsFailureMessages)
{
    const ponder::core::AssertionFailure assertion{ponder::core::AssertionFailureKind::Assertion, "ready", "not ready",
                                                   std::source_location::current()};
    const ponder::core::AssertionFailure verify{ponder::core::AssertionFailureKind::Verify, "valid", "not valid", std::source_location::current()};
    const ponder::core::AssertionFailure unreachable{ponder::core::AssertionFailureKind::Unreachable, "", "bad branch",
                                                     std::source_location::current()};

    EXPECT_EQ(ponder::core::FormatAssertionFailure(assertion), "Assertion failed: ready (not ready)");
    EXPECT_EQ(ponder::core::FormatAssertionFailure(verify), "Verification failed: valid (not valid)");
    EXPECT_EQ(ponder::core::FormatAssertionFailure(unreachable), "Unreachable code reached (bad branch)");
}

TEST(AssertionFailureTests, HandlesUnknownFailureKindWithFallbackPrefix)
{
    static_assert(std::is_same_v<std::underlying_type_t<ponder::core::AssertionFailureKind>, std::uint8_t>);

    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    const auto unknownKind = static_cast<ponder::core::AssertionFailureKind>(255);
    const ponder::core::AssertionFailure failure{unknownKind, "ready", "unknown", std::source_location::current()};

    EXPECT_EQ(ponder::core::FormatAssertionFailure(failure), "Failure: ready (unknown)");
}

TEST(AssertionHandlerTests, ScopedOverrideRestoresPreviousHandler)
{
    ResetCapture(assertionCapture);

    const ponder::core::ScopedAssertionFailureHandler outer{ThrowingAssertionHandler};

    {
        const ponder::core::ScopedAssertionFailureHandler inner{AlternateThrowingAssertionHandler};

        try
        {
            ponder::core::HandleAssertionFailure("inner", "message");
        }
        catch (const ponder::core::Exception& exception)
        {
            EXPECT_EQ(exception.GetMessage(), std::string_view{"alternate assertion captured"});
        }
    }

    try
    {
        ponder::core::HandleAssertionFailure("outer", "message");
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"assertion captured"});
    }

    EXPECT_EQ(assertionCapture.expression, "outer");
    EXPECT_EQ(assertionCapture.message, "message");
    EXPECT_EQ(assertionCapture.count, 2);
}

TEST(AssertionMacroTests, FormatsMessageAndCapturesSourceLocation)
{
    ResetCapture(assertionCapture);
    const ponder::core::ScopedAssertionFailureHandler handler{ThrowingAssertionHandler};

#if defined(NDEBUG)
    int sideEffect = 0;
    PONDER_ASSERT(++sideEffect == 1, "side effect {}", sideEffect);
    EXPECT_EQ(sideEffect, 0);
#else
    const auto expectedLine = __LINE__ + 3;
    try
    {
        PONDER_ASSERT(false, "value {}", 42);
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"assertion captured"});
        EXPECT_EQ(assertionCapture.kind, ponder::core::AssertionFailureKind::Assertion);
        EXPECT_EQ(assertionCapture.expression, "false");
        EXPECT_EQ(assertionCapture.message, "value 42");
        EXPECT_STREQ(assertionCapture.location.file_name(), __FILE__);
        EXPECT_EQ(assertionCapture.location.line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_ASSERT should invoke the assertion handler";
#endif
}

TEST(AssertionMacroTests, SupportsAssertionsWithoutMessages)
{
    ResetCapture(assertionCapture);
    const ponder::core::ScopedAssertionFailureHandler handler{ThrowingAssertionHandler};

#if defined(NDEBUG)
    int sideEffect = 0;
    PONDER_ASSERT(++sideEffect == 1);
    EXPECT_EQ(sideEffect, 0);
#else
    try
    {
        PONDER_ASSERT(false);
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"assertion captured"});
        EXPECT_EQ(assertionCapture.kind, ponder::core::AssertionFailureKind::Assertion);
        EXPECT_EQ(assertionCapture.expression, "false");
        EXPECT_TRUE(assertionCapture.message.empty());
        return;
    }

    FAIL() << "PONDER_ASSERT should support a message-free invocation";
#endif
}

TEST(AssertionMacroTests, DoesNotEvaluateMessageArgumentsWhenExpressionIsTrue)
{
    int sideEffect = 0;

    PONDER_ASSERT(true, "value {}", ++sideEffect);

    EXPECT_EQ(sideEffect, 0);
}

TEST(VerifyTests, InvokesHandlerFormatsMessageAndThrows)
{
    ResetCapture(verifyCapture);
    const ponder::core::ScopedVerifyFailureHandler handler{CapturingVerifyHandler};

    const auto expectedLine = __LINE__ + 3;
    try
    {
        PONDER_VERIFY(false, "value {}", 7);
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"Verification failed: false (value 7)"});
        EXPECT_EQ(verifyCapture.kind, ponder::core::AssertionFailureKind::Verify);
        EXPECT_EQ(verifyCapture.expression, "false");
        EXPECT_EQ(verifyCapture.message, "value 7");
        EXPECT_STREQ(verifyCapture.location.file_name(), __FILE__);
        EXPECT_EQ(verifyCapture.location.line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_VERIFY should throw";
}

TEST(VerifyTests, DoesNotEvaluateMessageArgumentsWhenExpressionIsTrue)
{
    int sideEffect = 0;

    PONDER_VERIFY(true, "value {}", ++sideEffect);

    EXPECT_EQ(sideEffect, 0);
}

TEST(UnreachableTests, InvokesAssertionHandler)
{
    ResetCapture(assertionCapture);
    const ponder::core::ScopedAssertionFailureHandler handler{ThrowingAssertionHandler};

    const auto expectedLine = __LINE__ + 3;
    void (*unreachableFunction)() = []()
    {
        PONDER_UNREACHABLE("branch {}", "bad");
    };

    try
    {
        unreachableFunction();
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"assertion captured"});
        EXPECT_EQ(assertionCapture.kind, ponder::core::AssertionFailureKind::Unreachable);
        EXPECT_TRUE(assertionCapture.expression.empty());
        EXPECT_EQ(assertionCapture.message, "branch bad");
        EXPECT_STREQ(assertionCapture.location.file_name(), __FILE__);
        EXPECT_EQ(assertionCapture.location.line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_UNREACHABLE should invoke the assertion handler";
}

#if defined(NDEBUG)
TEST(UnreachableTests, ThrowsExceptionAfterReturningHandlerInRelease)
{
    ResetCapture(assertionCapture);
    const ponder::core::ScopedAssertionFailureHandler handler{CapturingAssertionHandler};

    try
    {
        PONDER_UNREACHABLE("branch {}", "bad");
    }
    catch (const ponder::core::Exception& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"Unreachable code reached (branch bad)"});
        EXPECT_EQ(assertionCapture.kind, ponder::core::AssertionFailureKind::Unreachable);
        EXPECT_EQ(assertionCapture.message, "branch bad");
        return;
    }

    FAIL() << "PONDER_UNREACHABLE should throw in release builds";
}
#endif
} // namespace
