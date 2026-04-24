/// @file event.cpp
/// @brief Compilation unit for the event library.
///
/// The event system is header-only by design (all templates). This translation unit exists
/// solely to satisfy the static library build target. If non-template code is added in the
/// future (e.g., debug logging, profiling hooks), it belongs here.

#include <tempest/event_registry.hpp>
