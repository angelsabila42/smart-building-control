# CoCIS Smart Building Control and Monitoring System

A pilot smart-building system for the College of Computing and Information Sciences (CoCIS), Makerere University. The project uses ESP32 microcontrollers and the Wokwi simulation platform to demonstrate RFID-based access control, building and room occupancy monitoring, and automated lighting.

## Background

CoCIS needs better control over access to institutional facilities and equipment, as well as ways to reduce electricity use when rooms are unoccupied. This project proposes an integrated system for the main entrance and two lecture rooms.

## Scope

- **Main entrance:** Separate entry and exit points. Users scan an RFID tag at the appropriate point; the system validates the tag, controls the associated door mechanism, and records the event.
- **Lecture Room 1 and Lecture Room 2:** Occupancy detection controls each room's lights, switching them off when the room is unoccupied.
- **Three control terminals:** One ESP32 terminal for each lecture room and one for the entrance. The terminals coordinate as parts of one system.
- **Data and management:** Access events, occupancy information, and system activity can be stored on an SD card. A serial interface provides menus or commands for monitoring and configuration.

## System Goals

- Identify users with RFID tags and grant or deny access according to authorization.
- Record entry and exit events so authorized management can determine current building occupancy.
- Monitor each pilot room and control its lighting based on occupancy.
- Provide management with useful status information, including access records, current occupancy, room occupancy, and lighting state.
- Allow authenticated, authorized personnel to configure the permitted access schedule.
- Keep records and configuration consistent across the ESP32 terminals, including when operations happen concurrently.
- Demonstrate the design in Wokwi.

## Proposed Architecture

| Terminal | Main responsibilities |
| --- | --- |
| Main entrance ESP32 | Read RFID tags at entry and exit, validate access, control the door mechanisms, and track access events and building occupancy. |
| Lecture Room 1 ESP32 | Detect room occupancy and control the room lights. |
| Lecture Room 2 ESP32 | Detect room occupancy and control the room lights. |

The terminals need a communication and synchronization approach so shared records, occupancy state, and configuration remain reliable. The communication method and specific sensors, door mechanisms, and lighting hardware are implementation decisions to be documented as the prototype develops.

## Access Schedule and Safety

The access schedule is configurable through an appropriate management interface and may only be changed by authenticated, authorized personnel. The system should apply the configured schedule consistently to normal access and retain accurate occupancy records.

**Life safety takes precedence over schedule enforcement.** A schedule must not prevent emergency egress or trap anyone inside. Any policy restricting exit outside normal hours requires review against applicable fire and building-safety requirements, with a suitable emergency release and safe failure behavior.

## Management Interface and Data

The serial interface is intended to provide menus or commands for monitoring and configuration. It should expose relevant information such as:

- Recent access events and current building occupancy
- Occupancy and lighting state for both lecture rooms
- Current access schedule
- Authorized schedule updates and other protected configuration
- System and storage status, where available

An SD card module is proposed for persistent records and other data that exceed the microcontrollers' available storage. The implementation should define how terminals share or reconcile records and how it handles communication or storage failures.

## Management Interface (Implemented)

The entrance terminal implements a serial management interface on UART0 at **115200 baud**. It provides monitoring commands (`help`, `status`, `events`, `schedule`, `time`, `whoami`) and protected configuration commands for the access schedule, clock and administrator password.

- Configuration changes require `login <user> <password>`. Sessions expire after 5 minutes of inactivity, and three failed logins lock the account for 60 seconds.
- The default administrator account is `admin` / `ChangeMe123`; change it during commissioning with `passwd <old> <new>`.
- Schedule windows are validated (start before end, no overlaps, at most two windows per day, `24:00` end allowed) and invalid commands leave the previous configuration untouched.
- **Exit is never restricted by the schedule.** Entry decisions fail closed if the clock has not been set, and administrators can grant a short audited override.
- The schedule and credentials are stored in NVS and survive reboots.

Example session:

```
cocis> status
cocis> login admin ChangeMe123
OK: logged in as admin
cocis> schedule set mon 08:00 12:00 14:00 17:00
OK: Mon 08:00-12:00, 14:00-17:00 (day enabled)
cocis> schedule on
OK: schedule enforcement ENABLED
cocis> time set 2026-10-30 10:00:00
OK: clock set to 2026-10-30 10:00:00
cocis> override on 30
OK: override active until 2026-10-30 10:30:00
```

Full command reference, the access-policy rules and integration hooks are in [`docs/management-interface.md`](docs/management-interface.md). Cross-terminal test scenarios are in [`docs/system-test-plan.md`](docs/system-test-plan.md).

The portable management modules are covered by host unit tests that run without ESP-IDF or Wokwi:

```powershell
powershell -ExecutionPolicy Bypass -File tools/management-tests/run_tests.ps1
```

See [`tools/management-tests/README.md`](tools/management-tests/README.md) for details.

## Building the firmware

Each terminal is its own PlatformIO project (`entrance/`, `lecture_room_1/`, `lecture_room_2/`), using the ESP-IDF framework:

```powershell
cd entrance
pio run --environment esp32-s3-devkitc-1
```

**Important:** PlatformIO rejects project paths that contain a space (`Detected a whitespace character in project paths`). Clone the repository to a path such as `C:\Users\<you>\smart-building-control`, not a folder like `Desktop\My Projects\...`.

## Simulation and Implementation

The assignment calls for a Wokwi simulation using ESP32 microcontrollers. The system should demonstrate the behavior of the three terminals and the system goals above. As implementation details are finalized, add the Wokwi project link, wiring diagrams, required components, build instructions, and example serial commands here.

## Project Status

This README describes the project requirements and proposed design. It does not imply that the hardware, firmware, or simulation has already been implemented.
