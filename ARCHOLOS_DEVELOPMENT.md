# Local Archolos development log

Update this file at each verified milestone with the symptom, cause or uncertainty, change, verification, and remaining work. Commit meaningful checkpoints locally; never push or submit upstream without the user's explicit instruction.

Working branch: `archolos/performance-v092`, based on OpenGothic v0.92. The newer upstream checkout is separate. Game scripts are installed v1.2.11; the reference decompilation is v1.2.7 and must not be treated as exact source.

Workspace and user-facing evidence: `/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues`. Launcher and reports are under `outputs`; proprietary game data, saves, benchmark runs, and extraction tools are under `work` and are not committed.

## Current milestone: actual modal journal scrolling and in-game saving

The user confirms XP notices and both recipe displays work. Their follow-up disproved the earlier journal-scrolling acceptance: the detached dispatcher check missed a macOS modal event-loop defect. A replacement real-modal test now passes. Saving also had an inaccessible menu entry, diagnosed below. Installation and final regression results are recorded in the latest milestone. Cursor work remains deferred.

# Archolos: first performance milestone

The saved ship scene now runs at approximately **60 FPS instead of 12 FPS** at 1280×720 on this Apple M2. The working change is the existing OpenGothic `-bl 0` option, which selects its alternative resource-binding path. No visual effects were disabled for this comparison beyond the ray tracing, GI and AA settings already shared by both runs.

| Controlled comparison | Default binding path | Alternative binding path |
|---|---:|---:|
| FPS | 11.98 | 59.98 |
| Average frame time | 83.47 ms | 16.67 ms |
| Median GPU command-buffer duration | 84.29 ms | 14.49 ms |
| Submitted frames measured | 180 | 180 |

Both runs used the same native v0.92 source build, save hash, 1280×720 drawable, full internal resolution and ten-second warmup. A prior full-resolution test also reached 60.09 FPS with the alternative path; half scaling reached 59.75 FPS. The comparison check passed. Before/after screenshots were inspected and show consistent ship geometry, characters, water, lighting and shadows. This validates one stationary scene, not the entire game or every rendering feature.

This isolates the major slowdown to the bindless rendering path in this scene. The lower-level cause inside that path is not yet established. Selecting the already implemented alternative is sufficient for this milestone; a global default change for all Macs would need broader hardware testing.

## Ready to use

`Play-Archolos.command` beside this report starts the tested native build with `-bl 0`. Profiling is disabled, so it does not quit automatically or force the benchmark window size. It now opens the main menu so the player can select a save. Saves are stored persistently in:

`/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/work/playable`

Future saves from that launcher stay in this directory. The original playtest save is preserved. Running the launcher is optional; no further user test is needed to establish the measured improvement above.

The repository is now `/Users/lu2/projects/OpenGothic`. The diagnostic branch checkout is `/Users/lu2/projects/OpenGothic-v092`, branch `archolos/performance-v092`. Compatibility links preserve existing build paths. Diagnostic instrumentation remains isolated there to support subsequent tests; the normal launcher leaves it disabled.

## Remaining compatibility work

### Follow-up playtest, 9 September 2026

The user confirms performance is now playable while travelling from the ship to the mainland; creatures and NPCs react, and the visited environment appears loaded correctly. This expands the practical performance evidence beyond the stationary benchmark, without establishing full campaign compatibility.

The user bypassed the ship bars and reached shore before the intended quest progression. Some conversations ended normally, but an NPC-initiated conversation could not be exited and required force-quitting.

The attached log contains 20 complete traces. Repeated exit failures include `DIA_WILLEM_EXIT_INFO -> AI_RESETFACEANI -> AI_FUNCTION_NSII -> MEM_GETFUNCID`. The older v1.2.7 decompilation puts the face-animation reset immediately before `AI_STOPPROCESSINFOS`, the dialogue-stop operation. This identifies a concrete reproduction target; whether the installed v1.2.11 execution aborts before that operation remains unverified. The traces also retain increasingly many earlier dialogue frames, starting with the smuggler XP notification; investigate VM exception recovery/call-stack restoration as well as script-function resolution. These are observations and investigation targets, not a confirmed root cause.

`ZS_Fane_Loop` is a recurring NPC behaviour function with debug output in the decompiled source. Its repeated log line alone does not identify Fane as the stuck-dialogue NPC. The exit traces identify Willem. Earlier quest access may affect progression, but it does not adequately explain away the recorded compatibility failures.

GOG explicitly supplies a [Polish Voice-Over Pack](https://www.gog.com/en/game/the_chronicles_of_myrtana_archolos_polish_voiceover_pack) for English subtitles with Polish voices. The local installer listing contains no separate speech archive, consistent with the earlier mounted-file audit finding no dialogue clips. Dubbing exists for Archolos; it is absent from the supplied local payload. Downloading the matching voice-pack installer and all its companion files to `/Users/lu2/Downloads/archolos` will allow local extraction and playback verification.

Next acceptance milestone: progress through the opening without bypassing its barriers, exit both voluntary and NPC-triggered conversations, and reload a save with quest state intact. The user has set the ship barrier as first priority and dialogue exits as second. Then address shared script compatibility for XP/recipe/log updates, journal scrolling, and broader progression checks. Continue local-only commits.

The ship gate is present in the installed world with dynamic collision enabled. Its mesh material also permits collision. This rules out simply missing gate data, and the in-engine failure has now been reproduced and fixed in movement handling. The movement fix and its validation are recorded below.

Recipe generation, XP notifications and journal scrolling remain unresolved. The user has now confirmed that loading restores quest progression and inventory. This validates their tested save; broader save coverage remains future work.

The matching GOG 1.2.11 Polish voice pack is now installed: `KM_Speech1PL.mod` through `KM_Speech6PL.mod` in the game Data directory. Extraction completed successfully; all six archives mounted in ZenKit and contained only WAV audio (50,904 entries). Previously absent Fane and Willem clips are present; a Willem line successfully decoded to non-silent PCM using macOS. Script archives and save files retained their pre-installation SHA-256 hashes, so English text and existing saves are preserved. The user has confirmed that in-game dubbing works and that dialogue options appear more promptly. Installation evidence is in `archolos-voice-installation.json`.

Detailed machine-readable evidence is in `archolos-performance-milestone.json`. `archolos-render-before.png` and `archolos-render-after.png` contain the reviewed images. The local diagnostic source changes are in the two profiling patch files.

Post-installation smoke test: the saved ship scene loaded and completed 180 frames at 55.02 FPS, then exited normally. No archive-loading errors occurred and the original save hash was unchanged. This checks loading and rendering with the voice pack installed; it does not verify audible dialogue.

Launcher update: automatic loading of slot 1 is disabled. The same launch command opens the main menu; manual saves remain in `work/playable`, and reopening the launcher does not reset them.

## Ship-bar fix, 9 September 2026

**Implemented and installed locally.** Normal forward input previously walked through `SHIP_TRAPDOOR` onto the deck. Automated direct collision queries correctly detected the gate, so the mesh and mover collision were not missing.

The movement code kept horizontal progress when its upward ground adjustment failed against the bars. It could gradually sink the character into the stairs and eventually snap onto the gate. Rolling back to the displayed position alone did not solve it: sub-two-centimetre movement offsets may update the displayed position without updating physics, so a later rollback could place the collider inside the obstacle. The final change in `game/game/movealgo.cpp` rolls failed or incomplete upward adjustments back to the physics position captured before movement. The existing small-movement optimization remains intact.

Verification:
- The automated forward-input checker fails against the pre-fix run (`work/frame-profile-0-xg31gqx1/terminal.log`): the player reaches the deck through the closed gate.
- With the fix, 600 submitted frames of forward input remain below the closed gate (`work/gate-final-closed/terminal.log`, gate frame 0, idle; final feet height -1648.16).
- The normal mover trigger opens the gate to frame 2, idle; forward input reaches the deck (`work/gate-final-open/terminal.log`, 600 submitted frames). The player may subsequently turn or leave the deck while input remains held; the check tests successful passage, not the final location after wandering.
- Both game processes exit normally and the source save hash remains unchanged. Screenshots show the earlier deck access and the corrected blocked position.
- A shorter corrected run retained 60.05 FPS. The longer correctness runs overlapped heavy unrelated machine activity; their 19.64/30.37 FPS results are not a controlled performance comparison.

Scope: this verifies forward movement at the ship stairs and opening via the engine's normal trigger. It does not establish all collision edge cases or completion of the captain's full quest script. Dialogue exits remain the next gameplay blocker.

Evidence in workspace `outputs`: `archolos-ship-bars-fix.json`, `archolos-ship-bars-fix.patch`, and before/after PNGs. The normal launcher uses the tested executable; its previous version is backed up at `work/Gothic2Notr-before-ship-bars`. Proprietary assets and saves remain outside Git.

### Re-running the integration checks

From the workspace directory, run the following with a new output directory for each invocation. Set `--state open` for the complementary open-gate check. The runner copies the original pre-quest save, sets opt-in profiling/input probes, enforces a timeout, checks the trace and verifies that the source save is unchanged. It does not require UI automation permissions.

```sh
rtk proxy python3 /Users/lu2/projects/OpenGothic-v092/tests/run_archolos_gate.py --executable work/ArcholosProfile.app/Contents/MacOS/Gothic2Notr --game work/archolos-game --save work/playtest/save_slot_1.sav --output work/gate-next-closed --state closed
```

`tests/check_archolos_gate.py` can also check an existing trace. The probe in `mainwindow.cpp` drives the real `PlayerControl` forward action for 600 submitted frames; `movetrigger.cpp` prepares the starting position and reports gate state. These diagnostics are opt-in and separate from the movement fix. The normal launcher clears `OPENGOTHIC_PROFILE`, opens the main menu and stores manual saves persistently in `work/playable`.

## Cursor change reverted; next milestones

The user confirmed the ship bars work, then reported that cursor commit `ace46864` prevented mouse look and caused additional problems. Reverted all cursor code and its probe/checker. Restored both ArcholosFast.app and ArcholosProfile.app from `work/Gothic2Notr-before-cursor-fix`; their SHA-256 matches the previously validated ship-bar executable (`1bcf7246eb0e8b29196f99c2e57ec243bd006733a7abd5ea652b58d9a43c16b5`). The launch command is unchanged. Saves and game data were not modified.

The previous cursor test checked only requested cursor state, not working mouse look or native cursor behavior. Its passing result was insufficient for gameplay acceptance. No further cursor investigation is authorized for now; leave this issue deferred.

Latest user log (`2a01881e-f464-4dd3-8f39-11f6f53cd451/pasted-text.txt`) exits normally through LEAVE_GAME, with unsupported script hooks, memory-translation warnings and an ApplyHouseWallTexture null-pointer trace. It contains no Willem/MEM_GetFuncID exit trace; that does not establish a dialogue fix.

Next priorities:
1. Reproduce and fix dialogue exits, especially the Willem AI_RESETFACEANI / AI_FUNCTION_NSII / MEM_GetFuncID path; investigate shared function resolution and VM call-stack recovery.
2. Missing XP notifications and recipe journal entries, potentially sharing script compatibility failures; verify rather than assume a common cause.
3. Long journal text scrolling.
4. Normal opening-quest progression, quest-triggered gate opening, conversations, and save/reload checkpoints; then broaden campaign/system coverage.

More story progression is not required to begin these known issues. Later, saves before and after natural story transitions will improve coverage. Current status is a playable early compatibility build: performance, dubbing, basic movement/combat, tested inventory/quest save restoration and ship-bar traversal are established. Full campaign completion, later scripted sequences, crafting/trading coverage and world transitions remain unverified. A percentage of total completion or reliable completion date cannot be inferred from these early milestones.

## Dialogue exits and native notifications, 9 September 2026

Implemented and installed locally. The production change adds two compatibility adapters in `game/game/compatibility/directmemory.cpp`; cursor and movement code are unchanged.

### Cause and change

Installed v1.2.11 bytecode confirms `DIA_WILLEM_EXIT_INFO` calls `AI_RESETFACEANI` immediately before `AI_STOPPROCESSINFOS`. The latter was never reached in the reproduced failure. `AI_RESETFACEANI -> AI_FUNCTION_NSII -> MEM_GetFuncID` tried to resolve function references through the legacy Ikarus parser-memory implementation, producing the same MOB_CREATEITEMS.PAR1 / MEMINT_STACKPOS / unresolvable-function trace as the user's log. `MEM_GetFuncID` now uses the existing ZenKit `DaedalusFunction` resolver, which follows function-variable references and accepts script/external functions. Invalid references return -1.

The resolver-only candidate passed two dialogue exits, but a warmed-up XP award then reached unsupported LeGo notification allocation and crashed in `mem_insttoptr`. The combined fix also routes `PrintS_Ext` through OpenGothic's existing `onPrint` notification UI. `PrintS` and Archolos `PrintScreenS` delegate to this shared function; direct colored crafting-notice callers use it too. No new overlay or animation system was added. Notifications currently use native text styling; LeGo color and fade effects are deliberately omitted.

### Verification

- Direct installed-script baseline `work/frame-profile-0-v1ziefre`: reproduced the user's exact error; exit script returned without reaching the stop instruction.
- Resolver-only direct script check `work/frame-profile-0-oivt5jg6`: stop instruction reached, no resolver error in the exit segment.
- Full UI baseline `work/dialog-ui-before`: automatic Willem greeting was answered through normal selection; function resolution failed and the conversation remained active after 2400 frames.
- Resolver-only UI check `work/dialog-ui-greeting`: automatic greeting, response, exit, reopen and second exit all completed; 59.60 FPS.
- Final combined check `work/dialog-ui-native-notices`: after the ten-second warmup, `B_GIVEPLAYERXP(50)` increased XP by 50 and delivered `Experience + 50` to `DialogMenu::print`. Then Willem's automatic greeting, normal response, exit, reopen and second exit completed. Both exit instructions reached the AI queue and both conversations closed after spoken output. Final dialogue state inactive; 2400 frames at 59.09 FPS; normal process exit. This verifies delivery into the native message UI, not a pixel comparison of the notification.
- Final ship regressions: `work/gate-dialog-fix-closed` blocks ascent (600 frames, 59.72 FPS); `work/gate-dialog-fix-open` permits ascent after the normal mover trigger (600 frames, 58.90 FPS).
- All valid completed tests preserved the source save SHA-256. Release build and whitespace checks passed. An explicit `<cstdlib>` include added during cleanup produced a byte-identical executable to the combined dialogue test.

The test relocates Willem alongside the player in a disposable copy of the ship save, allowing his automatic greeting to trigger. It uses real dialogue choices and NPC output queues and lets voice lines finish. It does not replay the mainland journey or prove every dialogue branch. An earlier XP probe ran before script initialization and is excluded (`work/dialog-ui-after-xp`); its corrected warmed-up version established the real notification crash (`work/dialog-ui-after-xp-warm`). That crashing candidate was never installed in the normal app.

`tests/run_archolos_dialog.py` repeats the test with user-supplied local game data and save, a new output directory, a 180-second timeout, isolated OPENGOTHIC variables and source-save hash verification. Add `--after-xp` for the combined regression. Engine probes require profiling plus the dialogue-probe flag; normal launches leave them inactive.

```sh
rtk proxy python3 /Users/lu2/projects/OpenGothic-v092/tests/run_archolos_dialog.py --executable work/ArcholosProfile.app/Contents/MacOS/Gothic2Notr --game work/archolos-game --save work/playtest/save_slot_1.sav --output work/dialog-next --after-xp
```

The tested executable is installed in ArcholosFast.app. Previous normal executable: `work/Gothic2Notr-before-dialogue-fix`. Current executable SHA-256: `2b53e74324419f06f8a9688c1c1fee6f49b63c5a5b75273735b45da6949a57c6`. The launcher still opens the main menu with persistent manual saves. Source data, saves and cursor behavior are unchanged.

Remaining: learned-recipe journal generation, journal scrolling, normal opening-quest progression and broader campaign/save coverage. General VM exception recovery, legacy-memory APIs and the existing unimplemented NSII face-animation callback remain incomplete; this is a verified fix for the reproduced dialogue blocker, not proof that all scripted dialogue is supported. No additional user save is required to start the next known issues.

## Recipe learning and journal scrolling, 9 September 2026

The installed v1.2.11 recipe scripts are unchanged. The fix is in shared Ikarus/LeGo compatibility and the native journal input handler.

### Recipe cause and change

The copied save reproduces the empty Cooking topic: the real `USECOOKINGRECIPE` function leaves `PLAYER_TALENT_COOKING[18]` unset and produces no recipe entry. Installed bytecode confirms that it creates a `C_RECIPE`, reads/writes a cooking array, calls `BUILDRECIPELOG`, and then displays the recipe document.

Three shared compatibility gaps prevented this flow:
- LeGo `Create` depended on an uninitialized parser-symbol table and a failing legacy assembly call path. A native adapter now resolves the instance's parent class and reuses the existing engine constructor callback. A diagnostic parser reinitialization alone did not solve recipe learning.
- Transient instance arrays used class metadata as their element stride. Integer/float elements now advance four bytes; Gothic strings advance twenty bytes.
- Taking addresses of native item members was unsupported, and global arrays were treated as twenty-byte per-symbol slots instead of contiguous elements. References now use existing Mem32 callbacks to synchronize contiguous integer/float/string storage with the actual VM values. Native instance references are retained until the VM ends; large-scale mapping reclamation is deferred.

These are shared API changes, not a hardcoded recipe description or a replacement for Archolos's learning logic. A recipe whose learned flag was already saved as true but whose log is missing is not automatically repaired. The inspected original and current playable slot 1 both have the rat recipe flag at zero, so reading the recipe again can run the corrected learning path. Reading a recipe does not consume it in this engine's normal inventory-use path.

### Scrolling cause and change (initial, incomplete fix)

**Superseded acceptance:** the user subsequently reported that scrolling still failed. The detached test below missed modal timer starvation; see the latest milestone for the reproduction and replacement test.

`ListContentDialog` handled wheel input and key releases but lacked the repeat handler already present in `ListViewDialog`. Holding Down therefore did nothing until release, which moved only one line. The one-line change forwards repeat events to the existing Up/Down/W/S handler.

The regression loads the real MENU.DAT journal layout and copied save's Below the Deck text, dispatches held keys and wheel events through Tempest's EventDispatcher into ListContentDialog, and checks the normal draw path's clamped line range. Before: held Down produces offset zero; release produces one; wheel reaches eight. After: held Down reaches the final page at eight, held Up returns to zero, and wheel reaches the same final page. The 29-line test content has 21 visible lines. This verifies the engine dispatcher and rendered text range; it is not a native macOS event-injection or screenshot comparison of the modal overlay. Earlier modal probe attempts could not pump the application from timer/render callbacks and are excluded from acceptance evidence.

### Verification

- `work/frame-profile-0-rmmmxeo5`: original recipe failure, empty Cooking topic, learned flag zero.
- `work/recipe-native-create`: native construction plus member strides alone still leaves the recipe unlearned.
- `work/recipe-array-map`: learned flag and full ingredient log work, while document member access still fails; this intermediate build was not installed for normal play.
- `work/recipe-native-members`: final recipe test passes. Rat on a stick lists Heavy Branch, 2x Fried rat meat, Salt, and A bag of pepper. Both recipe-document displays contain the description, 35 HP restoration, and value 6. Rereading adds no duplicate; other cooking flags and pre-existing quest entries remain intact. No memory-translation failure or VM exception occurs inside the tested learning/document sequence. `LOG_MOVETOTOP` remains unimplemented, affecting ordering rather than entry creation.
- `work/journal-dispatch-before`: held-key regression fails as expected.
- `work/journal-after`: held Down, held Up, wheel, and final-page draw checks pass.
- `work/dialog-recipe-regression`: XP award/notice, automatic Willem greeting, two exits, and final inactive dialogue pass.
- `work/gate-recipe-closed` and `work/gate-recipe-open`: closed/open traversal regressions.

The recipe runner invokes the installed inventory-use script with the player and recipe item after ten seconds of initialization. It checks the script result and document delivery; it does not simulate clicking the inventory or cooking the meal. Both new runners use private save copies and verify that the source save hash is unchanged. Diagnostic manipulation requires OPENGOTHIC_PROFILE plus a specific probe flag; the normal launcher disables profiling.

The next milestone is natural opening-quest progression, including actual cooking and quest-triggered gate opening, followed by broader campaign/world-transition/save coverage. Cursor work remains deferred. No upstream push or submission is authorized.

Release build and whitespace checks pass. All five final integration runs exit normally and preserve the source save SHA-256. Installed executable SHA-256: `33e947e052e43c6c604d455dc95358acbd016aba59c45e3974fd0dd22630a42b`. Previous normal executable: `work/Gothic2Notr-before-recipe-journal-fix`. The launcher still opens the main menu with persistent manual saves.


## Modal journal scrolling and in-game saving, 9 September 2026

The user confirms XP notifications and recipe reading/journal entries work, but reports no journal scrolling and inability to save. The earlier statement that journal scrolling was fixed was premature. Its detached-dialog test did not enter the modal event loop; that probe and runner have now been replaced.

### Causes and changes

- **Journal:** real Cocoa key events reach the live ListContentDialog, and its scroll offset changes. However, macOS ran Tempest UI timers only in the outer application loop. The nested Dialog::exec loop calls processEvents directly, so the GameMenu repaint timer never fired. The production fix services timers from the inner event pump when no native event is waiting, as the other platform backends do. The previous held-key handler remains necessary.
- **Saving:** pressing Escape after loading opens MENU_MAIN, which has no Save Game or Resume entry in installed v1.2.11. The installed scripts declare INGAME_MENU_INSTANCE="MENU_GAME"; that menu has both entries. INIT_INGAMEMENU normally patches the legacy engine's menu-name memory. OpenGothic's save-loading constructor restores game/script state without replaying startup scripts, and DirectMemory initializes this runtime buffer to MENU_MAIN. The compatibility layer now initializes that same writable buffer from the mod's declared constant when it is a nonempty string that fits; otherwise the standard menu remains the default. Script patches can still change the buffer. This addresses menu initialization, not general persistence of every legacy memory patch.

### Replacement integration coverage

`tests/run_archolos_modal.py` loads private copies of the supplied ship save. Its journal mode opens the actual journal list and nested description. Opt-in diagnostic replay posts key events into the application's own Cocoa queue; wheel events enter through the window dispatcher. No global input injection or accessibility permission is required. The test checks visible text ranges separately for held Down, Up, S, W and wheel, and verifies the original save hash.

Save mode starts from gameplay and presses Escape through the real pause-menu path, selects Save Game, selects a private slot 2, types ModalTest, confirms, and checks the resulting save ZIP. It verifies the name, quest payload and player inventory against its input. Running it again with that new save proves fresh-process loading and re-saving. Tests use disposable directories; no user save is overwritten.

Baseline evidence:
- work/modal-journal-before: real modal receives input but never repaints the last page; expected failure.
- work/modal-save-before: direct save submenu works, but bypassing the pause menu was insufficient to test accessibility.
- work/modal-pause-save-before: real Escape opens MENU_MAIN without Save Game; expected failure.

Corrected evidence:
- work/modal-journal-after: first actual-modal Down/Up/wheel pass.
- work/modal-journal-final: held Down and S reach the last page (offset 7, 28 lines, 21 visible); Up and W return to offset 0; wheel reaches offset 7. Each phase checks an actual redraw while the modal is open.
- work/modal-pause-save-after: Escape opens MENU_GAME with Save Game; typing and confirming ModalTest creates a valid slot 2. Quest and player inventory payloads match the original.

The keyboard check includes native macOS event translation; the wheel check starts at the window dispatcher and does not validate physical trackpad/Cocoa delta conversion. Draw-range logging is not a screenshot comparison. The normal launcher disables these probes. Cursor behavior is unchanged.

- work/modal-pause-save-reload: a fresh process loads the new slot-2 save through normal loading, opens the real pause menu, saves again, and preserves byte-identical quest and player inventory payloads. The input save hash is unchanged.

Re-run journal mode using a new output directory:

```sh
rtk proxy python3 /Users/lu2/projects/OpenGothic-v092/tests/run_archolos_modal.py --executable work/ArcholosProfile.app/Contents/MacOS/Gothic2Notr --game work/archolos-game --save work/playtest/save_slot_1.sav --output work/modal-next-journal --mode journal
```

For saving, use `--mode save`. Repeat with the newly generated `save_slot_2.sav` as `--save` and another new output directory for a fresh-process load/save round trip. This runner targets the supplied single-world ship-save scenario; broader campaign saves need corresponding state expectations.


Final regression and installation:
- work/recipe-modal-regression: recipe learning, ingredients, document stats and repeat-read checks pass (59.76 FPS).
- work/dialog-modal-regression: XP +50 notification, automatic Willem greeting and two exits pass; final dialogue inactive (59.87 FPS).
- Release build and whitespace checks pass. No cursor or movement changes were made. The replaced journal checker is removed to prevent the earlier false acceptance from recurring.
- Installed tested binary SHA-256: `2e44bf55ae52550a6849b8555dbbe0d4a74d0fca65b83ac8a7041d11df82e7e7`. Previous normal build is preserved at work/Gothic2Notr-before-modal-save-fix. Normal launch command and persistent work/playable directory are unchanged. Original ship save hash remains `6a084b00dedd4a104f16c5a53e8c1569aba0b4b846d4b989dd91b36e97a1316d`.
- Tempest checkpoint: `9d524a7`. All changes stay local; nothing was pushed.

Next: normal opening-quest progression, actual cooking, quest-triggered gate opening, and later world-transition/campaign coverage. Neither this menu fix nor the earlier recipe fix establishes full legacy-runtime compatibility.


## User acceptance and upstream cursor review, 9 September 2026

The user confirms both the real-modal journal scrolling fix and the in-game Save Game menu now work. This supplements automated acceptance of checkpoint f758d0d4.

Reviewed OpenGothic PR https://github.com/Try/OpenGothic/pull/980 (open, commit 2de81bfce28955809228e2449c84baebc6ee0e1b). It hides the cursor at construction/focus gain, removes fullscreen-dependent cursor selection on resize, recenters using widget dimensions, and points Tempest to ee90967c084806d29fe3fed1e5564add3fbf3277. The construction/resize edits overlap substantially with our reverted ace46864; the PR does not establish macOS mouse-look correctness.

Its dependency https://github.com/Try/Tempest/pull/98 was tested on Linux/X11 and closed unmerged. It makes Window::setCursorShape apply a native cursor immediately regardless of hover state. The maintainer objected that this can override child-widget cursors and mishandle multiple windows, then closed it because a newer PR exists: https://github.com/Try/Tempest/pull/98#issuecomment-5532952841.

Replacement https://github.com/Try/Tempest/pull/100 is open and also reports Linux/X11 testing. It reevaluates hovered widgets on window entry, focus and geometry changes. It includes shared dispatcher changes but its new platform event handling is X11-specific; it contains no macOS backend fix. PR980 still references the older rejected Tempest commit at review time.

Assessment: a useful lead about startup/focus/hover cursor state, with no verified drop-in macOS solution. Our macOS backend still handles native hide/show and cursor warping separately. In this checkout ordinary mouse-move camera input is fullscreen-gated, while dragging has its own enabled path. These interactions require reproduction and actual camera-motion tests before any further cursor change is installed. No cursor source changes or app replacements were made for this review. The user authorized investigating this link; installation of an unverified cursor candidate is not part of the review.

Next gameplay checks, in priority order: actual cooking (ingredients consumed, meal produced, progression updated), completing the opening quest and opening the ship gate through the quest's real script, then natural travel/chapter/world transitions with save checkpoints. These are unverified flows, not confirmed new failures. Known lower-priority compatibility gaps include LOG_MOVETOTOP ordering, LeGo notification styling, and unsupported legacy runtime hooks. Reproduce a player-visible consequence before broad compatibility work.


## Open Lock spell blocker, 10 September 2026

The user confirms actual cooking works and asks that cursor behavior remain unchanged. Their next natural opening-quest blocker is targeting a chest with Open Lock equipped. The current user save is work/playable/save_slot_7.sav; all diagnostics use private copies.

Reproduction and isolation:
- work/lock-before: at the saved position, unarmed focus is Chest / Q101_CHEST_01, lock code LLL, uncracked. Drawing the existing spell gives Mage state / spell 103, but focus becomes empty. Casting raises the same unbound OCNPC.FOCUS_VOB error present in the user's normal log.
- Installed v1.2.11 bytecode confirms SPELL_PICKLOCK uses target type 128, focus collection, range 550 and azimuth 20. SPELL_LOGIC_PICKLOCK accesses native oCNpc focus and oCMobLockable memory. The v1.2.7 reference is broadly consistent, but its cast-delay tail differs from installed bytecode.
- work/lock-focus-only: adding locked-object targeting selects Q101_CHEST_01 with the spell drawn. Casting still fails on OCNPC.FOCUS_VOB, proving targeting alone is insufficient.
- work/lock-native-cast: the native compatibility adapter unlocks the chest through the NPC investment/cast animation flow; one scroll and one mana are consumed, and the normal chest inventory opens. 900 frames, normal exit, source save unchanged.

Production changes:
- Add the mod's locked-interactive target type. Use the active spell's range/azimuth for this type and filter to locked containers/doors. Existing ordinary spell, NPC, item and bow focus paths retain their policies.
- Adapt SPELL_LOGIC_PICKLOCK in DirectMemory to native focus and lock state. Honor the code length and script mana-cost constant, reject invalid/unlocked targets, stop if the target changes, issue the existing unlock notice/sounds and perception event, and use the existing cracked flag that is already serialized. NPC casting and scroll consumption remain in their existing engine paths. No quest variable, chest inventory or game archive is patched.

Limits: this adapter is for player Open Lock casts. Per-cast progress does not combine with partial conventional lockpicking, so the hybrid achievement is not implemented. It uses the engine spell definition's investment interval rather than the original script's randomized delay. General native oCNpc/oCMobLockable memory emulation and every theft-reaction hook remain outside this fix. These limits do not prevent the reproduced three-step ship chest unlock.


Final acceptance checks:
- work/lock-player-cast: the actual PlayerControl action/forward inputs perform the cast. Turning away, zero mana and an already cracked target are rejected through the mana-dispatch path before the valid cast. The valid cast unlocks the real chest, consumes exactly one scroll and one mana, and opens InventoryMenu::Chest. A private slot 2 is saved; existing quest payloads remain byte-identical. No script exception or translation failure occurs in the measured flow.
- work/lock-reload: a fresh process loads that newly saved slot, finds Q101_CHEST_01 cracked and opens its normal chest inventory. No recast or save patch is used.

`tests/run_archolos_lock.py` is the repeatable check. Use the player's supplied pre-chest save, local game data and a new output directory. It preserves source hashes, checks real runtime traces and private save integrity, and enforces a timeout. With `--mode reload`, pass the cast run's save_slot_2.sav and another new output directory. Rejected-target checks call the normal script mana dispatcher; the successful cast uses real PlayerControl actions and the NPC animation/cast loop. Chest access uses InventoryMenu::open, including its usual interaction/lock checks. These are in-process integration checks, not physical keyboard replay.

```sh
rtk proxy python3 /Users/lu2/projects/OpenGothic-v092/tests/run_archolos_lock.py --executable work/ArcholosProfile.app/Contents/MacOS/Gothic2Notr --game work/archolos-game --save work/playable/save_slot_7.sav --output work/lock-next --mode cast
```

Player slot-7 SHA-256: `09e4ffadba119604a4896b53b2e2c73c543f69db26134da782c4f32d8fbdce72`. The user may continue to change their saves; future tests should use a matching pre-chest checkpoint. No user save was overwritten by these tests.


Installed milestone: full PlayerControl cast, rejection checks, private save creation and fresh-process chest reload all pass. work/dialog-lock-regression also passes XP +50 delivery, automatic Willem greeting and two dialogue exits. Release build and whitespace checks pass. The tested executable is installed in ArcholosFast.app with SHA-256 `d8766a3a5895bf188ca9ed5ad9055d844ae625a09a20011b813ba49656251969`; previous executable preserved at work/Gothic2Notr-before-open-lock-fix. Launcher, user saves, proprietary data and cursor code are unchanged. Nothing is pushed upstream.

Next acceptance: the user continues from their existing pre-chest save using the spell, then progresses through the remaining opening quest. Quest-triggered ship-bar opening and later campaign transitions remain unverified.
