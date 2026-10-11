#ifndef tempest_editor_core_tests_mock_renderer_hpp
#define tempest_editor_core_tests_mock_renderer_hpp

#include <tempest/editor_log_sink.hpp>
#include <tempest/source_location.hpp>

namespace tempest::editor::tests
{
    inline auto emit_renderer_source_log(editor_log_sink& sink) -> void
    {
        sink.log(log_record{
            .level = log_level::info,
            .timestamp_ns = 1000,
            .thread_id = 1,
            .message = "Generic background work without keyword",
            .source = source_location::current(),
        });
    }
} // namespace tempest::editor::tests

#endif // tempest_editor_core_tests_mock_renderer_hpp
