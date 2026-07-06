# Security

`ponder` is in an early planning and scaffold phase. There are no supported
public releases yet.

## Reporting A Vulnerability

Before the project is public, report suspected vulnerabilities privately to the
maintainers through the project's private development channel.

After public hosting is configured, this file should be updated with the
canonical security contact or private advisory process.

## Current Security Posture

- CI vulnerability scanning is deferred until CI exists.
- Signed releases are a future requirement.
- Build and configure steps should avoid executing plugins, scripts, or
  external tools unless there is a clear reason.
- Generated code checked into the repository must document the generator version
  and regeneration command.
