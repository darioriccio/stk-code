# Immersive Relay Phase 5.1: Venue Acceptance

Phase 5.1 is an acceptance and correction pass. It does not add new projection
features, game modes, Swift UI, PWA, VR, Windows/Spout, or Phase 6 work.

## Status After Phase 5.2A

Phase 5 is software-complete and hardware-acceptance pending. The available
test machine was a MacBook Pro M1 Pro, not the Mac mini M1 base venue target.

Passed on the available M1 Pro package:

- vanilla build;
- relay build;
- Syphon build;
- Operator + FakeCrewPhones;
- Operator + FakeCrewPhones + Syphon;
- reset loop with Syphon active;
- report semantics for crew-active and operator-only runs;
- handoff state/confirm;
- error recovery;
- Phase 4.6 Syphon/projection regression.

Not declared:

- final venue performance;
- Mac mini M1 base 60 second benchmark;
- 2048 target profile as production default.

Current hardware acceptance state:

```json
{
  "hardware_acceptance": "pending",
  "tested_machine": "MacBookPro18,3 M1 Pro",
  "target_machine": "Mac mini M1 base",
  "venue_default_profile": "fulldome_safe",
  "target_profile_status": "pending_m1_benchmark"
}
```

The provisional venue default is `fulldome_safe`: 1536 domemaster, 768 cubemap,
`output=syphon`, `projection=fisheye_domemaster`, `camera=stabilized_chase`.
The 2048 target profile remains available but is not the production default
until the Mac mini M1 base benchmark passes.

Phase 5.3 freezes the TholusOS integration contract in
`docs/immersive_relay_tholustos_integration.md`. The default local API base URL
is `http://127.0.0.1:8890/immersive/v1`; `/capabilities` is the discovery
endpoint TholusOS should call before hardcoding outputs or projection support.

Official TholusOS startup command:

```bash
cmake_build_phase45_syphon/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  --race-now \
  --track=snowtuxpeak \
  --kart=tux \
  --numkarts=6 \
  --difficulty=0 \
  --unlock-all \
  --immersive-relay \
  --relay-local-api \
  --relay-api-bind=127.0.0.1 \
  --relay-api-port=8890 \
  --relay-api-token=immersive-dev-token \
  --relay-player-count=6 \
  --relay-handoff-policy=lap_based_handoff \
  --immersive-output=syphon \
  --immersive-projection=fisheye_domemaster \
  --immersive-domemaster-size=1536 \
  --immersive-cubemap-size=768 \
  --immersive-camera=stabilized_chase \
  --immersive-pitch-deg=35 \
  --immersive-forward-offset=0.25 \
  --immersive-vertical-offset=0.40 \
  --immersive-smoothing-factor=0.16 \
  --immersive-horizon-stabilization=on \
  --immersive-roll-limit-deg=12 \
  --immersive-stats=on \
  --syphon-server-name="SuperTuxKart Immersive Relay Domemaster"
```

After launch, TholusOS test setup should call `/operator/reset`, then configure
and start the session through the frozen operator endpoints.

## 1. What Phase 5.1 Verifies

- Real target hardware identity.
- 60 second benchmark matrix on Mac mini M1 base.
- Operator API and FakeCrewPhones running together against one STK process.
- Correct session report semantics for crew-active and operator-only runs.
- Reset loops with clean state and live API.
- Syphon regression when Syphon is available.
- Handoff state for a one-wheel physical pilot swap.
- Final error recovery behavior.

## 2. Implementation vs Acceptance

Phase 5 implemented the arcade operator layer. Phase 5.1 decides whether that
implementation can be called venue-ready on the target machine. Passing the
integration package on an M1 Pro is enough for the software-complete verdict,
but not for final venue performance. The Mac mini M1 base benchmark remains
the required hardware acceptance step.

## 3. Hardware Inventory

`tools/immersive/RunRelayPhase51VenueAcceptance.sh` writes:

```text
/tmp/stk_phase51_venue_acceptance/hardware.txt
/tmp/stk_phase51_venue_acceptance/hardware.json
```

Check:

- Mac model;
- chip;
- RAM;
- macOS version;
- active displays;
- Syphon.framework location;
- whether the machine is the Mac mini M1 base target.

If the machine is not the target, the correct verdict is:

```text
Fase 5 software-complete. Hardware acceptance su Mac mini M1 pending.
```

## 4. Benchmark M1 Base

Run on the Mac mini M1 base:

```sh
DURATION=60 tools/immersive/RunRelayPhase5M1Benchmark.sh
```

Required cases:

| case | projection | output |
| --- | --- | --- |
| relay 16:9 | none/debug |
| fisheye 1024 | window |
| fisheye 1536 | window |
| fisheye 2048 | window |
| fisheye 1024 | Syphon |
| fisheye 1536 | Syphon |
| fisheye 2048 | Syphon |

Log average FPS, p50, p95, p99, dropped frames, capture ms, projection ms,
output ms, Syphon publish ms, CPU/GPU info, macOS version, machine model, and
visible stutter notes.

## 5. Recommended Presets

| preset | domemaster | cubemap | venue meaning |
| --- | ---: | ---: | --- |
| debug | 1024 | 512 | setup/debug |
| safe / fulldome_safe | 1536 | 768 | provisional venue default until M1 base proves otherwise |
| target / fulldome_target | 2048 | 1024 | use only after stable target-hardware benchmark |

If 2048 is not stable on the Mac mini M1 base, that is not a Phase 5 failure.
Declare `safe=1536` as the realistic venue preset and keep `target=2048` as an
aspiration.

## 6. Integrated Operator + FakeCrewPhones Test

Run:

```sh
tools/immersive/RunRelayPhase51VenueAcceptance.sh
```

The no-Syphon suite runs:

- 2p full session with FakeCrewPhones;
- 4p full session with FakeCrewPhones;
- 6p full session with FakeCrewPhones;
- 6p full session with mistakes;
- 6p full session with silent-role hacker;
- reset loop with fake phone actions;
- mixed reset sequence: partial reset, full reset, force-finish reset, abort
  reset;
- handoff confirm test;
- error recovery table.

The Syphon suite runs when available:

- Syphon receiver probe;
- 6p full session with FakeCrewPhones and `output=syphon`;
- Syphon reset loop with fake phone actions.

## 7. Correct Session Report

Current result report:

```text
state.arcade_session.report
```

Previous completed report after reset:

```text
state.arcade_session.last_report
```

Required fields:

```json
{
  "session_id": "phase51-no-syphon-a-6p",
  "player_count": 6,
  "track_id": "snowtuxpeak",
  "total_laps": 12,
  "completed_laps": 12,
  "final_result": "finished_gold",
  "rank": "gold",
  "score": 51845,
  "combo_count": 13,
  "encounters_started": 67,
  "encounters_succeeded": 58,
  "encounters_failed": 7,
  "actions_by_role": {
    "navigator": 31,
    "engineer": 25,
    "defender": 7,
    "hacker": 19,
    "gunner": 7
  },
  "driver_turns_by_player": {
    "p1": 2,
    "p2": 2,
    "p3": 2,
    "p4": 2,
    "p5": 2,
    "p6": 2
  },
  "render_profile": "fulldome_safe",
  "projection": "fisheye_domemaster",
  "output": "syphon",
  "crew_mode_enabled": true,
  "crew_actions_enabled": true,
  "crew_actions_accepted": 58,
  "duration_seconds": 0
}
```

Operator-only or disconnected-phone reports may have score `0`, but must say
`crew_actions_enabled=false` and `crew_actions_accepted=0`.

## 8. Reset Loop

The acceptance script verifies:

```text
configure 6p -> start -> partial fake phone action -> reset
configure 6p -> start -> full fake phones -> results -> reset
configure 4p -> start -> force-finish -> reset
configure 2p -> start -> abort -> reset
```

For every reset:

- API still answers;
- session id changes on next configure;
- score returns to 0;
- crew action counters return to 0;
- cooldowns and encounters are clean;
- active driver returns to the first configured player;
- total laps are recalculated for the configured player count;
- `arcade_session.report` is null outside results.

## 9. One-Wheel Handoff

Automated acceptance verifies the observable state:

```text
operator force handoff
-> handoff_active true
-> input_locked true
-> next_driver correct
-> waiting_for_operator_confirm true
-> /operator/handoff/confirm accepted
```

Manual venue checklist must still verify the physical procedure:

- a driver can release the wheel safely;
- the next driver understands when to take it;
- the pause is long enough but not boring;
- the kart does not crash during the handoff;
- the dome/Syphon output remains alive;
- crew roles remain understandable after reassignment.

## 10. Error Recovery

The acceptance harness checks:

- invalid token;
- invalid player count;
- start without configure;
- pause outside running;
- resume outside paused;
- reset during running;
- reset during handoff;
- force-finish during running;
- abort during running;
- invalid render preset;
- projection unavailable;
- Syphon unavailable in non-Syphon build;
- FakeCrewPhones disconnected/operator-only finish.

Every case must return JSON, not crash, and leave state coherent.

## 11. First Dome Test Checklist

Before guests:

- run the acceptance script once;
- run `RUN_SYPHON=1 RUN_SYPHON_RECEIVER=1 tools/immersive/RunRelayTholusIntegrationSmoke.sh`;
- run the 60 second M1 benchmark on the target machine;
- start Syphon receiver before guest flow;
- verify the operator token is local/trusted only;
- verify one-wheel handoff with two staff members;
- run 2p, 4p, and 6p;
- confirm reset time is acceptable for queue turnover;
- keep `safe` preset selected until `target` is proven stable.

## 12. Venue MVP Criteria

Declare Phase 5 software-complete if:

- vanilla build passes;
- relay build passes;
- Syphon build passes when framework is available;
- integrated Operator + FakeCrewPhones passes for 2p, 4p, and 6p;
- Syphon + Operator + FakeCrewPhones + reset loop passes when Syphon is
  available;
- reports are correct for crew-active and operator-only runs;
- handoff state/confirm passes;
- error recovery passes.

Declare final venue performance only if:

- Mac mini M1 base 60 second benchmark is complete.

Until then the correct verdict is:

```text
Fase 5 software-complete. Hardware acceptance su Mac mini M1 pending.
```

## 13. Known Limits

- No production equirectangular output.
- No cylindrical/CAVE/cube production output.
- No Windows/Spout.
- No VR/OpenXR.
- No Swift companion app or PWA in this repo.
- The accelerated acceptance harness is not a substitute for the manual
  one-wheel handoff test.
