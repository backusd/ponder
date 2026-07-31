#include <ponder/application/Identifiers.hpp>

static_assert(!ponder::application::BackgroundProcessId{}.IsValid());
