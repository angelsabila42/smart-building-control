# Management Interface (Member 5)

This document describes the serial management interface on the **entrance**
terminal. It covers the command reference, administrator authentication, the
time-based access policy, and the hooks the other terminals use to integrate
with it.

Source layout:

| Path | Purpose |
| --- | --- |
| `entrance/src/management/time_utils.*` | Clock parsing/formatting and civil-calendar maths |
| `entrance/src/management/access_schedule.*` | Access-schedule model, validation and access decisions |
| `entrance/src/management/admin_auth.*` | Administrator credentials, sessions and lockout |
| `entrance/src/management/serial_menu.*` | Transport-independent command parser and all menu output |
| `entrance/src/management/access_events.*` | Ring buffer of recent access decisions |
| `entrance/src/management/system_status.*` | Status snapshot and report formatting |
| `entrance/src/management/management_app.*` | ESP32 adapter (NVS, clock, console, RFID hook) |
| `entrance/src/management/management_console.*` | UART0 line editor and console task |
| `entrance/src/management/management_store.*` | NVS persistence of schedule and credentials |

Everything except the three adapter files is plain C11 and is covered by host
unit tests in `tools/management-tests/`.

## Connecting

The console runs on `UART0` at **115200 baud**, 8-N-1. It shares UART0 with the
ESP-IDF log output, so both firmware logs and menu text appear in the Wokwi
serial monitor. Type a command and press Enter; `Backspace` edits the line.

On boot the terminal prints:

```
CoCIS Smart Building - Main Entrance Terminal
Management interface | firmware 0.1.0
Type 'help' for commands.
```

If the clock has not been set it also prints a warning, because access
decisions fail closed until it is.

## Command reference

Commands are case-insensitive. Times are 24-hour `HH:MM`; window ends may be
`24:00`, which means midnight at the end of that day.

### Monitoring (no login required)

| Command | Description |
| --- | --- |
| `help` / `?` | List all commands |
| `status` | Firmware, clock, heap, Wi-Fi/MQTT, storage, schedule, occupancy and room status |
| `events [count]` | Newest access events first; `count` is `1..32` (default 10) |
| `schedule` | Show the weekly schedule, override state and last change |
| `time` | Show the current device clock |
| `whoami` | Show the current administrator session |

Example `status` output:

```
Firmware: 0.1.0
Uptime: 0d 00:04
Clock: 2026-10-30 10:00:00 (valid)
Heap: 234512 bytes free (minimum 228900)
Wi-Fi: disconnected  MQTT: disconnected
SD card: not mounted  Storage log: RAM only
Schedule: enabled  Override: inactive
Building occupancy: 2
Last access event: 2026-10-30 09:58:11
Rooms:
  Room 1: occupied, light ON, updated 12s ago
  Room 2: offline
```

### Authentication

| Command | Description |
| --- | --- |
| `login <user> <password>` | Start an administrator session |
| `logout` | End the administrator session |
| `passwd <old> <new>` | Change the administrator password (session required) |

- Default credentials are **`admin` / `ChangeMe123`**. The banner and `whoami`
  warn while this default is still active; change it during commissioning.
- A session expires after **300 s** of inactivity. Each authorised command
  refreshes the timer, and `status`/`whoami` show the remaining time.
- **3** consecutive failed logins lock the account for **60 s**. The lock
  is checked before the password hash, so a valid password is refused while
  the lockout is active.
- Passwords must be **8–64 characters** with at least one letter and one
  digit, and must differ from the old password.
- Passwords are stored as a per-device salted SHA-256 digest with 5000
  iterations, never in clear text. This is a prototype KDF; PBKDF2/HMAC would
  be the production upgrade.

### Access-schedule configuration (login required)

| Command | Description |
| --- | --- |
| `schedule set <day> <start> <end> [<start2> <end2>]` | Replace a day's one or two windows and enable the day |
| `schedule clear <day>` | Remove a day's windows (closes the day) |
| `schedule enable <day>` | Re-enable a day that still has windows configured |
| `schedule disable <day>` | Close a day but keep its windows for later |
| `schedule default` | Restore Mon–Fri 07:00–21:00, Sat 08:00–13:00, Sun closed |
| `schedule on` / `schedule off` | Enable or disable enforcement globally |
| `override on [minutes]` | Temporarily bypass the schedule (default 30, max 1440) |
| `override off` | Cancel the temporary override |

`<day>` accepts `sun mon tue wed thu fri sat` or full day names. Days are
indexed `0` (Sunday) to `6` (Saturday).

Defaults:

```
Sun closed
Mon 07:00-21:00
Tue 07:00-21:00
Wed 07:00-21:00
Thu 07:00-21:00
Fri 07:00-21:00
Sat 08:00-13:00
```

Examples:

```
cocis> login admin ChangeMe123
OK: logged in as admin
WARN: default password is active; change it with 'passwd <old> <new>'

cocis> schedule set mon 08:00 12:00 14:00 17:00
OK: Mon 08:00-12:00, 14:00-17:00 (day enabled)

cocis> schedule set fri 07:00 24:00
OK: Fri 07:00-24:00 (day enabled)

cocis> schedule disable sun
OK: Sun closed

cocis> override on 45
OK: override active until 2026-10-30 10:45:00
```

Every accepted change is written to NVS immediately and audited. If NVS
cannot be written the menu reports
`WARN: change applied in RAM but could not be persisted`; the change still
applies until the next reboot.

### Clock configuration (login required)

| Command | Description |
| --- | --- |
| `time set YYYY-MM-DD HH:MM:SS` | Set the wall clock used for schedule decisions |

The terminal has no network time source yet. Until an administrator sets the
clock (or SNTP is added), schedule decisions **fail closed**: entry cards are
denied with reason `clock not set; failing closed`, while exit is still
allowed.

## Access policy

1. **Exit is never blocked.** `access_schedule_check_exit()` always returns
   `ACCESS_ALLOWED`. A scheduled restriction must never trap a person inside
   the building.
2. **Entry** is decided in this order:
   1. No valid clock → deny (`ACCESS_DENIED_CLOCK_INVALID`).
   2. Enforcement switched off → allow (`ACCESS_ALLOWED_SCHEDULE_DISABLED`).
   3. Temporary override active → allow (`ACCESS_ALLOWED_OVERRIDE`).
   4. Day closed → deny (`ACCESS_DENIED_DAY_CLOSED`).
   5. Current minute inside a window → allow (`ACCESS_ALLOWED`), otherwise
      deny (`ACCESS_DENIED_OUTSIDE_HOURS`).
3. Window **start is inclusive, end is exclusive**: `07:00-21:00` allows
   07:00 and denies 21:00.
4. A day may have at most **two** non-overlapping windows, and an enabled day
   must have at least one. The schedule must have at least one active day
   unless enforcement is switched off.
5. Invalid changes are atomic: the live schedule is only replaced after the
   whole candidate value validates, so a rejected command leaves the previous
   configuration intact.

`override` is intended for short, audited exceptions (for example a visitor or
a maintenance window). It is not a replacement for editing the schedule.

## Integration points

### RFID reader (`entrance/src/rfid.c`)

The card handler calls:

```c
const bool granted = management_access_check(uid_str, 'I');
```

`management_access_check` applies the policy, records an entry in the event
buffer (with UID, direction, decision and reason) and logs an `ACCESS_GRANTED`
or `ACCESS_DENIED` audit line. The current entrance has a single reader, so
presentations are treated as entry attempts. When the exit reader is added,
call `management_access_check(uid_str, 'O')` there; egress will always be
allowed.

### Storage owner

The SD-card module is not compiled into the current build. When it is wired up,
call:

```c
management_app_set_storage(true);
```

and optionally register the SD log as the audit sink:

```c
management_app_set_audit_hook(sd_log_append);
```

### Network / room terminals

The entrance has no Wi-Fi/MQTT yet. Once it does, call
`management_app_set_connectivity(true, true)` when connected and feed room
messages into:

```c
management_app_update_room(0, occupied, light_on); /* Room 1 */
management_app_update_room(1, occupied, light_on); /* Room 2 */
```

Rooms are marked offline automatically if no update arrives for 120 s. The
`status` command then shows `Room N: offline`.

The room terminals publish to `cocis/room1/status` and `cocis/room2/status`.
Note: on `main` the Room 2 firmware still publishes to the Room 1 topic and
labels its JSON `"Room 1"`; this is a cross-team bug tracked in
`docs/system-test-plan.md`, not something the entrance can compensate for.

### Coordination hooks

| Function | Owner | Purpose |
| --- | --- | --- |
| `management_app_start()` | entrance `app_main` | Initialise NVS, policy, menu and console |
| `management_access_check(uid, 'I'/'O')` | RFID/door owner | Apply policy and record the event |
| `management_app_set_storage(bool)` | SD-card owner | Reflect SD mount state in `status` |
| `management_app_set_connectivity(bool, bool)` | network owner | Reflect Wi-Fi/MQTT state in `status` |
| `management_app_update_room(index, occupied, light)` | room terminals via MQTT | Feed room status |
| `management_app_set_audit_hook(fn)` | storage owner | Persist audit events |

## Persistence

Configuration is stored in NVS namespace `cocis_cfg` as two versioned blobs:

- `schedule` — `magic`, `version`, `access_schedule_t`
- `creds` — `magic`, `version`, `admin_credentials_t`

Stored values are re-validated on load. If a blob is missing, corrupt or from
an unknown version, the defaults are used (default schedule, default
credentials). Erase NVS to return the terminal to its factory state.

## Unit tests

```powershell
powershell -ExecutionPolicy Bypass -File tools/management-tests/run_tests.ps1
```

See `tools/management-tests/README.md` for details.
