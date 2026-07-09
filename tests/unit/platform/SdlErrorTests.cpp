#include "SdlError.hpp"

#include <SDL3/SDL_error.h>

#include <gtest/gtest.h>
#include <string_view>

namespace
{
using pond::platform::detail::CaptureSdlFailure;

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
    constexpr pond::core::ErrorCode kCode{pond::core::ErrorCategory::Unsupported, 73};
    static_cast<void>(SDL_SetError("synthetic failure"));

    const pond::core::Error error = pond::platform::detail::CaptureSdlFailure(
        kCode, "SDL_TestOperation", "window 7");

    EXPECT_TRUE(error.GetCode() == kCode);
    EXPECT_EQ(error.GetMessage(),
              std::string_view{"SDL_TestOperation failed (window 7): synthetic failure"});
    EXPECT_STREQ(SDL_GetError(), "synthetic failure");
}

TEST_F(PlatformSdlErrorTests, OmitsEmptyObjectContext)
{
    static_cast<void>(SDL_SetError("synthetic failure"));

    const pond::core::Error error = pond::platform::detail::CaptureSdlFailure(
        pond::core::ErrorCode{}, "SDL_TestOperation");

    EXPECT_EQ(error.GetMessage(),
              std::string_view{"SDL_TestOperation failed: synthetic failure"});
}

TEST_F(PlatformSdlErrorTests, OwnsErrorTextAfterSdlStateChanges)
{
    static_cast<void>(SDL_SetError("first failure"));
    const pond::core::Error error = pond::platform::detail::CaptureSdlFailure(
        pond::core::ErrorCode{}, "SDL_TestOperation", "window 7");

    static_cast<void>(SDL_SetError("second failure"));

    EXPECT_EQ(error.GetMessage(),
              std::string_view{"SDL_TestOperation failed (window 7): first failure"});
}

TEST_F(PlatformSdlErrorTests, UsesFallbackForEmptySdlError)
{
    const pond::core::Error error = pond::platform::detail::CaptureSdlFailure(
        pond::core::ErrorCode{}, "SDL_TestOperation", "window 7");

    EXPECT_EQ(error.GetMessage(), std::string_view{
                                      "SDL_TestOperation failed (window 7): "
                                      "SDL did not provide an error message"});
}

TEST_F(PlatformSdlErrorTests, UsesCallerSourceLocationByDefault)
{
    constexpr pond::core::ErrorCode kCode;
    static_cast<void>(SDL_SetError("synthetic failure"));

    const auto expectedLine = __LINE__ + 1;
    const pond::core::Error error = CaptureSdlFailure(kCode, "SDL_TestOperation");

    EXPECT_STREQ(error.GetLocation().file_name(), __FILE__);
    EXPECT_EQ(error.GetLocation().line(), expectedLine);
}
} // namespace
