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
For checkpoint retention and retired worktrees, follow
[the storage policy](storage-retention.md). Use its selected-checkpoint inventory
when an old evidence path is absent; historical runs and archives are retired.

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

Historical installation/verification records are in the assigned GitHub issues and
`outputs/issue*-installation-*.json`. Superseded apps, rollback generations and raw
runs were retired under [storage-retention.md](storage-retention.md); those records
do not guarantee their old evidence paths still exist. Use
[retained-checkpoints.md](retained-checkpoints.md) for active inputs and exceptions.

Current Fast installation (26 September 2026, issue #37 / PR #41):
- Runtime source: `f395d778ac91e40991b5f62f322920ec0544c781`;
  merge: `bab501a881952de87d39695185e0e11e56c9a4b6`. Their source trees match.
- Release/Metal executable before signing:
  `d002a206eb04d6d9d90bb0b03f51d57b15474d9ed75eb595f3e8618c89df6b63`.
- Signed candidate and installed Fast are byte-identical:
  `e7a1752aec3984a4168ae72e864a0c9b9963057e1f001455ade10e0cdd566731`.
  Deep/strict ad-hoc signature verification passes; dependency pins unchanged.
- Native Kurt fishing replay verifies overlay removal on both actors, five fish,
  chosen strength/dexterity rewards, rod removal, preserved torch, camera/control
  return and save/restart. The original affected save and current player slot 12
  recover with byte-identical inventory and journal data. Signed candidate repeats
  scene/recovery; installed private recovery/movement/resave passes.
- Fresh-game repeated speed-potion use and natural expiry pass. Crafting feedback,
  usable alchemy and restart pass. A saved-fixture BUFFLIST_VIEWS bounds error
  reproduces on the prior installed build; that run is not claimed as passing.
- Player saves/config, launcher, Profile and assets are preserved. Paired rollback:
  `work/issue37-deployment-20260926-v1/rollback`.
- Exact provenance/protected-file and evidence hashes:
  `outputs/issue37-installation-20260926.json`. See [fishing coverage](fishing-pose.md).

## New tasks

Create one task for the assigned issue, point it at this repository or an isolated
worktree based on the working branch, and include the issue URL plus AGENTS.md path.
Only create the next task being started, not one running task for every backlog item.
Record task ID, branch and worktree in the issue. Keep shared builds/tests/installations
serialized; use this project's roadmap task for cross-issue coordination.
