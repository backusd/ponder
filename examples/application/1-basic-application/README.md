# Basic Application

This minimal example derives from `ponder::application::Application`. The base class creates the platform runtime, owns the event loop, dispatches
typed events, and destroys the window after a close request. Press Escape or use the native close command to exit.

The override of `PrePlatformInitialization(Runtime&)` configures a typed hint
after the base calls `Runtime::Create()` and before it calls
`Runtime::Initialize()` with the example's `ApplicationDesc` metadata.
