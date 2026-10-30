# System-Level Test Plan (Member 5)

This plan coordinates testing across the three terminals:

| Terminal | Owner focus | Wokwi project |
| --- | --- | --- |
| Main entrance | RFID, SD card, management interface | `entrance/` |
| Lecture Room 1 | PIR occupancy → light control, MQTT | `lecture_room_1/` |
| Lecture Room 2 | PIR occupancy → light control, MQTT | `lecture_room_2/` |

Member 5 owns the entrance management interface, authorization, access
decisions and this system-level test coordination. Anything marked
**(blocked)** depends on another member's work and should be reported, not
worked around locally.

## 1. Preparation

1. Install ESP-IDF v5.x and the Wokwi VS Code extension.
2. Open each terminal folder as its own ESP-IDF project.
3. Confirm the tools build:
   ```powershell
   idf.py build
   ```
4. For the management interface, also run the host tests:
   ```powershell
   powershell -ExecutionPolicy Bypass -File tools/management-tests/run_tests.ps1
   ```
   Expected result: `322 checks, 0 failed`.
5. Start the Wokwi simulator and open the serial monitor at **115200 baud**.

## 2. Entrance terminal tests

### T1 — Boot and menu

| Step | Expected |
| --- | --- |
| Start simulation | Banner, `Type 'help' for commands.`, default-password warning |
| Type `help` | Full command list including protected commands |
| Type `status` | Firmware, clock, heap, Wi-Fi/MQTT, storage, schedule, rooms |
| Type `time` | `Clock: NOT SET` on a cold boot |

### T2 — Authentication

| Step | Expected |
| --- | --- |
| `schedule set mon 08:00 09:00` (no login) | `ERR: authentication required` |
| `login admin wrong` ×3 | `invalid credentials (2 attempts left)`, `(1 attempt left)`, `locked out for 60s` |
| `login admin ChangeMe123` while locked | `ERR: too many failed attempts` |
| Wait 60 s, `login admin ChangeMe123` | `OK: logged in as admin`, default-password warning |
| `whoami` | `Authenticated as admin; idle timeout in ...` |
| Idle 300 s, `whoami` | Session expired / `Not authenticated` |
| `passwd ChangeMe123 Exampl3Pass` | `OK: administrator password changed` |
| `logout`, `login admin ChangeMe123` | Invalid credentials |
| `login admin Exampl3Pass` | Logged in |

### T3 — Schedule validation

| Step | Expected |
| --- | --- |
| `schedule` | Default Mon–Fri 07:00–21:00, Sat 08:00–13:00, Sun closed |
| `schedule set mon 08:00 12:00 14:00 17:00` | Two windows accepted |
| `schedule set mon 07:00 13:00 12:00 15:00` | `ERR: schedule rejected: windows overlap`; old value kept |
| `schedule set mon 10:00 08:00` | Start-before-end error |
| `schedule set mon 25:00 26:00` | Invalid start time |
| `schedule set funday 08:00 09:00` | Invalid day |
| `schedule set fri 18:00 24:00` | Accepted (24:00 end) |
| `schedule disable sun` then `schedule enable sun` | Window preserved |
| Clear every day except one, then clear the last | `ERR: that would leave no active day` |
| `schedule default` | Defaults restored |
| Reboot | Schedule survives (NVS) |
| Erase NVS, reboot | Defaults restored |

### T4 — Clock and fail-closed behaviour

| Step | Expected |
| --- | --- |
| Cold boot (no clock), scan an RFID tag | `ACCESS DENIED`, reason `clock not set; failing closed` |
| `time set 2026-10-30 10:00:00` | Clock accepted |
| `time set 2026-13-01 10:00:00` | Invalid date rejected |
| `time set 2026-10-30 25:00:00` | Invalid time rejected |
| Scan a tag inside a window | `ACCESS GRANTED` |
| Scan a tag outside a window | `ACCESS DENIED`, reason `outside scheduled hours` |
| Scan a tag on a closed day | `ACCESS DENIED`, reason `no access configured for this day` |
| `override on 2`, scan outside the window | `ACCESS GRANTED`, reason `administrator override active` |
| After override expiry, scan again | `ACCESS DENIED` |
| `schedule off`, scan at any time | `ACCESS GRANTED`, reason `schedule disabled` |
| Scan at an exit point (when exit reader exists) | Always granted, reason `egress is never blocked` |

### T5 — Events and status

| Step | Expected |
| --- | --- |
| `events` | Newest first; timestamp, UID, IN/OUT, GRANTED/DENIED, reason |
| `events 0` / `events 99` / `events abc` | Count error |
| More than 32 events | Oldest entries dropped, newest kept |
| `status` after grants | Building occupancy increases on entry, decreases on exit |
| Remove a room status feed | Room shows `offline` after 120 s |

### T6 — Persistence

| Step | Expected |
| --- | --- |
| Change the schedule and password, reboot | Both restored |
| Power-cycle during normal use | Configuration intact |
| Invalid/corrupt NVS blob | Defaults used, warning logged |

## 3. Room terminal tests

Repeat for `lecture_room_1` and `lecture_room_2`.

### T7 — Occupancy and lighting

| Step | Expected |
| --- | --- |
| No motion | Light off |
| Motion on PIR1 then PIR2 within the sequence timeout | Occupancy counted, light on |
| No motion for the configured timeout | Light off |
| Publish a status message | JSON on the room's MQTT topic |

### T8 — MQTT

| Step | Expected |
| --- | --- |
| Connect to `broker.hivemq.com` | `MQTT_EVENT_CONNECTED` in the log |
| Publish | `{"room":"Room N","occupancy":X,"light":true/false}` on `cocis/roomN/status` |
| Broker unreachable | Warning, no crash |

**(blocked / cross-team defect)** Room 2 currently publishes to
`cocis/room1/status` and labels its JSON `"Room 1"` (`lecture_room_2/src/app_state.h`
and `lecture_room_2/src/controller/controller.c`). Until the Room 2 owner fixes
the topic and label, Room 2 reports are indistinguishable from Room 1. Do not
merge a temporary workaround on the entrance; record the failure in the test
log.

## 4. End-to-end scenarios

### E1 — Normal access day

1. Set the clock to a Monday inside the configured window.
2. Scan an authorized tag at the entrance.
3. Expect `ACCESS GRANTED`, occupancy +1, an event entry, and an audit line.
4. Enter Room 1, trigger motion, confirm the light turns on and a status
   message is published.
5. Leave Room 1, wait for the timeout, confirm the light turns off.
6. Scan at the exit point (or simulate an exit) and confirm occupancy −1.

### E2 — Outside hours

1. Set the clock to 22:00 on a weekday.
2. Scan a tag: `ACCESS DENIED` / `outside scheduled hours`.
3. Confirm no occupancy change and a recorded event.
4. Confirm the exit decision is still allowed.

### E3 — Clock loss

1. Reboot the entrance with NVS erased so the clock is unset.
2. Scan a tag: denied, reason `clock not set; failing closed`.
3. Set the clock via the menu, then scan again: allowed inside the window.

### E4 — Administrator override during an event

1. Outside scheduled hours, log in and run `override on 5`.
2. Scan a tag: granted with the override reason.
3. Wait 5 minutes and scan again: denied.
4. Confirm both decisions appear in `events`.

### E5 — Configuration survives reboot

1. Change the schedule, change the password, add a two-window day.
2. Reboot and confirm `schedule` and `login` use the new values.

## 5. Cross-team integration checklist

| Check | Owner | Status |
| --- | --- | --- |
| Entrance schedule + RFID decisions | Member 5 | Done (host tests) |
| Entrance command menu and status | Member 5 | Done (host tests) |
| Entrance SD card logging wired | Storage owner | Pending — SD module not in `SRC_DIRS` |
| Entrance Wi-Fi/MQTT | Network owner | Pending — no networking on entrance yet |
| Room 1 occupancy → MQTT | Room 1 owner | Working on `main` |
| Room 2 topic/label fix | Room 2 owner | Pending — currently publishes as Room 1 |
| Room status consumed by entrance `status` | Member 5 + network owner | Hooked via `management_app_update_room()`; needs MQTT subscription |
| Shared broker (`broker.hivemq.com`) | All | Working for rooms |

## 6. Reporting

For each run record:

- date/time and Wokwi project,
- firmware commit (`git rev-parse --short HEAD`),
- command or stimulus used,
- observed output (paste the serial monitor lines),
- pass/fail and, for failures, the owner and the expected behaviour.

Blocked items should be reported to the owning member with the failing test ID
(for example `T8: Room 2 publishes to cocis/room1/status`) rather than patched
inside another member's module.

## 7. Known limitations to state in the report

- The entrance clock is manual. Until SNTP is added, the clock resets on a
  cold boot and entry fails closed.
- Building occupancy is a counter, not identity-aware: re-scanning the same
  tag increments it again. It is indicative, not authoritative.
- The entrance has one reader today; entry and exit direction support exists
  in the API but the exit reader is not wired.
- Password hashing is salted, iterated SHA-256 for the prototype; production
  would use PBKDF2/HMAC.
- The entrance console and RFID task share the schedule and status snapshot
  without a full lock; changes are short and the event buffer is mutex-protected.
