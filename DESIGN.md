# Design Decisions

Deliberate trade-offs and rationale for choices that might otherwise look like bugs or oversights.

## Published voice state packed into a single atomic uint64

Files: `src/audio/source/lowl_audio_voice.h`

`PublishedState` packs three fields into one `uint64_t`:

| Field          | Bits  | Range                    |
|----------------|-------|--------------------------|
| position       | 0-31  | 0 .. 2^32 - 1 frames    |
| playback_state | 32-33 | Stopped / Playing / Paused |
| detached       | 34    | bool                     |

This means the published transport position is 32 bits while the internal `render_position` is `size_t` (64 bits on most platforms).

**Why:** Packing everything into a single `uint64_t` allows the render thread to publish and the control thread to read the entire snapshot with one lock-free atomic operation — no mutex, no torn reads, no ABA risk across fields.

**Trade-off accepted:** Position wraps after 2^32 - 1 frames. At 48 kHz that is ~24.8 hours of continuous playback, well beyond any realistic clip length for this library's use case.
