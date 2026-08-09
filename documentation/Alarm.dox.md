# Alarm {#alarm}

A fully autonomous alarm clock that runs entirely on BUSY Bar. It does not depend on
a companion Mac, on Wi-Fi, on the mode selector position, or on any application being
open. Once armed, the only thing needed to ring is that the device has charge.

# Design goals

1. **Cannot be slept through.** Dismissing a ringing alarm requires a deliberate,
   cognitively non-trivial action, not a reflex button press.
2. **Autonomous.** Schedule, persistence and firing all live in firmware.
3. **Survives restarts.** Alarms are stored on eMMC and re-armed on boot; an alarm
   whose instant passed while the device was off is still reported.
4. **Compatible.** Built on the exact upstream release the device already runs
   (tag `1.1.1`, commit `ac59f45c`, HTTP API `25.0.0`), so the official BUSY apps and
   the documented HTTP API keep working unchanged.

# Architecture

Two components, following the existing firmware split between long-lived services and
user-facing applications.

## Alarm service — `applications/services/alarm`

`FlipperAppType.SERVICE`, started at boot, never exits. Modeled on
@ref applications/services/busy_timer.

Responsibilities:

- Owns the alarm set (up to `ALARM_MAX_COUNT` entries) and persists it through
  `SettingProvider` to `APP_DATA_PATH("settings.json")`.
- Runs a one-second tick driven by the `time` service, which is RTC-backed, and
  recomputes the next due instant for every enabled alarm from local wall time plus a
  weekday mask.
- Fires an alarm regardless of the current foreground application, the mode selector
  position, or display sleep state.
- Owns the ringing state machine, including the dismissal challenge.
- Publishes state and accepts edits through the `RECORD_ALARM` record, so the UI app
  never owns scheduling.

Firing sequence:

1. `low_power_lock()` — brings the displays out of sleep. This works because
   `low_power` only sleeps the panels; the MCU and the scheduler keep running, which is
   what makes soft-off ringing possible.
2. Acquire `GuiLayerIdTop` on both displays and render the ringing view. `GuiLayerIdTop`
   sits above `GuiLayerIdMain`, so it covers whatever application was open.
3. Ramp audio volume from `ALARM_VOLUME_RAMP_START` to the alarm's configured volume
   over `ALARM_VOLUME_RAMP_MS`, replaying the sound asset on `AudioEventPlayEnd`.
4. Ring until dismissed or until `ALARM_MAX_RING_MS` elapses, after which the alarm is
   recorded as missed and audio stops so a forgotten alarm cannot drain the battery.

## Alarm application — `applications/main/alarm`

`FlipperAppType.APP`, listed in the APPS menu. Modeled on
@ref applications/main/clock.

Scenes:

- **List** — every alarm with its time, weekday mask and enabled state. The wheel moves
  the selection, OK opens the editor, START toggles enabled, and a trailing "New alarm"
  row adds one.
- **Edit** — time, repeat, volume, enabled, save, delete. The wheel selects a field, OK
  enters and leaves edit mode for that field, BACK reverts.

Unlike the previous Mac-hosted approach, every alarm is reachable from the device
whether it is enabled or not, and new alarms can always be created.

# Dismissal challenge

There is no snooze. A ringing alarm exposes exactly one way out.

The back display shows a randomly generated two-digit target. The wheel adjusts a
counter; the user must match the target and press OK. A wrong confirmation generates a
new target and keeps ringing. This requires reading a number and operating a control
deliberately, which is difficult to do while asleep, but takes only a few seconds when
awake.

BACK, START and the mode selector do not stop a ringing alarm.

# Time and correctness

The `time` service is RTC-backed and synchronises over SNTP when Wi-Fi is available.
Alarm instants are computed in local wall time, so a timezone or DST change moves the
alarm the way a user expects.

If the RTC is not valid — for example after a complete power loss — an alarm cannot be
scheduled correctly. In that case the service refuses to arm and surfaces the condition
in the UI rather than silently failing to ring, on the principle that a visibly broken
alarm is far better than one that quietly does not go off.

# Operational notes

- **Automatic updates must be disabled.** The device ships with
  `auto_update_enabled: true`; leaving it on lets stock firmware overwrite this build.
- **Flash main firmware only.** `flash_usb` variants that include `sil_m4`/`sil_nwp`
  reflash the Si917 wireless co-processor, which is the component that can be bricked.
  The alarm needs no radio changes.
- **Recovery.** The signed upstream `1.1.1` bundle published on the releases page
  restores stock firmware, and the on-device recovery service performs a factory reset.
