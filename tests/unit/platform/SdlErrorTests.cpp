#include <ponder/core/Exception.hpp>
#include <ponder/platform/PlatformError.hpp>

#include <SDL3/SDL_error.h>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "SdlError.hpp"

namespace
{
using ponder::platform::detail::CaptureSdlFailure;
using ponder::platform::detail::CaptureSdlFailureMessage;
using ponder::platform::detail::FormatCapturedSdlFailureMessage;

class PlatformSdlErrorTests : public testing::Test
{
protected:
    void SetUp() override
    {
        static_cast<void>(SDL_ClearError());
    }

    void TearDown() override
    {
        static_cast<void>(SDL_ClearError());
    }
};

TEST_F(PlatformSdlErrorTests, PreservesCallerSelectedCodeAndFormatsContext)
{
    constexpr ponder::core::ErrorCode kCode{ponder::core::ErrorCategory::Unsupported, 73};
    static_cast<void>(SDL_SetError("synthetic failure"));

    const ponder::core::Error error = ponder::platform::detail::CaptureSdlFailure(kCode, "SDL_TestOperation", "window 7");

    EXPECT_TRUE(error.GetCode() == kCode);
    EXPECT_EQ(error.GetMessage(), std::string_view{"SDL_TestOperation failed (window 7): synthetic failure"});
    EXPECT_STREQ(SDL_GetError(), "synthetic failure");
}

TEST_F(PlatformSdlErrorTests, OmitsEmptyObjectContext)
{
    static_cast<void>(SDL_SetError("synthetic failure"));

    const ponder::core::Error error = ponder::platform::detail::CaptureSdlFailure(ponder::core::ErrorCode{}, "SDL_TestOperation");

    EXPECT_EQ(error.GetMessage(), std::string_view{"SDL_TestOperation failed: synthetic failure"});
}

TEST_F(PlatformSdlErrorTests, OwnsErrorTextAfterSdlStateChanges)
{
    static_cast<void>(SDL_SetError("first failure"));
    const ponder::core::Error error = ponder::platform::detail::CaptureSdlFailure(ponder::core::ErrorCode{}, "SDL_TestOperation", "window 7");

    static_cast<void>(SDL_SetError("second failure"));

    EXPECT_EQ(error.GetMessage(), std::string_view{"SDL_TestOperation failed (window 7): first failure"});
}

TEST_F(PlatformSdlErrorTests, UsesFallbackForEmptySdlError)
{
    const ponder::core::Error error = ponder::platform::detail::CaptureSdlFailure(ponder::core::ErrorCode{}, "SDL_TestOperation", "window 7");

    EXPECT_EQ(error.GetMessage(), std::string_view{"SDL_TestOperation failed (window 7): "
                                                   "SDL did not provide an error message"});
}

TEST_F(PlatformSdlErrorTests, FormatsAlreadyCapturedErrorWithoutReplacingItFromSdlState)
{
    constexpr ponder::core::ErrorCode kCode{ponder::core::ErrorCategory::Unsupported, 73};
    static_cast<void>(SDL_SetError("new SDL state"));

    const ponder::core::Error error =
        ponder::platform::detail::CaptureSdlFailure(kCode, "SDL_GetClipboardText", "clipboard text", "captured clipboard failure");

    EXPECT_TRUE(error.GetCode() == kCode);
    EXPECT_EQ(error.GetMessage(), std::string_view{"SDL_GetClipboardText failed (clipboard text): "
                                                   "captured clipboard failure"});
    EXPECT_STREQ(SDL_GetError(), "new SDL state");
}

TEST_F(PlatformSdlErrorTests, UsesFallbackForEmptyCapturedError)
{
    const ponder::core::Error error =
        ponder::platform::detail::CaptureSdlFailure(ponder::core::ErrorCode{}, "SDL_GetClipboardText", "clipboard text", "");

    EXPECT_EQ(error.GetMessage(), std::string_view{"SDL_GetClipboardText failed (clipboard text): "
                                                   "SDL did not provide an error message"});
}

TEST_F(PlatformSdlErrorTests, UsesCallerSourceLocationByDefault)
{
    constexpr ponder::core::ErrorCode kCode;
    static_cast<void>(SDL_SetError("synthetic failure"));

    const auto expectedLine = __LINE__ + 1;
    const ponder::core::Error error = CaptureSdlFailure(kCode, "SDL_TestOperation");

    EXPECT_STREQ(error.GetLocation().file_name(), __FILE__);
    EXPECT_EQ(error.GetLocation().line(), expectedLine);
}

TEST_F(PlatformSdlErrorTests, BuildsPlatformExceptionWithoutLosingSdlContextOrSourceLocation)
{
    static_cast<void>(SDL_SetError("synthetic failure"));

    const auto expectedLine = __LINE__ + 2;
    const ponder::core::Exception exception =
        PLATFORM_EXCEPTION(ponder::platform::PlatformErrorCode::BackendFailure, "{}", CaptureSdlFailureMessage("SDL_TestOperation", "window 7"));

    EXPECT_EQ(exception.GetMessage(), std::string_view{"Platform error [backend_failure]: "
                                                       "SDL_TestOperation failed (window 7): "
                                                       "synthetic failure"});
    EXPECT_STREQ(exception.GetLocation().file_name(), __FILE__);
    EXPECT_EQ(exception.GetLocation().line(), expectedLine);
    EXPECT_STREQ(SDL_GetError(), "synthetic failure");
}

TEST_F(PlatformSdlErrorTests, FormatsCapturedMessageWithoutConsultingCurrentSdlState)
{
    static_cast<void>(SDL_SetError("new SDL state"));

    const std::string message = FormatCapturedSdlFailureMessage("SDL_GetClipboardText", "clipboard text", "captured clipboard failure");

    EXPECT_EQ(message, std::string_view{"SDL_GetClipboardText failed (clipboard text): "
                                        "captured clipboard failure"});
    EXPECT_STREQ(SDL_GetError(), "new SDL state");
}
} // namespace
