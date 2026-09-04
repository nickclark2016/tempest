#include <tempest/profiler/capture.hpp>
#include <tempest/profiler/session.hpp>

#include <format>
#include <tempest/iterator.hpp>

namespace tempest::profiler
{
    auto create_capture_from_chunks(span<const unique_ptr<event_chunk>> chunks, const profiler_session* session)
        -> capture_session_data
    {
        auto result = capture_session_data{};
        auto has_events = false;
        auto min_ts = ~uint64_t{0};
        auto max_ts = uint64_t{0};

        for (const auto& chunk_ptr : chunks)
        {
            if (!chunk_ptr || chunk_ptr->empty())
            {
                continue;
            }

            const auto tid = chunk_ptr->get_thread_id();
            auto* target_track = static_cast<track_data*>(nullptr);

            for (auto& track : result.tracks)
            {
                if (track.track_id == tid)
                {
                    target_track = &track;
                    break;
                }
            }

            if (!target_track)
            {
                constexpr auto gpu_bit = uint64_t{0x8000'0000ULL};
                const auto is_gpu = (tid & gpu_bit) != 0;
                auto name_buf = std::string{};
                auto type = track_type::cpu_thread;

                if (session != nullptr)
                {
                    auto registered_name = session->get_track_name(tid);
                    if (!registered_name.empty())
                    {
                        name_buf = std::string{registered_name.data(), registered_name.size()};
                    }
                }

                if (is_gpu)
                {
                    type = track_type::gpu_queue;
                    if (name_buf.empty())
                    {
                        const auto queue_idx = (tid & 0x7FFF'FFFFULL) - 1;
                        switch (queue_idx)
                        {
                        case 0:
                            name_buf = "GPU: Graphics";
                            break;
                        case 1:
                            name_buf = "GPU: Async Compute";
                            break;
                        case 2:
                            name_buf = "GPU: Async Transfer";
                            break;
                        default:
                            std::format_to(tempest::back_inserter(name_buf), "GPU: Queue {}", queue_idx);
                            break;
                        }
                    }
                }
                else
                {
                    type = track_type::cpu_thread;
                    if (name_buf.empty())
                    {
                        std::format_to(tempest::back_inserter(name_buf), "Thread {}", tid);
                    }
                }

                auto new_track = track_data{
                    .track_id = tid,
                    .name = string{name_buf.data(), name_buf.size()},
                    .type = type,
                    .zones = {},
                    .markers = {},
                };
                result.tracks.push_back(tempest::move(new_track));
                target_track = &result.tracks.back();
            }

            const auto zones = chunk_ptr->zones();
            for (const auto& z : zones)
            {
                target_track->zones.push_back(z);
                has_events = true;
                if (z.start_ns < min_ts)
                {
                    min_ts = z.start_ns;
                }
                if (z.end_ns > max_ts)
                {
                    max_ts = z.end_ns;
                }
            }

            const auto markers = chunk_ptr->markers();
            for (const auto& m : markers)
            {
                target_track->markers.push_back(m);
                has_events = true;
                if (m.timestamp_ns < min_ts)
                {
                    min_ts = m.timestamp_ns;
                }
                if (m.timestamp_ns > max_ts)
                {
                    max_ts = m.timestamp_ns;
                }
            }

            const auto metrics = chunk_ptr->metrics();
            for (const auto& met : metrics)
            {
                has_events = true;
                if (met.timestamp_ns < min_ts)
                {
                    min_ts = met.timestamp_ns;
                }
                if (met.timestamp_ns > max_ts)
                {
                    max_ts = met.timestamp_ns;
                }

                auto* target_stream = static_cast<metric_stream*>(nullptr);
                for (auto& st : result.metrics)
                {
                    if (string_view{st.name.data(), st.name.size()} == met.name)
                    {
                        target_stream = &st;
                        break;
                    }
                }

                if (!target_stream)
                {
                    auto new_stream = metric_stream{
                        .name = string{met.name.data(), met.name.size()},
                        .unit = met.unit,
                        .samples = {},
                    };
                    result.metrics.push_back(tempest::move(new_stream));
                    target_stream = &result.metrics.back();
                }

                target_stream->samples.push_back(met);
            }
        }

        if (has_events)
        {
            result.start_time_ns = min_ts;
            result.end_time_ns = max_ts;
        }
        else
        {
            result.start_time_ns = 0;
            result.end_time_ns = 0;
        }

        return result;
    }

    auto create_capture_from_session(profiler_session& session) -> capture_session_data
    {
        auto chunks = session.drain_completed_chunks();
        const auto chunk_span = span<const unique_ptr<event_chunk>>{chunks.data(), chunks.size()};
        auto result = create_capture_from_chunks(chunk_span, &session);
        session.recycle_chunks(tempest::move(chunks));
        return result;
    }
} // namespace tempest::profiler
