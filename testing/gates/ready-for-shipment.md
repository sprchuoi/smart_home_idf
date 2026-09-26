# Gate — Ready for shipment

**Closes:** Phase 3 (`/build`). **Opens:** the act of tagging.

> **Standing exclusion — evidence is committed artefacts, plan results and live
> bench answers only.** Never memory, transcripts, or the author's account. A
> test that passed last month against different code is not evidence about this
> commit.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Every requirement is met — meaning every test verifying it is `successful`, **including the check that its prohibited outcome did not happen** | `testing/test-plan.yaml` results vs `docs/Functionality/FSD.md` | No requirement whose verifier is failed, blocked or unwritten |
| 2 | The journey is green | `JRN-01`…`JRN-07` | Each of the seven `successful` |
| 3 | Every declared test carries a non-empty `impl:` | `testing/test-plan.yaml` | No `impl: ""` remains among tests marked ready |
| 4 | Reconcile is empty in both directions | plan `impl:` vs `tests/{host,target,bench}/` | No declared-but-unwritten; no executable no entry points at |
| 5 | The user manual is current | `docs/UserDocumentation/`, the Sphinx OPERATE pages | Commands and status match the firmware actually built |
| 6 | Prohibited outcomes were checked, not merely absent from a happy-path run | each contract's `prohibited:` clause | A negative or deviation test exercises it |
| 7 | The version in the image is the version claimed | `esp_app_desc` in the built image vs the tag | They agree (see the version-provenance trap) |

## Mechanical checks

```bash
# 1-2. Results
python3 -c "
import yaml;d=yaml.safe_load(open('testing/test-plan.yaml'))
for t in d['tests']:
    print(t['id'], t.get('status'), repr(t.get('impl','')))
"

# 3-4. Reconcile, both directions
grep -n 'impl: ""' testing/test-plan.yaml
find tests -type f \( -name 'test_*.py' -o -name '*.c' \) 2>/dev/null | sort

# 5. Manual current
ls docs/UserDocumentation/ ; git log --oneline -1 -- docs/UserDocumentation/

# 7. Version provenance
idf.py --version 2>/dev/null || echo "no local toolchain (expected: CI builds)"
strings build/smart_home.bin 2>/dev/null | grep -i "$(git describe --tags --always)"
```

## Judgement checks — answer each with a quotation

1. For three requirements picked at random: quote the requirement, quote its
   contract, quote the test result. Does the result actually observe **what the
   contract says**, or something adjacent that happens to pass?
2. Was any test "fixed" rather than the code? Compare `git log` on `tests/`
   against the firmware changes in the same window. A test edited to match new
   behaviour without a spec amendment is a spec defect, not a passing test.
3. Does any advisory `Should` requirement carry no test at all? That is
   permitted; it is not permitted to be silently counted as met.
4. Is the user manual describing the firmware **as built**, or the firmware as
   intended? Quote a command from the manual against the code that implements it.

## Traps

Ways this project could look shippable and not be:

- **`JRN-06` has no assertion.** It withholds one because the approved journey
  wording contradicts the FSD and the code (30 s, `state`/`wifi`/`mqtt`, no
  `ip`; OD-15). A green board cannot make it pass — the conflict must be
  resolved in `/define` first. Counting it as met because the firmware "looks
  right" is the failure this trap exists for.
- **`JRN-07`'s dashboard checkpoint observes Smart_Server, not the DUT.**
  A green firmware test with a red dashboard is not a firmware failure, and a
  red dashboard silently marking the firmware unverified is the mirror error.
- **OD-4 and OD-5 are still open code defects.** Shipping with a known
  `wifi_clear` that does not clear, and an SSID buffer that cannot hold a legal
  32-character SSID, means a requirement (`FR-7.7`) is documented as
  *currently unmeetable*. That is a shipment blocker or an explicit accepted
  deviation — not something to pass over.
- **Prohibited outcomes are easy to skip.** `FR-7.4` (reconnect stops after 10
  failures), `FR-6.16` (no reading while disconnected) and `NFR-11.13` (rollback
  disabled) all describe behaviour that a happy-path run never exercises.
- **Version provenance degrades quietly.** The build job runs plain
  `idf.py build` with no `-DPROJECT_VER`, and checkout has no `fetch-depth: 0`.
  ESP-IDF falls back to `git describe`, and a shallow checkout has no tags — so
  the released image's embedded version can be a bare hash that cannot prove
  which release it is. Check `esp_app_desc`, do not assume.
- **"All tests green" on `host` tier only.** Host tests cost milliseconds and
  are exempt from gating; they are not evidence about the board. Check the tier
  of each green test.

## Verdict

`OPEN` · or `SHUT` with each unmet requirement named, the evidence looked for,
and what was found instead. A requirement is met only when **every** test
verifying it is successful, including its prohibited-outcome check.
