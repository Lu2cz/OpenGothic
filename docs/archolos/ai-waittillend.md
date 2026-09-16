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

Run from your current checkout. Set `TASK_WORK` to the environment guide's
`work` directory and choose unused output names. The installed binary supports
these checks; no retired issue worktree or rebuild is required.

```sh
rtk proxy python3 tests/run_archolos_ai_wait.py --help
rtk proxy python3 tests/run_archolos_ai_wait.py --executable "$TASK_WORK/ArcholosFast.app/Contents/MacOS/Gothic2Notr" --game "$TASK_WORK/archolos-game" --save "$TASK_WORK/issue6-captain-fixture-3/save_slot_2.sav" --output "$TASK_WORK/ai-wait-seed" --mode seed
rtk proxy python3 tests/run_archolos_ai_wait.py --executable "$TASK_WORK/ArcholosFast.app/Contents/MacOS/Gothic2Notr" --game "$TASK_WORK/archolos-game" --save "$TASK_WORK/ai-wait-seed/save_slot_2.sav" --output "$TASK_WORK/ai-wait-reload" --mode reload
rtk proxy python3 tests/run_archolos_ai_wait.py --executable "$TASK_WORK/ArcholosFast.app/Contents/MacOS/Gothic2Notr" --game "$TASK_WORK/archolos-game" --save "$TASK_WORK/issue6-captain-fixture-3/save_slot_2.sav" --output "$TASK_WORK/ai-wait-edges" --mode edges
```

For opening-story inputs and current-binary replay evidence, see
[opening-checkpoints.md](opening-checkpoints.md). The candidate results below
are historical; restore archived evidence using [storage-retention.md](storage-retention.md).

The reload probe requires `restored pending=1`, `still pending=1`, then
`complete`; it does not treat removal of the fallback warning as scene proof.
The edge probe requires `empty`, `completed`, `self`, `snapshot`, `reciprocal`,
and `removed` markers before it saves.
The normal-duration forest and captain runners remain separate scene evidence.

## Historical private verification — issue #6

Candidate `9b4c7b795bd037672ded991aef9d0a05cda2a1683e85ff6dd79e71af3461f809`
was built from the issue-6 worktree (since retired) and exercised without modifying source saves:

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

Candidate `445eb44c16f8a0e721dcef3ff4313f63ead9313d3235f86f49e521aab13b1dad`
generated a private v55 world payload with Jorn's primary queue empty, `go2`
active, and all timers inactive, then reloaded it as v56. The post-`go2.load`
migration restored ticket 1; a new `AI_WAITTILLEND` remained pending at frame 30
and completed by frame 420. `issue6-aiwait-legacy-nav-final-4-{seed,reload}`
recorded the v55/v56 world-version boundary, valid result ZIPs, and unchanged
source hashes. The seed is an opt-in mixed-header/world probe fixture, not a
general v55 save writer or a supported player-save boundary.

All four CRC-valid malformed private saves in
`issue6-invalid-queue-fixtures-v2` were loaded on candidate
`95a5d0769a9c620536e39071ce216d38171b97c9411736efa2d41b30aa2089e7` and
rejected before a probe or replacement save: empty-zero-next and zero-ticket
reported `Invalid AI action ticket`, invalid-active reported `Invalid active AI
ticket`, and duplicate-ticket reported `Duplicate AI action ticket`. The source
variant hashes stayed unchanged. The local generator remains
`outputs/issue6-coordination/prepare-invalid-queues.py`; the private malformed
fixtures are rejection evidence, not supported load boundaries.

Visual actor/camera staging remains a separate investigation in
[#21](https://github.com/Lu2cz/OpenGothic/issues/21). World-render screenshots
show an empty later camera shot even though required speech now plays; speaker
logs alone do not establish visual correctness. Integration and installed-app
verification are recorded in [#6](https://github.com/Lu2cz/OpenGothic/issues/6).
