# Immersive Relay Phase 5: Arcade Stabilization

Phase 5 makes Immersive Relay usable as a venue-ready MVP: repeatable sessions,
operator control, fast reset, recoverable API errors, safe lap-based pilot
handoff, state visibility, fake operator tooling, smoke tests, and benchmark
checklists.

## Status: Software Complete / Hardware Acceptance Pending

Phase 5 is software-complete as of Phase 5.2A. The arcade layer, Operator API,
FakeCrewPhones integration, Syphon path, report generation, reset loop, handoff
state, and error recovery have passed on the available MacBook Pro M1 Pro test
machine.

Final venue performance is not declared yet because the required 60 second
benchmark has not been run on the physical Mac mini M1 base target.

Current acceptance metadata:

```json
{
  "hardware_acceptance": "pending",
  "tested_machine": "MacBookPro18,3 M1 Pro",
  "target_machine": "Mac mini M1 base",
  "venue_default_profile": "fulldome_safe",
  "target_profile_status": "pending_m1_benchmark"
}
```

The provisional venue profile is `fulldome_safe`: 1536 domemaster, 768
cubemap, `output=syphon`, `projection=fisheye_domemaster`, and
`camera=stabilized_chase`. `fulldome_target`/2048 remains available for tests,
but must not become the production default until it passes the Mac mini M1 base
benchmark.

## 1. Implemented

- `ArcadeSessionStateMachine` under `src/modes/immersive_relay/`.
- Venue session config with player count, track, kart, render preset, output,
  projection, operator permissions, and lap/handoff policy.
- Local trusted operator API under `/immersive/v1/operator/*`.
- `/state.arcade_session` nested state for Swift/operator clients.
- Safe single-wheel lap handoff state with previous/next driver, countdown, and
  optional operator confirmation.
- Briefing, countdown, final lap, finish sequence, results, and report state.
- Reset path that clears score, cooldowns, encounters, role assignment, pilot
  turns, track effects, idempotency, and pending crew actions.
- `tools/immersive/FakeArcadeOperator.py`.
- `tools/immersive/RunRelayPhase5ArcadeSmoke.sh`.
- `tools/immersive/RunRelayPhase5M1Benchmark.sh`.
- `tools/immersive/RunRelayPhase51VenueAcceptance.sh` and
  `tools/immersive/Phase51VenueAcceptance.py` for final venue acceptance.

## 2. Not Implemented

Phase 5 does not implement equirectangular production output, cylindrical
projection, CAVE/cube/wall production, Windows/Spout, VR/OpenXR, a Swift app, a
PWA, payment, booking, new large game modes, new visual effects, renderer
refactors, or any change to the lap-based relay rule.

## 3. State Machine

States:

```text
boot -> idle / attract
idle -> attract / ready_for_players / session_configured
attract -> idle / ready_for_players / session_configured
ready_for_players -> session_configured / briefing / resetting
session_configured -> ready_for_players / briefing / resetting
briefing -> countdown / ready_for_players / resetting
countdown -> running / paused / resetting
running -> lap_handoff / paused / final_lap / finish_sequence / results / resetting
lap_handoff -> running / final_lap / paused / resetting
paused -> running / lap_handoff / final_lap / finish_sequence / resetting
final_lap -> lap_handoff / paused / finish_sequence / resetting
finish_sequence -> results / resetting
results -> resetting / ready_for_players / attract
resetting -> ready_for_players / attract
error_recoverable -> resetting / ready_for_players / attract / error_fatal
error_fatal -> shutdown
shutdown
```

Each transition logs exit and entry through `ImmersiveRelayArcade`. Invalid
transitions return `wrong_state` and do not mutate the active session.

## 4. Operator API

Default TholusOS base URL is
`http://127.0.0.1:8890/immersive/v1`. All endpoints require
`X-STK-Relay-Token` or `Authorization: Bearer <token>`.

| endpoint | method | purpose |
| --- | --- | --- |
| `/immersive/v1/capabilities` | GET | Discovery for outputs, projections, render profiles, policy, and frozen endpoint support. |
| `/immersive/v1/operator/state` | GET | Full state including `arcade_session`. |
| `/immersive/v1/operator/configure` | POST | Configure player count, track, kart, lap policy, render/output, permissions. |
| `/immersive/v1/operator/start` | POST | Move ready/configured/results into `briefing`. |
| `/immersive/v1/operator/countdown` | POST | Move briefing into countdown; `{ "skip": true }` starts immediately for tests. |
| `/immersive/v1/operator/pause` | POST | Freeze/lock session when running, countdown, final lap, or handoff. |
| `/immersive/v1/operator/resume` | POST | Resume from paused. |
| `/immersive/v1/operator/abort` | POST | End as operator abort and publish results/report. |
| `/immersive/v1/operator/reset` | POST | Clear runtime state and return to ready/attract. |
| `/immersive/v1/operator/force-finish` | POST | Finish current session and publish report. |
| `/immersive/v1/operator/handoff/confirm` | POST | Confirm active handoff. |
| `/immersive/v1/operator/render-preset` | POST | Apply debug/safe/target fisheye preset. |

`/operator/next-state`, `/operator/handoff`, `/session/*`, and
`/test/progress` remain dev/test compatibility endpoints. They are not in the
Phase 5.3 TholusOS freeze.

Common errors:

```text
wrong_state
session_not_configured
race_not_attached
already_running
not_paused
handoff_not_active
invalid_player_count
invalid_render_preset
invalid_track
invalid_token
reset_in_progress
immersive_output_unavailable:syphon
immersive_projection_unavailable:<projection>
```

## 5. Session Config

Phase 5.3 TholusOS example:

```json
{
  "player_count": 6,
  "track_id": "snowtuxpeak",
  "kart_id": "tux",
  "difficulty": "easy",
  "ai_karts": 5,
  "lap_policy": "player_count_based",
  "handoff_policy": "lap_based_handoff",
  "crew_mode": "enabled",
  "encounter_script": "snowtuxpeak_relay_encounters",
  "render_profile": "fulldome_safe",
  "output": "syphon",
  "projection": "fisheye_domemaster",
  "operator_mode": "arcade",
  "allow_pause": true,
  "allow_force_finish": true,
  "allow_abort": true
}
```

## 6. Lap Policy

The session remains lap-based:

| player_count | total_laps |
| ---: | ---: |
| 2 | 6 |
| 3 | 6 |
| 4 | 8 |
| 5 | 10 |
| 6 | 12 |

## 7. One-Wheel Pilot Handoff

At every lap boundary, the relay rotation enters `lap_handoff`, locks driving
input, freezes/brakes the kart through `RelayRaceMode`, keeps render output
alive, shows handoff HUD/debug messages, exposes previous/next driver in state,
and reassigns crew roles when handoff completes.

If `handoff_operator_confirm` is true and `auto_confirm_handoff` is false,
`/operator/handoff/confirm` is required after the minimum safety duration.
Smoke tests use auto-confirm/accelerated progress.

## 8. Fast Reset

`POST /operator/reset` clears crew score, multipliers, cooldowns, encounter
state, role assignments, pilot rotation, pilot turns, track effects, action
history, pending crew actions, idempotency records, countdown, pause state, and
handoff state. It leaves the process and local API alive.

## 9. Error Recovery

Recoverable API errors return JSON and keep the session coherent. Syphon output
requests in non-Syphon builds return `immersive_output_unavailable:syphon`.
Unsupported projections still fail as Phase 4.6 stubs and do not become
production features.

## 10. `/state.arcade_session`

The nested object includes:

```json
{
  "session_id": "phase5-reset-loop-3",
  "state": "running",
  "configured": true,
  "player_count": 6,
  "total_laps": 12,
  "current_lap": 4,
  "remaining_laps": 9,
  "active_driver": 4,
  "previous_driver": 3,
  "next_driver": 5,
  "handoff_active": false,
  "input_locked": false,
  "handoff_countdown": null,
  "waiting_for_operator_confirm": false,
  "countdown": null,
  "can_start": false,
  "can_pause": true,
  "can_reset": true,
  "can_abort": true,
  "can_force_finish": true,
  "last_error": null,
  "report": null,
  "last_report": null
}
```

## 11. Session Report

Reports are exposed in two slots:

- `arcade_session.report` is the current result report and is non-null only
  during `finish_sequence`/`results`.
- `arcade_session.last_report` keeps the last completed report as an operator
  reference after reset. Reset must clear active score/session state; it must
  not make an old report look like the current session report.

If `STK_RELAY_REPORT_DIR` is set, a JSON report is also written there.

Fields include session id, player count, track, total/completed laps, final
result, rank, score, combo count, encounter counts, actions by role, driver
turns, duration seconds, render profile, projection, output,
`crew_mode_enabled`, `crew_actions_enabled`, and `crew_actions_accepted`.
Operator-only reports with no fake/real phone actions should have score `0`,
`crew_actions_enabled=false`, and `crew_actions_accepted=0`. Crew-enabled runs
with FakeCrewPhones active should have positive score and accepted actions.

## 12. Fake Operator

Examples:

```bash
tools/immersive/FakeArcadeOperator.py --players 6 --run-session --fast
tools/immersive/FakeArcadeOperator.py --players 4 --run-session --with-fake-phones --fast
tools/immersive/FakeArcadeOperator.py --players 6 --pause-resume-test --fast
tools/immersive/FakeArcadeOperator.py --players 6 --reset-loop 3 --fast
```

## 13. Smoke Tests

Run:

```bash
tools/immersive/RunRelayPhase5ArcadeSmoke.sh
```

It covers vanilla build, relay build, Syphon build when available, 2p/4p/6p
sessions, reset, pause/resume, abort, force-finish, handoff confirm, invalid
operator command rejection, reset during handoff, and three consecutive sessions
in one process.

Final venue acceptance package:

```bash
tools/immersive/RunRelayPhase51VenueAcceptance.sh
```

It records hardware metadata, runs operator + FakeCrewPhones together for
2p/4p/6p, mistakes and silent-role sessions, checks report/reset cleanliness,
tests handoff confirmation and error recovery, and runs the Syphon path when
available.

## 14. Performance Presets

| preset | domemaster | cubemap | intended use |
| --- | ---: | ---: | --- |
| debug | 1024 | 512 | setup/debug |
| safe / fulldome_safe | 1536 | 768 | provisional venue default pending M1 base test |
| target / fulldome_target | 2048 | 1024 | target only after real M1 base validation |

`fulldome_debug`, `fulldome_safe`, and `fulldome_target` are aliases.
Operator/session configuration defaults to `fulldome_safe`. The low-level
immersive render config also defaults to 1536/768 so CLI launches without an
explicit size do not silently select the 2048 target profile.

## 15. Benchmark M1/M1 Pro

Phase 4.6/5.1 measured and validated the package on a MacBook Pro M1 Pro test
machine, including Operator + FakeCrewPhones + Syphon + reset loop. Phase 5 did
not measure a Mac mini M1 base in this workspace. Do not declare final venue
performance from M1 Pro numbers alone.

Phase 5.1 can declare Phase 5 closed only when the following 60 second cases
are run on the physical Mac mini M1 base target. On non-target hardware, report
the package as software-complete and ready for hardware acceptance, but not
venue-performance-final.

Run on the Mac mini M1 base:

```bash
DURATION=60 tools/immersive/RunRelayPhase5M1Benchmark.sh
```

Required cases:

```text
relay 16:9 no immersive
fisheye 1024 window
fisheye 1536 window
fisheye 2048 window
fisheye 1024 Syphon
fisheye 1536 Syphon
fisheye 2048 Syphon
```

The script writes CSV and machine metadata under `/tmp/stk_phase5_m1_benchmark`.

## 16. Human Playtest Checklist

Use the venue checklist in `docs/immersive_relay_phase3_human_playtest_checklist.md`
with the Phase 5 venue section. Test 2p, 4p, and 6p, one wheel, handoff clarity,
fake/real phones, Syphon, dome/receiver debug, operator reset, and recovery.

## 17. Passed To Swift

Swift/TholusOS receives the operator contract, `arcade_session`, report data,
render metadata, crew action validation, and Syphon video. Swift remains
authority for operator UI, real phones, identity, payment/booking later, and
player-facing recovery copy.

## 18. Passed To Phase 6

Phase 6 may implement production equirectangular output. It must not infer that
Phase 5 added it; equirectangular remains a stub.

## 19. Passed To Phase 7

Future phases may handle cylindrical, wall atlas/CAVE, Windows/Spout, VR/OpenXR,
booking/payment, and a real companion app.
