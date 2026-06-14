# Immersive Relay Phase 3: Crew Event Director

## 1. Scope

Phase 3 turns the Phase 2 crew action API into contextual gameplay for a
16:9/debug SuperTuxKart session. The session is still a thin fork, protected by
`ENABLE_IMMERSIVE_RELAY`, `ENABLE_RELAY_LOCAL_API`, and the debug-only
`ENABLE_RELAY_TEST_TOOLS` option.

Implemented in this phase:

- Lap-based relay sessions for 2-6 participants.
- Stable round-robin pilot rotation at every lap.
- Data-driven crew encounters loaded from JSON track scripts.
- Contextual `phone_states`: each fake phone sees only actions relevant to its
  current role/window.
- Encounter validation for role, window, cooldown, already-used, expired, and
  stale state.
- Score, role score, combo score, track effects, and HUD/debug state feedback.
- Accelerated test progress for repeatable smoke tests.
- Phase 3 `FakeCrewPhones.py` modes for perfect, slow, mistakes, random,
  silent-role, and negative API probes.

## 2. Explicit Non-Scope

Phase 3 does not implement fulldome rendering, fisheye, equirectangular output,
Syphon, Spout, VR, a real PWA, a Swift app, or the full arcade state machine for
later phases. It also does not make track geometry physically open/close gates;
gate, shield, shortcut, target, and sprint feedback is exposed through HUD/debug
state and `track_effects`.

## 3. Data Files

Encounter scripts live under:

- `data/immersive_relay/encounters/snowtuxpeak_relay_encounters.json`
- `data/immersive_relay/encounters/default_relay_encounters.json`

The director first looks for a track-specific script named
`<track_id>_relay_encounters.json`. If it cannot load one, it falls back to the
default wildcard script.

## 4. JSON Schema

Top-level shape:

```json
{
  "version": 1,
  "track_id": "snowtuxpeak",
  "encounters": []
}
```

Encounter fields:

- `id`, `title`, `enabled`
- `player_count_min`, `player_count_max`
- `trigger`: type and lap/checkpoint/progress parameters
- `duration_seconds`
- `roles`: involved roles
- `stages`: per-role phone windows and actions
- `success`: required actions, score, combo tag/score, track effects
- `fail`: soft-fail score, soft penalty, track effects

Supported trigger types:

- `lap_start`
- `lap_checkpoint`
- `lap_progress`
- `lap_end`
- `final_lap`
- `final_stretch`

Lap filters:

- `lap`
- `laps`
- `lap_pattern`: `all`, `odd`, `even`, `final`
- `lap_every` plus `lap_offset`

Action identity is `type`, optionally refined by `target_id` or `mode`.

## 5. Lap Counts

The session is lap-based, not duration-based. The production end condition is
the completed lap count.

| player_count | total_laps |
| --- | ---: |
| 1 | 6 |
| 2 | 6 |
| 3 | 6 |
| 4 | 8 |
| 5 | 10 |
| 6 | 12 |

The rule is:

```text
if player_count <= 3 -> total_laps = 6
if player_count == 4 -> total_laps = 8
if player_count == 5 -> total_laps = 10
if player_count == 6 -> total_laps = 12
```

## 6. Pilot Rotation

The active driver changes every lap using stable round-robin:

```text
active_driver_index = (current_lap - 1) % player_count
```

For 3, 4, 5, and 6 participants this gives exactly 2 driven laps per player.
For 2 participants this gives 3 driven laps per player.

## 7. Session and Encounter States

Session phases exposed in `/immersive/v1/state` include:

- `pre_race`
- `lap_start`
- `running_lap`
- `lap_end`
- `final_lap`
- `final_stretch`
- `finish`
- `reset`

Encounter runtime states:

- `pending`
- `preview`
- `active`
- `resolved_success`
- `resolved_fail`
- `expired`

Each encounter template can run once per matching lap. Runtime ids are exposed as
`<encounter_id>_lap<lap>`.

## 8. Contextual Phone States

`phone_states` include:

- `player_id`
- `role`
- `lap`
- `total_laps`
- `active_driver`
- `encounter_id`
- `next_encounter_id`
- `message`
- contextual `actions`
- cooldown and unavailable reason per action

The director publishes actions only for the best current encounter window. A
non-involved role sees stand-by copy and no filler buttons. Preview actions are
shown disabled with `wrong_window` so fake phones can display upcoming work
without accepting early taps.

## 9. Validation

Crew actions are rejected with Phase 3 reasons:

- `wrong_role`
- `wrong_window`
- `already_used`
- `cooldown`
- `encounter_expired`
- `stale_state`

The old Phase 2 static action surface still exists underneath, but Phase 3
director validation gates accepted gameplay effects.

## 10. Score and Combos

Accepted actions give base score through the Phase 2 action handler. Encounter
success adds payoff score. Combo encounters add combo score and increment
`combo_count`.

Implemented combo tags:

- `shortcut_combo`
- `defense_combo`
- `final_sprint_combo`

Score output in `/state` includes:

- `crew_score`
- `crew_multiplier_x100`
- `actions_accepted_by_role`
- `score_by_role`
- `combo_count`

Soft failure does not block the race and does not apply negative score in this
MVP.

## 11. Implemented Archetypes

- `route_choice_ridge`: Navigator route callout.
- `blue_gate_hack`: Hacker gate open/close debug effect.
- `engineer_boost_window`: Engineer boost/heat tradeoff.
- `combo_shortcut`: Navigator + Hacker + Engineer shortcut combo.
- `danger_tunnel_defense`: Defender + Engineer defense combo.
- `drone_target_clear`: Gunner target clear.
- `lap_end_handoff_call`: Navigator handoff pace note.
- `final_sprint_sync`: all available roles, final sprint combo.

## 12. Build and Smoke

Relay configure:

```bash
cmake -S . -B cmake_build_relay_phase1 \
  -DENABLE_IMMERSIVE_RELAY=ON \
  -DENABLE_RELAY_LOCAL_API=ON \
  -DENABLE_RELAY_TEST_TOOLS=ON
```

Vanilla build:

```bash
cmake --build cmake_build_vanilla --parallel 8
```

Relay build:

```bash
cmake --build cmake_build_relay_phase1 --parallel 8
```

Full Phase 3 smoke:

```bash
tools/immersive/RunRelayPhase3Smoke.sh
```

Manual server:

```bash
cmake_build_relay_phase1/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  --no-graphics \
  -N \
  --immersive-relay \
  --relay-local-api \
  --relay-test-accelerated-progress \
  --relay-api-token=immersive-dev-token \
  --relay-player-count=6 \
  --relay-handoff-policy=lap_based_handoff \
  --track=snowtuxpeak \
  --kart=tux \
  --numkarts=6 \
  --difficulty=0 \
  --unlock-all
```

Manual harness examples:

```bash
tools/immersive/FakeCrewPhones.py --players 2 --mode perfect
tools/immersive/FakeCrewPhones.py --players 3 --mode perfect
tools/immersive/FakeCrewPhones.py --players 4 --mode perfect
tools/immersive/FakeCrewPhones.py --players 5 --mode perfect
tools/immersive/FakeCrewPhones.py --players 6 --mode perfect
tools/immersive/FakeCrewPhones.py --players 6 --mode wrong-role
tools/immersive/FakeCrewPhones.py --players 6 --mode wrong-window
tools/immersive/FakeCrewPhones.py --players 6 --mode cooldown
tools/immersive/FakeCrewPhones.py --players 6 --mode already-used
tools/immersive/FakeCrewPhones.py --players 6 --mode expired
tools/immersive/FakeCrewPhones.py --players 6 --mode stale
```

## 13. Known Limits

- Track effects are debug/HUD/API state effects, not physical track mesh changes.
- The local API is still localhost/dev-token only.
- Accelerated progress is compiled only with `ENABLE_RELAY_TEST_TOOLS`.
- The arcade-grade Phase 5 state machine is not implemented yet.
- Phone UI is still `FakeCrewPhones.py`, not a PWA or native app.

## 14. Phase 4 Handoff

Phase 4 can consume the stable `track_effects`, `active_encounter`,
`dome_callout`, and contextual `phone_states` to drive fulldome/debug visual
composition. The Phase 3 director intentionally keeps rendering-specific output
generic so later fisheye/equirect/Syphon/Spout work can attach without changing
the gameplay contract.
