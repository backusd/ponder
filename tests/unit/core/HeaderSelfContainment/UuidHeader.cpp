#include <ponder/core/Uuid.hpp>

#include <span>
#include <type_traits>

using HeaderUuidEntropySource = void (*)(std::span<ponder::core::Uuid::Byte, ponder::core::Uuid::kByteCount>);

static_assert(std::is_same_v<ponder::core::UuidEntropySource, HeaderUuidEntropySource>);
static_assert(std::is_same_v<decltype(ponder::core::GenerateUuidV4()), ponder::core::Uuid>);
