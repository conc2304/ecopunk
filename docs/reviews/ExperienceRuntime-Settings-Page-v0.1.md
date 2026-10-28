# Experience Settings Page v0.1 (ExperienceRuntime)

Runtime-owned overlay page (not a scene) for installation automation and OS clock/timezone.
Opened with **F1** or **,** (keyboard entry point; see "Frozen-geometry conflict"). While open it is
the sole input consumer; **Esc / F1 / `,`** or the **Return to Experience** rows close it.

## Files
- `src/ExperienceSettingsPage.h` — page model (OF-free). Reuses `HudFocusNavigator` for focus.
- `src/ExperienceSettingsPageView.{h,cpp}` — plain boxes + bitmap text in the 1280x720 canvas.
- `src/SystemClockService.{h,cpp}` — the only OS-clock seam (fixed-argv `timedatectl`, no shell).
- `src/DpadInput.h`, `src/InputRouter.*` — five-way input (`translateToDpad`, `isSettingsToggle`).
- `src/InstallationControl.h` — `clock24Hour`, `SetClock24Hour`, `setAutomationSuspended()`.
- `src/ExperienceRuntime.*` — open/close, save-with-feedback (atomic temp+rename), suspension.
- `src/SettingsPageHarness.*` — keyboard-only scripted run (`EXPERIENCE_RUNTIME_SETTINGS_PAGE_HARNESS=1|verify`).
- `test/experience_settings_page_tests.cpp` (`make -C test -f Makefile.tests settings-page`).

## Navigation
Not editing: Up/Down move focus (wraps; disabled rows stay focusable so their reason is readable);
OK toggles, edits a value row, or activates Apply/Return. Editing: Up/Down move between edit slots
(value fields, then CANCEL); Left/Right adjust; OK on a value confirms; OK on CANCEL or Esc abandons.
Apply: OK, then OK again to confirm; any other input cancels. OK is debounced (250 ms); Left/Right repeat.
Keys never reach scene, HUD focus or runtime commands while the page is open (Esc never exits the app;
OF's escape-quit is suspended while open and restored one frame after closing). Pointer presses are ignored.

## Persistence and overrides
Single store: the canonical `installation-settings.json` (`bin/data/installation-settings.json`, or the
`EXPERIENCE_RUNTIME_INSTALLATION_SETTINGS` path) through `installation::Controller` (same validation:
scene 10..3600 s, media 5..3600 s). Confirmed edits persist immediately (atomic write); the page shows
`saved` / `SAVE FAILED` / `NOT SAVED: persistence is off`. No D-pad step persists. New field: `clock24Hour`
(default true). There is no environment override of the automation values themselves; harness runs have
persistence off unless the path env var is given (page then says "NOT SAVED").

## Automation while open
Scene auto-cycle: suspended (no accrual, no fire); resume starts a fresh period (no catch-up).
Media: Shared Video's timer is disabled while open and re-enabled on close (fresh dwell window).
Saved preferences are untouched. The artwork itself keeps running. Limitation: scenes that advance
media at their own boundary (Blueprint composition cycle) keep doing so — that is scene-owned cadence.
Cadence uses frame dt / `steady_clock`; wall-clock changes do not move it.

## Clock capability matrix
| Capability | macOS (dev) | Rock Pi / Armbian |
|---|---|---|
| Local date/time, timezone | read (localtime_r, /etc/localtime) | read (`timedatectl show`) |
| NTP on/off + synchronized | unknown (shown as such) | read (`timedatectl show`) |
| 12/24-hour display | app preference (saved) | app preference (saved) |
| Set timezone / NTP / date-time | disabled, "Requires platform setup" | disabled until Platform authorises (below) |

12/24-hour: the HUD clock widget (`HudClockFormat::timeText`) is hard-coded 24-hour and its schema is
frozen, so the preference currently drives the Settings page clock only. HUD Runtime change needed to honour it.

## MENU button (resolves the former geometry conflict)
Operator-directed exception: `controls.menu` — seventh 72x72 square at x 619, y 633.6 (right inset 32 px = the NEXT MEDIA->FULLSCREEN
gap, measured to the nav panel inner right edge x=723). Focus slot 7; OK dispatches `Runtime::OpenSettings` -> `openSettingsPage()`
(same entry as F1 / `,`). HUD focus is untouched while the page is open, so closing returns to MENU; the closing Enter is guarded
(0.6 s) so it cannot re-activate MENU.

## Platform handoff (clock writes) — not performed
Writes run `/usr/bin/timedatectl --no-ask-password set-ntp|set-timezone|set-time` (fixed argv, no shell, no sudo,
app never root) and only when `EXPERIENCE_RUNTIME_CLOCK_WRITE=1`. Platform must: (1) add a polkit rule scoped to the
kiosk user allowing only `org.freedesktop.timedate1.set-timezone`, `.set-ntp`, `.set-time`; (2) set that env var in the
kiosk service; (3) supervise first verification on the Pi. Manual time requires Time sync = MANUAL (NTP off) —
the page blocks Apply with that reason otherwise. Results shown are re-read from the OS, never assumed.
