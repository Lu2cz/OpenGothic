# Local Archolos development log

Update this file at each verified milestone with the symptom, cause or uncertainty, change, verification, and remaining work. Commit meaningful checkpoints locally; never push or submit upstream without the user's explicit instruction.

Working branch: `archolos/performance-v092`, based on OpenGothic v0.92. The newer upstream checkout is separate. Game scripts are installed v1.2.11; the reference decompilation is v1.2.7 and must not be treated as exact source.

Workspace and user-facing evidence: `/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues`. Launcher and reports are under `outputs`; proprietary game data, saves, benchmark runs, and extraction tools are under `work` and are not committed.

## Current milestone: Native KmLib gameplay integration

Menu soundtrack, shared zone gameplay notifications and persistent local achievement counters/unlocks are now implemented and installed. Menu/load/save/return, full music scenario, fresh-process override/counter restoration and fresh-game recipe regressions pass. Full external platform integration, legacy renderer/debug hooks and campaign completion remain outside verified coverage. See the latest dated section for exact evidence and limits.

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


## Vrazka stash and missing ship loot, 10 September 2026

The user reported that Vrazka's “Examine the boards” never became actionable, and the stove chest contained only sticks. They explicitly requested a fresh game for loot reproduction. Current source save: work/playable/save_slot_9.sav. Tests use private directories; cursor work remains deferred.

### Causes and implementation

1. **Container-content whitespace.** Installed ARCHOLOS_MAINLAND.ZEN specifies the stove chest as four sticks, one knife, two torches and eleven gold. Every entry after the first begins with a space. Interactive::implAddItem passed that space into script-symbol lookup, silently losing the later items. The same defect removed the box from Vrazka's hidden stash, and items from the spell chest and barrels. Trim item tokens before lookup, including whitespace immediately before a colon. Existing inventory parsing/count behavior is retained. This applies to newly initialized containers throughout the game.
2. **Frozen quest timers and missing registration after loading.** DirectMemory's mapped oGame.TIMESTEP stayed zero, so LeGo TIMERGT treated gameplay as paused. Full save loading also restores script variables but not the allocated frame-callback registry. Populate TIMESTEP from the actual simulation step; after loading, initialize Ikarus and invoke the mod's INIT_QUESTSEVENTSMANAGER once to restore its normal recurring dispatcher. No Q101 quest flag is patched; its own installed script tests distance, writes the log entry and triggers the mover. This deliberately restores the recurring quest manager, not arbitrary one-shot callbacks or the entire legacy heap. General callback/heap persistence remains incomplete.
3. **Follow-up container item.** The real Vrazka return function calls MOB_CREATEITEMS to put her broken box in KM_VRAZKA. The engine previously logged “not implemented call [MOB_CREATEITEMS]”. Add that external using native container inventories, matching the first named container like existing MOB_HASITEMS. Reject invalid item types, nonpositive counts and empty targets. All script callers use this shared implementation.

Fabio is different: B_CREATEAMBIENTINV chooses a random inventory set. Installed bytecode confirms the farmer set used by Fabio has one branch without gold; the other branches contain coins. Fresh runtime tests show five gold through the normal ransack iterator, while the player's save has zero. No evidence supports adding guaranteed gold or changing this random selection.

The supplied GameFAQs URL returned a Cloudflare challenge to the attempted fetch. Guide contents were not independently read; the user's observations were compared with installed v1.2.11 data/bytecode and the v1.2.7 reference decompilation. Exact item names/counts above come from the installed world.

### Reproduction evidence

- work/stash-before: copied slot 9 at the amulet waypoint remains flag 1 and boards height -4092.44, below the floor.
- work/loot-fresh-before: actual -nomenu new game gives four sticks only; the hidden stash contains eight gold only. Fabio has five gold and it appears in the ransack iterator.
- work/stash-direct-before: directly invoking the installed callback changes flag 1 to 2, isolating the scheduling failure from the quest condition.
- work/stash-init-candidate and work/stash-restore-candidate: normal timer dispatch changes flag 1 to 2 and the mover raises the boards to -1882.19. The latter uses production load restoration, without test initialization or direct quest invocation.
- work/loot-fresh-fixed: actual new game creates all four stove-chest item types and all four hidden-stash item types, plus the previously missing spell-chest/barrel loot.
- work/stash-repaired-test: a copied save with only the verified skipped ship items restored naturally reveals the boards, finds “Examine the boards” through World::findFocus, opens the normal chest inventory and writes a private save. No already collected first entry is refilled.
- work/stash-return-before: fresh-process reload preserves the raised boards and recovered loot; taking the box and invoking the return script consumes it, but reveals the missing MOB_CREATEITEMS external. The updated checker rejects this trace.
- work/stash-return-fixed: a diagnostic-only attempt to call that external with vm.call_function crashed because that API jumps to script bytecode, not native externals. Removed the incorrect test invocation. This executable was never installed in the playable app; validation now uses the quest's actual external-call instruction.

The reusable runner is tests/run_archolos_stash.py. Modes: fresh creates a new game; stash tests normal approach/focus/opening on an accepted-quest save; return additionally transfers the box through Npc::addItem and calls the existing return script; repair preserves the player's location and restores only entries supplied in an explicit local recovery file. All modes write to new directories, verify source hashes and validate their output saves. Return is a script/inventory integration check, not full physical conversation replay. Its assertion also requires the follow-up broken box to be created in KM_VRAZKA. An initial candidate incorrectly tested whether the item symbol already held an initialized instance; the corrected implementation checks its declared C_ITEM ancestry so never-before-created items work.

Recovery entries are extracted from the installed ship containers' whitespace-prefixed tokens, held in work/ship-missing-items.txt outside Git. The diagnostic repair rejects ambiguous target names, invalid items and any item already present, and never automatically runs from the normal launcher. The recovered ship save does not reconstruct arbitrary already initialized mainland containers. A new game is the clean baseline for complete world loot after this parsing fix.


Additional acceptance:
- work/stash-return-verified passes: reloaded boards focus/open, box transfer gives one box, the return script consumes it and creates exactly one broken box in KM_VRAZKA; no unsupported MOB_CREATEITEMS call remains. 900 frames, normal exit.
- work/ship-recovery-final generates the five repaired inventories from the original slot 9. Normal timer execution during this donor run also changes a journal entry, so that whole generated save is deliberately not delivered. tests/recover_archolos_containers.py merges only the five audited inventory ZIP entries into the untouched source, renames the save, and verifies every other gameplay entry remains byte-identical. Player/NPC inventories, quest progression, script variables, position and world state are preserved exactly in recovered.sav.

- work/stash-recovery-reload: exact merged recovery loads, naturally changes the stash flag from 1 to 2, raises the boards, finds their focus label, opens the inventory and creates a valid private save. The first checker matched the initial hidden-board inventory dump; its regex now targets the explicit final boards trace, and rechecking the captured run passes.
- work/dialog-stash-regression: XP +50 notification, automatic Willem greeting and both exits pass after the load/runtime changes.

Installation: tested SHA-256 9e41794d68231c5a07a72423c909ee8139c8c013df02261579549944896a184f in both diagnostic and playable bundles. Previous playable executable: work/Gothic2Notr-before-stash-loot-fix. The exact recovered copy is installed as work/playable/save_slot_10.sav, titled “Archolos loot recovery”; slots 1–9 retain their original hashes. The source is the user's slot 9. No item is inserted into the player's inventory and no quest is force-completed in this delivered recovery.

Release build, whitespace checks and Python syntax checks pass. No cursor changes or upstream pushes. Next: user continues Vrazka's quest from the recovery or a new game; full quest-triggered ship exit and later campaign progression remain unverified. A new game is recommended for a completely corrected distribution of initial loot across already initialized world areas; the recovery is limited to the five audited ship containers.


## Captain cutscene and scripted departure, 10 September 2026

The user’s slot 11 is a pre-captain checkpoint. All tests load a private copy, move the player next to Jorn, open the normal dialogue, and select “Anything new?” / “More or less.” No quest flag is forced, no camera is forcibly released, and no user save is patched.

### Confirmed causes

1. After loading, LeGo’s `_ANIM8_FFLOOP` recurring animation update was absent. `Q101_SHIP_FINISHCUTSCENE` reached the normal fade function, but the animation stayed at alpha 0 indefinitely. The script therefore never called `Q101_SHIP_TELEPORTNPC_FADESCREEN`. Register the animation callback using LeGo’s own `FF_APPLYONCEGT` during load restoration.
2. The subsequent ship departure stalled waiting for Rupert to swim. His installed `T_CUTSCENEJUMP_START` animation exists and carries MOVE | FLY flags, but `AI_PlayAni` supplied BS_NONE. Ordinary ground movement prevented the scripted jump from carrying him into the water. The NPC named-animation path now classifies an otherwise untyped flying transition as BS_JUMP, reusing existing vertical root motion, jump momentum and gravity handling. Explicit body states, normal player movement and non-NPC animations keep their existing paths. No animation name or quest is hardcoded into this fix.

### Evidence

- `work/captain-before`: normal Jorn and captain choices reproduce the reported freeze. Frames 5700–7800 repeatedly show camera active, dialogue closed, captain flag 6, fade state 1 and alpha 0.
- `work/captain-anim-tick`: repeat reproduction, then register only `_ANIM8_FFLOOP` at frame 7000. The unchanged script immediately resumes, relocates actors to the deck, clears the fade and reaches the next Timo/captain choice. This separates the timer failure from the earlier trialogue warnings.
- `work/captain-auto-anim`: automatic load restoration fixes the first freeze but exposes the later departure stop.
- `work/captain-jump-trace`: Rupert’s custom jump is present and executed; he remains on deck and returns to his routine, while SHIP_FINAL_03 waits at flag 6.
- `work/captain-jump-native`: native jump handling allows Rupert/Jorn to enter the water and the installed scripts advance through flags 8 and 11, reach the beach, clear the fade and return the camera. The private save ZIP is valid. This diagnostic run exited immediately after starting asynchronous saving and crashed during teardown; it is not a passing end-to-end result. The probe was corrected to wait for save finalization. No crashing candidate was installed in the playable app.

### Scope and remaining compatibility gaps

The main cutscene still emits legacy-memory/trialogue warnings: NPC visual impersonation, some face-animation callbacks, HUD/view fades and engine music hooks are incompletely supported. This milestone restores progression; it does not claim faithful multi-speaker presentation or complete LeGo memory emulation. The fixes do not persist the full legacy heap or arbitrary one-shot callbacks. The verified starting point is a save made before the captain event; loading mid-cutscene remains outside the tested contract.

The repeatable runner is `tests/run_archolos_captain.py`. It preserves the source hash, chooses the captain’s first answer and the Timo intervention branch through the native dialogue UI, waits for normal camera return on shore, and writes a private save. `--skip-dialogue` uses the game’s existing phrase-skip handler after a short interval; final acceptance also needs a normal-duration run. Original assets, saves, cursor behavior and upstream repositories remain untouched.

### Final full-duration replay

`work/captain-final-full` passes without skipping voice lines. It selects the normal Jorn responses, the captain’s first answer and the Timo intervention branch, follows the scripted jumps, and reaches the beach at frame 18900 with captain flag 11, no active camera or dialogue, fade state/alpha zero and trialogue inactive. A private “Captain sequence test” save finishes, has a valid ZIP and quest/script payloads, and the process exits normally. Source slot 11 is unchanged. `captain-complete.png` was visually inspected and shows the normal third-person beach view.

The post-save modal test initially received no keyboard events because the terminal-launched app had no Cocoa key window. No Save Game menu was reached; this was a replay limitation rather than evidence that game saving failed. The opt-in Tempest replay now falls back to a Tempest-owned window in the same process. It neither activates nor injects input into other apps. Its input path is disabled during normal play. The previously passing full-sequence binary’s SHA-256 is f443eac5ec1248848ecb48ff7a69cae04f39b4cd87061c9cfcfefdd181113fb6; the final build additionally includes this diagnostic-only targeting adjustment.

`work/captain-shore-save-local-window` passes with the final build: a fresh process loads the completed beach save, Escape opens MENU_GAME, Save Game accepts the typed name “ModalTest”, and a second private save is created. Both quest and player inventory payloads are byte-identical across this load/save round trip. ZIP integrity and source hashes pass. The earlier no-input runs (`captain-shore-save-reload`, `captain-shore-save-focused`) are excluded from acceptance.

Final regressions: `work/gate-captain-closed` blocks the stairs; `work/gate-captain-open` permits passage through the opened mover. Both pass, exit normally and preserve their source save. These moving correctness checks are not a controlled performance comparison. Release build, whitespace checks and runner syntax checks pass.

Installed in both bundles with SHA-256 `542b758243e8814d6925e54678509d0add889ca0fb3370f8d32683f49790aedf`. Previous playable executable is preserved at `work/Gothic2Notr-before-captain-fix`. Tempest local diagnostic checkpoint: `60b734c`. The user can load their existing pre-Jorn save (tested slot 11) using the unchanged launcher; no new game or recovery patch is needed for this fix. No private test save is copied into the playable directory.

Next: normal story progression from the beach and broader dialogue/quest/world-transition checks. Only the first captain answer and the intervention branch are covered by this full replay; the other narrative branch is not yet verified. Cursor work remains deferred. All commits are local; nothing is pushed.


## Dropped torches, Ezekiel on the beach, and Urs loot, 10 September 2026

The user reported a torch hanging in mid-air after drawing a weapon, Ezekiel leaving the beach instead of sitting beside a corpse, and missing loot from the corpse they called Ulf. Installed data identifies the corpse as Urs (`Q101_URS_BODY`). User slot 12 (“ch1”) is preserved as work/beach-player-source.sav with SHA-256 31d6a9e1dfb9714e7d5ae40ddb29153b0e8086a3b39cb982d132edb802d7f9f9. Diagnostics use private copies. The user closed the playable app when asked to avoid resource contention.

### Causes and changes

1. **Torch physics:** ItemTorchBurning already supplies the burned-torch mesh as its collision shape because its displayed visual is a ZEN composition. Item::setPhysicsEnable(const ProtoMesh*) ignored that argument and checked the displayed mesh instead; the displayed mesh is absent, so the dropped torch received no rigid body. Use the supplied mesh and its bounds. Drawing a weapon now drops a physically simulated torch; Bullet can deactivate the body normally after it settles. This is the shared mesh-based item-physics initializer, not an Archolos item-name patch.
2. **Ezekiel walking away:** the saved routine table correctly contains Pray/PART_13_DARRYL_DEAD, but the active state and navigation still target SHIP_EZEKIEL_02/FP_SHIP_IDLE_01. The native TELEPORTNPCTOWP adapter moved the NPC without cancelling ongoing travel. Npc::tick services travel before its AI queue, so the queued AI_ContinueRoutine waited for the old trip to finish. Clear the active navigation before the teleport, allowing the existing queued routine continuation to execute. The same compatibility adapter serves other callers. No NPC name, quest flag, routine change or forced sitting is added to production code.
3. **Urs loot:** installed ARCHOLOS_MAINLAND.ZEN specifies `ItMi_Pocket:1, itsc_lightheal:1, itmi_gold:13`. The earlier whitespace parser fix covers new games. Old saves retain the missing second and third entries because these world containers were already initialized. No new loot-parser change is needed. Recovery is explicitly limited to this corpse and Ezekiel; other omitted loot in already initialized old saves remains a limitation.

### Diagnosis evidence and excluded attempts

- work/beach-before: copied completed-departure save shows Ezekiel walking toward the ship despite his beach schedule; dropped torch is static at roughly hand height. This run timed out while the playable app was also running. Its trace establishes the symptoms, not a passing completed test or a performance result.
- work/beach-torch-resume: corrected bounds make the torch fall to the ground. A diagnostic direct resume at frame 240 changes Ezekiel to the correct state, but he is already far offshore and cannot return. This is an isolation experiment, not a complete fix.
- work/beach-clear-goto: clearing only navigation releases the queued routine change. A prototype attempted vm.call_function on TELEPORTNPCTOWP, which bypasses ZenKit’s native BL override and therefore did not replay the intended teleport. That misleading replay was removed. Neither prototype is accepted as end-to-end verification. Final proof uses the real departure script.

### Repeatable acceptance

- tests/run_archolos_captain.py now also asserts that Ezekiel has reached his beach waypoint and is sitting at departure completion. work/captain-beach-navigation replays the actual Jorn/captain/Timo choices and native script calls with phrase skipping enabled, reaches shore, restores camera/control and finalizes a valid private save. Ezekiel is seated at (-46922.80, -1903.86, -144937.81). The previous milestone separately verified full-duration voices; this run does not claim another full-duration replay.
- tests/run_archolos_beach.py supports observe (no NPC repair), repair (explicit private donor generation), and fresh (new-game loot audit). It draws a weapon through PlayerControl after attaching a torch, detects the new dropped item, checks its physical fall and ground contact, requires sustained stationary sitting, and waits for save finalization. Preparation attaches the torch directly; it does not replay selecting it in the inventory.
- work/beach-after-departure: fresh-process load of the naturally completed departure save; Ezekiel remains seated, and the torch falls about 80 cm and rests with its center about 16 cm above the ground. Valid save, normal exit, source hash unchanged.
- work/beach-player-repair: private copy of slot 12; resume only Ezekiel’s already selected Pray routine at its existing waypoint and restore one missing healing scroll and 13 gold in Urs. The pouch is not refilled, no quest variable is forced, and no item is inserted into the player’s inventory. Seated NPC, torch drop and loot checks pass; a private donor save finishes.

The one-time work/merge-beach-recovery.py validates the preserved source hash and entity identities, then copies only NPC 3’s data/visual and mobsi 312’s inventory from the donor into the original slot-12 ZIP. It renames the copy “Archolos beach recovery”. All other entries—including player state, inventory, quest/script state, other NPCs and world state—are byte-identical. Recovery SHA-256: 17646fc766baef446486fc3be034ef86ea03accc7cf522467d27df3400de209c. Audit: work/beach-recovery-audit.json.

- work/beach-recovery-reload: the exact merged recovery loads without a repair probe; all three corpse items are present, Ezekiel remains stationary and seated, the newly dropped torch lands, and another valid private save finalizes. Original and merged source hashes remain unchanged.
- The first fresh-game checker unnecessarily expected a save during the opening scene, when saving is intentionally blocked. It verified the loot but was excluded from full acceptance. The checker now treats fresh mode as a loot audit and exits without attempting an ineligible save. This diagnostic-only correction does not change normal saving or the production fixes.

- work/beach-fresh-loot-final passes: actual new game has pouch, Heal Light Wounds scroll and 13 gold through the normal inventory iterator, and exits cleanly. No original world archive is edited.

Final installation: both app bundles contain executable SHA-256 `0c0b88b54c3e315c95bbbbe6169d9517a5c75959dfbec600f0fec2f0a51e13c3`. The previous playable binary is preserved as work/Gothic2Notr-before-beach-fixes. Added “Archolos beach recovery” in slot 13; all twelve prior saves retain their hashes. The recovery comes from the user's slot 12 and was verified in a fresh process before delivery. The original slot 12 remains available.

Release build, whitespace and runner syntax checks pass. The only final rebuild after the successful recovery reload changes the opt-in fresh-test exit path; production behavior is unchanged. No cursor edits or upstream pushes. The two production changes are one line in TELEPORTNPCTOWP and use of the supplied collision-mesh bounds in Item. Production diff: outputs/archolos-beach-fixes.patch; evidence: outputs/archolos-beach-fixes.json.

Next: continue beach dialogue and the route toward Silbach from slot 13. Full campaign/world-transition coverage remains unverified. Already-floating torches in older saves are not retroactively simulated; new torch drops use the fix. The corpse recovery repairs only Urs, not every old mainland inventory. Other teleports that bypass TELEPORTNPCTOWP are outside this specific navigation fix.


## Trialogue speaker labels and unrelated startup subtitles, 10 September 2026

The user reported Fabio's badge during Jorn's lines in the forest village/cave discussion, plus “Go bother someone else” at the start of multiple cutscenes. A private copy of slot 14 reproduces both through the normal Fabio dialogue entry. The probe positions the three participants at the forest meeting waypoint; it does not replay the walk there or force the information function directly.

### Causes and implementation

- ZenKit's CutsceneLibrary::block_by_name used lower_bound without checking equality. Empty or absent IDs returned the next sorted block. The installed English OU.BIN's first block is DIA_11075_Hobo_JustInCase_03_01, with exactly “Go bother someone else.” TRIA_STARTEXT queues AI_OUTPUT(HERO, SELF, ""); the lookup turned that empty command into the unrelated subtitle. Require an exact match in the shared library. This repairs absent IDs everywhere, not just Archolos startup.
- LeGo emits all non-player trialogue lines through the original dialogue partner, using TRIA_NEXT plus native oCNpc memory swaps to impersonate the next speaker. OpenGothic neither performs those swaps nor implements AI_WAITTILLEND. Reading TRIA_LAST at playback is unreliable: callbacks can already have advanced or finished. The failed candidate work/trialog-forest-native demonstrates this and was never installed in the playable app. Its experimental wait-marker implementation was removed completely.
- The native adapter wraps TRIA_STARTEXT/TRIA_NEXT/TRIA_FINISH while executing their original script bodies. It records the selected invited NPC during queue construction. Each AI_Output stores its optional speaker in the existing serialized NPC-reference field otherwise unused by that action (victim). Dialogue playback snapshots that NPC's display name. Original dialogue partner, choice handling, output order and inventory identity remain intact. _TRIA_COPY becomes a no-op; no NPC identity or equipment is swapped. The adapter resets capture after finish, and player/ordinary lines retain their original speaker.
- The installed v1.2.11 bytecode was checked against the v1.2.7 reference for the relevant LeGo functions. No speaker is inferred from voice-number suffixes: the forest _05_10 line is explicitly selected as Jorn by the script and is tested as such.

### Evidence and scope

- Asset-free CutsceneLibrary.exact_lookup regression: empty library, empty key, missing keys before/between/after valid keys, and exact hits. Before: 3 failed assertions; after: 7/7 pass. Local ZenKit commit f3d4962. The existing proprietary fixture cases are not needed for this test.
- work/trialog-forest-before: both symptoms reproduced, scene completes and private save finalizes. Original slot 14 unchanged.
- work/trialog-forest-queued: corrected introductory/choice/final-line labels, no unwanted startup text or _TRIA_Copy errors, normal camera return, valid private save and unchanged source hash. Phrase skipping enabled.

- work/trialog-captain-final: actual Jorn/captain/Timo choices reach the beach with normal camera, Ezekiel seated, and a finalized valid private save. Jorn/Timo labels and absence of the spurious startup text pass; ordinary Jorn/player speech also retains its correct identity. Phrase skipping enabled. The initial checker incorrectly required the captain's display name to be “Captain”; the installed script names him Beckett. The corrected check requires his own displayed name to match the emitting actor, and all original progression/save/hash checks were rechecked against the same successful (exit-code-zero) run. No game changes were needed for that checker correction.

This milestone concerns subtitle identity and exact text lookup. Full NPC visual impersonation, face/gesture routing and AI_WAITTILLEND/camera choreography remain incomplete. No generic queue-wait implementation is shipped. Mid-cutscene reloads and the full legacy heap/callback persistence remain outside the tested contract. No cursor changes or upstream pushes.

### Starting a clean playthrough

New Game in the current build is the cleanest baseline for future campaign testing. Inventories already initialized by the old whitespace parser remain serialized in older saves, even if the player never opened those containers. Only affected item lists lose entries; it is not every chest. Worlds first initialized after the parser fix use the corrected parser. Slot-10 and slot-13 recoveries repair specific entities, not the entire world. Reinstallation is unnecessary, and all existing saves remain available.

Final acceptance: work/trialog-forest-final replays at normal voice durations (no phrase skipping), checks sixteen specific NPC lines plus player dialogue, selects the normal question/route choices, verifies capture reset for Jorn/Fabio, and finishes with camera=0, dialogue=0, chosen=1, tria=0 at frame 6900. Private save ZIP and source hash pass; game exits normally. Renderer-only snapshots were inspected for scene continuity; they exclude UI and are not pixel evidence of subtitle labels. Native app screenshot access timed out. Label assertions cover the exact name field used by the subtitle renderer.

Installed in both app bundles with SHA-256 615ae8183a8599f88ddec62f10de18943ddcb1239e1de5d24ca9240da0db888f. Previous playable binary preserved at work/Gothic2Notr-before-trialogue-fixes. All fifteen user saves retain their recorded hashes. Release build, whitespace checks, Python syntax checks and the focused lookup regression pass. The launcher remains unchanged. Evidence: outputs/archolos-trialogue-fix.json and outputs/archolos-trialogue-fix.patch (includes opt-in dialog tracing and the local ZenKit dependency patch).

Next: clean-playthrough progression toward Silbach, further dialogue and world-transition coverage. Old saves remain useful for targeted testing; no recovery save is produced for this batch.

## Save reset requested by the user, 10 September 2026

The user explicitly requested deletion of all saves. Deleted all 145 Archolos save files in this workspace: 15 playable slots and 130 benchmark, recovery, backup and source copies, totalling 3,400,164,289 bytes. A ZIP-header scan found no additional renamed save archives; a final scan found no remaining .sav files. Game data, installers, source, compiled apps, settings, logs and reports remain intact. No new save backup was retained.

Earlier preservation statements describe the historical tests, not the current filesystem. Regression runners still exist, but their old source checkpoints must now be regenerated from a new game before replay. Start New Game with the existing launcher. Deletion inventory: outputs/archolos-save-deletion.json.

## Current-upstream migration assessment, 10 September 2026

Fetched upstream origin/master at 711a69cb (75 commits beyond our v0.92 starting revision 2855fa51) in /Users/lu2/projects/OpenGothic. A git merge-tree trial against our functional milestone 2089182a leaves both checkouts unchanged. Git follows the game/ -> common/ source reorganization and automatically merges most touched engine code. Two unresolved paths are reported: common/world/objects/item.cpp (new bboxMesh/addDynamicObj API versus our older bbox()/dynamicObj spelling) and lib/Tempest (the newer submodule commit is not checked out locally and needs deliberate dependency migration). This is a source-level feasibility check, not a compiled or tested migration.

Recommendation: create a separate branch/check-out based on current upstream, carry the final production fixes and needed tests forward, adapt to current engine/dependency APIs, then rebuild and regenerate regression checkpoints from a fresh game. Retain the working v0.92 app as the fallback until the newer build passes. Review instrumentation separately instead of treating every diagnostic/revert commit as production work. ZenKit and Tempest local changes must be included deliberately. A GitHub fork remains optional; no fork, branch migration, upstream push or PR was created by this assessment.

## Recipe inventory callback context, 10 September 2026

The user started a new game and reported that reading Rupert's rat-skewer recipe produced the reading animation but no document, Cooking entry or stove choice. Their attached log contains null item memory accesses and `Cannot init $INSTANCE_HELP: parent class not found`.

**Cause:** Inventory::use invoked ON_STATE without binding the script global ITEM to the item being used. GameScript::invokeItem scoped only SELF. USECOOKINGRECIPE begins by reading ITEM.HP to construct its recipe metadata, so an unset or stale ITEM selects invalid metadata. The same callback helper handles ON_EQUIP and ON_UNEQUIP. Installed v1.2.11 bytecode confirms ITEM.HP at recipe entry and cooking talent index 18 in PC_ITFO_RATSTICK_CONDITION.

**Fix:** pass the actual Item to GameScript::invokeItem at all three callers and scope global ITEM using the existing ScopeVar helper. Restore the prior ITEM and SELF after the callback, including when recipe helpers change these globals internally. Preserve the previous equipment item before clearing its slot for ON_UNEQUIP. No recipe-name checks, forced learning, asset edits or new compatibility adapters exist in production code. The earlier native recipe-memory fixes remain necessary.

**Test gap corrected:** the original recipe probe initialized a separate recipe item, explicitly assigned ITEM and directly called USECOOKINGRECIPE. It still passed on the user's new save while normal inventory use failed. The replacement uses a real inventory item and Npc::useItem, the entry point used by InventoryMenu::onItemAction. It starts with ITEM=null and rereads with ITEM pointing to an unrelated old coin. Both reads must show the full document, retain it across rendered frames, learn the recipe, expose the actual stove condition, preserve unrelated talents/quests and restore script context. Rereading must not duplicate journal entries. The probe uses an existing recipe or adds one to its private inventory and resets only its learned flag for test setup; it does not replay Rupert's handover conversation or perform the actual cooking action. The stove condition runs with the stove's production/menu context, restored afterward.

**Evidence:** work/recipe-new-save-baseline passes the old shallow test. work/recipe-inventory-before and work/recipe-inventory-regression-before reproduce missing learning/document/stove availability through real inventory use, with the same null-instance/recipe errors. work/recipe-inventory-regression-after passes all replacement assertions using a private copy of the user's newly created slot 1. work/recipe-inventory-fresh-ready passes the same complete read/reread assertions in an actual new game with no loaded save.

The first fresh probes attempted inventory use while the opening movie was active; first reading succeeded but simulation was paused, preventing the character from becoming ready for a second read. work/recipe-inventory-fresh-after failed its reread check, and work/recipe-inventory-fresh-final was terminated while waiting. Neither is acceptance evidence. The final opt-in probe dismisses opening videos/chapter screens through Escape, waits for warm-up, and waits for standing before rereading. The opening-video diagnostic confirms this cause. Normal startup is unchanged.

The user's new save is preserved. Loading an existing save and reading the recipe again should suffice; another New Game is unnecessary. Other recipe types share the corrected callback, but this milestone directly tests only Rupert's cooking recipe. Next: resume clean-playthrough quest testing. Cursor work remains deferred; changes remain local.

Final acceptance: work/recipe-inventory-save-final repeats every assertion against the final executable using the new user save; normal exit and unchanged source hash. The final fresh-game run also exits normally. Release build, diff whitespace and runner syntax checks pass. Installed in both bundles with SHA-256 `3d40392f4fb4c9e3a3215daedc95141407cb44b11a130d543c67bc001c243912`; previous playable executable is work/Gothic2Notr-before-item-callback-fix. User slot 1 remains SHA-256 `19837af3d85d5275909369aea393bd9a29a71f2b8e9a54d06c224187a22e2a0e`. Evidence: outputs/archolos-item-callback-fix.json and outputs/archolos-item-callback-fix.patch. No user save or original game asset was edited.

## Chapter 2 city exploration checkpoint, 10 September 2026

The user requested a separate city save for walking around and testing performance, with no requirement for inventory or quest progression. Created playable slot 2, named `CITY EXPLORATION - Chapter 2`, at the Archolos city market near `PARTM2_MARKET_06`, at midday. Save SHA-256: `9cfb0aaa10444965e191cb5d918dd15e0dc1204623a79785cdc9a902a1d51674`. Existing story slot 1 retains SHA-256 `19837af3d85d5275909369aea393bd9a29a71f2b8e9a54d06c224187a22e2a0e`.

The opt-in OPENGOTHIC_CITY_PROBE reuses the installed story-helper Chapter 2 common preset at step 2 (city entered), including its preceding chapter-change calls, then places Marvin at the helper's city-market waypoint with native engine positioning. Installed symbols were verified. This intentionally produces a synthetic exploration checkpoint: the helper grants its normal incidental items/XP and sets quest flags; it does not establish a fully played or verified campaign history. Earlier skipped routine changes emit compatibility warnings, so use the original slot for story progression. No production gameplay behavior or original assets changed; the playable app remains recipe-fix binary `3d40392f4fb4c9e3a3215daedc95141407cb44b11a130d543c67bc001c243912`.

- work/city-exploration-create: initial fresh-game attempt retained an active conversation after skipping ahead; walking did not advance and the diagnostic guard aborted before saving. Not delivered.
- work/city-exploration-seeded: private copy of the user's existing ship checkpoint, normal player state cleared for relocation, same chapter helper. Walking input moves 287 cm, 13 nearby NPCs, chapter=2, entered=1, normal camera, no dialogue, valid finalized save. Creation measured 20.84 FPS but rendered at 3420x2146; the initial 1280x720 window-size log was not the final render size.
- work/city-exploration-reload: fresh process loads the generated save without invoking the preset again, walks another 499 cm, observes 13 nearby NPCs and no camera/dialogue lock, and finalizes another valid private save. 34.11 FPS at 1280x720. Source hash unchanged.
- work/city-exploration-playable-check: exact unchanged ArcholosFast executable loads the delivered checkpoint with only the existing profiler enabled, no city setup probe. Inspected rendered city image and normal exit. 180 measured frames average 35.89 FPS at 1280x720. This is a brief stationary market sample, not a city-wide performance guarantee. Source hash unchanged.

The repeatable generator/reload check is tests/run_archolos_city.py; --seed creates from a private copy and --save validates an existing city checkpoint. The diagnostic build includes the opt-in generator; no updated binary is needed by the user to load the save. Build, whitespace and runner syntax checks pass. Report: outputs/archolos-city-exploration.json; preview: outputs/archolos-city-exploration.png. All changes remain local. Next: user exploration of city performance; wider campaign and district access remain unverified.


## Native KmLib gameplay music, 11 September 2026

The installed KmLib DLL cannot execute in the native macOS process. Its suppressed music calls left Archolos's Ogg soundtrack unused. Data/KM_Music.mod already contains 42 tracks, and the installed GOTHIC.DAT defines MUSICTRACK/MUSICZONE metadata. Investigation: outputs/archolos-kmlib-assessment.md and the issue maintainer's https://github.com/Try/OpenGothic/issues/231#issuecomment-1411046739 .

### Change and root causes

- Register KmLib's custom track/zone classes with the existing opaque-instance API. Read filenames, fade/overlap durations and day/night/combat instance references from the installed VM, without hardcoded track IDs or world coordinates in production.
- WorldSound offers its normal selected zone and tags to the compatibility adapter before its legacy DirectMusic fallback. The adapter activates only when the KmLib music contract exists. MUSIC_OVERRIDETRACK/MUSIC_DISABLEOVERRIDE preserve the authoritative MUSIC_CURRENTOVERRIDE script global, which existing saves already serialize. Playback is applied during game ticks, not during asynchronous script/world loading.
- GameMusic plays the existing Ogg assets through Tempest. One background decoder owns its input bytes; it never retains a VFS reference or the shared Resources lock. Keep the old source playing until a valid requested track is decoded, discard obsolete results, and avoid retrying a failed unchanged request every frame. Only the active track, outgoing tails and one pending decode are buffered.
- Apply script-defined incoming/outgoing fades. DLL disassembly confirms positive loop offsets start the next copy at duration minus offset while the old tail finishes; reproduce that behavior using the existing source-time API. Zero-offset tracks restart at their end. Audio settings control gains, including all outgoing sources. Fades/overlaps continue while ordinary gameplay is paused.
- Found and fixed a shared Tempest duration overflow: samples*1000 overflowed before conversion to uint64_t. The 116.5-second 03.ogg was reported as 27.021 seconds, causing premature music loops. Promote before multiplying (Tempest local commit fb9fa22). This corrects all long Sound durations, including non-music callers.

### Verification and rejected candidates

- work/music-city-before: normal city load/walk/save passes, but the new music checker fails because no native track reaches playback.
- work/music-city-first: rejected integration candidate crashes because the two custom classes were not registered before opaque instance allocation. Registration is now explicit.
- work/music-city-registered: native source advances at nonzero gain; the trace reveals the long-track duration overflow. Not final acceptance.
- work/music-city-transitions: playback/settings/loop sequence succeeds, but the extended diagnostic accidentally starts another save after requesting exit. Rejected. Gate its completion by cityProbeSaved; ordinary game saving is unchanged.
- work/music-city-combat and work/music-city-async: complete intermediate scenario checks, normal save/exit. Synchronous decoding measured roughly 100–230 ms for ambient tracks and motivated the background decoder.
- work/music-city-final: final candidate passes city day/night changes, shared village selection, day/night combat variants, script-dispatched overrides and release, actual overlapping loop and tail completion, volume=0.2, mute/unmute, walking, normal camera/dialogue state, finalized save and unchanged source hash. Cancellation starts decoding a village track then returns to the current city track; the discarded track never plays and the current source does not restart. Background decode durations vary up to 343 ms; the measured cancellation dispatch on the main thread is 40.47 ms, so file-read/dispatch latency is not claimed to be zero. 53.65 FPS over the longer changing-scenario sample at 1280x720.
- work/music-override-reload: fresh process restores the saved BATTLE2_36 override as its first track, advances playback, walks and saves again; 59.30 FPS at 1280x720. Source hash unchanged.
- work/music-city-clean-reload: fresh process loads the corrected exploration copy and naturally selects CIT / AMONGTHEWALLS_06 without forced music setup, walks and saves; 59.55 FPS at 1280x720. The brief pre-change city baseline was 59.99 FPS; the longer scenario is not a controlled performance comparison with that short baseline.

The repeatable runner is tests/run_archolos_music.py (--full for the scenario, --expect-track for initial selection on reload). It uses private copies and checks native source progress/gain/length plus VM selections. Village and combat variants enter through the shared selection method; they do not replay a journey to Silbach or an actual fight. No OS loopback recording or subjective listening test is claimed. Profiling/probe code is opt-in and absent from normal launch behavior.

### City checkpoint and remaining scope

The old synthetic city preset retained the prologue MUSIC_CURRENTOVERRIDE=21913. The engine correctly honors this value; it must not globally erase legitimate scripted overrides. work/music-city-no-override.sav is a separate corrected copy named CITY EXPLORATION - Music, clearing only that global to zero. The ZIP payload comparison requires every other entry byte-identical except the header label. Original story slot 1 and city slot 2 remain unchanged. Audit: work/music-city-save-audit.json. This is still an exploration preset, not a validated played-through campaign.

This milestone implements gameplay soundtrack behavior, not the entire DLL. Menu music, platform services/achievements, other initializer hooks and the music-zone gameplay notification audit remain separate. Frame-driven fades/loops are not sample-accurate BASS parity; full-track PCM buffering can need replacement with streaming for much longer tracks. Save/reload restarts the correct selected track rather than persisting its precise audio position. Broader world transitions, cutscene choreography and campaign completion remain unverified. Cursor work remains deferred, and nothing is pushed upstream.

Final acceptance: work/music-fresh-recipe passes fresh-game initialization and actual recipe use/reread, with document persistence, journal/stove eligibility and preserved script context. Release build, diff whitespace and runner syntax checks pass. Installed gameplay executable SHA-256 `1c5fbb03505cca6c1e069d971763223842215673d9fbb5ffb1de87096cdba2d8`; previous playable build is work/Gothic2Notr-before-music. Profile candidate SHA-256 `b3b7533fe6c6e50f6761c59d704f0cdb43d960609801a9ad21c61712fb2bb4cf`. The bundles have different Info.plists and require different ad-hoc signatures; every byte before the Mach-O signature (offset 26316768) is identical, and the installed signature verifies. No untested executable code was introduced during installation.

Added playable slot 3, CITY EXPLORATION - Music, from the verified corrected copy (SHA-256 `10b11f7040aad1dfeba34c305f81aa9e2a0b81bb05d1d63f13674624e036ba55`). Slots 1 and 2 retain their original hashes. Use slot 3 to hear normal city music; slot 2 retains its old prologue override. The story save needs no migration or new game. Evidence: outputs/archolos-music-fix.json and outputs/archolos-music-fix.patch. Next: audit remaining KmLib initialization and music-zone gameplay notifications separately from audio playback.

## Remaining KmLib gameplay integration, 11 September 2026

Audited all nine exports of the installed 32-bit DLL and every installed-script KMLIB_GETPROCADDRESS caller. The extra ChromeTraceEvent lookup belongs to the optional developer profiler and is not exported by this release. GAMESERVICES_GETSTAT has only the achievement-threshold helper as a caller; there is no installed GameServices_SetStat wrapper or lookup. Full audit: outputs/archolos-kmlib-coverage.md. Disassembly and installed bytecode: work/kmlib-full-disasm.txt, work/kmlib-installed-bytecode.txt, work/kmlib-gameplay-bytecode.txt and work/kmlib-export-callers.txt.

### Changes

- Deliver ONZONEMUSICCHANGEDHOOK through the shared native zone selection method, supplying the original day/night/combat theme name through a virtual zString in EDX. Restore EDX on all exits. Execute the installed hook itself; it retains its location-entry and Water Circle conditions and its call to scaling logic. Notify while muted and during overrides; suppress identical theme repeats. Reset transient selection on save load and KMLIB_INITIALIZEALWAYS/world initialization. No production scene IDs, coordinates or quest outcomes are hardcoded.
- Native menu music uses the DLL's hardcoded 02.ogg and 7651 ms overlap, reusing the already tested file playback. Identify Archolos from its world/music assets. Resume menu music when a game session ends; settings continue to control volume/mute.
- Native GetStat/IncrementStat/UnlockAchievement adapters use the existing local Gothic.ini store, under KMLIB_STATS and KMLIB_ACHIEVEMENTS. Progress survives save rollback and application restarts, and unlocks are idempotent. Validate keys, clamp local counters to nonnegative int32 range and flush changes through existing settings persistence. No external service requests or platform popups.
- Handle KmLib initialization natively. Original InitializeAlways only looks up version symbols; INIT_ALWAYS still owns migrations and version assignments. Other initializer responsibilities include legacy engine/debug-console hooks, menu/save-format patches, Windows crash reporting/checksum checks and platform initialization. Native audio/UI/save handling supplies the applicable core behavior; unsupported legacy/platform features are documented rather than claimed as emulated.

### Evidence

- work/kmlib-before: probe-only baseline fails with CURRENTMUSICZONE empty, no Haven scene request, no Water Circle release and local counter=0.
- work/kmlib-native: first corrected city run passes natural city notification, muted region checks, suppression and repeat guards, night/combat suffix, EDX restoration, Water Circle region release and counter update; normal walking/save/exit, 58.99 FPS.
- work/kmlib-menu: rejected diagnostic run. Startup menu, city checks and saving worked; after session exit the probe used a readiness flag computed before tick and dereferenced the destroyed world. Crash report/ARM64 instruction identifies the diagnostic access. Sampling now rechecks world availability after tick for all probes. This candidate was never installed in ArcholosFast.
- work/kmlib-menu-final: final executable passes menu → city → save → menu, native playback clocks/gains, normal process exit, and counter restoration/increment 2→4. Short 1280x720 city sample 60.00 FPS.

- work/kmlib-full-final: final executable passes all 13 stages, including cancellation, day/night/village/combat selections, volume/mute, script-dispatched overrides/release, a complete overlap loop, native source progress and save completion. Local counter restores 4→6 and achievement unlock becomes 1. Longer 1280x720 city sample 57.32 FPS.

- work/kmlib-override-reload: fresh-process reload starts 36.ogg from the saved override, still delivers all region checks while that override remains active, restores/increments the counter 6→8 with unlock still 1, walks and saves again; 59.25 FPS.

- work/kmlib-fresh-recipe: actual new game and inventory read/reread pass, including document persistence, journal ingredients, stove eligibility and restored ITEM/SELF context; normal process exit.

### Limits

This is native support for the identified gameplay-facing KmLib contract, not full DLL/platform parity. Steam/GOG publishing, Discord presence, legacy debugging-console additions, Windows crash reporting and original menu/save metadata hooks remain unported. Existing save flags cannot reconstruct every achievement earned before local tracking existed. Local progress belongs to the launcher directory, not a cloud account.

The music-zone hook is adjacent LeGo integration, not a KmLib export. Its original-engine outdoor sky-controller fields are still unmapped, so UPDATESCALINGFACTOR's guard skips legacy automatic draw-distance adjustments. The installed 1.2.11 bytecode has real mutually exclusive branches, unlike the misleading 1.2.7 decompilation. The native selector retains its five-second cadence and existing overlapping-zone priority. Region tests capture scene requests rather than replaying their choreography; Water Circle release is exercised without a full faction quest. Full campaign/world-transition behavior, arbitrary callback/heap persistence and complete trialogue choreography remain separate work. Cursor remains unchanged.

Next: normal story progression toward Silbach and actual world-transition/save consistency tests. Continue local-only commits; no user save migration or new game required for zone/music behavior.

Final installation: Fast SHA-256 `9408e554791b0d1d9e78f96ec5cbc6d1773498eab011fa0705e6f697e6285ba5`; tested Profile SHA-256 `b7b12286af0eb9ffc6f69ac8c7aa87ac95796258af8c78e4b3f2661ce8be859b`. All executable bytes before Mach-O signature offset 26350992 match; Fast ad-hoc signature verifies. Prior executable is `work/Gothic2Notr-Fast-before-kmlib-rest`. All three user-save hashes remain unchanged. Release build, runner syntax and diff whitespace checks pass. Evidence: outputs/archolos-kmlib-rest-fix.json, outputs/archolos-kmlib-rest-fix.patch and outputs/archolos-kmlib-coverage.md.


## Menu audio overlap and appearance audit, 11 September 2026

User confirms music, combat changes and travelling outside the city appear smooth. This is useful gameplay evidence, not proof of a different-ZEN world transition. Steam/GOG/Discord integration is explicitly deferred.

### Cause and change

The previous menu test observed the intended Ogg clock/gain but missed competing sources. MainWindow::render also unconditionally played GAMESTART.WAV as a global sound effect. In the installed game that is the original Gothic Addon startup music, 43.142 seconds long. It bypasses GameMusic and its mute/volume/provider selection. Instrumented baseline work/menu-audio-before confirms this WAV starts alongside 02.ogg while the legacy music provider is disabled and there are no Ogg tails. The strengthened no-startup-WAV assertion fails against this baseline.

Move startup sound ownership to MenuRoot::processMusicTheme, after the custom soundtrack branch. Play the vanilla startup sound once per menu root only when an out-of-game legacy menu requests music. Rendering no longer triggers sound effects. Direct save/new-game launches no longer start an unrelated menu WAV. No game assets or cursor behavior changed.

### Verification

- work/menu-audio-final: --menu --kmlib --full passes all 13 music stages, cancellation, volume/mute, day/night/combat variants, overrides, complete overlapping loop, region hooks/local stats, city movement, saving and return to menu. Both menu samples play 02.ogg at advancing clocks/nonzero gain, with legacy=0, tails=0 and no GAMESTART.WAV emission. The varied run reports 47.44 FPS; window discovery/screenshot collection and uncontrolled desktop workload make this unsuitable as a controlled performance comparison.
- Strengthen tests/run_archolos_music.py --menu to reject the competing WAV and enabled legacy provider/tails, and verify the installed Archolos background and logo are loaded. Dedicated probe logging is opt-in; no subjective listening or OS audio loopback recording is claimed.
- Captured the actual private application window after returning to the menu: outputs/archolos-menu.png. It already shows the Archolos-specific knight background, The Chronicles of Myrtana / Archolos logo and Mod of the Year badge. Runtime background is 2048x2048 and the logo is menu_km_archolos.tga. Archive mounting reproduces the correct KM_Textures winner; no evidence warrants changing VFS priority. Gothic-style bitmap text, native layout, the Gothic II window title and OpenGothic version footer remain cosmetic differences. No menu artwork replacement was needed or shipped.
- Release build, runner syntax and diff whitespace pass. Three user save hashes remain unchanged. The installed executable matches tested code before its bundle-specific signature; both signatures verify.

### Log assessment and remaining scope

The supplied af2ed609 log contains three launches and normal menu exits, no crash/stacktrace. Outstanding leads: malformed/missing mesh and ambient sound names, archive section overflow/MDS parser warnings, null OCNPC.FOCUS_VOB access, unsupported LOG_MOVETOTOP (journal ordering), mdl_applyrandomani/freq (idle variation), and a suppressed Windows MessageBox call whose message is not recorded. Do not label the unknown message harmless without capturing its contents. Repeated Fane state messages alone do not demonstrate a stuck scene. The user did not report a new progression blocker in this run.

Old Windows crash/LAA checks and original-engine debugger hooks are not needed to run the native 64-bit macOS engine. They are not universally obsolete for the Windows original; recreating them is unnecessary here unless a concrete debugging need emerges. Original automatic draw-distance scaling remains a possible future native optimization if measured performance warrants it. Menu presentation is optional polish. Broader story progression, save/world-transition consistency, cutscene choreography and callback persistence remain the substantive compatibility priorities.

Installed Fast SHA-256 `1bc44a88eb2a145eac6f3806ca91929398bb7912d5dc0ab0287fc75be56bbfc8`; tested Profile SHA-256 `f416ffba48516b1720a614e876031d66831dd5d9b04b455ed7a40754d72c3acd`; matching executable bytes before signature offset 26351168. Previous Fast executable: work/Gothic2Notr-Fast-before-menu-audio. Evidence: outputs/archolos-menu-audio-fix.json and .patch. Local-only commit; no push.


## Hidden Windows message diagnosis, 11 September 2026

Captured the installed 1.2.11 message without displaying a blocking dialog or changing the emulated call's behavior:

> Information: This should never happen! If it does anyway, please report to Lehona on WorldOfGothic.

Call chain: DirectMemory::tick's load recovery → INIT_QUESTSEVENTSMANAGER → FF_APPLYONCEEXTGT → FF_APPLYEXTGT → _FF_CREATE → NEW → MEM_INFOBOX → MEM_MESSAGEBOX → unsupported Windows MessageBox call. This is LeGo's missing-handle-table diagnostic, not a platform-service request. No upstream report was sent.

Installed NEW bytecode checks HANDLESPOINTER and emits this notice when zero, then creates HANDLESPOINTER/HANDLESINSTANCE hash tables and HANDLESWRAPPED before continuing with allocation. Its foreach table is also created lazily. The installed pointer globals are mutable script constants initially zero; GameScript::saveSym/loadVar intentionally exclude constants. The virtual heap is not serialized. Our existing load recovery reconstructs the recurring quest and animation callbacks, which triggers this fallback. NEXTHANDLE is an ordinary saved variable; recreating the tables does not restore old heap objects or arbitrary pending callbacks.

Evidence:

- work/message-before: unchanged Profile build reproduces the opaque unsupported-message line while city load/walk/save passes.
- work/message-captured: temporary demangling-boundary instrumentation reads MEM_MESSAGEBOX.TXT/CAPTION and prints the live VM stack; exact text/caller captured, normal city run/save completes.
- work/message-recovery-reload: fresh process loads the preceding private output save. Same diagnostic reproduces, followed by nonzero pointer/instance/wrapped/foreach tables and NUMHANDLES()==2 after quest/animation registration. City load/walk/save passes, 12 nearby NPCs, no camera/dialogue lock. This verifies the fallback and reconstruction in this scenario, not persistence of arbitrary original heap objects.
- Installed pointer-symbol flags and NEW bytecode inspected directly; reference decompilation alone is insufficient. Captured trace: outputs/archolos-hidden-message-trace.txt. Repeatable transient instrumentation: outputs/archolos-hidden-message-probe.patch, applied to 60b52d73; run the existing tests/run_archolos_city.py on private saves. Probe observations are read-only.

No gameplay patch is shipped for this diagnosis. Initializing empty tables earlier would remove the warning without providing missing persistence. Treat full LeGo object/callback save restoration and actual world-transition consistency as the substantive follow-up, with pending one-shot callbacks and live script-object references as explicit test cases. Current ordinary quest/inventory save tests do not establish that broader guarantee.

Temporary DEBUG-message code removed; Profile restored byte-for-byte to its signed menu-audio build, playable Fast executable never replaced, and all three current user saves retain their hashes. Build directory rebuilt from clean source to remove instrumentation. No new launch command or New Game is required. The unsupported-message line can still occur in the current playable build; its cause is now identified rather than silently suppressed.
