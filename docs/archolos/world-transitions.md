# Archolos world-transition regression

Issue [#4](https://github.com/Lu2cz/OpenGothic/issues/4), PR [#19](https://github.com/Lu2cz/OpenGothic/pull/19).
Use [environment.md](environment.md) for build, assets and private-run rules.
Installed scripts: Archolos 1.2.11. Reference decompilation is 1.2.7.

## Cause and lifecycle

The native world change omitted LeGo's `_LEGO_CHANGELEVELHOOKBEGIN/END` calls.
Without the level-change flag, destination initialization treated the change as an
ordinary load and entered persistent-memory unarchive during Sewer startup. The
candidate invokes the original hooks around the native transition. A missing world
returns before BEGIN; a load exception discards the failed session through the
existing loader. No quest-specific camera reset or skipped initializer is used.

`HeroStorage` transfers gameplay state into recreated native objects. Old hero and
inventory addresses therefore become zero-reading, write-ignoring tombstones;
fresh references bind the new objects. Multiple tombstones for the same member
remain distinct in the multimap. Callback binding types are allocated uniquely
across retained regions, and the ASMINT call-target pin is reused/restored rather
than duplicated. The compatibility v1/v2 format is unchanged.

The compatibility system-time bridge reads `GameSession::tickCount()`, which
advances in the active session tick. The game is removed from active ticking during
loading. Pending/recurring callbacks consequently resume in the destination using
simulation time; loading duration itself does not demand catch-up dispatches.
Callbacks remain session-global while the departed world's native objects are
inactive. The regression observes this bounded case; it does not claim every
LeGo timer mode or arbitrary long-frame catch-up behavior has been verified.

## Reproduction and boundaries

Supply a private Mainland checkpoint with locked `Q101_CHEST_01` at partial
progress 1 and at least two different inventory item classes. The existing
`run_archolos_lock.py --mode partial` produces that progress from a suitable locked
checkpoint. The verified source is `work/issue3-final-partial/save_slot_2.sav` in
the environment workspace, SHA-256
`5574d61f7aa31390b1db05d4b9f24cfa417ef5739ed9e548185dac9e17ac22ed`.
Always choose a new output directory; the runner copies the source save.

```sh
rtk proxy python3 tests/run_archolos_world_transition.py \
  --executable build/opengothic/Gothic2Notr \
  --game /path/to/archolos-game \
  --save /path/to/private/partial-checkpoint.sav \
  --output /path/to/new/private/output
```

Synthetic setup invokes the loaded entrance's `onIntersect` handler with its real
destination/start point. It does not physically walk the player into the entrance
volume. After arrival, normal forward input must move the player and a screenshot
must show the destination. This isolates the reported loading blocker without
claiming normal campaign progression or every sewer entrance.

| Stage | Verified behavior |
|---|---|
| Mainland → Sewers | `CHANGELEVEL_ARCHOLOS_2_SEWERS` → `FP_M6_2_SEWERS`; scene/input, inventory, quest archive, old/fresh mappings, pending callback once, recurring dispatch and completed save |
| Fresh sewer process → Mainland | Restored recurring callback runs before it is removed and a new leg is seeded; `CHANGELEVEL_SEWERS_2_M6` → `FP_SEWERS_2_M6`; scene/input, inventory, quest archive, returned Q101 partial progress and completed save |
| Fresh Mainland process | Q101 partial progress; old hero/two inventory addresses read zero; fresh hero/two item mappings read/write the actual native instances; recurring callback resumes and is removed before final resave |

The one-shot fires after each transition, before that leg's save. Same-world
pending-one-shot save/restart is checked separately by the city persistence runner.
This is not a claim that the same one-shot crosses every travel/restart combination.
The test callback's timer side effect is undone only in the opt-in probe.
`run.json` records commands, input/binary hashes, game exit and PASS/FAIL/TIMEOUT.
The source hash is checked afterward, and completed saves must be valid ZIP files.

## Candidate provenance — 11 September 2026

Runtime source: `9f0c8accb70f866b89e6327b3ced350382ea07b9`.
Working-branch base: `d5b568a4595ffcd844d09dd9652abb38bd5454c6`.
Tempest: `fb9fa22d2e66fd350ca93cbef4a9cfca05ed5669`.
ZenKit: `6fa71bfbd8f3c349be59bbc485f3e7bac7fa3961`.
Candidate executable SHA-256:
`5601c9b8f351a00a48c7da4652d695926d61a1296be81c5688b51f9a5cbba1ce`.
Later documentation-only commits do not change this binary.

Local `work/issue4-world-transition-restart-bindings-21/run.json` records three
PASS stages with game exit 0 on that binary. `issue4-current-v1-upgrade` and
`issue4-current-v1-restart` verify a genuine v1 snapshot loads, saves v2 and restarts
with runner/game exit 0. The earlier incomplete/terminated runs are diagnostic
history, not acceptance. Final coordinator regressions and installation/rollback
provenance are recorded in issue #4; raw saves/logs/screenshots remain private.
