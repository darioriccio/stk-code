# Immersive Relay TholusOS Integration Contract

Phase 5.3 freezes the STK-side contract for TholusOS integration. TholusOS is
the operator/session/phone authority. SuperTuxKart remains the local gameplay
engine, render engine, Syphon publisher, and trusted local API provider.

## Base URL and Auth

Default base URL:

```text
http://127.0.0.1:8890/immersive/v1
```

All calls require either:

```http
X-STK-Relay-Token: immersive-dev-token
Authorization: Bearer immersive-dev-token
```

The API is a trusted local API. Default bind is `127.0.0.1`; phones must not
call STK directly. Phones talk to TholusOS, TholusOS talks to STK, and Syphon is
video only from STK to TholusOS.

## Frozen Endpoints

```text
GET  /state
GET  /operator/state
GET  /capabilities
POST /operator/configure
POST /operator/start
POST /operator/countdown
POST /operator/pause
POST /operator/resume
POST /operator/abort
POST /operator/reset
POST /operator/force-finish
POST /operator/handoff/confirm
POST /operator/render-preset
POST /render/config
POST /crew/action
```

The implementation still has a few legacy/dev endpoints such as `/session/*`,
`/operator/handoff`, `/operator/next-state`, and `/test/progress`. TholusOS
should not depend on those for production control.

## Capabilities

`GET /immersive/v1/capabilities` lets TholusOS discover outputs, projections,
render profiles, policy, endpoint availability, and local security assumptions.
The Syphon build reports `supported_outputs` including `syphon`; non-Syphon
builds report Syphon as unavailable in `output_adapters.syphon.status`.

See `docs/fixtures/tholustos_integration/capabilities.json` for a captured
Syphon-build response.

## Safe Render Preset

The provisional TholusOS venue preset is:

```json
{
  "render_profile": "fulldome_safe",
  "projection": "fisheye_domemaster",
  "output": "syphon",
  "domemaster_size": 1536,
  "cubemap_size": 768,
  "camera": {
    "mode": "stabilized_chase",
    "pitch_deg": 35,
    "forward_offset": 0.25,
    "vertical_offset": 0.4,
    "smoothing_factor": 0.16,
    "horizon_stabilization": true,
    "roll_limit_deg": 12
  },
  "syphon_server_name": "SuperTuxKart Immersive Relay Domemaster"
}
```

`target`/2048 remains available for explicit testing, but it is not the venue
default until the Mac mini M1 base benchmark accepts it.

## Session Configure

The frozen 6-player TholusOS request is:

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

Expected state after configure: `configured=true`, `player_count=6`,
`total_laps=12`, `handoff_policy=lap_based_handoff`,
`render_profile=fulldome_safe`, `projection=fisheye_domemaster`,
and `output=syphon`.

## Crew Action Forwarding

Canonical payload:

```json
{
  "session_id": "relay-local",
  "player_id": "p2",
  "role": "navigator",
  "type": "nav_select_route",
  "action_id": "phone-generated-action-id",
  "idempotency_key": "uuid",
  "state_version_seen": 123,
  "payload": {
    "route_id": "ridge_left"
  }
}
```

Compatibility accepted for TholusOS Codable tests:

- `state_version` is accepted as an alias for `state_version_seen`.
- numeric `player_id` such as `2` is accepted as `p2`.
- missing `session_id` is filled from the active trusted local session.
- `action_id=navigator_mark_left_route` maps to the existing
  `nav_select_route` action with `route_id=ridge_left`.

Stable crew rejection reasons include `wrong_role`, `wrong_window`, `cooldown`,
`already_used`, `encounter_expired`, `stale_state`, `invalid_token`, and
`invalid_payload`.

## Official Startup Command

Use the Syphon build for TholusOS end-to-end tests:

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

The startup race gives STK a live render context. TholusOS test setup should
call `POST /operator/reset`, then configure/start/countdown through the frozen
operator endpoints.

## Smoke

API-only:

```bash
RUN_SYPHON=0 tools/immersive/RunRelayTholusIntegrationSmoke.sh
```

Full Syphon:

```bash
RUN_SYPHON=1 RUN_SYPHON_RECEIVER=1 tools/immersive/RunRelayTholusIntegrationSmoke.sh
```

The smoke verifies capabilities, state, safe render config, 6-player configure,
start/countdown, crew forwarding, score growth, optional Syphon 1536x1536
publish, reset, and a final readable report.
