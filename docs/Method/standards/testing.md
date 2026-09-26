# Standard — Testing

Testing is part of the build contract. Every behaviour rule in the FSD is
verifiable, and every change ships the test that proves it.

## Tiers

Order tests by cost. Use the cheapest tier that can catch the failure, and
reserve the expensive ones for behaviour no cheaper tier can reach.

| Tier | Runs on | Speed | Catches |
|---|---|---|---|
| **host** | Dev machine, plain compiler, no ESP-IDF, no hardware | ms, every commit | Pure logic: topic formatting, JSON bodies, sensor payload formatting, device-id and range validation |
| **target** | One ESP32-S3 board, real ESP-IDF, USB or UART attached | seconds, pre-merge | NVS persistence, WiFi association, MQTT wire behaviour, the console REPL, flash writes, RTOS behaviour |
| **bench** | Board plus real peers: AP, Mosquitto, Smart_Server, an image server | minutes, pre-release | End-to-end recovery, reconnection, wall-clock timing, the Last Will, OTA |

**Tier availability today:** no host test target is wired, no ESP32 is connected
and no testbench Pi exists. A case whose tier is unavailable is **not run**; it
is recorded as unproven, never as passing. The FSD records the same fact in its
§1.6 and §21.0.

## Rules

- **Every Must/Should rule has a test case.** A rule with no case is an untested
  contract, surfaced as a gap rather than left silent.
- **Every change ships its test.** Not done until the case is written and the
  test passes.
- **Bug fixes write the regression test first**, and it must fail before the fix
  — a regression test that never failed proves nothing.
- **Test cases cite requirement IDs.** Each case names the requirement(s) it
  verifies, so coverage is computed rather than asserted.
- **Coverage is generated, never hand-maintained.** A hand-filled
  "Covered / GAP" column is stale the moment a test changes.
- **State machines are covered per transition, not per state.** Reaching a state
  proves nothing about the edges into it; the FSD's §5.3 transition table is the
  case list.
- **Prohibited outcomes are asserted, not implied.** A recovery case fails if
  the DUT recovered by rebooting, re-provisioning or replaying a stale payload.
  Every contract in the FSD names those outcomes; the case must check them.
- **No silent gaps.** A specified-but-unbuilt case is marked with its
  prerequisite so the coverage picture stays honest.
- **The suite is green before commit.** A failing suite blocks the commit.

## Running a case

Setup, teardown and evidence rules that hold for every run. They live here
rather than in a file beside this one: a procedure restates the rules it sits
next to, and two documents drift.

- **Capture evidence before resetting anything** — a reset destroys the reason
  for the reset.
- **A timeout is a failure, not an absent result.** A device that crashes prints
  nothing, so "no failure seen" is not a pass. Require a completion marker.
- **A null observation accuses the instrument before it accuses the subject.**
  Silence, an empty capture or a timeout does not distinguish "the system did
  nothing" from "the measurement did not work". Show a **positive control in the
  same session** before concluding anything from an absence.
- **An experiment whose control produces nothing is void, not negative.** Say so
  and stop; a comparison against a dead baseline compares nothing to nothing.
- **Prefer the instrument's own API** to raw access to the thing it manages.
- **On the ESP32-S3's native USB, the serial control lines are boot-mode
  signals** — DTR asserted means download mode, RTS asserted means reset. A
  client that opens the port to watch the device stops it and looks exactly like
  a firmware hang. Lower both lines before opening the port.
- **Verify the precondition; if it fails record `not done`, never `failed`.**
  A failure claims you learned something, and with a broken baseline you did not.
- **Leave the rig as you found it** — a fault left set corrupts every later test.

## Bench procedures

Per-procedure setup and teardown sections are added when the bench exists and
its capabilities are declared (Phase 1, `/harness`). Until then, every bench-tier
case is unrun and its prerequisite is the bench itself.

## Security testing

Security cases are grounded in the FSD's declared threat profile (§18), not
imported wholesale.

- Every security-relevant rule — credential handling, input validation, OTA
  trust, protocol exposure — has a case tagged with its requirement ID.
- A security criterion must be able to **fail**. "Encrypted or obfuscated"
  cannot; "the literal passphrase does not appear in a raw partition dump" can.
- Where the profile does not justify a protection, the accepted risk is recorded
  in FSD §18.2 rather than a case being written for a requirement nobody
  implements.
