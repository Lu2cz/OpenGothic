# Local evidence and storage retention

Keep enough evidence to reproduce and review fixes without retaining every full
test copy forever. This policy is authorized by the project owner (16 September
2026); it applies to this project's generated data, not unrelated device caches.

## Keep available

- Player saves/config in `work/playable`, game assets/installers, Fast and Profile.
- Current app plus a recent known-good executable **paired with its saves**.
- Inputs needed by unresolved issues and reusable regression checkpoints.
- A representative failing run and final passing evidence for each completed fix:
  commands, source/dependency and executable hashes, logs, relevant lossless images.
- Small manifests and provenance in `outputs`; public summaries in GitHub issues.

Use `tests/archolos_test_data.py::copy_save` in runners. On macOS this uses
`cp -c -p` (APFS copy-on-write), with ordinary independent copies as a fallback.
Never hard-link writable saves. A test must be able to overwrite or edit its copy
without changing its source. Source-hash assertions remain required.

## After implementation and testing finish

The owner reaffirmed automatic worktree retirement on 26 September 2026. Treat
retirement as part of finishing the task, not an optional later cleanup. An issue
waiting only for player confirmation does not need its full checkout/build retained:
keep its protected inputs, final/baseline evidence and paired rollback outside the
worktree, then remove it. Keep the issue open if acceptance remains pending.

Retain a worktree only for active implementation/testing or a concrete dependency
that cannot yet be preserved elsewhere; record that reason and the release condition
in the issue. No new permission is needed for cleanup within this policy.

Source belongs in published commits; build caches and object files are reproducible.
Preserve exact binaries needed to interpret evidence or rerun an unresolved check,
build settings and dependency pins, using an existing verified retained copy when
available. A small executable may be worth retaining; the whole build directory is
not. Check cross-worktree symlinks/path dependencies before retirement.

Follow this checklist:

1. Identify the final passing run and baseline failure; record their exact paths
   and hashes in the issue. Do not infer success from directory names.
2. Check references in open issues, docs and local provenance before retiring
   intermediates. Unknown data stays. Keep archives private: no saves/assets in Git.
3. Deduplicate identical retained saves with independent APFS clones when possible.
   Verify both hashes, preserve file metadata, and atomically replace only the test
   copy. Do not rewrite player saves or the current paired rollback to save space.
4. Compress cold runs losslessly; store one compressed save per original SHA-256
   plus a mapping from original paths. Keep logs/config together for reproduction.
   Verify every decompressed file hash and test restoration before deleting inputs.
   Already-compressed PNGs need not be converted to lossy images.
5. Retire completed worktrees through `git worktree remove` only after checking
   root/submodule changes, untracked and ignored files, and published commits.
   Preserve non-reproducible work and any required binaries/patches/provenance.
   Generated build directories can be regenerated. Keep the main checkout usable.
   Never manually remove `.git/worktrees` or shared submodule repositories.
6. Record logical bytes removed and measured filesystem free-space change
   separately: APFS clones/snapshots and concurrent activity affect recovery.

Do not clean during a game, build or agent run that owns the affected paths.
No age-based blanket deletion, automatic archive expiry or game-data pruning.

## Existing archives and recovery

Workspace base: the path in [environment.md](environment.md). Paths below are
relative to that workspace, **not** the source checkout.

The 16 September 2026 maintenance stores cold evidence under
`work/evidence-archive/20260916`. `index.json` maps original paths to compressed
save objects and per-issue tar archives, with original SHA-256 and metadata.
Referenced evidence and linked game fixtures remain available at their old paths.
Retired build executables are in `worktree-binaries/`, indexed by
`outputs/storage-maintenance-20260916/retired-worktrees.json`.
The separately retired baseline-check clone is recorded in
`retired-baseline-clone.json` in the same audit directory. Source branches remain
in Git: recreate a needed worktree at its recorded commit, then initialize the
pinned submodules. Do not restore generated build caches into a different path.

List archived runs and restore one into a new private directory:

```sh
rtk proxy python3 work/evidence-archive/20260916/restore.py
rtk proxy python3 work/evidence-archive/20260916/restore.py RUN_NAME work/restored-RUN_NAME
```

The restore tool refuses existing destinations and verifies every restored file.
Use the restored save with a runner's `--save` and a different fresh `--output`.
Logs/manifests are restored too; the buff runner reads the seed's adjacent log.
Never extract an old save over the player's live save. Older binaries may require
their paired historical saves; compression does not change save compatibility.

Maintenance audit: `outputs/storage-maintenance-20260916/` contains the original
inventory, reviewed archive plan, hashes, retired worktree revisions and final
space/protection checks. Keep it with the archives. An old evidence path missing
from disk should be checked against this index before treating it as lost.

Issue #27 closure (21 September 2026): the published clean issue worktree and its
reproducible build/cache were retired after installation verification. The unsigned
executable, CMake configuration and retirement audit are retained in
`outputs/issue27-retirement-20260921/`; recreate source at
`ea81887fad935f2c02aa858beb5398760e02a302`. Baseline, final and intermediate private
runs remain at their original paths, including story checkpoints. Current paired
rollback is `work/issue27-deployment-20260921-ocx5n_8b/rollback`; installation and
evidence hashes are in `outputs/issue27-installation-20260921.json`. A redundant
superseded-app copy was removed only after matching every file to that rollback.
No player data or regression checkpoint was deleted.

26 September 2026 maintenance retired the published implementation worktrees for
#29, #33 and #39 while leaving their pending player-verification statuses unchanged.
Their protected checkpoints, final/baseline evidence and paired rollbacks remain
outside the worktrees. Exact unsigned executables, CMake settings, dependency pins,
file inventories and verified retirement/protected-file hashes are in
`outputs/storage-maintenance-20260926/retirement.json` and its per-issue directories.
Recreate source at the manifest commit with `git worktree add --detach NEW_PATH SHA`,
initialize the pinned submodules, and configure a fresh build when needed. Old
CMake caches document the build but must not be reused at a different source path.
The original repository and main working checkout remain; no issue worktree remains.
