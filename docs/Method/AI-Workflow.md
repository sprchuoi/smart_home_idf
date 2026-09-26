# ESP32-S3 Smart Home — AI Workflow (the build contract)

Read this before any change. It is how new functionality is built and how the
documentation stays in sync with it.

## The loop

**1. Locate the contract.** Find the FSD rule the work serves in
[`../Functionality/FSD.md`](../Functionality/FSD.md). If none exists, the work
starts by defining the **WHAT** — a new, atomic, falsifiable requirement — not
by writing code. A change with no rule behind it is either scope creep or an
undocumented requirement; both are resolved before implementation.

**2. Build per the Method.** Follow [`standards/`](standards/) and
[`project/`](project/). Reuse an existing module before adding new code. Make
the smallest change that satisfies the rule — no speculative scope, no drive-by
refactors.

**3. Test — the gate, not an afterthought.** A change is **not done** until its
test exists and passes. Add the case beside the code, cite the requirement ID it
verifies, and run the suite green. A bug fix writes its **regression test
first**, and that test must fail before the fix and pass after — a regression
test that never failed proves nothing. The test plan and the tier available for
a case are Phase 1 (`/harness`) artefacts: `testing/test-plan.yaml` and
`testing/gates/`.

**4. Reconcile the documentation.**
- The **FSD** absorbs new or changed behaviour — *verify, don't transcribe*. If
  the code deviates from the intended spec, fix the code; do not enshrine the
  defect as a requirement.
- The **user documentation** absorbs anything an operator would notice.
- The **Method stays put** unless the change taught a rule that is *universally*
  true for this project, not just for this change.
- All present-state. No "now uses", no "previously", no dated narrative.

**5. Verify both directions.** Confirm the implementation matches the FSD, and
that no FSD rule is silently unimplemented. Deviations fix the code; genuine
gaps are recorded as gaps in the FSD's §21.2; contradictions are escalated
rather than guessed at.

## Requirement quality gate

Before a new requirement enters the FSD it must be:

- **Atomic** — one obligation. Split anything joining two verbs, a behaviour and
  a deadline, or a success and a failure path.
- **Falsifiable** — precondition, stimulus, observable response, deadline,
  tolerance, failure behaviour, verification tier.
- **Free of weasel words** — *appropriate, graceful, user-friendly, reasonable,
  sufficient, robust, seamless, acceptable, normal operation, best effort*.
- **Provenance-tagged** — `[user]`, `[derived]`, `[code]`, `[pack:esp32]`.
- **Contracted** — every Must/Should carries preconditions, stimulus, expected
  observations, timing, tolerance, prohibited outcomes, tier, evidence and
  cleanup.

## Proposing a value

A missing threshold, timeout, tolerance or default is reported, never invented.
A value this loop proposes is written as `source: proposed-by-skill`,
`status: proposed`, and remains so until accepted. A proposed number silently
promoted to approved is how a spec acquires values nobody chose.

## The three planes in this repository

| Plane | Location | Authority |
|---|---|---|
| WHAT | `docs/Functionality/FSD.md` | Wins on *what must be true* |
| HOW | `docs/Method/` (this plane) | Wins on *how to get there* |
| OPERATE | `docs/UserDocumentation/User-Manual.md`, binding to the Sphinx pages under `docs/` | Describes the system as built |

## Roles

- **The repository owner** is the only person who may accept a `proposed`
  requirement, resolve an open decision in FSD §4.6, or approve the plane
  retrofit in [`project/plane-retrofit-plan.md`](project/plane-retrofit-plan.md).
- **Any contributor or agent** may add a `[derived]` or `[code]` requirement
  whose parent is already approved, and must cite it in the change.
- **A contradiction between planes is escalated, not resolved unilaterally.**
  The FSD wins on *what must be true*; this plane wins on *how to get there*;
  the user manual is stale if it disagrees with either.
- **The FSD is never edited to describe a defect.** A detected behaviour that
  contradicts a requirement means the code changes, or the requirement is
  formally amended with provenance.
