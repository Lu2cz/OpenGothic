# Archolos on macOS

## Goal
Make Archolos playable from beginning to end on the target Mac: reliable story
progression, saves, essential gameplay/UI/audio, and acceptable performance.
Full Windows DLL or general LeGo/Ikarus parity is not required.

## Sources of truth
- Roadmap and completion contract: https://github.com/Lu2cz/OpenGothic/issues/1
- Board: https://github.com/users/Lu2cz/projects/1
- Evidence, scope, acceptance and progress: the assigned GitHub issue.
- Implementation: commits and pull requests in Lu2cz's forks.
- Build/test/launch environment: docs/archolos/environment.md.
- Initial compatibility audit: docs/archolos/lego-ikarus-audit.md (dated evidence).
- ARCHOLOS_DEVELOPMENT.md is historical; read only relevant sections.
Do not duplicate the backlog or completed-work history here.

## Starting an issue
- Read this file, the assigned issue, and relevant linked evidence/code.
- Check branch, worktree changes and pinned dependencies before editing.
- Use one substantial issue per Codex task and isolated branch/worktree.
- Base issue branches on origin/archolos/performance-v092 until migration lands.
- Use explicit --repo Lu2cz/OpenGothic for gh issues/PRs; origin is our fork.
- Record the task link/ID, branch and worktree in the issue when starting.
- Keep one active implementation/test run initially; coordinate builds/game runs
  and installation into shared app bundles. Do not spawn agents without a request.
- Advance the assigned scope autonomously; record newly found unrelated work in
  linked issues rather than expanding this task into a library-wide rewrite.

## Engineering
- Trace the real flow; fix shared causes when evidence supports them.
- Reuse existing mechanisms; prefer the smallest correct implementation.
- Distinguish confirmed failures, hypotheses and untested compatibility gaps.
- Verify through real gameplay entry points and appropriate regression checks.
- Static callers, absent warnings and synthetic checkpoints alone do not prove
  gameplay correctness, visible rendering or campaign completion.
- Installed scripts are 1.2.11; reference decompilation is 1.2.7, not exact source.
- Snapshot mapping ABI changes require explicit version/compatibility review.

## Boundaries and authorization
- GitHub administration, issues, branches, pushes and PRs in Lu2cz's project forks
  are authorized. Earlier local-only instructions are superseded for these forks.
- Upstream submissions remain a separate user decision; do not send unsolicited
  messages or PRs to Try/OpenGothic or other upstream projects.
- Preserve user saves, original assets/installers and known-good binaries.
- Use private save/config copies and new output directories for testing.
- Do not commit game assets, personal saves, credentials or raw private logs.
- Leave cursor behavior unchanged unless explicitly requested.
- Defer Steam/GOG/Discord services and legacy Windows debug/renderer emulation;
  gameplay LeGo View/Render UI remains in scope when demonstrated.
- Prefix shell commands with rtk; use rtk proxy when raw output is needed.
- Give the single launch command from the environment guide, not script contents.

## Finishing and handing off
- Update the issue with cause, commits/PR, verification and material limitations.
- State whether the playable app was updated and identify its exact source/build.
- Move the board status honestly: Backlog, Ready, In progress, Needs verification,
  Done. Close only when acceptance is satisfied; pending player checks stay open.
- Push meaningful checkpoints to the fork. Integrate verified fixes through a PR
  targeting our working branch; do not replace the playable app before validation.
- Leave a concise issue handoff if unfinished. Keep raw evidence local and link
  reproducible instructions plus a public-safe summary.

## Maintaining this file
Update only durable goals, constraints, workflow or authoritative reference links.
Replace obsolete instructions; do not append debugging notes or milestone history.
Keep this file around 80 lines or fewer. Use issues for changing project state.
