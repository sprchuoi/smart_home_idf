# Gate — Shipped

**Closes:** the journey. There is no later phase.

> **Standing exclusion — evidence is the published release, the CI run that
> built it, and what the bench answers now.** Never memory, transcripts, or the
> author's account. The whole point of this gate is that the bytes users
> download were run, so nothing short of that counts.

## Requirements

| # | Must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | A version tag was pushed by a human | the repository's tags | The tag exists and points at a commit |
| 2 | The release was built in the pinned container from that tag | the `build` job of the tag's run | The run is green and is associated with the tag ref |
| 3 | The release artifacts were published from that run | GitHub Releases | The tarball and its images are attached |
| 4 | The published tarball is actually flashable as documented | the tarball's contents vs its own `flash_args` | **Every image `flash_args` names is present, at the path it names** |
| 5 | The journey ran green **on the released bytes** | the `release-verify` job of the tag's run | The job ran (not skipped) and passed |
| 6 | The image identifies itself as that version | the released image's `esp_app_desc` | It reports the tag, not a bare hash |
| 7 | The bench answered throughout | `release-verify` step summary | Preconditions met; no "not done" summary |

## Mechanical checks

```bash
TAG="${TAG:?set TAG to the released version, e.g. v0.2.0}"

# 1-3. The release exists and came from CI
git tag --list "$TAG"
gh release view "$TAG"
gh run list --workflow ci.yml --branch "$TAG" -L 3

# 4. THE ONE THIS PROJECT HAS GOT WRONG BEFORE.
#    flash_args names bootloader/bootloader.bin and
#    partition_table/partition-table.bin; upload-artifact nests them, and the
#    old packaging used `./*.bin`, which does not recurse.
tmp=$(mktemp -d)
gh release download "$TAG" -D "$tmp" -p '*.tar.gz'
tar -xzf "$tmp"/*.tar.gz -C "$tmp"
python3 - "$tmp" <<'EOF'
import os, sys
root = sys.argv[1]
missing = []
for line in open(os.path.join(root, 'flash_args')):
    p = line.split()
    if len(p) == 2 and p[0].startswith('0x'):
        if not os.path.isfile(os.path.join(root, p[1])):
            missing.append(p[1])
print("MISSING FROM TARBALL:", missing or "none - flashable")
EOF

# 5. The journey ran on the released bytes, not skipped
gh run view --log --job "$(gh run list --workflow ci.yml -L1 --json databaseId -q '.[0].databaseId')" | grep -i "release-verify\|journey"

# 7. Was the bench actually there?
gh run view --json jobs -q '.jobs[] | select(.name|test("journey";"i")) | .conclusion'
```

## Judgement checks — answer each with a quotation

1. Did `release-verify` **run**, or was it *skipped* by `TESTBENCH_READY`? Quote
   the job conclusion. "Green run" with a skipped verify job means the bytes
   were never flashed, and the release claim is unearned.
2. Was the flashed image the **published** artifact from this run, or a rebuild?
   Quote the download step and the `flash_args` it used. A rebuild is a
   different binary, and a green journey on it proves nothing about the release.
3. Does the tarball's own `flash_args` resolve inside the extracted package?
   Quote the check output. This is the defect the job exists to catch.
4. Does the release body's installation instructions match what the tarball
   actually contains? Quote both.

## Traps

Ways a release could look shipped and not be:

- **The tag push publishes even when nothing was verified.** The `release` job
  uses `always() && !failure() && !cancelled()`, so a *skipped* `release-verify`
  does not suppress it — deliberately, so a tag still publishes today. The
  consequence: a published release is **not** evidence that the journey ran.
  Only requirement 5 is, and it must be checked separately.
- **`TESTBENCH_READY` unset.** Then `release-verify` is skipped before it is
  queued. Every release in that state ships on a build-and-size claim only.
- **The tarball defect, previously live.** `./*.bin` does not recurse, so
  bootloader and partition table were omitted while `flash_args` named them, and
  `esptool.py write_flash @flash_args` failed on every extracted release. Fixed
  in `90e7868` by archiving the whole artifact — verify it stays fixed, because
  the failure is invisible until someone actually flashes.
- **A self-hosted runner that does not exist leaves the job queued, not failed.**
  That reads as a hang rather than an honest skip, and the release waits behind
  it. If a tag run looks stuck, check for a queued `release-verify` first.
- **Version provenance.** Without `-DPROJECT_VER` and `fetch-depth: 0`, the
  embedded version can degrade to a hash, so a shipped image cannot prove which
  release it is. A green journey on an unidentifiable image is not shipment.
- **A pre-release tag.** `v*` matches `v0.2.0-rc1`; check the tag is the one
  meant to ship.

## Verdict

`OPEN` · or `SHUT` with each unmet requirement named, the evidence looked for,
and what was found instead. This gate is never opened by the session that cut
the release.
