# Proposed plane retrofit for the existing `docs/` set

**Status: proposal only — nothing has been moved, deleted or rewritten.**
This file exists so the user can approve or reject the plan later. The three
planes created in Phase 0 **bind** to the existing Sphinx documents; they do not
replace them.

## Why a plot is needed at all

`docs/` predates the three-plane split. Its pages mix WHAT, HOW and OPERATE
content, and several of them describe software that no longer exists or a topic
schema the firmware does not use. Two rules apply:

1. **Do not relocate published documents.** They are the source of the live
   GitHub Pages site and are cross-referenced by the README and by Doxygen.
2. **One fact, one home.** Content that duplicates a plane becomes a pointer;
   content that contradicts the code is either fixed or marked stale.

## Inventory and triage

| Existing document | Currently fills | Plane after retrofit | Action proposed |
|---|---|---|---|
| `docs/index.rst` | Landing page: concept + status + quick start | OPERATE entry, with a WHAT pointer | Keep. Replace the status table with a link to FSD §1.6; keep the quick start as the OPERATE entry. |
| `docs/getting-started.rst` | Build, flash, provision, troubleshoot | OPERATE | Keep as a chapter source. **Fix stale content**: it claims the node publishes Home Assistant discovery and "appears in Home Assistant automatically" (OD-7). |
| `docs/architecture.rst` | Firmware layout, callback edges, tasks, MQTT topics, provisioning, OTA, absent list | HOW for layout/tasks; WHAT for topics/behaviour | Keep. Reduce the WHAT half to a summary plus a link to FSD §2, §5, §9, §11; keep the layout and task tables as HOW. |
| `docs/deployment.rst` | Release build, OTA procedure, monitoring, node replacement | OPERATE | Keep. **Fix stale content**: the OTA and monitoring commands use the retired `smart_home/<id>/cmd/ota` and `homeassistant/#` topics (OD-6). |
| `docs/development.rst` | Adding services, console commands, telemetry, commands; testing; code style | HOW | Keep. **Fix stale content**: it names `publishState()` and `publishDiscovery()`, which do not exist; the code-style and testing rules are superseded by `docs/Method/standards/`. |
| `docs/api/*.rst` | Per-component API notes bound to Doxygen | HOW (reference) | Keep the six pages in the toctree (`wifi`, `mqtt`, `ota`, `statemachine`, `uart`, `errorhandler`). **Delete** `audio.rst`, `eventbus.rst`, `oled.rst`, `powermanager.rst`, `watchdog.rst` — they describe removed components and are already outside the toctree. |
| `docs/DEPLOYMENT.md` | How the docs site is built and deployed | HOW | Keep, unchanged. |
| `docs/README.md` | How to build the docs locally | HOW | Keep, unchanged. |
| `Doxyfile` (repo root) | API reference configuration | HOW | Keep. Its `INPUT` and `FILE_PATTERNS` are already correct. |

## Proposed file operations

None of these has been performed.

| # | From | To | Kind |
|---|---|---|---|
| 1 | `docs/deployment.rst` (topic examples) | corrected in place | fix, not move |
| 2 | `docs/getting-started.rst` (discovery claims) | corrected in place | fix, not move |
| 3 | `docs/development.rst` (superseded rules) | replace body with a pointer to `docs/Method/` | trim, not move |
| 4 | `docs/architecture.rst` (WHAT half) | summary + link to `docs/Functionality/FSD.md` | trim, not move |
| 5 | `docs/api/audio.rst`, `eventbus.rst`, `oled.rst`, `powermanager.rst`, `watchdog.rst` | deleted | delete |
| 6 | new `docs/planes.rst` | MyST toctree entries rendering `docs/Functionality/FSD.md`, `docs/Method/README.md` and `docs/UserDocumentation/User-Manual.md` into the published site | add |
| 7 | `docs/index.rst` toctree | add `planes` | add |

Operation 6 uses `myst-parser`, already a declared docs dependency
(`docs/requirements.txt`), so Markdown can be included without moving or
converting the plane files. If the user prefers the published site to render
only the Sphinx sources, drop operation 6 and keep the planes as repository
documents.

## What is deliberately **not** proposed

- **Moving or renaming any existing `.rst`/`.md` file.** History and inbound
  links follow the files; the planes are roles, not filenames.
- **Converting the Sphinx sources to Markdown**, or the planes to reST.
- **Merging the FSD into `docs/architecture.rst`.** The FSD is the WHAT contract;
  a page that also explains source layout is edited by everyone and owned by
  no one.
- **Deleting the five stale `docs/api/` pages without the user's confirmation.**
  File removal needs explicit approval in this repository (see the ROADMAP's
  note on `main/Kconfig`).

## Verification of the retrofit, when approved

1. `./make.sh doc` passes with `-W` (no broken cross-references).
2. Diff the sorted non-blank lines of each trimmed page before and after: only
   headings and links may differ; no fact is lost without replacement.
3. `grep` the tree for the retired topic patterns
   (`cmd/`, `availability`, `<ch>/state`, `homeassistant/`) and confirm each hit
   is either fixed or deliberately retained in a historical note.
4. Confirm `docs/Functionality/FSD.md` remains the single home for each
   requirement, and that no requirement text is copied into a Sphinx page.
