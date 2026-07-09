#pragma once

#include <ponder/platform/WindowGraphics.hpp>

#include <cstdint>
#include <vector>

namespace pond::platform::detail
{
enum class ApplicationMetadataProperty : std::uint8_t
{
    Name,
    Version,
    Identifier
};

struct BackendWindowCreateDesc final
{
    const char* title{};
    int width{};
    int height{};
    bool resizable{};
    bool highPixelDensity{};
    WindowGraphicsCompatibility graphicsCompatibility{
        WindowGraphicsCompatibility::Default};
};

struct PlatformWindowBackend final
{
    void* context{};
    void* (*create)(void* context, const BackendWindowCreateDesc& desc){};
    void (*destroy)(void* context, void* window){};
    std::uint32_t (*getId)(void* context, void* window){};
    const char* (*getTitle)(void* context, void* window){};
    bool (*setTitle)(void* context, void* window, const char* title){};
    bool (*getPosition)(void* context, void* window, int* x, int* y){};
    bool (*setPosition)(void* context, void* window, int x, int y){};
    bool (*getSize)(void* context, void* window, int* width, int* height){};
    bool (*getSizeInPixels)(void* context, void* window, int* width, int* height){};
    bool (*setSize)(void* context, void* window, int width, int height){};
    bool (*setMinimumSize)(void* context, void* window, int width, int height){};
    bool (*show)(void* context, void* window){};
    bool (*hide)(void* context, void* window){};
};

enum class BackendDisplayOrientation : std::uint8_t
{
    Unknown,
    Landscape,
    LandscapeFlipped,
    Portrait,
    PortraitFlipped
};

struct BackendScreenRectangle final
{
    int x{};
    int y{};
    int width{};
    int height{};
};

struct PlatformDisplayBackend final
{
    void* context{};
    bool (*enumerate)(void* context, std::vector<std::uint32_t>& displayIds){};
    const char* (*getName)(void* context, std::uint32_t displayId){};
    bool (*getBounds)(void* context, std::uint32_t displayId,
                      BackendScreenRectangle* bounds){};
    bool (*getUsableBounds)(void* context, std::uint32_t displayId,
                            BackendScreenRectangle* bounds){};
    bool (*getCurrentRefreshRate)(void* context, std::uint32_t displayId,
                                  float* refreshRateHertz){};
    BackendDisplayOrientation (*getCurrentOrientation)(void* context,
                                                       std::uint32_t displayId){};
    float (*getContentScale)(void* context, std::uint32_t displayId){};
    std::uint32_t (*getForWindow)(void* context, void* window){};
    float (*getWindowPixelDensity)(void* context, void* window){};
    float (*getWindowDisplayScale)(void* context, void* window){};
};

struct PlatformRuntimeBackend final
{
    void* context{};
    bool (*isMainThread)(void* context){};
    bool (*hasInitializedSubsystems)(void* context){};
    bool (*hasExpectedRuntimeSubsystems)(void* context){};
    const char* (*getAppMetadataProperty)(void* context,
                                          ApplicationMetadataProperty property){};
    bool (*setAppMetadataProperty)(void* context, ApplicationMetadataProperty property,
                                   const char* value){};
    const char* (*getHint)(void* context, const char* name){};
    bool (*setHintOverride)(void* context, const char* name, const char* value){};
    bool (*resetHint)(void* context, const char* name){};
    bool (*initializeVideo)(void* context){};
    void (*quit)(void* context){};
    std::uint64_t (*getTicksNanoseconds)(void* context){};
};

inline constexpr char kMouseFocusClickThroughHint[]{"SDL_MOUSE_FOCUS_CLICKTHROUGH"};
inline constexpr char kMouseAutoCaptureHint[]{"SDL_MOUSE_AUTO_CAPTURE"};

[[nodiscard]] bool IsWindowGraphicsCompatibilitySupported(
    WindowGraphicsCompatibility compatibility) noexcept;
[[nodiscard]] std::uint64_t BuildSdlWindowFlags(
    const BackendWindowCreateDesc& desc) noexcept;
[[nodiscard]] bool IsReservedSdlWindowPosition(std::int32_t value) noexcept;

[[nodiscard]] PlatformRuntimeBackend GetPlatformRuntimeBackend() noexcept;
[[nodiscard]] PlatformWindowBackend GetPlatformWindowBackend() noexcept;
[[nodiscard]] PlatformDisplayBackend GetPlatformDisplayBackend() noexcept;

// The override must outlive every runtime created while it is installed.
void SetPlatformRuntimeBackendForTesting(const PlatformRuntimeBackend* backend) noexcept;
void SetPlatformWindowBackendForTesting(const PlatformWindowBackend* backend) noexcept;
void SetPlatformDisplayBackendForTesting(const PlatformDisplayBackend* backend) noexcept;
} // namespace pond::platform::detail
