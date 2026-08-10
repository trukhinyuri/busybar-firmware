# Debugging {#debugging}

How to find out what the firmware is actually doing, without guessing and
without taking the device apart. Everything here works over the USB virtual
ethernet link on an assembled device.

# The CLI is the primary tool

The firmware serves a command line interface on TCP port 23 of the device's USB
address:

```shell
nc 10.0.4.20 23
```

`help` lists the commands. The ones that matter for diagnosis:

| Command | Purpose |
| --- | --- |
| `log` | Stream log records live as they are emitted |
| `log_dump` | Write the in-memory log buffer to `/ext/log.txt` |
| `loader open <Name>` | Launch an application by its `name` from `application.fam` |
| `loader kill` | Terminate the foreground application |
| `top` | Per-thread CPU and stack usage |
| `free` / `free_blocks` | Heap totals and fragmentation |
| `power info` | Charge state, voltages |
| `power reboot` | Restart; `power boot` drops to the DFU bootloader |
| `device_info` | Serial number, hardware and firmware identifiers |
| `factory_reset` | Wipe user data back to defaults |

`log_dump` pairs well with the HTTP storage API when the CLI output is long or
needs to be searched on a workstation:

```shell
curl -H 'X-API-Sem-Ver: 25.0.0' \
  'http://10.0.4.20/api/storage/read?path=/ext/log.txt' -o devlog.txt
```

Log lines are prefixed with the tick count in milliseconds since boot, then the
level and the tag passed to `FURI_LOG_*`. The tick is what makes application
lifecycle problems obvious: a service that starts and returns within a few
milliseconds did not run, it bailed out.

`loader open` is the fastest way to isolate an application from the menu that is
supposed to launch it. If `loader open Alarm` works but the application cannot
be reached through the UI, the fault is in the menu, not the application.

# Reading device state over HTTP

`GET /api/status` is the single most useful health check. Beyond the obvious
fields, three of them diagnose the inter-chip link:

- `firmware.nwp_version` — `null` means the Si917 is not answering.
- `firmware.intercom_version` — the handshake string the STM32 expects.
- `device.firmware_security` — `unknown` means the SL security state could not
  be read, which is itself a symptom of the radio not answering.

When all three go quiet at once and Wi-Fi reports `state: unknown`, the two
chips have stopped talking. That is almost always a handshake version mismatch
after flashing only the STM32 — see @ref alarm for the `INTERCOM_FORCE_VERSION`
rule.

`GET /api/storage/read?path=…` and `POST /api/storage/write?path=…` reach any
file, including the JSON a service persists under `/ext/apps_data/<appid>/`.
Reading a service's own settings back is a direct way to confirm that it
started, parsed its configuration and wrote its defaults.

# Interpreting SettingProvider warnings

`Failed to load "<key>" as struct` is emitted whenever a key is absent from the
stored JSON. It is a fallback notice, not an error: the default is applied and
written back, and `setting_provider_load()` still returns true. Treat it as
noise on first boot after a schema change; only a false return from
`setting_provider_load()` indicates real storage trouble.

# When the device will not boot far enough to talk

- The on-device recovery service performs a factory reset.
- `power boot` from the CLI, or the updater, exposes the DFU path.
- The signed upstream release bundle reinstalls stock firmware through
  `POST /api/update`, which is the same path the device's own updater uses.
- SWD is available only after partial disassembly and attaching the BSB debug
  board, and is the last resort rather than the first.
