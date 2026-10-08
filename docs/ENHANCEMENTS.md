# Enhancement implementation

Public source includes authored adapters and metadata. Game code, disc files
and resident resource blobs are generated locally and are never committed.

## Timing assistance

SCUS-94448's scoring gate at `0x8001B08C` accepts
`((tick + 12) / 4) % 6` in `[1,4]`: phases `[-8,7]` on a 24-tick grid.
Each beat contains 96 ticks. The signed BPM-times-100 value at the song
descriptor's offset 6 is truncated to integer BPM by the original code.

The input hook at `0x8001B66C` applies compensation after the timestamp's load
delay. Only live input routes 0 and 1 qualify. The scoring caller with return
address `0x80017338` can widen the acceptance window by at most four ticks per
side. Replay routes, unrelated callers and invalid contexts fall through.
Music time, button patterns and the game's remaining scoring rules are retained.

## Display

The mod selects the framework's native wide renderer and adaptive window
aspect service. GTE activity distinguishes 3D scenes from 2D menus; MDEC frames
retain 4:3 presentation. No MMX6-specific tile or culling addresses are reused.
The 1080p preset uses integer 5x rendering from a 240-line reference (1200
internal lines), resolved to the output window. This is not an HD texture pack.

## Resident loading

The adapter targets the INT archive reader at `0x800152D8`. Each block has four
header sectors and a sector-aligned raw payload; block types 1, 2 and 3 contain
TIM textures, VAB sound-bank pairs and model/resource data. The traced load
path does not require Tomba 2's texture decompressor.

Fifty stock archives (73,525,248 bytes) are prepared through the framework's
resident service. The cache is private, SHA-256 verified and keyed by the mod
plan and adapter version. Modified stock resources or preparation failures
retain original loading. Code-range hashes, file extents, loader mode and RAM
bounds guard the adapter. The original allocator and sector-position helper
run as guest calls; the original resource consumers initialize textures, banks
and resource tables. XA and IKI/IK2 music/movie streams are excluded.

The first menu archive took 249 guest frames through the original reader and
12 with resident loading (approximately 4.15 s and 0.20 s). Initial cache
preparation was approximately 0.51 s on the development PC. WP1 and the first
stage archive also completed through the adapter. These are initial results,
not a full-game compatibility claim.

For local diagnosis, `LAMMY_LOADING_TRACE=1` logs resource reads;
`LAMMY_LOADING_RETAIL=1` retains original reads for comparison.
`resident_status`, `mod_counters`, `video_info` and `present_shot` are available
in builds with debug tools. The timing callback tests run without game assets.
