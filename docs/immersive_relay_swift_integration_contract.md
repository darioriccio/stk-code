# Immersive Relay Swift Integration Contract

Phase 5.3 TholusOS contract freeze: the current STK-side integration contract is
documented in `docs/immersive_relay_tholustos_integration.md`. The default API
base URL is:

```text
http://127.0.0.1:8890/immersive/v1
```

All frozen endpoints require the local trusted token. TholusOS/Swift owns
operator UI, phones, session authority, roles, reset, Syphon ingest, dome
routing, and forwarding crew actions. STK remains gameplay/render/Syphon/local
API provider.

Phase 3.5 documents how the future Swift/macOS control layer will talk to the
Phase 3 SuperTuxKart relay runtime. It does not add a real companion app.

## Current Reality

- There is no production Swift companion app yet.
- `tools/immersive/FakeCrewPhones.py` is a test harness for repeatable smoke and
  playability audits.
- `tools/immersive/SwiftFakeRelayClient.swift` is a dev tool and API proof, not
  the production phone UI.
- SuperTuxKart is the local gameplay engine.
- The STK relay local API is trusted, local, token-protected, and intended to be
  called by the future Swift/macOS app, not by phones directly.

## Ownership Boundary

The future Swift/macOS app is expected to be authority for:

- player join and identity;
- participant slots and operator assignment;
- crew role presentation;
- phone routing and UI;
- session start, reset, stop, and operator controls;
- player-facing errors and recovery flows;
- eventual payment, booking, and slot management.

SuperTuxKart remains authority for:

- race/lap progress;
- active driver rotation;
- crew encounter state;
- contextual action validation;
- score, combo, cooldown, and fail-soft payoff;
- local gameplay/HUD/debug state.

Phones must talk to the Swift/macOS app. The Swift/macOS app talks to STK.

```text
phone browser/native view -> Swift/macOS authority -> STK local relay API
STK local relay API -> Swift/macOS authority -> per-phone UI state
```

## Endpoints

Phase 5.3 frozen endpoint set:

```http
GET  /immersive/v1/state
GET  /immersive/v1/operator/state
GET  /immersive/v1/capabilities
POST /immersive/v1/operator/configure
POST /immersive/v1/operator/start
POST /immersive/v1/operator/countdown
POST /immersive/v1/operator/pause
POST /immersive/v1/operator/resume
POST /immersive/v1/operator/abort
POST /immersive/v1/operator/reset
POST /immersive/v1/operator/force-finish
POST /immersive/v1/operator/handoff/confirm
POST /immersive/v1/operator/render-preset
POST /immersive/v1/render/config
POST /immersive/v1/crew/action
```

Swift/TholusOS reads state:

```http
GET /immersive/v1/state
X-STK-Relay-Token: <local-token>
```

Swift may request an already filtered phone state for development:

```http
GET /immersive/v1/state?player_id=p2
GET /immersive/v1/state?player_id=p2&role=navigator
```

Swift sends crew actions:

```http
POST /immersive/v1/crew/action
X-STK-Relay-Token: <local-token>
Content-Type: application/json
```

Legacy session/operator endpoints still used by dev tools, but not part of the
TholusOS freeze:

```http
POST /immersive/v1/session/start
POST /immersive/v1/session/reset
POST /immersive/v1/operator/handoff
POST /immersive/v1/operator/next-state
```

## Phone Public State

A real phone UI should render only the corresponding entry in `phone_states`.
Public per-phone fields are:

- `player_id`
- `role`
- `lap`
- `total_laps`
- `active_driver`
- `encounter_id`
- `next_encounter_id`
- `title`
- `message`
- `resource_text`
- `remaining_ms`
- `primary`
- `actions`

Each action contains:

- `type`
- `label`
- `role`
- `target_key`
- `target_id`
- `mode_key`
- `mode`
- `amount`
- `available`
- `unavailable_reason`
- `cooldown_ms`

The UI should hide unavailable actions by default, or show them disabled only
when that helps explain timing or cooldown. It should not infer future encounter
answers from operator/debug fields.

## Operator and Debug State

The top-level `/state` payload contains extra fields for the operator console,
debug HUD, and automated tests. These must not be treated as phone knowledge:

- `active_encounter`
- `track_effects`
- `last_crew_event`
- `last_crew_effect`
- `last_accepted_actions`
- `last_rejected_actions`
- `cooldowns`
- `role_assignments`
- `score_by_role`
- `actions_accepted_by_role`
- `adaptive_difficulty`

The Swift/macOS app may show these fields to an operator view, but ordinary
phones should receive only their own mapped `phone_states` entry.

## Real State Example

Captured from Phase 3.5 with 3 players on `snowtuxpeak`, lap 1, progress 0.16:

```json
{
  "ok": true,
  "session_id": "phase35-swift-contract-sample",
  "state": "running",
  "race_phase": "running_lap",
  "state_version": 10,
  "player_count": 3,
  "current_lap": 1,
  "total_laps": 6,
  "lap_progress": 0.16,
  "active_driver": {
    "index": 0,
    "player_id": "p1",
    "display_name": "Player 1"
  },
  "crew_score": 0
}
```

The matching active encounter debug object:

```json
{
  "active": true,
  "id": "route_choice_ridge_lap1",
  "title": "Ridge route choice",
  "stage": "navigator_route",
  "state": "active",
  "trigger_type": "lap_progress",
  "combo_tag": "",
  "dome_callout": "ACTIVE Ridge route choice",
  "remaining_ms": 16000,
  "required_actions": 1,
  "completed_actions": 0,
  "roles": ["navigator"]
}
```

The phone-facing state for `p2`:

```json
{
  "player_id": "p2",
  "role": "navigator",
  "lap": 1,
  "total_laps": 6,
  "active_driver": {
    "index": 0,
    "player_id": "p1"
  },
  "encounter_id": "route_choice_ridge_lap1",
  "next_encounter_id": "route_choice_ridge_lap1",
  "title": "Ridge split",
  "message": "Mark LEFT for the shorter ridge line.",
  "resource_text": "boost=0 heat=0 armor=100",
  "remaining_ms": 16000,
  "primary": true,
  "actions": [
    {
      "type": "nav_select_route",
      "label": "Mark LEFT",
      "role": "navigator",
      "target_key": "route_id",
      "target_id": "ridge_left",
      "mode_key": "",
      "mode": "",
      "amount": 0,
      "available": true,
      "unavailable_reason": "",
      "cooldown_ms": 0
    },
    {
      "type": "nav_select_route",
      "label": "Safe right",
      "role": "navigator",
      "target_key": "route_id",
      "target_id": "ridge_safe",
      "mode_key": "",
      "mode": "",
      "amount": 0,
      "available": true,
      "unavailable_reason": "",
      "cooldown_ms": 0
    }
  ]
}
```

## Phone Action Mapping

The phone sends a user intent to Swift/macOS. Swift validates session ownership,
player identity, and UI freshness, then forwards an STK action:

```json
{
  "session_id": "phase35-swift-contract-sample",
  "action_id": "phone-generated-action-id",
  "idempotency_key": "phone-generated-idempotency-key",
  "state_version_seen": 10,
  "player_id": "p2",
  "role": "navigator",
  "type": "nav_select_route",
  "payload": {
    "route_id": "ridge_left"
  },
  "route_id": "ridge_left"
}
```

For Phase 5.3 compatibility, STK also accepts `state_version` as an alias for
`state_version_seen`, numeric `player_id` values mapped to `pN`, missing
`session_id` filled from the active trusted local session, and the fixture alias
`action_id=navigator_mark_left_route` mapped to `nav_select_route` with
`route_id=ridge_left`.

STK can reject the action with a stable reason:

- `wrong_role`
- `wrong_window`
- `already_used`
- `cooldown`
- `encounter_expired`
- `stale_state`
- `invalid_token`
- `invalid_payload`

Swift should convert these into short human messages, for example "Too late",
"That role is not active", or "Wait for cooldown".

## Security Expectations

Phase 3 uses a local trusted token. Production integration should keep this
contract:

- bind STK API to localhost or a private operator network only;
- issue tokens from the Swift/macOS authority, not from phone clients;
- never expose the STK token to phones;
- rate-limit phone actions at the Swift layer before forwarding to STK;
- keep STK idempotency keys per user action;
- reject stale phone UI actions before forwarding when possible;
- log operator/debug actions separately from player actions.

## Phase 4 Video Output

Phase 4 adds a same-machine video path from STK to the future Swift/macOS dome
app. It does not change phone authority or local API ownership.

Phase 4.5 validation note, 2026-06-09: the Syphon transport path is buildable
and a minimal receiver can see a live 1024x1024 server. The current STK
projection is a forward-facing 180 degree fisheye lens, not a zenith-centered
dome aperture: the center of the disc is the active driver's forward view, with
the kart visible in the low stabilized chase preset. The Swift app may implement
receiver diagnostics against this contract while venue calibration continues.

STK video output:

- transport: Syphon on macOS;
- default server name: `SuperTuxKart Immersive Relay Domemaster`;
- projection mode: production `fisheye_domemaster`;
- texture shape: final projected frame; fisheye/domemaster is square today,
  future projections may not be square;
- supported resolutions: 1024x1024, 1536x1536, 2048x2048;
- expected frame rate: best effort from the STK render loop;
- lifecycle: server starts when `--immersive-output=syphon` initializes, then
  publishes one projected texture per rendered frame;
- error cases: Syphon build unavailable, Syphon framework missing, no OpenGL
  context, server initialization failure, publish skipped, frame stalled,
  resolution mismatch, wrong projection, wrong orientation, or visible cubemap
  stitching artifact.

Swift/macOS dome app responsibilities:

- discover/select the Syphon server;
- capture the texture or frame from the selected server;
- present the domemaster to the dome pipeline;
- apply projector blend/warp/calibration outside STK;
- optionally show receiver diagnostics such as server name, resolution, frame
  age, and dropped frames;
- keep phone/session authority separate from the video receiver.

Important boundary:

- phones do not receive Syphon;
- Syphon is only `STK -> Swift/macOS dome app` video;
- the local API is still for state, crew actions, and operator commands;
- projector blend/warp belongs to the Swift/dome presentation layer, not to STK
  Phase 4.

The top-level `/immersive/v1/state` payload includes an `immersive_render`
debug object in relay builds. The Swift app may use it for operator diagnostics,
but phones should not treat it as player knowledge.

Relay local API builds also expose `/immersive/v1/render/config` for operator
camera/render tuning. The endpoint accepts the same camera parameters used by
the CLI, including `yaw_deg`, `pitch_deg`, `roll_deg`, `dome_tilt_deg`,
`forward_offset`, `vertical_offset`, `smoothing_factor`,
`horizon_stabilization`, and `roll_limit_deg`. Positive `pitch_deg` looks up.

## Projection-General Video Contract

Phase 4.6 makes the video contract projection-general while keeping
`fisheye_domemaster` as the only production projection.

STK responsibilities:

- render gameplay;
- capture a monoscopic full sphere as six cubemap-equivalent faces;
- project that capture into the selected final projection;
- publish the final projected frame through the selected output adapter;
- expose `/state.immersive_render` with capture/projection/output/camera/stats;
- reject unavailable projection kinds instead of publishing placeholder video.

TholusOS/Swift responsibilities:

- discover/select the Syphon server;
- read `/state.immersive_render` before routing video;
- use `projection.type`, `projection.width`, `projection.height`,
  `projection.aspect`, `projection.coverage`, and `projection.orientation`;
- capture the Syphon texture/frame;
- route it to the venue pipeline;
- apply projector warp/blend/layout;
- show receiver diagnostics;
- keep phone/session authority separate from STK video output.

Swift should not infer whether the frame is fisheye, equirectangular, or any
future profile by dimensions alone. It should read the state contract.

Current and future profiles:

```json
{"profile_id":"fulldome_180","projection":"fisheye_domemaster","preferred_resolution":"2048x2048","output":"syphon","status":"production"}
{"profile_id":"equirect_360","projection":"equirectangular","preferred_resolution":"2048x1024","output":"syphon","status":"future"}
{"profile_id":"cylindrical_270","projection":"cylindrical","status":"future"}
{"profile_id":"cave_cube","projection":"wall_atlas_or_cube_faces","status":"future"}
```

Important boundaries:

- Syphon is video only: `STK -> Swift/TholusOS`.
- Phones do not receive Syphon.
- Phones talk to Swift/TholusOS.
- Swift/TholusOS talks to STK local API for state and commands.
- Swift does not convert fisheye/equirect in Phase 4.6; STK sends the final
  projected frame.

## Phase 5 Operator/Session Contract

Phase 5 adds a venue operator layer. There is still no real Swift companion app
in this repository; Swift/TholusOS is expected to become the operator and phone
authority.

Authority split:

- Swift app is the operator/session authority.
- STK is the gameplay/render engine.
- Swift configures sessions through `/immersive/v1/operator/configure`.
- Swift starts, pauses, resumes, aborts, resets, force-finishes, and confirms
  handoffs through `/immersive/v1/operator/*`.
- Swift reads `/immersive/v1/operator/state` or `/immersive/v1/state`.
- Swift forwards validated crew actions to `/immersive/v1/crew/action`.
- STK validates crew actions against session state, role, cooldown, encounter
  window, and idempotency.
- Swift receives video through Syphon.
- Swift shows current results from `state.arcade_session.report` while the
  session is in `results`.
- Swift may show or archive the previous completed result from
  `state.arcade_session.last_report` after reset.
- Swift manages real phones, identities, join flow, and player-facing errors.

Operator endpoints:

```http
GET  /immersive/v1/capabilities
GET  /immersive/v1/operator/state
POST /immersive/v1/operator/configure
POST /immersive/v1/operator/start
POST /immersive/v1/operator/countdown
POST /immersive/v1/operator/pause
POST /immersive/v1/operator/resume
POST /immersive/v1/operator/abort
POST /immersive/v1/operator/reset
POST /immersive/v1/operator/force-finish
POST /immersive/v1/operator/handoff/confirm
POST /immersive/v1/operator/render-preset
POST /immersive/v1/render/config
```

`/operator/next-state` remains a dev/test endpoint and is not in the Phase 5.3
TholusOS freeze.

All calls require the local trusted token. Swift must never expose the STK token
to phones.

`state.arcade_session` is the operator-ready object. Swift should use it to
decide which operator controls are enabled, who is driving, whether handoff is
active, whether the session can reset, and which result/report to show.

Minimum fields Swift should consume:

- `session_id`
- `state`
- `configured`
- `player_count`
- `total_laps`
- `current_lap`
- `remaining_laps`
- `active_driver`
- `previous_driver`
- `next_driver`
- `handoff_active`
- `input_locked`
- `handoff_countdown`
- `waiting_for_operator_confirm`
- `countdown`
- `pilot_turns_by_player`
- `can_start`
- `can_pause`
- `can_resume`
- `can_reset`
- `can_abort`
- `can_force_finish`
- `last_error`
- `config`
- `report`
- `last_report`

Report fields Swift should preserve for results and diagnostics:

- `session_id`
- `player_count`
- `track_id`
- `total_laps`
- `completed_laps`
- `final_result`
- `rank`
- `score`
- `combo_count`
- `encounters_started`
- `encounters_succeeded`
- `encounters_failed`
- `actions_by_role`
- `driver_turns_by_player`
- `duration_seconds`
- `render_profile`
- `projection`
- `output`
- `crew_mode_enabled`
- `crew_actions_enabled`
- `crew_actions_accepted`

Operator-only sessions can legitimately report `score=0` and
`crew_actions_enabled=false`. A crew-enabled session with real phones or
FakeCrewPhones active should report positive score and accepted crew actions.

Phase 5 still expects phones to talk to Swift first:

```text
phone -> Swift authority -> STK /crew/action
STK /state -> Swift authority -> phone/operator UI
STK Syphon -> Swift receiver -> dome/projector pipeline
```

## Future Swift/PWA Needs

Later phases need:

- a real Swift/macOS authority app;
- per-phone UI rendering from `phone_states`;
- operator dashboard using top-level debug state;
- session discovery and join flow;
- local network transport from phones to Swift/macOS;
- secure slot/player identity;
- better UX copy for rejection reasons and handoffs;
- optional web/PWA surface only after the authority boundary is implemented.
