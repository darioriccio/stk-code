# Immersive Relay Phase 2 API Contract

Phase 2 keeps phones fake on purpose. `tools/immersive/FakeCrewPhones.py`
talks directly to STK only as a smoke-test harness.

Production ownership stays:

```text
Phone -> Swift app/session server -> STK local API
```

The Swift app remains authoritative for venue session lifecycle, join flow,
role display, phone UI, reset, operator control and token handling. STK accepts
validated local commands from the Swift app through the localhost API.

## Runtime And Launcher Rule

Opening a relay-capable STK `.app` is not enough to enable Immersive Relay. STK
intentionally stays vanilla-like unless the process is launched with
`--immersive-relay` or the relay local API is explicitly requested. This is what
keeps the fork thin and lets a vanilla build/run remain comparable to upstream.

All relay API, handoff, lap-rotation, and sala giochi UI tests must launch STK
through a relay command/launcher that passes the relay flags. For venue UI tests,
the launcher must include `--relay-arcade-ui`; otherwise players will still see
vanilla setup screens and editable vanilla options such as lap count and AI
karts.

Local launcher intent:

```text
Apri Immersive Relay Phase 2.command       direct keyboard race smoke
Apri Immersive Relay Sala Giochi.command   kart -> track -> champions -> race
```

The sala giochi flow should keep track champions/high scores visible as a
motivational leaderboard. Only vanilla technical race settings are hidden:
editable lap count, AI count, reverse/random options, and the ghost replay
recording checkbox.

`POST /immersive/v1/crew/action` is a local/trusted API. It is not a public
endpoint for real phones. `GET /immersive/v1/state` is the Swift app read model;
Swift republishes the relevant phone state to actual devices.

The relay token is mandatory in development and production. Send either:

```http
X-STK-Relay-Token: immersive-dev-token
```

or:

```http
Authorization: Bearer immersive-dev-token
```

## Valid Crew Action

```json
{
  "session_id": "tholus-fake-crew-001",
  "action_id": "68f33c10-c572-4dd0-a0a3-304f0d3a04f0",
  "idempotency_key": "p5-open-blue-gate-001",
  "state_version_seen": 42,
  "player_id": "p5",
  "role": "hacker",
  "type": "hacker_open_gate",
  "payload": {
    "gate_id": "gate_blue_02"
  }
}
```

## Success Response

```json
{
  "ok": true,
  "event": "crew_action_accepted",
  "accepted": true,
  "duplicate": false,
  "idempotency_key": "p5-open-blue-gate-001",
  "state_version": 43,
  "state": "running",
  "effect": "gate_open:gate_blue_02",
  "score_delta": 120,
  "crew_score": 120,
  "cooldown_ms": 10000,
  "crew_multiplier_x100": 100
}
```

## Duplicate Idempotency Response

```json
{
  "ok": true,
  "event": "duplicate_ignored",
  "accepted": false,
  "duplicate": true,
  "idempotency_key": "p5-open-blue-gate-001",
  "state_version": 43,
  "state": "running",
  "effect": "gate_open:gate_blue_02",
  "score_delta": 0,
  "crew_score": 120,
  "cooldown_ms": 10000,
  "crew_multiplier_x100": 100
}
```

## Rejection Response

```json
{
  "ok": false,
  "error": "role_action_mismatch",
  "state_version": 44
}
```

Rate-limit and cooldown rejections can include:

```json
{
  "ok": false,
  "error": "crew_action_rate_limited",
  "state_version": 44,
  "retry_after_ms": 180
}
```

## Error Codes

```text
malformed_json:<detail>
invalid_or_missing_token
method_not_allowed
unknown_endpoint
relay_runtime_not_enabled
race_not_attached
session_paused
session_not_running
single_pilot_no_crew
crew_actions_locked_during_handoff
missing_session_id
wrong_session_id
missing_player_id
unknown_crew_player
active_pilot_cannot_use_crew_action
missing_action_type
unknown_crew_action
missing_idempotency_key
idempotency_key_reused
missing_state_version
stale_state_version
future_state_version
unknown_crew_role
role_action_mismatch
role_not_assigned_to_player
crew_action_on_cooldown
crew_action_rate_limited
boost_not_charged
```

Rejected actions must not modify crew score, cooldowns, track effects or kart
effects. Rejections can update `last_rejected_actions` debug history.

Accepted actions are validated and queued for race-thread gameplay effects.
The HTTP thread does not touch kart/world objects.

## State Contract

`GET /immersive/v1/state` always includes:

```text
state_version
relay_enabled
race_attached
active_driver
handoff_policy
crew_score
crew_multiplier
crew_heat
crew_armor
boost_charge
role_assignments
phone_states
track_effects
cooldowns
last_accepted_actions
last_rejected_actions
adaptive_difficulty_enabled
adaptive_difficulty_profile
adaptive_difficulty_intensity
adaptive_pressure
human_gap_ms
human_position
assist_level
ai_pace_level
leaderboard_bucket
adaptive_difficulty
```

`POST /immersive/v1/session/start` can enable the deterministic adaptive
difficulty director:

```json
{
  "adaptive_difficulty": {
    "enabled": true,
    "profile": "arcade_balanced",
    "visibility": "leaderboard_bucket",
    "min_intensity": 0,
    "max_intensity": 100
  }
}
```

Flat compatibility fields are also accepted:

```json
{
  "adaptive_difficulty_enabled": true,
  "adaptive_difficulty_profile": "arcade_balanced"
}
```

When adaptive is enabled, `leaderboard_bucket` is `arcade_adaptive`. When it is
disabled, it is `arcade_fixed` unless Swift intentionally uses the
`solo_no_assist` profile for no-assist solo records.

`phone_states` can be filtered:

```bash
curl -H 'X-STK-Relay-Token: immersive-dev-token' \
  'http://127.0.0.1:8890/immersive/v1/state?player_id=p3&role=engineer'
```

Each phone action includes:

```text
type
role
available
unavailable_reason
cooldown_ms
target_key / target_id
mode_key / mode
amount
```

## Role Assignment Matrix

```text
player_count | active_driver | crew_slots | assigned_roles                                      | combined_roles                         | smoke_result
2            | p1            | 1          | p2: navigator, engineer, defender, hacker, gunner  | p2 all five roles                      | FakeCrewPhones --players 2 --mode perfect
3            | p1            | 2          | p2: navigator+hacker; p3: engineer+defender+gunner | p2, p3                                 | FakeCrewPhones --players 3 --mode perfect
4            | p1            | 3          | p2: navigator; p3: engineer; p4: defender+hacker+gunner | p4                                  | FakeCrewPhones --players 4 --mode perfect
5            | p1            | 4          | p2: navigator; p3: engineer; p4: defender; p5: hacker+gunner | p5                                | FakeCrewPhones --players 5 --mode perfect
6            | p1            | 5          | p2: navigator; p3: engineer; p4: defender; p5: hacker; p6: gunner | none                         | FakeCrewPhones --players 6 --mode perfect
```

## Smoke Commands

Run the full Phase 2 smoke:

```bash
tools/immersive/RunRelayPhase2Smoke.sh
```

Expected final line:

```text
[done] relay phase 2 smoke passed
```

Individual commands:

```bash
cmake --build cmake_build_vanilla --parallel 8
cmake --build cmake_build_relay_phase1 --parallel 8
tools/immersive/ValidateRelayState.py
tools/immersive/FakeCrewPhones.py --players 6 --mode perfect
tools/immersive/ValidateRelayState.py --player-id p3 --role engineer
tools/immersive/FakeCrewPhones.py --players 5 --mode perfect
tools/immersive/FakeCrewPhones.py --players 4 --mode perfect
tools/immersive/FakeCrewPhones.py --players 3 --mode perfect
tools/immersive/FakeCrewPhones.py --players 2 --mode perfect
tools/immersive/FakeCrewPhones.py --players 6 --mode wrong-role
tools/immersive/FakeCrewPhones.py --players 6 --mode duplicate
tools/immersive/FakeCrewPhones.py --players 6 --mode stale
tools/immersive/FakeCrewPhones.py --players 6 --mode spam
tools/immersive/FakeCrewPhones.py --players 6 --mode invalid-token
tools/immersive/ProbeRelayApiHardening.py --players 6
tools/immersive/FakeCrewPhones.py --players 6 --mode disconnect-reconnect
tools/immersive/FakeCrewPhones.py --players 6 --mode slow
```

Expected exit code is `0` for every command above when the relay API is
running, except build failures or unreachable server.
