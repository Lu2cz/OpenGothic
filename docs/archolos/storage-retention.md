# Local checkpoint and storage retention

Owner-authorized policy, 1 October 2026. Retain reusable inputs, not a history of
full test runs. This supersedes the September evidence/archive retention policy.

## Keep

- Original game assets/installers and `work/playable` player saves/configuration.
- Current `ArcholosFast.app` and the shared diagnostic `ArcholosProfile.app`.
- **One useful input checkpoint per distinct scenario.** Share a checkpoint between
  tests where possible. Different save-format migrations, malformed-input rejection
  cases, and genuinely different story states are distinct scenarios.
- **One known-good rollback**, including its app, compatible saves, settings and
  launcher. Never pair an older binary with unsupported newer saves.
- Compact verification records in the assigned GitHub issue: cause, commit/PR,
  source/dependency revisions, tested entry point, result and material limitations.
- Additional raw logs, images, exact binaries or inputs only for a **specific
  unresolved investigation**. Record its issue, reason and release condition.
- Small checkpoint provenance/hash manifests and cleanup audits locally. Private
  saves/assets/logs must never be uploaded to GitHub.

Do not retain failing/passing full runs, intermediate saves, historical apps or
compressed evidence archives by default. Published commits preserve source;
generated builds can be recreated. A completed issue awaiting player confirmation
needs its useful input, not its old full build and every deployment generation.

## During testing and finishing

Use `tests/archolos_test_data.py::copy_save`: independent APFS copy-on-write clones
(`cp -c -p`) with ordinary-copy fallback. Never hard-link writable saves. Verify
source hashes before/after a run; do not use `work/playable` as test output.

Select inputs based on narrative state and provenance, not directory names or age.
Prefer a clean checkpoint before the relevant sequence. Label instrumented states
and saves containing an old defect; an old save cannot prove a fresh-game fix.
Capture new seed/restart outputs temporarily, then discard them after verification
unless one becomes the sole useful input for a new scenario.

After implementation/testing, publish source, record compact results in the issue,
then retire the clean issue worktree/build and temporary test runs. Preserve unique
unpublished work before removal; inspect submodule changes, ignored/untracked files,
symlinks and cross-path dependencies. Keep the main checkout/shared Git repository
usable. Managed worktrees use their lifecycle tool; ordinary ones use Git removal.
Do not clean paths owned by a running game, build or other task.

When installing, replace the previous rollback only after the new app is privately
validated and signed, and a compatible paired rollback is safely captured. Do not
rewrite player saves or the retained rollback merely to save space.

## Current retained collection and exceptions

[Retained checkpoints](retained-checkpoints.md) lists the selected private inputs,
provenance boundaries and unresolved-investigation exceptions. Paths are relative
to the workspace in [environment.md](environment.md), not the source repository.
That inventory supersedes old run/evidence paths in historical documents/comments.

Current rollback: `work/issue37-deployment-20260926-v1/rollback`.
Old evidence paths are historical records, not guaranteed recoverable fixtures.
The September compressed archive and superseded rollback generations were retired
under this policy; use the selected inputs and published runners to regenerate tests.

Cleanup audit: `outputs/storage-maintenance-20261001/` in the workspace contains
selected hashes, the explicit removal plan, compact original run metadata, retired
archive index, verified reconstruction patches for the two historical source copies,
and before/after protection/storage checks. The old archive index is provenance
only; its discarded objects/tar files cannot be restored.

Record allocated/logical size reduction separately from measured free-space change:
APFS shared copies, snapshots and concurrent activity make them differ. No blanket
cleanup of unknown data, unrelated device caches, original game data or user saves.
