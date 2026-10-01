# Retained private regression checkpoints

Selected 1 October 2026 under [storage-retention.md](storage-retention.md):
**28 distinct inputs, 0.694 GB**. These are inputs for future builds, not full
historical runs. Paths are relative to the coordination workspace in
[environment.md](environment.md). SHA-256, ZIP CRC and compatibility versions are
recorded locally in `outputs/storage-maintenance-20261001/plan.json`.

Always clone an input into a new private output; do not use its directory as the
writable game profile. Integrity checks alone do not establish gameplay acceptance.

| Issue / scenario | Input within workspace |
|---|---|
| [#5](https://github.com/Lu2cz/OpenGothic/issues/5) — ship gate and original loot | `work/issue5-checkpoints-20260916/fresh/save_slot_2.sav` |
| [#5](https://github.com/Lu2cz/OpenGothic/issues/5) — pre-captain and stash preparation | `work/issue6-captain-fixture-3/save_slot_2.sav` |
| [#5](https://github.com/Lu2cz/OpenGothic/issues/5) — stash reveal/return | `work/issue5-checkpoints-20260916/stash-ready/save_slot_1.sav` |
| [#3](https://github.com/Lu2cz/OpenGothic/issues/3) — locked chest ordinary/spell interaction | `work/issue3-candidate-ordinary/save_slot_1.sav` |
| [#5](https://github.com/Lu2cz/OpenGothic/issues/5) — post-captain beach and forest | `work/issue5-checkpoints-20260916/captain/save_slot_2.sav` |
| [#4](https://github.com/Lu2cz/OpenGothic/issues/4) — synthetic city / legacy save without compatibility | `work/city-exploration-seeded/save_slot_2.sav` |
| [#3](https://github.com/Lu2cz/OpenGothic/issues/3) — v1 lock migration | `work/issue3-before-focus/save_slot_2.sav` |
| [#3](https://github.com/Lu2cz/OpenGothic/issues/3) — v2 lock metadata rejection | `work/issue3-final-ordinary/save_slot_2.sav` |
| [#4](https://github.com/Lu2cz/OpenGothic/issues/4) — partial lock across world transitions | `work/issue4-final-v2-partial/save_slot_2.sav` |
| [#6](https://github.com/Lu2cz/OpenGothic/issues/6) — saved v56 pending AI wait | `work/issue6-aiwait-final2-seed/save_slot_2.sav` |
| [#6](https://github.com/Lu2cz/OpenGothic/issues/6) — v55 navigation migration | `work/issue6-aiwait-legacy-nav-final-4-seed/save_slot_2.sav` |
| [#6](https://github.com/Lu2cz/OpenGothic/issues/6) — empty zero-next queue rejection | `work/issue6-invalid-queue-fixtures-v2/empty-zero-next.sav` |
| [#6](https://github.com/Lu2cz/OpenGothic/issues/6) — zero ticket rejection | `work/issue6-invalid-queue-fixtures-v2/zero-ticket.sav` |
| [#6](https://github.com/Lu2cz/OpenGothic/issues/6) — invalid active ticket rejection | `work/issue6-invalid-queue-fixtures-v2/invalid-active.sav` |
| [#6](https://github.com/Lu2cz/OpenGothic/issues/6) — duplicate ticket rejection | `work/issue6-invalid-queue-fixtures-v2/duplicate-ticket.sav` |
| [#7](https://github.com/Lu2cz/OpenGothic/issues/7) — saved SQ416 boss encounter | `work/issue7-installed-reconfirm-20260914/save_slot_1.sav` |
| [#7](https://github.com/Lu2cz/OpenGothic/issues/7) — v1 partial-start boss UI boundary | `work/issue7-historical-v1-active-20260913/save_slot_2.sav` |
| [#7](https://github.com/Lu2cz/OpenGothic/issues/7) — v2 partial-start boss UI boundary | `work/issue7-historical-v2-active-20260913/save_slot_2.sav` |
| [#8](https://github.com/Lu2cz/OpenGothic/issues/8) — active speed buff restart/expiry | `work/issue8-staged-seed-20260915/save_slot_2.sav` |
| [#27](https://github.com/Lu2cz/OpenGothic/issues/27) — Fabio/Rupert tavern arrival | `work/silbach-arrival-20260917/source.sav` |
| [#29](https://github.com/Lu2cz/OpenGothic/issues/29) — before first Silbach sleep | `work/issue27-signed-story-20260921/save_slot_2.sav` |
| [#29](https://github.com/Lu2cz/OpenGothic/issues/29) — post-sleep villagers before escort (title 2) | `work/silbach-next-day-20260921/named-2.sav` |
| [#29](https://github.com/Lu2cz/OpenGothic/issues/29) — post-sleep villagers during escort (title 3) | `work/silbach-next-day-20260921/named-3.sav` |
| [#32](https://github.com/Lu2cz/OpenGothic/issues/32) — Kurt/Jorn tavern meeting (title 6) | `work/kurt-jorn-20260922/named-6.sav` |
| [#33](https://github.com/Lu2cz/OpenGothic/issues/33) — noticeboard map (earlier title 9) | `work/silbach-map-20260925/named-9.sav` |
| [#36](https://github.com/Lu2cz/OpenGothic/issues/36) — post-fishing clock/scroll quest and pose recovery | `work/silbach-scroll-fishing-20260925-0gb3tgv4/named-9.sav` |
| [#37](https://github.com/Lu2cz/OpenGothic/issues/37) — before native Kurt fishing scene | `work/issue37-prefishing-explore-b/before-kurt.sav` |
| [#39](https://github.com/Lu2cz/OpenGothic/issues/39) — missing flask/feather station feedback | `work/issue39-feedback-input-20260925/save_slot_12.sav` |

## Provenance and use

- Opening fixtures are instrumented; see [opening-checkpoints.md](opening-checkpoints.md)
  and its retained primary manifest for creation procedures and validated consumers.
- City/lock and boss/AI migration fixtures are synthetic. Legacy v1/v2 UI inputs
  intentionally lack metadata that was never saved; they test that boundary.
- Silbach arrival and named 2/3/6/9 inputs are private player checkpoint copies.
  The earlier map title 9 and later post-fishing title 9 are different saves.
- The pre-sleep input comes from the automated native tavern/Jorn replay.
  The pre-fishing input follows native quest choices with shortened diagnostic travel.
- The affected post-fishing input contains the old pose defect: test load recovery
  with it and a new fishing scene separately with the pre-fishing input.
- Four malformed queue inputs are valid ZIPs with invalid AI fields. These are
  rejection tests, not supported gameplay saves.
- The buff reload runner requires the retained adjacent `terminal.log` for timers.
- World transitions use the partial-lock input. Regenerate seed/restart outputs;
  fresh-game recipe/loot/buff checks require no additional historical saves.

## Rollback and unresolved exceptions

The only full rollback is `work/issue37-deployment-20260926-v1/rollback`, including
compatible pre-install player saves/config, launcher and diagnostic app. Its signed
Fast hash is `c504f77f0eb6e72726c562ecb3171e93bb63ceed5ccc186c9270e609284dc426`.
Current Fast remains `e7a1752aec3984a4168ae72e864a0c9b9963057e1f001455ade10e0cdd566731`.

Additional material is retained for these specific unresolved investigations:

| Issue | Retained exception | Release condition |
|---|---|---|
| [#31](https://github.com/Lu2cz/OpenGothic/issues/31) loading stall | Sample/logs/config in `work/issue29-sleep-signed-arrival` (use selected arrival save); exact stalled-era app in `work/issue33-deployment-20260925-v1/rollback/work/ArcholosFast.app`; comparison app in `work/issue29-deployment-20260921-fehfnm2d/rollback/ArcholosFast.app` | Investigation and comparison verification complete. These two apps are investigation fixtures, not extra full rollbacks. |
| [#33](https://github.com/Lu2cz/OpenGothic/issues/33) map baseline | `work/issue33-baseline-20260925` logs/config; shares #31’s stalled-era app | Remaining baseline post-selection check resolved. |
| [#29](https://github.com/Lu2cz/OpenGothic/issues/29), [#32](https://github.com/Lu2cz/OpenGothic/issues/32), [#33](https://github.com/Lu2cz/OpenGothic/issues/33) reproduction | Original intake logs and #29 saved-state inspection script beside selected inputs | Raw intake evidence no longer needed for the unresolved reproduction. Useful scenario inputs remain. |

Small creation/config/provenance records stay beside selected checkpoints.
Existing acceptance results remain in GitHub and published guides. Old output-run
paths and archive objects are historical and may no longer exist; the local audit
retains compact original records, not a promise to restore discarded data.
No new campaign advancement or gameplay acceptance is claimed by consolidation.
