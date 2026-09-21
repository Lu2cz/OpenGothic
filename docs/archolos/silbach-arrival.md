# Silbach arrival recovery

Issue [#27](https://github.com/Lu2cz/OpenGothic/issues/27),
PR [#28](https://github.com/Lu2cz/OpenGothic/pull/28), 21 September 2026.

## Cause and fix

Martha's live pose retained `S_FOODHUGE_S0` with item-use state and destination
both `-1`. `Pose::stopItemStateAnim` returned immediately for that completed
state without removing the non-transition item layer. Each movement request
encountered the same layer and returned, leaving Martha 694 units from the
waypoint required by the arrival conversation (maximum 300).

The shared animation exit path now removes completed non-transition item layers
through the existing layer cleanup callbacks. Genuine exit transitions remain
active so their final frames/events can run. This covers live movement requests
and restored saves; no NPC-specific code, position reset, quest repair or save
format change is involved. The earlier load-only interruption was removed.
An earlier unmapped broom observation was not Martha's live pose.

## Verification and boundaries

The protected source is the player's Fabio/Rupert-route save, SHA-256
`a7485a9c1bac3eff6993f17118c2bb7dcb3d11a74511a1a1ffb02752c60274f9`.
Installed scripts are Archolos 1.2.11; reference source 1.2.7 is only a guide.

`run_archolos_silbach.py` retains its read-only conversation-availability check
and adds `--story seed` / `--story reload`. Each run uses a new independent APFS
save copy, verifies the original hash and records its executable hash. Story
automation queues native waypoint walking on the hero, using normal navigation,
animation and collision. Important conversations trigger during approach. A
nearby interaction fallback uses the ordinary player-controller interaction.
Only actual presented dialogue choices are selected through `DialogMenu`.
No actors are teleported by test setup, no quest values are forced and no INFO
function is invoked directly. Script-driven routine changes/teleports resulting
from completing dialogue remain normal game behaviour.

The seed run verifies the Martha/Viktor lines and speaker identities, the
split-up answer and room-key award, movement upstairs, Jorn's two answers,
`SILBACHSLEEP=1`, normal camera/dialogue release, backward movement via player
input, and a finalized save with a valid ZIP CRC. A separate process reloads it,
checks known conversations and the key, moves again and saves again. This is an
instrumented gameplay integration test, not an OS-input playthrough. Native
desktop attachment timed out; that unsuccessful attempt is not acceptance proof.

Additional coverage: the real inventory recipe close/reopen test passes; the
existing forest replay covers shared dialogue/camera behaviour using its documented
instrumented setup. No full-campaign or later-chapter correctness is implied.

## Reproduction

Use the explicit executable/game/output arguments described in
[environment.md](environment.md), and a new output directory on every invocation.
Never use `work/playable` as test output.

```sh
rtk proxy python3 tests/run_archolos_silbach.py --help
rtk proxy python3 tests/run_archolos_silbach.py --executable "$TASK_EXE" --game "$TASK_GAME" --save "$TASK_SOURCE" --output "$TASK_OUT_SEED" --story seed
rtk proxy python3 tests/run_archolos_silbach.py --executable "$TASK_EXE" --game "$TASK_GAME" --save "$TASK_OUT_SEED/save_slot_2.sav" --output "$TASK_OUT_RELOAD" --story reload
```

`--expect unavailable` reproduces the conversation gate failure on the baseline
probe build; the repaired build passes `--expect available`. Story runs fail by
stage, with a bounded timeout and private screenshots/logs. Normal game launches
leave all `OPENGOTHIC_*` test variables unset.

Retained private development evidence under the workspace `work` directory:
`issue27-pose-baseline3-20260918`, `issue27-pose-shared-final2-20260918`,
`issue27-recipe-exit-fresh-20260921`, `issue27-story-native-20260921-b` and
`issue27-story-restart-20260921`. Final signed-app and regression evidence,
source/build hashes and paired rollback are recorded in #27 and local deployment
provenance. Preserve the input and final evidence under the storage policy.
