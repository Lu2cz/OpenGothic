# Lockpick compatibility and regression checks

Issue: [#3](https://github.com/Lu2cz/OpenGothic/issues/3).
PR: [#17](https://github.com/Lu2cz/OpenGothic/pull/17).
Scope extension: [#18](https://github.com/Lu2cz/OpenGothic/issues/18).

## Confirmed caller and behavior

Installed Archolos 1.2.11 `G_PICKLOCK` reads `OCNPC.FOCUS_VOB` on `SELF`, then
reads the focused `OCMOBLOCKABLE.BITFIELD` when its target changes. Its result
branches request sounds and printed messages; broken picks can send a quiet-sound
perception; finishing a previously partial lock invokes the hybrid achievement.
The original function still executes. Platform achievement delivery and missing
sound assets are not claimed by this test (sound asset investigation is #11).

A control build with only the raw focus registration removed reproduced the real
InventoryMenu/PlayerControl interaction failure. The opt-in probe's exception
handler printed `in G_PICKLOCK at 331a2e`, then
`illegal access of unbound member OCNPC.FOCUS_VOB`. The game exited 0 because its
VM is lenient; the regression runner correctly exited 1. The binding was restored
before the successful checks. Merely observing a cracked chest is insufficient:
native lock state can advance even when this hook failed.

Bit 0 is locked, bit 1 is auto-open, and bits 2..31 hold turn progress. Native
locks have no auto-open state. The callback exposes freshly derived bits and the
progress before the current turn. Native partial progress is shared with Open
Lock so the two methods can combine. Non-DMA mods keep the existing PlayerControl
progress and script invocation path.

## Ownership and save format

Each Interactive owns a virtual address value, not a cached native pointer.
The compatibility heap owns the allocated bytes. The NPC focus field and the
exposed bytes are cleared after each callback, including exception unwinding.
Unknown/non-lock addresses are not dereferenced during cleanup. A deleted native
lock cannot leave a dangling native pointer: its zeroed block remains script data,
and a new object, even at the same native address, starts with address zero and
receives a different allocation. Blocks are retained until session destruction;
reclaiming them would require tracking all script references.

The safety probe checks locked/unlocked transitions, null and unknown pointers,
target changes, inactive bytes, and a simulated native-address reuse by resetting
the same object's construction-time metadata. It does not claim a gameplay
demonstration of deleting a world chest. There is no native pointer index to go
stale. Ordinary restart checks also compare the restored address with the installed
hook's `LASTMOB` and verify its inactive bytes are zero.

`game/compatibility` now writes version 2. The v1 prefix is unchanged; v2 appends
a count and `(mobsiId, virtualAddress, progress)` records. Load validates object
IDs, duplicate IDs/addresses, progress bounds, and exact heap allocation
address/size/type/name before binding an object. Current-world IDs use the existing
mobsi serialization identity. No native pointer is serialized.

Baseline v1 snapshots remain readable. They contain no lock ownership records,
so their old virtual blocks are retained as script data without guessing native
owners; `G_PICKLOCK.LASTMOB` is reset. Snapshots without a compatibility entry
also reset this transient identity. V2 retains and restores the recorded identity.
Older binaries reject v2 explicitly. Raw focus registration now occurs after the
original script fingerprint is computed, preserving the baseline v1 fingerprint
`14284172930240330287` for the installed scripts. Early draft snapshots made with
the changed fingerprint are intentionally rejected, not silently reinterpreted.

The appended `INpc::focus_vob` field is transient and remains omitted from native
NPC serialization; existing native fields retain their offsets. Partial progress
also has an optional per-mob `lockpick-progress` world entry, so WorldStateStorage
can retain it independently of compatibility addresses. Missing entries mean zero
and invalid progress is rejected. Actual distinct-world travel/return acceptance
remains #4; this test can isolate the native world storage path without claiming
that travel was exercised.

## Private fixture and commands

Use [environment.md](environment.md) for paths and build prerequisites. These
commands run in the issue worktree. Set `TASK_WORK` to its documented `work`
directory, and use new output names on every invocation.

```sh
rtk cmake --build build --target Gothic2Notr --parallel 4
rtk proxy python3 tests/run_archolos_lock.py --help
rtk proxy python3 tests/run_archolos_lock.py --executable build/opengothic/Gothic2Notr --game "$TASK_WORK/archolos-game" --save "$TASK_WORK/playable/save_slot_3.sav" --output "$TASK_WORK/lock-ordinary" --mode ordinary
rtk proxy python3 tests/run_archolos_lock.py --executable build/opengothic/Gothic2Notr --game "$TASK_WORK/archolos-game" --save "$TASK_WORK/lock-ordinary/save_slot_2.sav" --output "$TASK_WORK/lock-reload" --mode ordinary-reload
rtk proxy python3 tests/run_archolos_lock.py --executable build/opengothic/Gothic2Notr --game "$TASK_WORK/archolos-game" --save "$TASK_WORK/playable/save_slot_3.sav" --output "$TASK_WORK/lock-cast" --mode cast --setup-chest
```

Every mode copies the source save, writes private settings, checks the source hash,
and records exact commands, executable/source SHA-256, game exit code, and result
in `run.json`. Normal checks require the 900-frame normal exit, a valid finalized
ZIP save where applicable, and no script exception or unmapped-memory error during
the interaction. Inputs and saves are one-shot even if rendering skips a frame.
Only rejection tests terminate the private process after a confirmed load error
returns it to the menu; they never interrupt a save.

The synthetic setup finds the real `Q101_CHEST_01` (`LLL`), detaches any restored
interaction, and places the private player 150 units in front of it. Ordinary
tests use normal inventory and key handling, deterministic dexterity values for
failure with/without breaking, then the correct turns. Spell setup supplies the
installed `ITSC_PICKLOCK`, equips it, and supplies 100 mana; spell 103 is drawn and
cast through player input with natural focus. The source save is never replaced.
This is fixture evidence for #5, not an opening-story/campaign checkpoint.

Additional runs use the same command prefix and a fresh output directory:

| Mode / flags | Source | Required result |
|---|---|---|
| `ordinary-reload` twice in sequence | Previous ordinary/reload output | Stable binding, chest access, finalized new save |
| `reload` | Full cast output | Chest access and v2 resave |
| `reload --setup-chest` | Genuine v1 unlocked-chest checkpoint | v1 load, native access, upgrade to v2; repeat on its output |
| `partial` | Locked chest source | One real ordinary turn, still locked, saved partial progress |
| `cast --setup-chest --expect-hybrid` | `partial` output | Spell completes the existing partial lock and invokes achievement hook |
| `spell-partial --setup-chest` | Locked chest source | Interrupt real cast after one turn; no scroll consumed |
| `ordinary --expect-hybrid` | `spell-partial` output | Original hook sees partial state and reaches final hybrid branch |
| `cast --setup-chest --world-state-only --expect-hybrid` | `partial` output | Native world entry alone retains progress; heap omitted only in private copy |
| `reload --reject address` | Ordinary v2 output | Invalid lock address rejected before gameplay |
| `reload --reject truncated` | Ordinary v2 output | Truncated metadata rejected before gameplay |
| `reload --reject fingerprint` | Ordinary v2 output | Different script fingerprint rejected before gameplay |

General NPC/item focus and the additional static focus readers are explicitly
tracked in #18. The lock-only API accepts an Interactive, and the NPC field is zero
outside the supported callback. No general focus implementation is claimed.

## Verification manifest — 11 September 2026

Runtime source: `6b4890fe86656ddb96741cf70cd35a83b3be91ce`.
ZenKit: `6fa71bfbd8f3c349be59bbc485f3e7bac7fa3961`.
Tempest remains `fb9fa22d2e66fd350ca93cbef4a9cfca05ed5669`.
Release/Metal build, Vulkan off; build and `git diff --check` exited 0.
Candidate executable SHA-256:
`fe9094f364b521649319af04dddc7574c79ee57842101f428180a7be8a23d1fc`.
Later documentation-only commits do not change this binary.

Local output directories below are relative to the environment guide's `work`.
Each contains private `terminal.log`, copied/generated saves and `run.json`.
The latter records exact runner/game commands, input paths/hashes and exit/result
data; it is intentionally kept local with the private fixture. Mode/flag meanings
and the common invocation are documented above.

| Output | Mode / additional flags | Source within work |
|---|---|---|
| `issue3-candidate-ordinary` | `ordinary` | `playable/save_slot_3.sav` |
| `issue3-candidate-reload` | `ordinary-reload` | `issue3-candidate-ordinary/save_slot_2.sav` |
| `issue3-final-v1-upgrade-2` | `reload --setup-chest` | `issue3-before-focus/save_slot_2.sav` (genuine v1) |
| `issue3-final-v1-restart-2` | `reload --setup-chest` | `issue3-final-v1-upgrade-2/save_slot_2.sav` |
| `issue3-final-partial` | `partial` | `playable/save_slot_3.sav` |
| `issue3-final-world-progress` | `cast --setup-chest --world-state-only --expect-hybrid` | `issue3-final-partial/save_slot_2.sav` |
| `issue3-final-cast` | `cast --setup-chest` | `playable/save_slot_3.sav` |
| `issue3-final-cast-reload` | `reload` | `issue3-final-cast/save_slot_2.sav` |
| `issue3-candidate-spell-partial` | `spell-partial --setup-chest` | `playable/save_slot_3.sav` |
| `issue3-candidate-hybrid-ordinary` | `ordinary --expect-hybrid` | `issue3-candidate-spell-partial/save_slot_2.sav` |
| `issue3-final-reject-address` | `reload --reject address` | `issue3-final-ordinary/save_slot_2.sav` |
| `issue3-final-reject-truncated` | `reload --reject truncated` | `issue3-final-ordinary/save_slot_2.sav` |
| `issue3-final-reject-fingerprint` | `reload --reject fingerprint` | `issue3-final-ordinary/save_slot_2.sav` |

Normal scenarios require both game and runner exit 0. The rejection scenarios
require runner exit 0 after observing the expected rejection; their menu process
is then terminated (game exit -15). Source-save hashes must remain unchanged.
The fixture source SHA-256 is
`10b11f7040aad1dfeba34c305f81aa9e2a0b81bb05d1d63f13674624e036ba55`.

Additional repeated-restart evidence is in `issue3-final-ordinary`,
`issue3-final-reload-1`, and `issue3-final-reload-2`: all exited 0 and the v2
snapshots retained exactly three named focus blocks through both reload/save
cycles. The control `issue3-v2-before-stack` correctly produced runner exit 1
despite game exit 0. Replaying the old `f42ffcdf` ordinary-reload validator against
a successful restart log also reproduced its `IndexError` (exit 1); the corrected
runner validates mode-specific output and normal shutdown.
