# Contributing

`ponder` is not public-ready yet, but contributions should follow the rules we
want the project to keep long term.

## Developer Certificate Of Origin

This project uses a Developer Certificate of Origin. By contributing, you
certify that you have the right to submit the contribution under this project's
license.

Each commit should include a sign-off line:

```text
Signed-off-by: Your Name <you@example.com>
```

Git can add this automatically:

```text
git commit -s
```

## Contribution Expectations

- Keep changes focused.
- Follow the root `AGENTS.md` guidance.
- For subsystem work, also follow the local `<subsystem path>/docs/Codex.md`
  file once it exists.
- Prefer changes that preserve clear library boundaries and long-term
  maintainability.
- Add or update tests when changing behavior.
- Record architectural decisions in ADRs when a choice should survive beyond
  the immediate patch.

## Formatting And Static Analysis

C++ formatting is controlled by the root `.clang-format` file. Until the
project adds wrapper scripts, format edited C++ files directly:

```text
clang-format -i path/to/File.cpp path/to/File.hpp
```

Static-analysis expectations are captured in the root `.clang-tidy` file. Run
clang-tidy against a configured build directory when checking source files:

```text
clang-tidy -p build/windows-msvc-debug path/to/File.cpp
```

Include-what-you-use is not enforced yet. Keep public headers clean manually and
prefer narrow project-owned wrapper APIs around third-party dependencies.

## License

Unless explicitly stated otherwise, contributions are submitted under the
Apache License, Version 2.0.
