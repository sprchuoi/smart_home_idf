# Debugging agenda

What has to be observed working **before** a failing test means anything. This
is debugging work, not test cases — it does not enter `testing/test-plan.yaml`.
Each item's measurement belongs on the capability it proves
(`test-design.md` §5).

**The testbench is deliberately absent from this list.** A project depends on
the testbench's quality the way it depends on the compiler's; a bench fault is
fixed in the bench's own repository, and only once DUT evidence disproves the
declaration (`commission/SKILL.md`). The bench does not yet exist here, which
is recorded as a capability (`testbench-installed: no`) and blocks every
bench-tier test — it is not something this project debugs.

## The agenda

| # | Part | Unproven because | Proven by | Capability |
|---|---|---|---|---|
| 1 | The board + this firmware combination | Nothing in this repository has ever executed on a real ESP32-S3. Capability is build-verified only (`./make.sh smoke`, CI). | The board boots, enumerates on a bench slot, and prints the §17 banner | `dut-board` |
| 2 | Octal PSRAM mode | `CONFIG_SPIRAM_MODE_OCT` is asserted only from `sdkconfig`, never against silicon. A wrong mode gives a **boot loop, not a build error** (FSD §4.3). | Boot completes and `esp_spiram` reports 8 MB octal | `psram-octal-mode` |
| 3 | Partition layout on real flash | Offsets in `partitions.csv` are checked by parsing the *generated* table in CI, which proves the table is self-consistent — never that esptool writes it where the bootloader looks. | A cold flash from the artifact's own `flash_args` boots the new image rather than the previous one | `flash-partition-layout` |
| 4 | Serial-console provisioning into NVS | `wifi_set`/`mqtt_set` write NVS through `nvs_set_str`; the write→reboot→read path has never run on a target. | Credentials survive a reboot and the DUT associates | `nvs-provisioning` |
| 5 | MQTT reachability and the `allow_anonymous` assumption | Every broker interaction so far is host-side or asserted, never a live session. `NFR-18.6` records `allow_anonymous true` as bench-only. | The DUT establishes a session and the Last Will does **not** fire | `broker-anonymous-auth` |
| 6 | Console command plumbing | The command library that replaced per-command boilerplate has never executed on a target; registration, argument parsing and exit statuses are exercised only by reading. | `help` lists the registered commands and `wifi_status` answers on a live node | `console-command-library` |
| 7 | Smart_Server stack | An external system (FSD §2.5). ROADMAP records its app container as unbuilt, so it has never received anything from a real node. | A device row appears from live traffic | `smart-server-stack` |
| 8 | OTA image server | The firmware can pull an OTA image over HTTP, but nothing has ever served one to a real node. | A node fetches and applies an image | `ota-image-server` |
| 9 | Self-hosted CI runner | No runner is registered; the release-verify job has never been queued on one. | The runner picks up a job with labels `[self-hosted, testbench]` | `self-hosted-runner` |
| 10 | Release verification | The release-verify job exists but is gated on `TESTBENCH_READY` and has never run. | A tagged release flashes and passes the journey on the released bytes | `release-verification` |

Items 1–6 are aspects of the same physical unknown — the board running this
image — and all six share one capability, `dut-board`, as *unproven*. Items 7–10
are separate project-side capabilities. Every `unproven` capability declared in
`testing/test-plan.yaml` appears above; none is left implicit.

## Already-known defects the agenda will expose

These are recorded in FSD §4.6 and are code fixes, not spec questions. They are
listed here because each one will corrupt an observation if it is not fixed
first — debugging item 4 against `wifi_clear` measures the bug, not the store.

| ID | Defect | Why it corrupts an observation |
|---|---|---|
| OD-4 | `clearCredentials()` writes `""` instead of erasing the NVS keys, so `getSSID()` succeeds and `hasCredentials()` stays true | Item 4 would conclude "credentials persist correctly" from a store that was never cleared |
| OD-5 | `Wifi_cfg.hpp`'s `char ssid[32]` cannot hold a 32-character SSID (the `MAX_LEN + 1` rule that `Mqtt_cfg.hpp` already applies) | Item 4 fails for a 32-character SSID and looks like an association fault |
| OD-14 | WiFi reconnect stops permanently after 10 failed attempts until reset | Item 5 mid-run looks like a broker outage |

## Bring-up decision

Recorded explicitly because `test-design.md` §5 requires the call to be made,
not assumed: **bring-up comes first here.** No physical interface on this
device has ever been driven, so the journey's first three steps
(`JRN-01.1`–`JRN-01.3`) are the bring-up — they answer "is this connected the
way we think it is?" directly, rather than by inferring from a journey that
stops five steps later. They are `standard` kind, ordered ahead of the rest of
the journey, not a separate bin.

## Exit

The agenda is empty of project-side items when items 1–6 have each been
observed working. That is the `DUT ready` gate, and it is Phase 2
(`/commission`), not Phase 1.
