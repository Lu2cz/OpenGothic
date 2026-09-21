# Silbach first-sleep placement

Issue [#29](https://github.com/Lu2cz/OpenGothic/issues/29).

## Cause and scope

Installed Archolos 1.2.11 calls `Npc_ExchangeRoutine` and immediately reads the
NPC's `WP` for a teleport during the first sleep. OpenGothic previously rebuilt
the routine table but only updated `WP` when the queued AI state started later.
The teleport therefore returned refugees to their cave destinations; they then
walked towards the inn. The same stale destination delayed Viktor's wake-up visit.

A separate activation path switches village residents from routines at `TOT`,
the hidden removal point, to their normal schedules. They previously tried to
walk back from that disconnected area. Routine exchange now publishes the current
waypoint synchronously and places living non-player actors leaving `TOT` at their
new routine point. It preserves queued actions. Ordinary routine changes retain
position; dead NPCs and the player are excluded from reactivation. No quest flags,
NPC-specific coordinates, asset changes or save-format changes are involved.

## Reproduction and checks

Use `tests/run_archolos_silbach.py` with explicit `--executable`, `--game`, `--save`
and a new `--output` directory. See [environment.md](environment.md) for paths.
Each run clones the source save independently and checks its hash afterwards.

- `--story sleep --placement broken`: baseline replay from the retained #27
  pre-sleep checkpoint. Native waypoint walking, door interaction and BEDHIGH
  interaction reach the installed first-sleep callback. Baseline refugees remain
  31,600–32,600 units away and sampled residents 38,000–42,000 units away.
- `--story sleep --placement placed`: candidate replay additionally completes
  Viktor's actual presented morning dialogue choices and checks living survivor
  placement before saving. This uses normal navigation/collision and game script
  entry points; it does not invoke wake-up INFO functions or set story flags.
- `--story sleep-reload --placement placed`: load the resulting save in a separate
  process and recheck placement/progression before resaving.
- `--story routines`: isolated synthetic boundary check using actual installed
  routine functions. Nearby and distant ordinary changes preserve position and
  queued actions; living hidden actors reactivate; a dead-state actor is excluded.
  The synthetic dead-state setup is never saved and is not campaign evidence.
- `--story sleep-again`: experimental ordinary second-sleep replay on private
  post-sleep copies. Existing saves already contain misplaced actors; the fix does
  not retrospectively replay the first-sleep quest callback on load.

Retained local diagnosis: `work/issue29-sleep-baseline-h`,
`work/issue29-sleep-wp-only-a`, `work/issue29-sleep-both-a`, and
`work/issue29-sleep-routine-regression` in the coordination workspace. The WP-only
experiment confirmed refugee placement while residents remained hidden; its save
assertion failed because restored Viktor dialogue was still active. The combined
replay waits for control return and completes save validation.

Pending acceptance: existing-save recovery, restart, arrival regression and signed
installation. The installed playable app remains unchanged until validation.
