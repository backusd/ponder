#include <ponder/core/Identifier.hpp>

namespace
{
struct HeaderIdentifierTag;
}

static_assert(!pond::core::Identifier<HeaderIdentifierTag>{}.IsValid());
static_assert(pond::core::Identifier<HeaderIdentifierTag>{42}.GetValue() == 42U);
