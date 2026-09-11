# Archolos: LeGo/Ikarus coverage and latest play-log audit

Historical audit snapshot; current work/status is in [the roadmap](https://github.com/Lu2cz/OpenGothic/issues/1).

11 September 2026. Local engine commit 3e1d893c. Read-only investigation; no engine changes, rebuild, game launch or save modification.

## What the 60 FPS means

The persistence patch did not establish a new performance improvement. Historical evidence already shows 59.989380 FPS in work/music-city-before, before the native music implementation and well before persistence. The final persistence replay measured 59.873550 FPS, while the consumed-callback reload measured 59.549905 FPS.

The proven earlier optimization was the existing `-bl 0` option. A controlled ship comparison with the same build, save and 1280×720 drawable changed 11.98 FPS / 84.29 ms median GPU duration to 59.98 FPS / 14.49 ms. This selects the alternative resource-binding path. It establishes a bindless-path bottleneck for that scene on this M2, without identifying its internal driver/shader cause or demonstrating that every Mac needs the setting.

The first city checks were slower: 34.111172 FPS in work/city-exploration-reload, and 35.890115 FPS in a short stationary playable-app sample. The latter recorded 20.35 ms/frame in rendering command encoding. These were different runs, routes/views and sampling intervals; no controlled old/new replay isolates a later change as their cause. Machine load, warmed resources and exact view could contribute, but none is established as the explanation. The original creation run also actually rendered at 3420×2146 despite its initial 1280×720 window log. That explains why initial window-size reports alone are insufficient, not the entire 36→60 difference.

Most later samples use a small 1280×720 test window, a warmup, a short walk and then a settled view. The regular launcher does not force that size. “About 60 FPS in the tested city scene” is justified; “the persistence fix accelerated the whole city” or “60 FPS everywhere” is not. The new user log contains no FPS measurements.

## Latest play log

Four launches end with LEAVE_GAME; the first three show no world load. The fourth enters the game and exits normally. No fatal crash/stack trace appears, but there are real caught script errors:

- Nine `illegal access of unbound member OCNPC.FOCUS_VOB` exceptions, plus one initialization access without an instance. This is missing access to the NPC's focused/targeted object, not generic evidence of damaged geometry. The installed G_PICKLOCK function has three direct reads, while other callers include telekinesis, boss UI, finishing moves and party-focus/diving hooks. Their presence and the nearby PICKLOCK_UNLOCK warning make ordinary lockpicking a strong lead. The log alone does not identify the actual caller; capture a stack before changing behavior.
- One MEM_MESSAGEBOX unsupported external and no `[COMPATIBILITY] Restored...` line. Read-only ZIP inspection shows all three current playable saves still lack game/compatibility. The installed Fast hash matches the persistence release. This is consistent with loading an old-format save and its documented recovery warning, rather than evidence that snapshot restoration regressed. Save in the updated app and load that new save to exercise the new path; old missing state cannot be recovered retrospectively.
- AI_SND_PLAY is reached and unsupported: a standard Gothic queued sound external, separate from LeGo itself. Installed callers include giving inventory items, Ruud dialogues and a later quest. The single warning is not a call count because the generic unimplemented-external logger reports each name only once per process.
- LOG_MOVETOTOP is a deliberately stubbed Archolos helper, called by B_LOGENTRY. Journal topic ordering remains missing; this is distinct from the scrolling and recipe-content fixes.
- Missing insect/lockpick SFX and malformed mesh filenames are asset/loading leads. Fifty-three one-byte animation-section overflow notices and an unexpected MDS event need comparison with the actual assets before declaring them harmless or patching the parser. Their count is not the number of distinct broken animations.
- Thirty-nine Fane state messages do not by themselves prove an endless dialogue. The log ends normally. OpenAL also reports a device-type query error; that line alone does not establish playback failure.

## What Ikarus and LeGo do

Ikarus gives Daedalus scripts low-level operations that ordinary Gothic scripting lacks: memory access, pointer/instance conversions, arrays and strings, reflection and indirect function calls, control-flow extensions, access to engine objects, and calls into engine/x86 code. LeGo builds higher-level systems on that foundation: object/handle management, timers and frame callbacks, save integration, queued AI functions, multi-person conversations, events, animation/interpolation, custom views/bars/sprites, buffs and other UI/interaction features.

OpenGothic executes much of the original script code. Native compatibility is needed at the places where those scripts assume original Gothic memory, C++ objects, execution hooks or Windows functions. A missing primitive can affect many otherwise ordinary script functions. Implementing only named library functions cannot cover direct `_@` / `_^` access to engine fields; issue #231 explicitly discusses this limitation and the virtual-memory/proxy approach we are extending.

The installed game reports Ikarus 10202 and LeGo 2.7.0. Installed INIT_GLOBAL bytecode enables 19 modules: PrintS, HookEngine, AI_Function, Trialoge, Dialoggestures, FrameFunctions, Random, Saves, PermMem, Anim8, View, Interface, Bars, Timer, EventHandler, Sprite, ConsoleCommands, Buffs and Render. It excludes GameState, Buttons, Names, Cursor, Draw3D, Focusnames and Bloodsplats from LeGo_All. These are initialization requests, not proof that all 19 work. Current upstream LeGo dev is 2.9.0 and is used only for library context; installed bytecode is the version-specific evidence.

## Coverage assessment

| Area | Evidence/status | Remaining boundary |
|---|---|---|
| Allocated memory, script addresses, arrays, strings, transient instances | Shared Mem32/DirectMemory implementation; recipe and persistence tests exercise useful paths | Arbitrary engine object layouts and every string/reflection edge case are not mapped/tested |
| Handles, timer/frame callbacks, save/load | Pending and consumed callbacks, linked/resized memory, strings, native NPC-array bindings and deleted-item references survive process restarts | Exact script/mapping version required; old losses unrecoverable; unusual native identities and world transitions unverified |
| Queued AI functions and trialogue | Common argument forms and local intro/captain/forest cases work; speaker identity is adapted | NSII queued-call form is still unfinished; full choreography/visual swaps and AI_WAITTILLEND synchronization remain incomplete |
| Engine hooks and NPC/world fields | Specific native equivalents exist for teleports, animation callbacks, music-zone events and spell lockpicking | HookEngineI still logs rather than installs arbitrary hooks; MEM_ReplaceFunc is mostly a logging stub. Focus, world searches, inventory-slot helpers and other raw engine fields remain partial |
| Custom UI, bars, sprites, render, buffs | Native recipe documents and XP notices work | Generic view open/back-texture/texture-loading paths contain logging placeholders; _RENDER_INIT is explicitly unimplemented; tickUi reads text without rendering it. Complete boss bars, buff display and crafting overlays are not established |
| Settings, keyboard, quivers and console | Several INI reads/existence checks implemented | MEM_SetGothOpt logs instead of writing; MEM_SETKEYS and INIT_QUIVERS_ALWAYS are no-ops; script-added console registration lacks behavior |
| KmLib integration | Native music, zone gameplay hook and local achievement counters verified separately | This does not supply the missing LeGo APIs. Platform integration remains intentionally deferred |

LeGo's Render/View features are relevant to gameplay UI. They should not be confused with the legacy Windows renderer/debug adjustments previously deferred during KmLib work. Native OpenGothic can draw the world correctly while failing to draw a script-created boss bar.

Static installed-bytecode scan finds 83 calls to HookEngineF, across initialization for cutscenes, inventory, boss/crafting UI, quivers, underwater gathering, lockpicks, music and other systems. This is a list of places to classify against native equivalents, not 83 confirmed failures. It also finds 1,577 AI_WAITTILLEND call sites. That is a standard Gothic external which has no registered implementation in this checkout; its wide use makes shared NPC synchronization a meaningful follow-up. It does not mean 1,577 broken scenes. FINAL is stubbed but has no direct references in this scan, illustrating why counting TODOs is a poor priority system. Scan stops before the final executable symbol and does not resolve all indirect calls or runtime reachability.

Conclusion: the foundation is much stronger and several important gameplay paths are verified, but complete LeGo/Ikarus support is not under control yet. No defensible completion percentage follows from the issue checklist, module count or number of stubs. The GitHub checklist is not a live inventory of this local branch: for example, current native string/INI helpers and our snapshot work exceed some of its unchecked entries, while broad engine-memory coverage remains genuinely incomplete.

## Recommended next work

1. Capture the runtime caller of FOCUS_VOB and reproduce ordinary lockpicking, then implement the shared native-NPC/target-object bridge needed by the observed path. Test no target, NPC/item/interactive targets, target removal and reload. Avoid a blanket zero return that merely silences the exception. Retain the existing spell-lockpick regression.
2. Exercise an actual transition between distinct ZEN worlds in private saves: live quest/animation callbacks before leaving, native references after arrival, save/restart on each side and return to the original world. Moving outside the city within the same mainland ZEN is not this test.
3. Complete common queued-AI synchronization using a reproducible two-NPC scene, including AI_WAITTILLEND and needed AI_Function argument forms. This is a shared way to improve later cutscenes.
4. Build one real gameplay test for boss/buff UI, then fill the common View/Render paths required by it. Treat native world rendering and mod UI rendering separately.
5. Smaller concrete fixes can follow: LOG_MOVETOTOP, AI_SND_PLAY and verified asset-name/loading defects. Continue ordinary story progression alongside these targeted tests; a save just before any observed problem is especially useful.

Engine files and playable saves were left unchanged during this audit. Machine-readable evidence: archolos-lego-ikarus-audit.json.

## Sources

- [OpenGothic issue #231](https://github.com/Try/OpenGothic/issues/231), main description and all 20 currently returned comments.
- [Maintainer explanation of virtual memory/proxies](https://github.com/Try/OpenGothic/issues/231#issuecomment-1366101174).
- [Preference for low-level address compatibility](https://github.com/Try/OpenGothic/issues/231#issuecomment-1366799737).
- [Later examples of native-object reinterpretation](https://github.com/Try/OpenGothic/issues/231#issuecomment-3524306793).
- [LeGo source/module definitions](https://github.com/Lehona/LeGo/blob/dev/LeGo.d); current upstream version differs from installed 2.7.0.
- [Ikarus description](https://github.com/Lehona/Ikarus).
- Local source: /Users/lu2/projects/OpenGothic-v092/game/game/compatibility/directmemory.cpp; mem32.cpp; cpu32.cpp; game/gothic.cpp; ARCHOLOS_DEVELOPMENT.md.
- Installed Data/KM_ScriptsEN.mod GOTHIC.DAT, read with ZenKit; older tcom-decompilation used only for navigation, with key references checked in installed bytecode.
