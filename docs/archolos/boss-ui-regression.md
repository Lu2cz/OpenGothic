# Boss UI verification boundaries

PR #22 merged as c1b35645cd05463a24de2db185399eb841e09279. The reviewed
candidate is installed in Fast only; deployment details follow below.

Current-created SQ416 Razor UI has private visual/runtime coverage for health,
focus changes, pause/resume, resize roundtrip, v4 save/restart and final cleanup.
Native view destruction unregisters rendering without taking allocation ownership;
installed DELETE, VIEW_DELETE and VIEWPTR_DELETE paths have reuse checks.

## Historical saves: supported boundary

Real v1/v2 binaries were built from 3e1d893c and aabd6bf9 with their pinned
dependencies. A private opt-in patch called installed START_BOSSUI(hero, 0) and
saved normally. These are genuine historical snapshots, not stripped v4 files,
but **partial-start fixtures**, not fully validated old encounters: both original
runs logged a swallowed VM exception during temporary BAR destruction.

Loading these snapshots retains active script handles and safely replaces the old
float-bit screen dimensions. The installed resize callback recreates the title.
The old boss bar remains absent: v1/v2 never recorded native texture/font metadata.
No quest reset, guessed allocation ownership, or reconstructed texture assignment
is used. An already-active old bar remains missing until that encounter ends.
Newly created UI after cleanup passes natural health updates and v4 save/restart
with final cleanup for both historical inputs. This is not full old-UI recovery.

The initial legacy-seed check exposed a failure: after old UI cleanup, the newly
created v2-fixture bar renders but stays full when hero health halves. Its
CURRENTBOSS is the same hero and sees the changed health. The trace identifies
BAR_CREATE -> FREE -> BAR_DELETE: FREE passes an instance to BAR_DELETE's integer
parameter, whose assignment throws. Original Gothic instead writes zero for an
untagged instance argument; the shared dynamic-call bridge now matches that
behavior for the single-integer-parameter seam, without changing typed destructors.
Native evidence: Gothic2.exe SHA256
0655b4811e008a353ce372fde463ef00b6101f3e89be850e2f14980140447c7f,
MOVI at 0x791b21, invalid-source zero branch 0x791b71,
PUSHINST raw symbol push 0x792693 (inspect with objdump -d).

The frozen v2 display had a separate fixture cause: _TIMER_PAUSED=1 with the
correct boss and an active callback. Explicit BOSSUI_FF immediately updated it.
Only legacy-seed now calls TIMER_SETPAUSE(0) before its new lifecycle; ordinary
loading and production timers/quests remain untouched. Natural callbacks then
follow health. Suspected reused-handle deletion was not established.

## Reproduction

Use tests/run_archolos_boss_ui.py with explicit --executable, --game, --save,
--output (fresh private directory), and --mode:

- legacy-reload: genuine v1/v2 partial-start snapshot, read-only load and visual
  boundary checks; does not restart/finish the boss or save.
- legacy-seed: same input, documented private timer unpause, installed cleanup/new
  START, natural health update and v4 save. Checks valid fresh bar and zero
  destructor argument, and rejects VM exceptions.
- reload: current synthetic half-health v4 save, then cleanup.
- event-seed / event-reload: installed SQ416 active encounter save/restart.
- event-geometry: native fullscreen/window roundtrip and adjacent focus/pause/XP.
- focus-seed / focus-reload: NPC binding and view destructor/reuse checks.

In legacy-seed, the inherited filename boss-ui-cleanup.png captures the newly
created UI after old cleanup, not an empty final-cleanup screen. Inspect images
alongside phase markers. No private saves/assets/logs are committed.

## Scale policy and adjacent consumers

Native status bars and LeGo's 180-unit HP metric share
min(width/800, height/600) * min(SystemPack multiplier, 1).
The authored 800x600 canvas is the maximum-fit layout: bar multipliers above one
saturate there, while smaller multipliers shrink bars. This changes ordinary bar
dimensions, not native font/menu/document scaling. It does not promise arbitrary
zoom or aspect-ratio layout parity.

Overlap assertions use the actual native inner health-fill rectangle against
the boss fill, and keep the native outer bar separate from the title. At
3420x2146/1.25x the outer art begins at y=110 but its fill begins at y=123;
the boss fill ends at y=119. An earlier outer-art assertion rejected this
four-pixel fill gap incorrectly. However, at 1280x720/1.25x the width-only policy
really overlaps fills: native fill starts at y=40, boss fill ends at y=41.
That candidate was rejected in favor of the full-canvas ceiling.
Decorative/background overlap is not health information overlap.

Optional geometry arguments --size-1024, --interface-scale and --consumers use
native Cocoa framebuffer resize and private settings. The runner rejects a
fullscreen-restored initial window and checks the return to original dimensions.
The uncapped 1.25 multiplier produced a 1280-pixel bar at x=-128 on a 1024-pixel
viewport; the full-canvas fit ceiling addresses that overflow and vertical spacing.

Adjacent checks are deliberately not full UI parity:

- Native MENU_STATUS leaves STATUSSCREEN_EXPBAR=0: its activation hook is not
  wired. Explicit installed creation yields valid, in-bounds XP bar geometry,
  but the native menu paints over it. Native menu scaling itself remains oversized
  at 1024x768 and is unchanged by this PR.
- Explicit CRAFTINGVIEW_SHOW creates text and an in-bounds authored dialog.
  Its background/item remain invisible because _RENDER_INIT/list rendering is
  still unimplemented. This is not evidence of a working crafting preview.
- Recipe-document learning/rereading and XP notice checks remain separate.

These diagnostic consumer checks add no new issue, UI subsystem or dependency.

## Final private verification — 14 September 2026

Candidate SHA256: 5ba4a86ae645dffa63a1c5560cc03f7090a7c3ab4963445ec2793abe35594d7d.
Build, runner syntax and diff checks pass. On this candidate, real native
1024x768 and 1280x720 windows at multiplier 1.25 pass fullscreen 3420x2146 and
return, boss/other/no-focus states, pause/resume, half health and death cleanup.
1024x768 at 0.8 passes the same matrix; shrinking remains effective. Each also
checks XP and the bounded adjacent-consumer geometry described above.
The inventory recipe learning/rereading regression passes on the same executable.

Earlier production-identical dynamic-call and view-ownership probes pass, as do
v1/v2 private fresh-UI creation, natural health update, v4 save/restart and cleanup.
Their executable hashes differ only because later opt-in geometry diagnostics
were added; default 1280x720 scaling is unchanged by the final zoom ceiling.
Private manifests and captures are indexed in issue #7; no campaign advancement
or full LeGo/Render parity is claimed.

## Installed deployment — 14 September 2026

Runtime source d5e85407b424f2e72f57cb62540d202f844ab75b is tree-identical to
the merge above. Its candidate hash is recorded above; after bundle ad-hoc signing,
Fast SHA256 is 45f98622183d06deb28ae91431671630b448ec9194c8c6ca24bd7ad4f9bfadec.
Deep/strict signature verification passes. Later documentation commits do not
change the installed runtime. The issue branch is retained.

Private installed checks (paths relative to the environment-guide workspace):

- `work/issue7-installed-modal-save-20260914`: run_archolos_modal.py --mode save,
  runner/game exit 0. Real Escape pause menu, Save Game, visibly typed ModalTest,
  accepted slot 2, ZIP CRC, unchanged quest state and HERO inventory all pass.
- `work/issue7-installed-menu-probe-20260914`: run_archolos_music.py --menu
  using that new save. Initial main-menu background/logo/music, load, normal
  walking, resave, return to menu and process exit 0 pass. The full runner exits 1
  on its later Chapter 2 preset assertion because this input is Chapter 1
  (`chapter=1 entered=0`). This is bounded deployment evidence, not a full music
  suite pass. Both menu stages and finalized save are logged; save CRCs pass.
  The probe initiates load/session exit programmatically, not by manual menu input.
- `work/issue7-installed-normal-menu-20260914`: profiling-off startup logged no
  runtime failure, but computer-control attachment timed out. That private process
  was intentionally terminated (-15); it is not counted as a UI/clean-exit pass.

All six protected non-Fast files match the verified pre-install copies: three
player saves, Gothic.ini, launcher and Profile executable. The rollback directory
`work/issue7-install-rollback-20260914.FI5mKf` also retains the prior Fast binary.
All three rollback save CRCs pass. Assets and dependencies were not changed.
Exact private provenance is in `outputs/issue7-installation-20260914.json`.
