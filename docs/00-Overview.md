# ESP32-S3 Smart Home — Documentation

Three planes, three questions, three readers. Every sentence belongs to exactly
one of them.

| Plane | Question | Location | Reader |
|---|---|---|---|
| **WHAT** | What must be true of this system? | [`Functionality/FSD.md`](Functionality/FSD.md) | Anyone judging whether the firmware is correct — reviewer, tester, future maintainer |
| **HOW** | How is it built and changed? | [`Method/README.md`](Method/README.md) | Whoever writes the next change, human or agent |
| **OPERATE** | How do I install and run it? | [`UserDocumentation/User-Manual.md`](UserDocumentation/User-Manual.md) | Whoever installs, drives, or recovers a running node |

**Authority order: the FSD defines the target; the Method defines the way there.**
On conflict the FSD wins on *what must be true*, the Method wins on *how to get
there*. User documentation describes the system as built — if it disagrees with
either it is stale, which means reality or the spec moved.

None of the three carries history or rationale narrative. Those live in `git log`.

## System under specification

The **WHAT** plane specifies the **ESP32-S3 firmware in this repository** — the
device under test (DUT). It does not specify Smart_Server, the Raspberry Pi,
Home Assistant, the MQTT broker, or Google Home. Those are **declared external
interfaces** the firmware depends on; the FSD states what the firmware requires
of them and how it behaves when they are present or absent, and nothing more.

The DUT has **never run on real hardware**. Every requirement in the FSD is
therefore currently unproven; the only evidence that exists is build-level (the
image compiles and `./make.sh smoke` passes its image and partition checks).
Each requirement names the tier that would prove it — `host`, `target`, or
`bench`.

## Where to start

| If you are… | Read |
|---|---|
| Making a change | [`Method/AI-Workflow.md`](Method/AI-Workflow.md) — the loop every change follows |
| Judging correctness | [`Functionality/FSD.md`](Functionality/FSD.md) — requirements, state model, verification contracts, security profile |
| Installing or running a node | [`UserDocumentation/User-Manual.md`](UserDocumentation/User-Manual.md) |
| Wondering why a decision was made | `git log` — a decision leaves its effect in a plane and its reason in the commit |

## Binding to the existing documentation set

This repository already ships a Sphinx set under `docs/` (`index.rst`,
`getting-started.rst`, `architecture.rst`, `deployment.rst`, `development.rst`,
`api/`, `DEPLOYMENT.md`). The three planes **bind to it** rather than replace it:

- The Sphinx **`architecture`**, **`api/`** and **`development`** pages partly
  duplicate the WHAT and HOW planes. They are not relocated or rewritten here.
- The Sphinx **`getting-started`**, **`deployment`** and **`index`** pages are
  the OPERATE plane's step-by-step procedures; the user manual links to them.
- [`Method/project/plane-retrofit-plan.md`](Method/project/plane-retrofit-plan.md)
  records the proposed move/retire plan for the duplicate content. **Nothing is
  moved until the user approves that plan.**

## Routing a new sentence

Ask in order; the first yes wins:

1. Externally observable and must be true → **Functionality**
2. Constrains how code is written or verified → **Method**
3. Tells a human how to run or recover the system → **UserDocumentation**
4. About collaborating with an AI assistant → `CLAUDE.md`, which is not a plane
5. Why a past decision was made → the commit message

Two questions settle the hard cases. *Could a black-box tester verify it?* — yes
means WHAT. *Would it survive a rewrite in another language?* — no means HOW.

## There is no decisions file

A settled decision leaves its **effect** as present-state content in the plane
it governs — a load-bearing "must not" becomes a stated negative requirement —
and its reason in the commit message. Open product decisions are carried in
[`Functionality/FSD.md`](Functionality/FSD.md) §4.6, each naming the requirement
it gates.

## Related

- [`../README.md`](../README.md) — what the firmware does today
- [`../ROADMAP.md`](../ROADMAP.md) — phase plan and hardware verification checklist
- [`../docs/index.rst`](index.rst) — the published documentation site
