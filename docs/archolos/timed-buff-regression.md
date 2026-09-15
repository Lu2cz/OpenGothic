# Timed buff regression

Issue #8 verifies Archolos's installed `ITPO_SPEED` potion through the normal
inventory path. Its `BUFF_SPEED` script adds the native sprint overlay, shows
`ITPO_SPEED2.TGA`, and owns removal through the existing buff callback.

`NPC_FINDBYID` now resolves the installed `AIVAR[89]` native NPC ID against the
current world and returns the existing safe virtual NPC VOB mapping. The optional
`SIN` bridge supports the script's existing alpha fade curve; scripted View alpha
is applied by the native UI brush.

## Acceptance evidence

- The signed staged Fast binary created an active save with handle 13 and restored
  the exact saved timer and remaining duration (`21973`, `228628`).
- Reload rendered the icon at alpha 255, then alpha 127, and removed it at timer
  250618 with sprint absent. The capture images are `buff-ui-active.png`,
  `buff-ui-fade.png`, and `buff-ui-expired.png` in
  `work/issue8-staged-reload-20260915`.
- Reusing the potion preserved the same handle, consumed the inventory item, and
  doubled both script duration and end time. The installed private smoke is
  `work/issue8-installed-repeat-20260915`.

The test harness exits the private process after a completed save only after the
loader is idle for ten frames. It does not prove a generic shutdown-during-save
lifecycle fix; no such unrelated engine change was made.

The signed staged binary used for these captures and the installed Fast binary
have the same SHA-256: `4a7193bcec3e96ed85de4d0b5a5e21c93a97713a1743d5467ae9a9a5d5b31a76`.
