# `AI_WAITTILLEND` synchronization

`AI_WAITTILLEND(self, target)` snapshots the target's current/latest primary AI
action. It does not wait for the target's future queue to become empty; that
would deadlock reciprocal script forms. Completion includes queued work and an
active movement, wait, animation, or output action. A missing or self target is
a no-op.

Native world saves written by this change are version 56. They persist queue
ticket/watch identities and an NPC's active action ticket. Version-55 saves
remain readable: an active movement/wait/animation/output with no queued action
receives a transient identity after its navigation state loads. Newer save
versions are rejected by this binary. Older binaries do not contain that guard,
so retain the pre-upgrade executable when keeping a rollback save.

Supported: saves outside an active dialogue/cutscene, including the private
pending-wait regression below. Live mid-scene saves have not been accepted as a
supported replay boundary.

## Reproduction

Build the isolated Release Metal target, then use private output directories:

```sh
rtk proxy cmake --build /Users/lu2/projects/OpenGothic-issue-6/build-issue-6 --target Gothic2Notr --parallel 4
rtk proxy python3 /Users/lu2/projects/OpenGothic-issue-6/tests/run_archolos_ai_wait.py --executable /Users/lu2/projects/OpenGothic-issue-6/build-issue-6/opengothic/Gothic2Notr --game /Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/work/archolos-game --save SOURCE --output SEED --mode seed
rtk proxy python3 /Users/lu2/projects/OpenGothic-issue-6/tests/run_archolos_ai_wait.py --executable /Users/lu2/projects/OpenGothic-issue-6/build-issue-6/opengothic/Gothic2Notr --game /Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/work/archolos-game --save SEED/save_slot_2.sav --output RELOAD --mode reload
```

The reload probe requires `restored pending=1`, `still pending=1`, then
`complete`; it does not treat removal of the fallback warning as scene proof.
The normal-duration forest and captain runners remain separate scene evidence.

## Private verification

Candidate `9b4c7b795bd037672ded991aef9d0a05cda2a1683e85ff6dd79e71af3461f809`
was built from this worktree and exercised without modifying source saves:

- `issue6-aiwait-final2-seed` then `issue6-aiwait-final2-reload` passed the
  persisted pending wait (`restored pending=1`, `still pending=1`, `complete`).
- `issue6-forest-final-2` passed every required trialogue line, returned player
  control, reset the speaker, and finalized its private save. Dialog participants
  retain normal processing while a dialogue is open, so distant trialogue staging
  cannot silently discard their output; the player retains its Player policy.
- `issue6-world-transition-final2` passed sewer transition, mainland transition,
  and restart. Its `run.json` records the executable and chained save hashes; the
  original fixture is `5574d61f7aa31390b1db05d4b9f24cfa417ef5739ed9e548185dac9e17ac22ed`.

The captain replay is still pending a regenerated, real pre-captain fixture. The
available mainland save is not such a fixture, so no captain acceptance is claimed.
