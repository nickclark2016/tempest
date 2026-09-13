# Profiler & Telemetry Guidelines

This document provides deep-dive specifications, post-mortem rationales, and architectural constraints for the profiler session, telemetry server, network transport, and web profiler UI.

---

## 1. Frame-Level `scoped_zone` RAII Scope Before Telemetry Capture
* **Destructor Timing**: In frame runner loops or per-frame execution functions (such as `editor_engine_context::_render_editor_frame`), any top-level CPU profiling zones (`profiler::scoped_zone` at depth 0) must be encapsulated in an explicit nested scope (`{ ... }`) that terminates **before** invoking telemetry collection and broadcast (`collect_and_broadcast_telemetry()`).
* **Why**: Because `scoped_zone` records its completed duration in its destructor, failing to close its scope prior to telemetry capture causes the top-level zone to be flushed into the *subsequent* frame's payload ($N+1$), desynchronizing top-level CPU durations and frame hover correlations from child call stack zones ($N$).

```cpp
// Correct: Explicit scope closes and writes zone before telemetry broadcast
{
    auto frame_zone = profiler::scoped_zone{"Main Frame"};
    // ... frame execution ...
} // frame_zone destructor fires here, committing event to current frame

collect_and_broadcast_telemetry();
```

---

## 2. Profiler & Chunk Arena Recycling
* **Chunk Recycling**: Chunks drained from profiler sessions or chunk arenas (`event_chunk`, binary chunk streams) must be recycled via `session.recycle_chunks()` or arena reset methods rather than destroyed (`operator delete`).
* **Why**: Recycling eliminates redundant 64 KB heap allocations and mutex contention on per-frame loops.

---

## 3. Web / JSON Serialization Number Precision (JavaScript `MAX_SAFE_INTEGER`)
* **Safe Integer Limit**: When generating 64-bit integer identifiers (such as track IDs, resource handles, or entity IDs) that are serialized to JSON for consumption by web/browser JavaScript UIs, ensure numeric values fit within JavaScript's safe integer range ($< 2^{53} - 1 \approx 9.007 \times 10^{15}$).
* **32-Bit Prefixes**: Use a 32-bit prefix (e.g. `0x8000'0000ULL`) rather than setting bit 63 (`0x8000'0000'0000'0000ULL`), or serialize the 64-bit integer as a quoted JSON string.
* **Why**: Bit 63 integers exceed $2^{53}$ and will be silently truncated/rounded to identical floating-point values by JavaScript's `JSON.parse()`.

---

## 4. Non-Blocking Socket Framing & WebSocket Transport
* **Loop on Partial Sends**: When broadcasting large payloads (such as telemetry JSON frames) over non-blocking TCP sockets (`ioctlsocket(FIONBIO)` / `O_NONBLOCK`), partial sends (`0 < res < size`) must **never** discard the remaining unsent bytes.
* **Write Readiness Polling**: Senders must loop with non-blocking write readiness polling (`select` / `WSAPoll`) or buffered queues to complete frame transmission, preventing torn frames from corrupting the active WebSocket stream framing.
* **Socket Buffers**: Sockets should also be configured with adequate send/receive buffers (e.g. 1 MB).

---

## 5. Profiler UI Timeline Multi-Lane Track Hierarchy
Timeline tracks in web/desktop profiler UIs must maintain 3 distinct vertical lanes to prevent visual text and badge collisions:
1. **Track Header Strip** (`trackHeaderHeight`): Houses the collapse chevron, thread/queue name (`MAIN THREAD`, `[GPU] Graphics Queue`), and zone count.
2. **Frame Header Lane** (`frameHeaderHeight`): Houses frame boundary badge pills (`CPU #N`, `GPU #N`) and correlation latency markers (`Flight: ...`).
3. **Call Stack Zone Area**: Call stack capture zones must start strictly beneath the frame header lane:
   $$\text{rowY} = \text{currentY} + \text{trackHeaderHeight} + \text{frameHeaderHeight} + 4 + \text{depth} \times (\text{zoneHeight} + \text{zoneSpacing})$$
   with hit-testing synchronized to the exact same vertical offset.
