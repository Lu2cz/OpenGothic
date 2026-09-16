# Archolos development environment

This is the local setup reference, not the backlog. Read AGENTS.md and the assigned
issue first. Commands assume `rtk` is installed. Roadmap: https://github.com/Lu2cz/OpenGothic/issues/1.

## Source and dependencies

Working checkout: `/Users/lu2/projects/OpenGothic-v092`, branch
`archolos/performance-v092`, based on v0.92. A separate newer-upstream checkout
exists at `/Users/lu2/projects/OpenGothic`; do not confuse it with the playable source.
`origin` points to Lu2cz/OpenGothic; `upstream` points to Try/OpenGothic.

Initial gameplay baseline: `3e1d893c1e48b110c0b69be1c9a1ecfe113a2cae`.
The baseline setup tag `archolos-baseline-2026-09-11` adds documentation and fork
submodule URLs without changing engine code. Keep that tag as the rollback reference.

Pinned modified dependencies are published in Lu2cz/Tempest (`fb9fa22d2e66fd350ca93cbef4a9cfca05ed5669`)
and Lu2cz/ZenKit (`6fa71bfbd8f3c349be59bbc485f3e7bac7fa3961`). Do not update submodules
with `--remote`; ordinary recursive initialization retrieves the recorded revisions.

Fresh source checkout (choose an unused destination):

```sh
rtk proxy git clone --recurse-submodules --branch archolos/performance-v092 https://github.com/Lu2cz/OpenGothic.git /Users/lu2/projects/OpenGothic-fresh
```

Build with Xcode command-line tools, CMake and glslang available. This Mac already
has these dependencies. Use a separate build directory per worktree:

```sh
rtk proxy cmake -S /Users/lu2/projects/OpenGothic-fresh -B /Users/lu2/projects/OpenGothic-fresh/build -DCMAKE_BUILD_TYPE=Release -DTEMPEST_BUILD_METAL=ON -DTEMPEST_BUILD_VULKAN=OFF
rtk proxy cmake --build /Users/lu2/projects/OpenGothic-fresh/build --target Gothic2Notr --parallel 4
```

Output: `build/opengothic/Gothic2Notr`. The existing build cache uses the preserved
workspace symlink `work/OpenGothic-v092`; do not copy that cache into new worktrees.
Build verification is recorded in baseline issue #2; a source build alone is not
a claim that all gameplay regressions passed.

## Local assets, profiles and evidence

Workspace base:
`/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues`

Paths relative to that base:

| Path | Purpose |
|---|---|
| `work/archolos-game` | Installed Archolos 1.2.11 with Polish voice pack |
| `work/playable` | User's persistent saves and Gothic.ini; preserve |
| `work/ArcholosFast.app` | User's installed playable app |
| `work/ArcholosProfile.app` | Shared diagnostic app; coordinate replacements |
| `work/tcom-decompilation/Source` | Reference scripts 1.2.7; verify against installed bytecode |
| `work/python-deps` | Local Python ZenKit bindings for archive/script inspection |
| `outputs` | Historical reports and local evidence indexes |
| `work/<unique-test-name>` | Independent run data, logs, settings and private saves |

## Launch for the user

Always supply this one command; it opens the menu and permits normal saving/loading:

```sh
rtk proxy /bin/zsh "/Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/outputs/Play-Archolos.command"
```

The launcher uses the installed Fast app, persistent `work/playable`, profiling off,
and `-game:TheChroniclesOfMyrtana.ini -window -rt 0 -gi 0 -bl 0`.
`-bl 0` is the verified workaround for the original severe ship slowdown on this M2.
Changing current working directory changes save/config location; do not launch a
private test from `work/playable`.

## Test and installation rules

Tests live in `tests/run_archolos_*.py`; run the relevant runner's `--help` first.
They take explicit executable, game, output and (where needed) source-save paths.
Use a new output directory and private settings every run. Profile probes are opt-in;
never leave OPENGOTHIC_* test variables in a normal player launch.
Runners clone input saves with copy-on-write on APFS, retaining independent writes.
For checkpoint retention, retired worktrees and archived evidence recovery, follow
[the storage policy](storage-retention.md). Check its archive index when an old
evidence path is no longer present; do not assume the checkpoint was lost.

Use the private input inventory and procedures in
[opening-checkpoints.md](opening-checkpoints.md) for gate, lock, stash, captain,
beach and forest regressions. Historical reset-era commands are not current inputs.
The city presets are synthetic exploration states, not campaign-validation evidence.
The user confirmed inventory/quest reload, cooking, recipe/XP display and menu saves;
these reports supplement, not replace, reproducible checks.

Persistence runners: `run_archolos_city.py --persistence seed`, then `reload`, then
`completed`, chaining each output's `save_slot_2.sav` as the next source. `--reject
fingerprint`, `--reject truncated` and `--save-failure` cover rejection/data protection.
Old saves without `game/compatibility` use legacy recovery; the next save stores
current state but cannot recreate state already lost. Mapping changes must review
snapshot ABI/version and identical-script requirements.

Lockpicking introduced compatibility v2; boss UI now writes v4 and reads older
supported snapshots. Older binaries reject newer saves; keep pre-upgrade saves
with rollback binaries. See
[lockpick regression coverage](lockpick-regression.md) for ownership, migration
and exact verification boundaries.

NPC queue synchronization writes native world saves version 56 and reads version 55.
Keep rollback saves paired with their executable: older binaries cannot read the new
queue layout safely. See [AI wait coverage and supported save boundaries](ai-waittillend.md).

Before replacing either shared app, ensure no test/build/game run conflicts; preserve
the prior playable executable, test the candidate privately, and verify the bundle's
ad-hoc signature. Record candidate source/dependency revisions, executable hashes,
commands/results and rollback path in the issue. Bundle signing changes executable
bytes: compare code before its signature or use appropriate build provenance.
Do not install merely because compilation succeeded.

Historical Fast installation (14 September 2026, superseded by issue #8 below):
- Runtime source: `d5e85407b424f2e72f57cb62540d202f844ab75b`.
- Merge: `c1b35645cd05463a24de2db185399eb841e09279`; main checkout fast-forwarded.
- Pre-sign executable: `5ba4a86ae645dffa63a1c5560cc03f7090a7c3ab4963445ec2793abe35594d7d`.
- Installed Fast: `45f98622183d06deb28ae91431671630b448ec9194c8c6ca24bd7ad4f9bfadec`.
- Ad-hoc deep/strict signature verification passes. Profile is unchanged.
- Paired prior Fast/Profile executables, three saves, Gothic.ini and launcher:
  `work/issue7-install-rollback-20260914.FI5mKf`. Save CRCs pass; slot 1 is v2,
  slots 2/3 lack the compatibility entry. Do not pair rollback binaries with v4 saves.
- Private actual pause-menu save and bounded menu/load/resave/exit checks pass;
  see [boss UI verification boundaries](boss-ui-regression.md) for runner qualifications.
- Local provenance: `outputs/issue7-installation-20260914.json`. Player saves/config,
  launcher, Profile, assets and dependency pins were preserved.

Current Fast installation (15 September 2026, issue #8 / PR #24):
- Runtime merge: `33fd4acde6b582e71162e323c686c9f840348125`.
- The issue-worktree development build was
  `d8150f240a1426e22a9653c11d08d83193ddea4c3600d0d167bdc369fd838ee5`;
  the fresh Release deployment build was
  `758684ea8493c0ea9d6450647b56605eb19b1bb119661a466fde9a153bedb742`.
- Ad-hoc signing changed the executable to
  `4a7193bcec3e96ed85de4d0b5a5e21c93a97713a1743d5467ae9a9a5d5b31a76`.
  The signed staged and installed Fast binaries are byte-identical.
- Ad-hoc deep/strict signature verification passes. Profile, launcher, assets,
  Gothic.ini and all persistent saves are unchanged.
- Paired rollback executable, Profile, launcher, config and saves:
  `work/issue8-install-rollback-20260915.UNKrjB`.
- Signed-stage seed/reload verifies exact active-buff snapshot restoration, opaque
  and fading UI, and expiry cleanup; the installed private repeat smoke passes.
  See [timed buff regression](timed-buff-regression.md) and
  `outputs/issue8-installation-20260915.json`.

Baseline installed hashes (11 September 2026; later installations belong in issues):
- Fast: `a65b95d1ed0221d53632f94da1d97c55ba4e008d2947a5304f0efe48d0932ad5`
- Profile: `6ab21c0e1e13a0c84c567db5ba14bc16ae854fc846bea2af0172cb479752ce13`
- Rollback executable: `work/Gothic2Notr-Fast-before-persistence`.

## New tasks

Create one task for the assigned issue, point it at this repository or an isolated
worktree based on the working branch, and include the issue URL plus AGENTS.md path.
Only create the next task being started, not one running task for every backlog item.
Record task ID, branch and worktree in the issue. Keep shared builds/tests/installations
serialized; use this project's roadmap task for cross-issue coordination.
