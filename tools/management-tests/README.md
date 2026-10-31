# Management interface host tests

This directory contains plain-C unit tests for the portable part of the
management interface in `entrance/src/management/`.

The tests deliberately do **not** depend on ESP-IDF. They compile the same C
files that the firmware compiles, but on a normal PC, so configuration logic,
validation and the command parser can be checked quickly without Wokwi or an
ESP32 toolchain.

## What is covered

| Test file | Area |
| --- | --- |
| `test_time.c` | `HH:MM` / date-time parsing, formatting, weekday maths, epoch conversion |
| `test_schedule.c` | Schedule defaults, window validation, overlaps, access decisions, overrides, exit always allowed |
| `test_auth.c` | Login, lockout, session timeout, password policy, salted hashing, password change |
| `test_events.c` | Access-event ring buffer, ordering, formatting |
| `test_status.c` | Status snapshot, room updates, offline expiry, report formatting |
| `test_menu.c` | End-to-end serial commands: public vs protected commands, invalid input, save failures, lockout |

The ESP-IDF adapter files (`management_app.c`, `management_console.c`,
`management_store.c`) are not part of the host tests because they need the
ESP32 SDK. They are covered by the Wokwi test plan in
`docs/system-test-plan.md`.

## Running the tests

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File tools/management-tests/run_tests.ps1
```

The script uses the `zig cc` compiler that ships with the Python `ziglang`
package:

```powershell
python -m pip install ziglang
```

To use another compiler (for example MinGW GCC or clang), set
`COCIS_TEST_CC`:

```powershell
$env:COCIS_TEST_CC = "gcc"
powershell -ExecutionPolicy Bypass -File tools/management-tests/run_tests.ps1
```

A successful run ends with:

```
322 checks, 0 failed
```

The binary is written to `tools/management-tests/build/`, which is ignored by
git.

## Adding a test

1. Add checks in an existing `test_*.c`, or add a new file and register its
   `run_*_tests()` function in `test_main.c` / `test_util.h`.
2. Keep assertions in the `CHECK`, `CHECK_INT`, `CHECK_STR` and
   `CHECK_CONTAINS` style so failures show file, line and values.
3. Re-run the script; the `-Werror` build fails on warnings, so warnings are
   treated as defects.
