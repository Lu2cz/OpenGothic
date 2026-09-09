# Local Archolos development log

Update this file at each verified milestone with the symptom, cause or uncertainty, change, verification, and remaining work. Commit meaningful checkpoints locally; never push or submit upstream without the user's explicit instruction.

Working branch: `archolos/performance-v092`, based on OpenGothic v0.92. The newer upstream checkout is separate. Game scripts are installed v1.2.11; the reference decompilation is v1.2.7 and must not be treated as exact source.

Workspace and user-facing evidence: `/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues`. Launcher and reports are under `outputs`; proprietary game data, saves, benchmark runs, and extraction tools are under `work` and are not committed.

## Current investigation: ship bars

Status: the normal-forward-input failure is fixed and the tested build is installed. Relevant object `SHIP_TRAPDOOR`, mover id 1795, visual `OC_LOB_GATE_BIG.3DS`. World data enables dynamic collision; compiled mesh has four collidable triangles. The checks below establish blocked passage while closed and successful passage after opening. Dialogue exits are next.

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
