# Um Jammer Lammy v0.1.2 - 2026-11-25

- Use the shared native BIOS executive for SDK startup, interrupt callbacks,
  controller packets, and memory-card file completion.
- Give native-provider overlay modules their own cache identity, preserving
  live-code validation before cached code is used.
- Provide Windows and Linux packages through the shared release builder.
  Original disc selection, native memory cards, adaptive widescreen and the
  optional Rhythm Timing Assist retain their existing behavior and defaults.

## Previous release 0.1.1 - 2026-10-30

- Pin the merged shared engine and current launcher UI while preserving the
  original rhythm assistance, adaptive widescreen, title resident resource loader,
  digital controller mode and native memory cards. Netplay remains OFF.
- Shared Internal Resolution defaults ON at 4x, with saved Native (1x) through
  32x choices in Mods; disabling it uses the native fallback.
- Correct the audio configuration to the parsed [audio] buffer_ms = 60 setting,
  matching the documented and previously owner-selected target. The old
  [runtime] audio_buffer_ms value was ignored.
- Use the maintained common public release builder and fresh title-specific
  selective PGO. Boot profiling is build work, not additional gameplay coverage
  or a new per-title speedup claim. Developer setup remains available; explicit
  release setup OFF is respected.
- Keep generic CD Speed and Fast Loading packages out of the public catalog.
  The title-owned resident loader is retained.

## Previous release 0.1.0

The 2026-10-08 preview had owner-approved initial Windows controller gameplay,
Adaptive Widescreen and title resource loading. Rhythm Timing Assist was optional
and default OFF. Complete stage clears, duets, PaRappa mode, every widescreen
scene and measured audio coverage were not established. Those limits remain.
The new package receives its own build/package review and short owner quick look.
