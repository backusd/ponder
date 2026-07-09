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

C++ formatting is controlled by the root `.clang-format` file. Use the
repository wrapper in check mode to verify project C++ files:

```powershell
.\scripts\format.ps1 -Check
```

Static-analysis expectations are captured in the root `.clang-tidy` file. From
a Visual Studio Developer PowerShell or Command Prompt on Windows, configure the
Ninja analysis preset so clang-tidy has a compilation database, then run the
wrapper's tidy pass:

```powershell
.\scripts\build.ps1 -Preset windows-ninja-analysis -ConfigureOnly
.\scripts\format.ps1 -Check -Tidy -Preset windows-ninja-analysis
```

The wrapper currently checks all project C++ files, so reserve the full format
and tidy pass for integration gates unless a task specifically requires it.

Include-what-you-use is not enforced yet. Keep public headers clean manually and
prefer narrow project-owned wrapper APIs around third-party dependencies.

## License

Unless explicitly stated otherwise, contributions are submitted under the
Apache License, Version 2.0.
