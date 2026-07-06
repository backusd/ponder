# Core UUID Support

`Uuid` is the core-owned stable identifier primitive. It represents a generic
128-bit UUID value and intentionally does not encode domain meaning.

## Public API

- `Uuid` stores exactly 16 bytes.
- A default-constructed `Uuid` is the nil UUID.
- `Uuid::ToString()` returns canonical lowercase text:
  `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`.
- `ParseUuid` accepts canonical text with lowercase or uppercase hexadecimal
  digits and returns `Result<Uuid>` for recoverable parse failures.
- `MakeUuidV4` creates an RFC 4122 variant, version 4 UUID from caller-provided
  random bytes. This is the deterministic generation hook used by tests.
- `GenerateUuidV4(UuidEntropySource)` creates a version 4 UUID using an injected
  entropy source.
- `GenerateUuidV4()` uses the default standard-library entropy source and
  returns a recoverable error if entropy is unavailable.

## Boundaries

Core owns only the generic UUID primitive. Domain-specific identifiers such as
`AssetId`, `NodeId`, `MoleculeId`, or project-file handles should be defined in
the library that owns that domain concept, usually as a wrapper around `Uuid` or
another stable representation.