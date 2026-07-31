# Application Examples

These examples use `ponder::application::Application` as the high-level owner of the platform runtime, event loop, windows, dialogs, and managed
background-process shutdown policy.

The Application base owns Runtime's explicit startup sequence: it calls
`Runtime::Create()`, invokes the derived `PrePlatformInitialization(Runtime&)`
hook for typed hint configuration, then calls `Runtime::Initialize()` with the
`ApplicationDesc` metadata. Derived code never supplies a Runtime factory
callback or retains the borrowed pre-initialization Runtime reference.
The hook receives an already constructed backend object that is not yet
initialized; hint calls configure it, while `Initialize()` alone activates it.

Application's derived-facing dialog submission and pending-state methods are
`noexcept` and Result-bearing. Submission mirrors platform's fallible Results;
pending-state wrappers add Application lifecycle and owner-thread validation
around platform's direct observers. The base consumes direct completion
observations, checks the shutdown Result internally, and preserves mandatory
window/dialog cleanup.
Application timing uses `ponder::core::Timestamp` in the core steady-clock
domain rather than an SDL-specific clock.
