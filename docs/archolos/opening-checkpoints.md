# Private opening regression checkpoints

Issue [#5](https://github.com/Lu2cz/OpenGothic/issues/5), 16 September 2026.
Paths below are relative to the environment guide's workspace `work` directory.
No saves, assets or raw private logs belong in Git.

The private collection is `issue5-checkpoints-20260916`. Its `manifest.json`
indexes source/build/script hashes, narrative flags, creation procedures and
validation commands. Each run has `checkpoint-run.json`, logs and independent
save copies. `retained-inventory.json` records the reviewed existing candidates;
the storage archive was consulted, but no restore was needed.

## Inputs and provenance

| Input | Purpose and provenance |
|---|---|
| `issue5-checkpoints-20260916/fresh/save_slot_2.sav` | Fresh installed 1.2.11 ship game, stash flag 0 and captain flag 0. The stash runner audits original loot, teleports to the amulet waypoint and saves. Reuse for both gate states; this precedes the running opening quest and cannot seed stash reveal. |
| `issue5-checkpoints-20260916/stash-ready/save_slot_1.sav` | Instrumented copy of the retained pre-captain fixture: only the scalar `Q101_VRAZKACHEST` changes from 0 to 1. Input for reveal/open and return checks. This skips accepting Vrazka's quest. |
| `issue6-captain-fixture-3/save_slot_2.sav` | Retained pre-captain fixture, SHA-256 `2eff2ed8d108382ef5fd2dc118d374306e14339c1421f7deb4bad5de69c73434`. Fresh-game preparation dispatches initial Jorn info, clears setup queues and positions Jorn/player at `SHIP_JORN_02`. |
| `issue5-checkpoints-20260916/captain/save_slot_2.sav` | Shared beach/forest input: normal-duration captain replay completed on the installed build, departure flag 11, no active trialogue/fade and forest choice unmade. The prerequisite setup remains instrumented. |
| `issue3-candidate-ordinary/save_slot_1.sav` | Retained locked-chest input, SHA-256 `10b11f7040aad1dfeba34c305f81aa9e2a0b81bb05d1d63f13674624e036ba55`. Synthetic Chapter 2 state from the historical player-slot copy. It is a lock interaction fixture, not a pre-quest story save. |

None of these inputs establishes genuinely played opening progression. Captain
and forest probes select real dialogue choices after instrumented positioning;
stash return invokes the installed return-info function directly. Gate probes
position the player at the stairs and hold forward; open mode also triggers the
trapdoor. Ordinary lock tests use deterministic dexterity and supplied picks;
spell setup supplies a scroll and mana before real casting input. Those boundaries
remain even when every assertion passes. Clean campaign validation stays in #14;
forest actor/camera framing remains #21.

The retained locked input's original executable was not independently recorded.
Its source SHA-256/native v55 format are known; current consumption and new
ordinary/spell result saves have exact executable/script hashes. The manifest
keeps that lineage limit explicit rather than assigning a guessed creator build.

## Reproduction

Run from a current checkout, with `TASK_WORK` set to the environment guide's
`work` directory. Check that no player game is running. Use a new output directory
for every invocation; never use `playable` as output. The installed Fast executable
is adequate; no app replacement or build is needed for these fixtures.

Example: replay the retained pre-captain input at normal dialogue duration:

```sh
rtk proxy python3 tests/run_archolos_captain.py --help
rtk proxy python3 tests/run_archolos_captain.py --executable "$TASK_WORK/ArcholosFast.app/Contents/MacOS/Gothic2Notr" --game "$TASK_WORK/archolos-game" --save "$TASK_WORK/issue6-captain-fixture-3/save_slot_2.sav" --output "$TASK_WORK/opening-captain-repeat"
```

Use the same executable/game/output arguments for the following runners; read
each runner's `--help` first. `collection/` below means the existing
`issue5-checkpoints-20260916/` directory, not an additional filesystem path.

| Runner | Input (`--save`) | Additional arguments |
|---|---|---|
| `run_archolos_stash.py` | Omit; only if a fresh seed is needed | `--mode fresh` |
| `run_archolos_gate.py` | `collection/fresh/save_slot_2.sav` | `--state closed`, then separate `--state open` run |
| `run_archolos_stash.py` | `issue6-captain-fixture-3/save_slot_2.sav` | `--mode stash --prepare` |
| `run_archolos_stash.py` | Previous stash output `save_slot_2.sav` | `--mode stash` (reveal/open after restart) |
| `run_archolos_stash.py` | `collection/stash-ready/save_slot_1.sav` | `--mode return` |
| `run_archolos_lock.py` | Retained locked input above | `--mode ordinary`; separate `--mode cast --setup-chest` run |
| `run_archolos_lock.py` | Corresponding ordinary/cast output `save_slot_2.sav` | `--mode ordinary-reload` / `--mode reload` |
| `run_archolos_beach.py` | `collection/captain/save_slot_2.sav`; repeat with its output `save_slot_2.sav` | `--mode observe --expect-loot` |
| `run_archolos_forest.py` | `collection/captain/save_slot_2.sav` | `--full-dialogue` |
| `run_archolos_captain.py` | Omit; only if retained fixture is unavailable | `--prepare` |

Stash `--prepare` requires the opening quest running (`MIS_Q101=1`), departure
not started, and exactly one unstarted scalar stash flag. The installed 1.2.11
event returns before reveal if the opening quest is absent; a fresh loot seed
with only the stash flag set reproduced that boundary. Reuse the Jorn-initialized
pre-captain input instead. It verifies the ZIP and every payload after preparing the independent
copy. The game must then reveal the boards (flag 2), focus/open their real container
and finalize a valid save. The return check additionally requires box transfer,
consumption and creation of the follow-up item. This fixture does not validate the
skipped Vrazka dialogue/log entry or later quest conditions. Jorn's opening
quest initialization is supplied by the existing captain preparation procedure.

For historical v2 corruption and partial/hybrid lock fixtures, use
[lockpick-regression.md](lockpick-regression.md). Do not regenerate every legacy
fixture or use end-of-v2 offsets on current v4 snapshots.

## Verification — 16 September 2026

Fourteen runner cases passed on the installed executable below, each with normal
game exit, applicable finalized-save/CRC checks and unchanged source hashes:

| Cases | Observed result |
|---|---|
| Fresh loot; gate closed/open | Original expected loot; closed stairs block ascent, open stairs permit ascent |
| Ordinary lock + restart; Open Lock + restart | Hook branches, chest access, spell/resource checks and preserved unlock; current resave is v4 |
| Stash preparation + restart + return | Flag 1 → 2, raised boards, correct focus/container access; box taken/consumed and follow-up item created |
| Captain; beach + restart | Normal-duration required choices/speakers, departure flag 11 and control return; stable seated NPC, expected corpse loot, dropped torch falls/settles |
| Forest | Normal-duration required dialogue/speakers, Jorn choice, player identity/control and speaker reset, finalized save |

The retained captain result is the common beach/forest input; the old forest
fixture did not need regeneration or adoption. Five primary input references
plus the required result saves are indexed in the private manifest. This is a
regression foundation, not a whole-campaign replay.

The representative failed run is `issue5-checkpoints-20260916/stash`: its fresh
input lacks the running opening quest, so the boards remain hidden. Installed
bytecode and serialized state identify the missing prerequisite. The preparation
guard now rejects that source before launching the game; the initialized-source
reveal/restart/return checks pass. No engine repair was needed.

`archolos_test_data.py`'s independent-write/overwrite-protection self-check and
Python compilation pass. Private records include exact command vectors, runner
and executable hashes, source/output hashes, narrative state and retention notes.
New save copies have separate inodes. The final audit compares all 84 protected
player, app, asset and paired-rollback files with the preflight hashes.

## Runtime and retention

Installed Fast SHA-256:
`4a7193bcec3e96ed85de4d0b5a5e21c93a97713a1743d5467ae9a9a5d5b31a76`.
Runtime source `031c75281676c06b7881cf2937082ea5ebd84a87`, merged as
`33fd4acde6b582e71162e323c686c9f840348125`; Release/Metal, native saves v56,
compatibility v4. Installed `KM_ScriptsEN.mod` (1.2.11) SHA-256:
`cb7025bed2f298c537f9dd3c7aff2b70f3e0169f2c01591d05738261523797b1`.
Tempest remains `fb9fa22d2e66fd350ca93cbef4a9cfca05ed5669`; ZenKit remains
`6fa71bfbd8f3c349be59bbc485f3e7bac7fa3961`.

Keep the indexed inputs and final passing evidence under the
[retention policy](storage-retention.md). Saves are independent APFS copies;
source hashes must remain unchanged. Historical evidence paths are records of
past runs, not commands to paste or permission to overwrite player saves.
