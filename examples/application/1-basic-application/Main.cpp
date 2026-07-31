#include <ponder/application/Application.hpp>
#include <ponder/core/Exception.hpp>
#include <ponder/platform/Hints.hpp>
#include <ponder/platform/Keyboard.hpp>

#include <exception>
#include <iostream>

namespace
{
namespace application = ponder::application;
namespace platform = ponder::platform;

class BasicApplication final : public application::Application
{
public:
    BasicApplication() :
        Application(application::ApplicationDesc{.applicationName = "ponder Basic Application",
                                                 .applicationVersion = "1.0.0",
                                                 .applicationIdentifier = "org.ponder.basic-application"})
    {
    }

private:
    void PrePlatformInitialization(platform::Runtime& runtime) override
    {
        runtime.HintPush(platform::hints::QuitOnLastWindowClose{false});
    }

    void OnStart() override
    {
        platform::Window& window = WindowCreate(platform::WindowDesc{.title = "ponder Basic Application", .logicalSize = {960, 600}});
        m_windowId = window.GetId();
    }

    void OnKeyboardKeyEvent(const platform::KeyboardKeyEvent& event) override
    {
        if (event.pressed && event.physicalKey == platform::PhysicalKey::Escape && m_windowId.IsValid())
        {
            WindowClose(m_windowId);
        }
    }

    platform::WindowId m_windowId;
};
} // namespace

int main()
{
    try
    {
        BasicApplication application;
        return application.Run();
    }
    catch (const ponder::core::Exception& exception)
    {
        std::cerr << "Application failure: " << exception.GetMessage() << '\n';
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Unexpected failure: " << exception.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown failure\n";
    }
    return 1;
}
