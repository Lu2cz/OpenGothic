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
  placement plus all 52 activated residents before saving. This uses normal navigation/collision and game script
  entry points; it does not invoke wake-up INFO functions or set story flags.
- `--story sleep-reload --placement placed`: load the resulting save in a separate
  process and recheck placement/progression before resaving.
- `--story routines`: isolated synthetic boundary check using actual installed
  routine functions. Nearby and distant ordinary changes preserve position and
  queued actions; living hidden actors reactivate; a dead-state actor is excluded.
  The player is also excluded. The synthetic setups are never saved and are not
  campaign evidence.
- `--story sleep-again`: ordinary second-sleep replay on private
  post-sleep copies. Existing saves already contain misplaced actors; the fix does
  not retrospectively replay the first-sleep quest callback on load.

Retained local diagnosis: `work/issue29-sleep-baseline-h`,
`work/issue29-sleep-wp-only-a`, `work/issue29-sleep-both-a`, and
`work/issue29-sleep-routine-regression` in the coordination workspace. The WP-only
experiment confirmed refugee placement while residents remained hidden; its save
assertion failed because restored Viktor dialogue was still active. The combined
replay waits for control return and completes save validation.

Separate-process restart passes (`work/issue29-sleep-restart-a`). An ordinary second
sleep also restores the sampled NPCs in a private copy of player save titled 2
(`work/issue29-sleep-existing-2-second-sleep`); all SQ103/main-stage globals compared
unchanged. This advances game time normally and is not an on-load migration.

The driver can use a bounded forward-key movement fallback when native hero AI
navigation stalls on the route back to the inn; this changes only the private test
input. Later replays use the existing dialogue-skip input to shorten spoken lines;
the initial combined replay completed the full audio sequence.

The escort-active save titled 3 also passes second-sleep recovery
(`work/issue29-sleep-existing-3-route-c`). For both player inputs, the inventory
bytes and parsed journal entries/statuses remain identical; all SQ103/main-stage
globals compared unchanged. The known-dialogue set gains the ordinary noon-sleep
choice. Normal sleep advances time and resets NPC routine positions, including
Rupert's guide destination; it is not a repair that freezes the rest of the world.
Original saves remain untouched. A complete rescue-quest playthrough is still
pending campaign verification.

Final signed first-sleep replay passes all 52 activated residents, survivor
placement, actual Viktor choices and save CRC (`work/issue29-sleep-signed-final`).
Signed nearby/distant/hidden/dead/player/queue checks pass
(`work/issue29-sleep-signed-routines`). The signed arrival regression also passes Martha/Viktor, the room key, Jorn,
control return and saving (`work/issue29-sleep-signed-arrival-b`). Its first attempt
stalled before the probe in `InstanceStorage::join`; the sampled intermittent
renderer stall is tracked separately in #31, with no renderer edit in this fix.
Installation and installed smoke results are recorded in #29 and local provenance.

The initial all-resident check used a 600-unit sphere and rejected Elsa at ~665
units from her routine point, already at the correct village elevation. The
activation check now respects the existing `fixNpcPosition` collision-search
bounds (800 horizontal, up to 1000 vertical); the original sampled refugee and
resident checks retain their 600-unit bound. The baseline 38,000+ unit failures
remain well outside either bound.
