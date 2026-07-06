#include <ponder/core/Assert.hpp>
#include <ponder/core/PonderException.hpp>

#include <gtest/gtest.h>
#include <source_location>
#include <string>
#include <string_view>

namespace
{
struct CapturedFailure
{
    pond::core::AssertionFailureKind Kind{pond::core::AssertionFailureKind::Assertion};
    std::string Expression;
    std::string Message;
    std::source_location Location;
    int Count{0};
};

CapturedFailure g_assertionCapture;
CapturedFailure g_verifyCapture;

void ResetCapture(CapturedFailure& capture)
{
    capture = CapturedFailure{};
}

void Capture(CapturedFailure& capture, const pond::core::AssertionFailure& failure)
{
    capture.Kind = failure.GetKind();
    capture.Expression = std::string{failure.GetExpression()};
    capture.Message = std::string{failure.GetMessage()};
    capture.Location = failure.GetLocation();
    ++capture.Count;
}

void ThrowingAssertionHandler(const pond::core::AssertionFailure& failure)
{
    Capture(g_assertionCapture, failure);
    throw PONDER_EXCEPTION("assertion captured");
}

void AlternateThrowingAssertionHandler(const pond::core::AssertionFailure& failure)
{
    Capture(g_assertionCapture, failure);
    throw PONDER_EXCEPTION("alternate assertion captured");
}

void CapturingVerifyHandler(const pond::core::AssertionFailure& failure)
{
    Capture(g_verifyCapture, failure);
}

TEST(AssertionFailureTests, StoresFailureData)
{
    const auto location = std::source_location::current();
    const pond::core::AssertionFailure failure{pond::core::AssertionFailureKind::Verify,
                                               "value != 0", "bad value", location};

    EXPECT_EQ(failure.GetKind(), pond::core::AssertionFailureKind::Verify);
    EXPECT_EQ(failure.GetExpression(), std::string_view{"value != 0"});
    EXPECT_EQ(failure.GetMessage(), std::string_view{"bad value"});
    EXPECT_STREQ(failure.GetLocation().file_name(), location.file_name());
    EXPECT_EQ(failure.GetLocation().line(), location.line());
}

TEST(AssertionFailureTests, FormatsFailureMessages)
{
    const pond::core::AssertionFailure assertion{pond::core::AssertionFailureKind::Assertion,
                                                 "ready", "not ready",
                                                 std::source_location::current()};
    const pond::core::AssertionFailure verify{pond::core::AssertionFailureKind::Verify, "valid",
                                              "not valid", std::source_location::current()};
    const pond::core::AssertionFailure unreachable{pond::core::AssertionFailureKind::Unreachable,
                                                   "", "bad branch",
                                                   std::source_location::current()};

    EXPECT_EQ(pond::core::FormatAssertionFailure(assertion), "Assertion failed: ready (not ready)");
    EXPECT_EQ(pond::core::FormatAssertionFailure(verify), "Verification failed: valid (not valid)");
    EXPECT_EQ(pond::core::FormatAssertionFailure(unreachable),
              "Unreachable code reached (bad branch)");
}

TEST(AssertionHandlerTests, ScopedOverrideRestoresPreviousHandler)
{
    ResetCapture(g_assertionCapture);

    const pond::core::ScopedAssertionFailureHandler outer{ThrowingAssertionHandler};

    {
        const pond::core::ScopedAssertionFailureHandler inner{AlternateThrowingAssertionHandler};

        try
        {
            pond::core::HandleAssertionFailure("inner", "message");
        }
        catch (const pond::core::PonderException& exception)
        {
            EXPECT_EQ(exception.GetMessage(), std::string_view{"alternate assertion captured"});
        }
    }

    try
    {
        pond::core::HandleAssertionFailure("outer", "message");
    }
    catch (const pond::core::PonderException& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"assertion captured"});
    }

    EXPECT_EQ(g_assertionCapture.Expression, "outer");
    EXPECT_EQ(g_assertionCapture.Message, "message");
    EXPECT_EQ(g_assertionCapture.Count, 2);
}

TEST(AssertionMacroTests, FormatsMessageAndCapturesSourceLocation)
{
    ResetCapture(g_assertionCapture);
    const pond::core::ScopedAssertionFailureHandler handler{ThrowingAssertionHandler};

#if defined(NDEBUG)
    int sideEffect = 0;
    PONDER_ASSERT_MESSAGE(++sideEffect == 1, "side effect {}", sideEffect);
    EXPECT_EQ(sideEffect, 0);
#else
    const auto expectedLine = __LINE__ + 3;
    try
    {
        PONDER_ASSERT_MESSAGE(false, "value {}", 42);
    }
    catch (const pond::core::PonderException& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"assertion captured"});
        EXPECT_EQ(g_assertionCapture.Kind, pond::core::AssertionFailureKind::Assertion);
        EXPECT_EQ(g_assertionCapture.Expression, "false");
        EXPECT_EQ(g_assertionCapture.Message, "value 42");
        EXPECT_STREQ(g_assertionCapture.Location.file_name(), __FILE__);
        EXPECT_EQ(g_assertionCapture.Location.line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_ASSERT_MESSAGE should invoke the assertion handler";
#endif
}

TEST(VerifyTests, InvokesHandlerFormatsMessageAndThrows)
{
    ResetCapture(g_verifyCapture);
    const pond::core::ScopedVerifyFailureHandler handler{CapturingVerifyHandler};

    const auto expectedLine = __LINE__ + 3;
    try
    {
        PONDER_VERIFY(false, "value {}", 7);
    }
    catch (const pond::core::PonderException& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"Verification failed: false (value 7)"});
        EXPECT_EQ(g_verifyCapture.Kind, pond::core::AssertionFailureKind::Verify);
        EXPECT_EQ(g_verifyCapture.Expression, "false");
        EXPECT_EQ(g_verifyCapture.Message, "value 7");
        EXPECT_STREQ(g_verifyCapture.Location.file_name(), __FILE__);
        EXPECT_EQ(g_verifyCapture.Location.line(), expectedLine);
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
    ResetCapture(g_assertionCapture);
    const pond::core::ScopedAssertionFailureHandler handler{ThrowingAssertionHandler};

    const auto expectedLine = __LINE__ + 3;
    void (*unreachableFunction)() = []()
    {
        PONDER_UNREACHABLE("branch {}", "bad");
    };

    try
    {
        unreachableFunction();
    }
    catch (const pond::core::PonderException& exception)
    {
        EXPECT_EQ(exception.GetMessage(), std::string_view{"assertion captured"});
        EXPECT_EQ(g_assertionCapture.Kind, pond::core::AssertionFailureKind::Unreachable);
        EXPECT_TRUE(g_assertionCapture.Expression.empty());
        EXPECT_EQ(g_assertionCapture.Message, "branch bad");
        EXPECT_STREQ(g_assertionCapture.Location.file_name(), __FILE__);
        EXPECT_EQ(g_assertionCapture.Location.line(), expectedLine);
        return;
    }

    FAIL() << "PONDER_UNREACHABLE should invoke the assertion handler";
}
} // namespace
