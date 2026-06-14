# Immersive Relay: Mac mini M1 Hardware Acceptance Checklist

This checklist closes the remaining Phase 5 hardware acceptance item. It does
not add Phase 6, equirectangular output, Windows/Spout, VR, PWA, Swift
companion work, new projections, new crew mechanics, or new gameplay modes.

## 1. Prerequisites

- Physical Mac mini M1 base target available, preferably `Macmini9,1` with 8 GB
  RAM.
- macOS updated and display/output connected as it will be in the venue.
- Repository checked out with the Phase 5.2A code.
- Xcode command line tools installed.
- Network restricted to trusted local operator traffic.
- Syphon receiver app or probe available for Syphon validation.
- One wheel and pedals connected for the manual handoff pass.

Record before testing:

```sh
sw_vers
sysctl -n hw.model
sysctl -n hw.memsize
system_profiler SPHardwareDataType SPDisplaysDataType
```

## 2. Syphon.framework

Verify Syphon:

```sh
test -d "$HOME/Library/Frameworks/Syphon.framework" || test -d "/Library/Frameworks/Syphon.framework"
```

If missing, install `Syphon.framework` in one of those two locations, then rerun
the check. Do not declare the Syphon acceptance path green if the framework is
absent.

## 3. Builds

From the repository root:

```sh
cmake --build cmake_build_vanilla --target supertuxkart --parallel 8
cmake --build cmake_build_relay_phase1 --target supertuxkart --parallel 8
cmake --build cmake_build_phase45_syphon --target supertuxkart --parallel 8
```

If a build directory is missing, run the matching acceptance script without
`SKIP_BUILDS=1` so CMake configures it.

## 4. 60 Second Benchmark

Run:

```sh
DURATION=60 RUN_SYPHON=1 tools/immersive/RunRelayPhase5M1Benchmark.sh
```

Expected output:

```text
/tmp/stk_phase5_m1_benchmark/phase5_m1_benchmark.csv
/tmp/stk_phase5_m1_benchmark/machine.json
/tmp/stk_phase5_m1_benchmark/hardware.txt
```

Required benchmark rows:

| machine | preset | domemaster | cubemap | output | avg fps | p50 | p95 | p99 | dropped | verdict |
| --- | --- | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: | --- |
|  | relay_16x9_no_immersive |  |  | main |  |  |  |  |  |  |
|  | debug | 1024 | 512 | window |  |  |  |  |  |  |
|  | safe | 1536 | 768 | window |  |  |  |  |  |  |
|  | target | 2048 | 1024 | window |  |  |  |  |  |  |
|  | debug | 1024 | 512 | syphon |  |  |  |  |  |  |
|  | safe | 1536 | 768 | syphon |  |  |  |  |  |  |
|  | target | 2048 | 1024 | syphon |  |  |  |  |  |  |

## 5. Integrated Operator + FakeCrewPhones + Syphon

Run the full venue acceptance package using the provisional safe preset:

```sh
RUN_SYPHON=1 SYPHON_DOMEMASTER_SIZE=1536 SYPHON_CUBEMAP_SIZE=768 tools/immersive/RunRelayPhase51VenueAcceptance.sh
```

Expected result:

```text
PASS Phase 5.1 venue acceptance package
```

This covers 2p, 4p, 6p, mistakes, silent-role hacker, report generation, reset
cleanup, handoff confirm, API error recovery, Syphon receiver probe, and Syphon
reset loop.

## 6. Reset Loop

The command above includes the reset loop. For a faster rerun after successful
builds:

```sh
SKIP_BUILDS=1 RUN_SYPHON=1 SYPHON_DOMEMASTER_SIZE=1536 SYPHON_CUBEMAP_SIZE=768 tools/immersive/RunRelayPhase51VenueAcceptance.sh
```

Acceptance checks:

| run | players | action | result | session_id_changed | state_clean | notes |
| --- | ---: | --- | --- | --- | --- | --- |
| 1 | 6 | partial with fake phones, reset |  |  |  |  |
| 2 | 6 | full with fake phones, results, reset |  |  |  |  |
| 3 | 4 | force-finish, reset |  |  |  |  |
| 4 | 2 | abort, reset |  |  |  |  |

## 7. Handoff Full-Session Test

Automated state test:

```sh
SKIP_BUILDS=1 RUN_SYPHON=1 tools/immersive/RunRelayPhase51VenueAcceptance.sh
```

Manual one-wheel venue pass:

1. Run a 6 player full session on `fulldome_safe` with Syphon receiver visible.
2. At each lap boundary, confirm the session enters `lap_handoff`.
3. Verify the kart is safe/frozen/stabilized and driving input is locked.
4. Verify the dome/debug HUD shows "Cambio pilota" and the correct next driver.
5. Have the current driver leave the wheel and the next driver take it.
6. Confirm handoff by operator command when required, then watch countdown back
   to running.

Record:

| players | total_laps | handoff_count | confirm mode | result | notes |
| ---: | ---: | ---: | --- | --- | --- |
| 6 | 12 |  | operator/manual |  |  |

## 8. Green / Yellow / Red Criteria

Green:

- all three builds pass;
- benchmark has all required rows;
- `safe` 1536 Syphon has acceptable p95/p99 and no visible drop pattern;
- integrated Operator + FakeCrewPhones + Syphon passes;
- reset loop passes in one process;
- handoff is physically understandable with one wheel;
- reports show positive score/actions when FakeCrewPhones act.

Yellow:

- 2048 target has stutter or dropped frames, but 1536 safe is stable;
- Syphon receiver needs restart once but acceptance then passes;
- manual handoff needs copy/timing tuning but is physically safe.

Red:

- 1536 safe Syphon is unstable;
- API hangs or dies after reset loop;
- Syphon cannot publish or receive;
- session report leaks score/actions/session id across resets;
- one-wheel handoff is physically unsafe or unclear.

## 9. If 2048 Fails

Do not block Phase 5. Keep `fulldome_safe` as production default, record
`fulldome_target` as not accepted on Mac mini M1 base, and keep 2048 only as a
future target.

## 10. If 1536 Fails

Do not declare venue-ready. Switch the on-site test to `fulldome_debug` 1024,
collect logs, and open a P0 venue blocker for performance/render output. Phase
5 remains software-complete but hardware acceptance failed.

## 11. When Phase 5 Can Be Declared Venue-Ready

Declare:

```text
Fase 5 venue-ready.
```

only after:

- Mac mini M1 base benchmark is complete;
- the chosen production preset is green or explicitly accepted as yellow;
- integrated Operator + FakeCrewPhones + Syphon passes on the same machine;
- reset loop passes on the same machine;
- manual one-wheel handoff is observed;
- no P0 venue blockers remain.
