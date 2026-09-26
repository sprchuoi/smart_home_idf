# Gate — DUT ready

**Closes:** Phase 2 (`/commission`). **Opens:** Phase 3 (`/build`), wave 1.

> **Standing exclusion — evidence is committed artefacts and live bench answers
> only.** Never memory, transcripts, or the author's account. A failing test
> must be allowed to mean the code; this gate is what buys that.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | A testbench record exists, with a live answer | `git ls-files` + a bench call | `$(bench)/api/info` answers and the record is committed |
| 2 | The DUT is the unit the FSD names | FSD §2.2, `bench/devices` | The slot reports `esp32s3`; flash is 16 MB; PSRAM is 8 MB octal — not merely "an ESP32" |
| 3 | The forward path is proven end to end | CI run → artifact `flash_args` → serial marker | A CI-built artifact is flashed using **its own** `flash_args` offsets and the DUT then prints the §17 boot banner |
| 4 | Every project-side `unproven` capability in the plan is resolved or explicitly still blocked | `testing/test-plan.yaml` `capabilities:`; `testing/debugging-agenda.md` | `dut-board`, `smart-server-stack`, `ota-image-server` are `yes`, or their block is stated |
| 5 | The debugging agenda is empty of project-side items | `testing/debugging-agenda.md` | Items 1–6 observed working, each with its measurement |
| 6 | Peers are recorded and proven | Plan capabilities `bench-mqtt-broker`, `bench-wifi-softap`, `smart-server-stack` | The broker accepts a session; the SoftAP yields a lease; Smart_Server answers |
| 7 | The board's identity is recorded | `bench/devices` | Slot label, chip, MAC/device id, and which physical unit it is |

## Mechanical checks

```bash
BENCH="${BENCH:?set BENCH to the bench IP, never a .local name}"

# 1-3, 6. The bench answers and carries the right silicon
curl -sf "http://$BENCH:8080/api/info" | head -40
curl -sf "http://$BENCH:8080/api/devices" | python3 -c '
import json,sys
for s in json.load(sys.stdin)["slots"]:
    if s.get("detected_chip"): print(s["label"], s["detected_chip"], s.get("debugging"))
'

# 3. Boot marker on the DUT
python3 - <<'EOF'
# The marker this project asserts, from FSD NFR-17.2 / FR-12.1.
need = ["esp-idf", "FreeRTOS"]          # banner
prompt = "esp32>"
print("assert the serial capture contains:", need, "and the prompt", prompt)
EOF

# 4. Capabilities still unproven
grep -n "available: unproven" testing/test-plan.yaml

# 5. Agenda items outstanding
grep -cE '^\| [0-9]+ \|' testing/debugging-agenda.md
```

## Judgement checks — answer each with a quotation

1. Is the unit on the bench **the unit the FSD specifies** — N16R8, 8 MB
   *octal* PSRAM — or merely an ESP32-S3? Quote what the bench reports and the
   FSD clause it is matched against. A wrong PSRAM mode boot-loops rather than
   failing, so "it boots" does not settle it.
2. Was the flashed image built by CI, and were its offsets taken from its own
   `flash_args`? Quote the CI run and the offsets. A binary from a local `build/`
   proves nothing about the bytes anyone else gets.
3. For each agenda item marked done: what **test or declared check** produced
   the observation? An observation with no test behind it has no owner and never
   re-runs — it is a finding, not evidence.
4. Is the bench's own quality being *depended on* rather than re-proven here? A
   project never proves the testbench; a bench fault is fixed in the bench repo,
   and only once DUT evidence disproves the declaration.

## Traps

Ways this project could look ready and not be:

- **OD-4 (`wifi_clear` does not erase) will make "credentials persist" look
  correct.** A DUT whose store was never actually cleared reports credentials
  present — which is the bug, not the behaviour. Verify the store is erased, not
  that `hasCredentials()` is true.
- **OD-5 (`ssid[32]`) makes a 32-character SSID fail as if association were
  broken.** Commission with a shorter SSID, or fix OD-5 first; otherwise the
  gate blames the radio.
- **OD-14: WiFi reconnect stops permanently after 10 failed attempts.** A bench
  that has been up through a few outages can look "already associated" while the
  retry path is dead. Reset before asserting association.
- **Nothing has ever run on this board.** The first successful boot is the
  milestone, so it is tempting to declare the gate open on the boot banner
  alone. The gate also wants the *forward path* (CI artifact → its own offsets →
  observed marker), which is what makes later failures attributable.
- **The bench not existing.** `testbench-installed` is `no` today. Until a Pi
  exists and answers `/api/info`, this gate cannot open at all, and no
  target- or bench-tier result in the plan means anything.
- **`smart-server-stack` is unproven too.** The journey's step 7 has a
  dashboard checkpoint that observes an *external* system. Opening DUT ready
  while Smart_Server has never received from a real node leaves JRN-07
  permanently red for a reason that is not the firmware.

## Verdict

`OPEN` · or `SHUT` with each unmet requirement named, the evidence looked for,
and what was found instead. A checker runs declared tests and reads
instruments; it never improvises a procedure. If no declared test produces a
required observation, that **is** the finding.
