# Core ScopeExit

`ScopeExit` is a tiny RAII helper for local cleanup and temporary state
restoration. Use it when a full named RAII type would add ceremony without
creating a reusable concept.

## Public API

- `MakeScopeExit(callback)` creates a move-only guard.
- The cleanup callback runs when the active guard leaves scope.
- Moving a guard transfers cleanup ownership and leaves the moved-from guard
  inactive.
- `Dismiss()` makes the guard inactive without running the callback.
- `IsActive()` reports whether the guard still owns cleanup.

## No-Throw Contract

`ScopeExit` destructors are `noexcept`. Cleanup callbacks must therefore be
no-throw invocable, no-throw move constructible, and no-throw destructible. Use a
normal named RAII type or explicit cleanup path when cleanup can fail and needs a
recoverable error path.

## Boundaries

Use `ScopeExit` for small local restoration paths such as temporarily overriding
a handler in a test. Do not use it as a substitute for durable resource types
such as file handles, locks, threads, or domain objects that deserve named
ownership semantics.