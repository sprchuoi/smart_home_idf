# Gate — Load defined

**Closes:** Phase 0 (`/define`). **Opens:** Phase 1 (`/harness`).

> **Standing exclusion — evidence is committed artefacts and live bench answers
> only.** Never memory, transcripts, a prior session's notes, or the author's
> account of what was done. A checker told what was decided will confirm it was
> done. If something cannot be found in the repository, that is a finding, not a
> reason to look elsewhere.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | The three planes exist and are committed | `docs/00-Overview.md`, `docs/Functionality/`, `docs/Method/`, `docs/UserDocumentation/` | `git ls-files` lists a file in each; the OPERATE plane is one manual with chapters, not a directory of siblings |
| 2 | Every Must/Should in the FSD carries a verification contract | `docs/Functionality/FSD.md` — each requirement table row, plus its contract row | For every `Must`/`Should` id, a contract naming preconditions, stimulus, expected observation, prohibited outcome, tier |
| 3 | No requirement claims hardware verification | `docs/Functionality/FSD.md` §4.4, OD-8 | No requirement's status claims a run on a board; capability is stated as build-verified only |
| 4 | Architecture, state model, interfaces, configuration catalogue and security profile are present | FSD §2.4 (layering), §5.3 (transition table), §2.5 (context), §15, §18 | Each section exists and the transition table is normative |
| 5 | provenance tags are present and no `(assumed)` marker remains on anything architecture-critical | FSD throughout | `[user]` / `[derived]` / `[code]` / `[pack:esp32]` present; grep for `(assumed)` returns only non-architectural items |
| 6 | Open decisions are recorded rather than silently resolved | FSD §4.6 | Each OD names the requirement it gates; OD-4, OD-5, OD-8, OD-14, OD-15 are present |
| 7 | Every phase is enterable from the phase before it | FSD §3.1–§3.9 | For each phase's exit criteria, the phase supplying every capability it rests on is named and is earlier |

## Mechanical checks

```bash
# 1. Planes exist
test -f docs/00-Overview.md
test -d docs/Functionality && test -d docs/Method && test -d docs/UserDocumentation
git ls-files docs/00-Overview.md docs/Functionality docs/Method docs/UserDocumentation

# 2. The FSD has requirements and contracts
grep -cE '^\| (FR|NFR)-[0-9]+\.[0-9]+ \| (Must|Should)' docs/Functionality/FSD.md
grep -c 'shall' docs/Functionality/FSD.md

# 3. No weasel words in a requirement statement
grep -inE 'appropriate|graceful|robust|seamless|user-friendly|as needed|reasonable|sufficient|acceptable|best effort|normal operation' docs/Functionality/FSD.md

# 4. Open decisions recorded
grep -cE '^\| OD-[0-9]+' docs/Functionality/FSD.md

# 5. Plane map committed
git log --oneline -1 -- docs/00-Overview.md
```

## Judgement checks — answer each with a quotation

1. Pick three `Must` requirements at random. For each, could a competent
   stranger **build a rig from the contract alone**, without reading the
   firmware? Quote the contract. If any needs the code to be understood, that
   requirement is unfinished.
2. Does any requirement state a behaviour that contradicts what the firmware
   does? Quote both. (The known instances are OD-6, OD-15; a *new* one is a
   finding.)
3. Are the phases ordered so that no phase's exit criterion depends on a later
   phase? Name the phase that supplies each capability an exit criterion rests
   on. A device configured only through a portal cannot publish before the
   portal exists.
4. Does `docs/00-Overview.md` route each sentence to exactly one plane, and does
   the repository's existing Sphinx set get *bound to* rather than duplicated?

## Traps

Ways this project could look defined and not be:

- **`docs/` already contained a Sphinx set.** A checker sees documentation and
  reports the planes present. Check that the *three planes* exist and that
  `docs/Method/` in particular is committed — the Sphinx set is mostly OPERATE
  and API, and it does not fill the WHAT or HOW planes.
- **The FSD is long, so it looks complete.** Length is not contracts. Count the
  Must/Should rows against the rows carrying a contract; a 1,900-line FSD with
  ten unconracted requirements fails.
- **ROADMAP.md and docs/deployment.rst contradict the FSD** on the MQTT topic
  schema and the heartbeat. They are stale by decision (OD-6, OD-15). A checker
  that reads ROADMAP as specification will report a false conflict — and one
  that ignores the contradiction entirely will miss that the FSD is the
  authority. Both are findings.
- **A requirement marked `detected-in-code` is not `approved`.** Detected is
  evidence of what the code does, not that anyone wants it. Treating it as
  approved silently adopts behaviour nobody chose.
- **The three planes can all exist while the OPERATE plane is a stub.** Check
  the user manual carries a real status line rather than a placeholder.

## Verdict

`OPEN` · or `SHUT` with each unmet requirement named, the evidence looked for,
and what was found instead. Nothing else. A checker that edits files has stopped
being an instrument.
