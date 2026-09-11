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
rtk proxy python3 /Users/lu2/projects/OpenGothic-issue-6/tests/run_archolos_ai_wait.py --executable /Users/lu2/projects/OpenGothic-issue-6/build-issue-6/opengothic/Gothic2Notr --game /Users/lu2/Documents/Codex/2026-09-09/https-github-com-try-opengothic-issues/work/archolos-game --save SOURCE --output EDGES --mode edges
```

The reload probe requires `restored pending=1`, `still pending=1`, then
`complete`; it does not treat removal of the fallback warning as scene proof.
The edge probe requires `empty`, `completed`, `self`, `snapshot`, `reciprocal`,
and `removed` markers before it saves.
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

The captain runner now accepts `--prepare` without `--save` to generate a private
fresh ship fixture. It executes the initial Jorn info through the existing native
choice dispatcher, clears the setup queues, and positions the participants at
SHIP_JORN_02. This is synthetic prerequisite setup, not campaign validation.

`issue6-captain-fixture-3/save_slot_2.sav` was generated on candidate
`cec49f70e37e6ddf0db2334ee2501298ce6a4d4c706df20922ea55716ea5d474`.
Normal-duration replay `issue6-captain-final-1` passed on candidate
`95a5d0769a9c620536e39071ce216d38171b97c9411736efa2d41b30aa2089e7`.
It selected the Jorn prompt, the installed 1.2.11 `DIA_JORN_Q101_WHATSUP_NO`
captain response, and both required trialogue responses; it then returned control
with flag 11, finalized a CRC-valid private save, and preserved the fixture hash.
The runner accepts either `WHATSUP_YES` or `WHATSUP_NO`: 1.2.11 exposes `NO`
where the 1.2.7 reference lists `YES`, and both dispatch the captain sequence.
This corrects the runner's choice expectation, not the campaign; the fixture and
replay remain synthetic private scene evidence, not campaign-progression proof.

Candidate `4673359a853f41bb58bdd2eb78803ac0a5ba113fc094f5a69246d98a20a5c624`
also passed the pending-wait seed/reload pair and the private edge matrix in
`issue6-aiwait-edgefinal-{seed,reload}` and `issue6-aiwait-edges-final`.
The legacy active-navigation migration and malformed-ticket rejection still need
their own runtime checks. Four CRC-valid malformed private saves are prepared in
`issue6-invalid-queue-fixtures-v2`: empty-zero-next, zero-ticket, invalid-active,
and duplicate-ticket. Its manifest records source/variant hashes. The local
coordinator generator is `outputs/issue6-coordination/prepare-invalid-queues.py`;
it anchors the exact reviewed one-wait v56 fixture rather than guessing offsets.
These variants have not yet been loaded by the game.

Visual actor/camera staging remains a separate investigation in
[#21](https://github.com/Lu2cz/OpenGothic/issues/21). World-render screenshots
show an empty later camera shot even though required speech now plays; speaker
logs alone do not establish visual correctness. PR #20 remains draft/uninstalled.
