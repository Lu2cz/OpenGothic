# Boss UI verification boundaries

Draft PR #22 / issue #7 remain unmerged; the installed playable app is unchanged.

Current-created SQ416 Razor UI has private visual/runtime coverage for health,
focus changes, pause/resume, resize roundtrip, v4 save/restart and final cleanup.
Native view destruction unregisters rendering without taking allocation ownership;
installed DELETE, VIEW_DELETE and VIEWPTR_DELETE paths have reuse checks.

## Historical saves: unresolved acceptance gate

Real v1/v2 binaries were built from 3e1d893c and aabd6bf9 with their pinned
dependencies. A private opt-in patch called installed START_BOSSUI(hero, 0) and
saved normally. These are genuine historical snapshots, not stripped v4 files,
but **partial-start fixtures**, not fully validated old encounters: both original
runs logged a swallowed VM exception during temporary BAR destruction.

Loading these snapshots retains active script handles and safely replaces the old
float-bit screen dimensions. The installed resize callback recreates the title.
The old boss bar remains absent: v1/v2 never recorded native texture/font metadata.
No quest reset, guessed allocation ownership, or reconstructed texture assignment
is used. This missing old active bar alone is a possible supported limitation, not
proof that subsequent encounters work.

The legacy-seed check exposes a separate blocker: after old UI cleanup, the newly
created v2-fixture bar renders but stays full when hero health halves. Its
CURRENTBOSS is the same hero and sees the changed health. The trace identifies
BAR_CREATE -> FREE -> BAR_DELETE: FREE passes an instance to BAR_DELETE's integer
parameter, whose assignment throws. The hypothesis is that the old parameter
value deletes a reused new bar handle. Original Gothic stack semantics and exact
handle reuse still need verification before choosing a shared repair.

## Reproduction

Use tests/run_archolos_boss_ui.py with explicit --executable, --game, --save,
--output (fresh private directory), and --mode:

- legacy-reload: genuine v1/v2 partial-start snapshot, read-only load and visual
  boundary checks; does not restart/finish the boss or save.
- legacy-seed: same input, then installed cleanup/new START, health change and
  v4 save. Currently fails on the v2 frozen-bar assertion; this is a reproducer,
  not a passing acceptance test.
- reload: current synthetic half-health v4 save, then cleanup.
- event-seed / event-reload: installed SQ416 active encounter save/restart.
- event-geometry: native fullscreen/window roundtrip and adjacent focus/pause/XP.
- focus-seed / focus-reload: NPC binding and view destructor/reuse checks.

In legacy-seed, the inherited filename boss-ui-cleanup.png captures the newly
created UI after old cleanup, not an empty final-cleanup screen. Inspect images
alongside phase markers. No private saves/assets/logs are committed.

Remaining gates: resolve the shared call/destructor failure, verify fresh UI and
save/restart after both historical inputs, and test 1024x768/custom interface scale
plus status-screen XP and crafting-view scale consumers.
