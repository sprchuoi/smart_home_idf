# Gate — AI harnessed

**Closes:** Phase 1 (`/harness`). **Opens:** Phase 2 (`/commission`).

> **Standing exclusion — evidence is committed artefacts and live bench answers
> only.** Never memory, transcripts, a prior session's notes, or the author's
> account of what was done. The session that built the harness is the worst
> judge of whether it landed.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | A test plan exists with capabilities declared and the journey seeded | `testing/test-plan.yaml` | `capabilities:` present; `journeys:` present; one test per journey step |
| 2 | Blocked is computed, never typed | `testing/test-plan.yaml` | No test carries a hard-coded blocked status; unavailability comes from `needs:` against `capabilities:` |
| 3 | A debugging agenda exists and covers every `unproven` project capability | `testing/debugging-agenda.md` vs the plan's `capabilities:` | Every `available: unproven` project entry appears in the agenda; the testbench is *not* on it |
| 4 | The testing standard exists — setup/teardown/evidence rules plus shared procedures, one file | `docs/Method/standards/testing.md` | The file exists and is committed |
| 5 | Firmware hooks match the FSD's scope — no module the spec never asked for | `main/`, `CMakeLists.txt`, FSD §4.5 | Every integrated module traces to a requirement; orphaned `main/services/audio/`, `main/core/audio/`, `main/services/wakeup/` are out of the build and recorded as retired |
| 6 | CI runs build on push, release on tag, and carries a release-verify job | `.github/workflows/ci.yml` | Jobs `build`, `release`, `release-verify` all present |
| 7 | CI has actually run green at least once | GitHub Actions | `gh run list -L 5` shows a green run on this workflow |
| 8 | Gate checks generated, one per gate, project-specific | `testing/gates/` | Five files; each names this project's paths, ids, DUT, capabilities and CI |
| 9 | A GitHub Actions runner for the bench is installed (registration is a human grant) | repository runners | A runner with labels `[self-hosted, testbench]` is registered, or its absence is recorded as `self-hosted-runner: unproven` |

## Mechanical checks

```bash
# 1-2. Plan and capabilities
python3 -c "import yaml;d=yaml.safe_load(open('testing/test-plan.yaml'));print(len(d['capabilities']),'capabilities');print(len(d['tests']),'tests')"
grep -c "available: unproven" testing/test-plan.yaml

# 3. Agenda covers the unproven capabilities
test -f testing/debugging-agenda.md
grep -c '^| [0-9]' testing/debugging-agenda.md

# 4. Testing standard
test -f docs/Method/standards/testing.md && git log --oneline -1 -- docs/Method/standards/testing.md

# 5. Firmware hooks vs scope
ls main/services main/core 2>/dev/null
grep -n "audio\|wakeup" CMakeLists.txt main/CMakeLists.txt 2>/dev/null || echo "audio/wakeup not in the build"

# 6-7. CI
grep -nE "^  (build|release|release-verify):" .github/workflows/ci.yml
gh run list -L 5 --workflow ci.yml

# 8. Gates
ls testing/gates/ | wc -l

# 9. Runner
gh api repos/{owner}/{repo}/actions/runners --jq '.runners[].labels[].name' 2>/dev/null | sort -u
```

## Judgement checks — answer each with a quotation

1. **The behavioural check, and the only one that matters.** Open a fresh
   `/build` session with `testing/test-plan.yaml` and the repository, nothing
   else — no summary of this session. Can it state its position and name the
   next act **without asking anything**? Quote its answer. If it stalls, the
   missing input is the finding, and it belongs to the step that left it out.
2. Does every journey step have its own test, or was the journey collapsed into
   one end-to-end test? One test per step; a single test that only says "nothing
   arrived" cannot say where. Quote the test ids.
3. Does each `unproven` capability's agenda row name **what proves it**, or only
   that it is unproven? An agenda row without a proof is a worry, not a task.
4. Does the plan cite real FSD ids? Spot-check three `verifies:` values against
   `docs/Functionality/FSD.md`. An invented id makes traceability a guess.

## Traps

Ways this project could look harnessed and not be:

- **`testing/test-plan.yaml` is 650 lines, so it reads as complete.** The
  journey tests all have `impl: ""` and `status: not done` — the plan is a
  backlog, and every hardware test is blocked on `dut-board: unproven`. A
  checker that counts entries reports harnessed; a checker that reads `impl:`
  reports the truth.
- **The heartbeat conflict is unresolved by design.** `JRN-06` withholds its
  assertion because the user-approved wording ("every 2 s, `alive | wifi:up |
  ip:...`") contradicts the code and FSD (30 s, `state`/`wifi`/`mqtt`, no `ip`;
  OD-15). This is a *correctness* finding to escalate, not a missing test to
  paper over. Do not accept an assertion that silently picked one side.
- **`release-verify` exists but has never run.** Its `if:` is gated on the
  repository variable `TESTBENCH_READY`. The job's presence is not evidence that
  a released artifact was ever exercised. Check the variable and the runner.
- **The release job is downstream of `release-verify`.** It uses
  `always() && !failure()` so a *skip* does not suppress publishing. A checker
  who sees `needs: [build, release-verify]` and concludes releases are blocked
  is wrong — and one who removes `always()` to "simplify" breaks every tag push.
- **OD-4 and OD-5 are known code defects that will fail the journey for the
  wrong reason.** `wifi_clear` does not erase, and `ssid[32]` overflows a
  32-character SSID. A harness declared ready while a journey step is known to
  fail for a recorded defect is not ready.
- **`docs/` contains a Sphinx set.** Presence of documentation is not presence
  of the testing standard.

## Verdict

`OPEN` · or `SHUT` with each unmet requirement named, the evidence looked for,
and what was found instead. Run it in a fresh checker. On `SHUT`, loop back to
the step owning each finding and re-run the **whole** check with a **new**
checker.
