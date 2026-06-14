# AGENTS.md — SuperTuxKart Immersive Relay

**Project:** SuperTuxKart fork/mod for immersive arcade venues.
**Owner context:** Tholus / Swift dome app / smartphone controller system.
**Primary milestone:** macOS + Mac mini M1 + wheel/pedals pilot + up to 5 phone crew + Relay gameplay + fisheye/domemaster via Syphon.
**Future milestones:** equirectangular, Windows/Spout, panoramic/cylindrical, CAVE/wall atlas, VR/OpenXR later.
**Last updated:** 2026-06-09.

This file is the root instruction set for AI coding agents working in this repository. Keep changes small, tested, documented, and easy to rebase onto upstream SuperTuxKart.

---

## 0. Mandatory first step

Before coding, read:

```text
supertuxkart_immersive_relay_master_spec.md
README.md
INSTALL.md
CMakeLists.txt
```

For Phase 4 render/output work, also read:

```text
immersive_relay_phase4_fulldome_syphon.md
immersive_relay_phase46_projection_generalization.md
immersive_relay_swift_integration_contract.md
immersive_relay_upstream_sync.md
```

If the master spec is not in `docs/`, ask the operator to place it there or copy `/mnt/data/supertuxkart_immersive_relay_master_spec.md` into:

```text
docs/supertuxkart_immersive_relay_master_spec.md
```

Then inspect the actual repository. Do not assume class names.

Use searches like:

```bash
rg "RaceManager|World|Camera|Checkpoint|Kart|Input|Irrlicht|OpenGL|Vulkan|Metal|render|framebuffer" src .
rg "lap|checkpoint|camera|controller|gamepad|wheel|network|http|websocket" src .
rg "CMAKE|option\(|feature|ENABLE" CMakeLists.txt .
```

Write findings into:

```text
docs/immersive/repo_audit.md
```

---

## 1. Mission

Implement **SuperTuxKart Immersive Relay**, a thin fork/mod that turns SuperTuxKart into an immersive cooperative arcade attraction.

The first real product is:

```text
Dome Relay Crew Mode
```

It must support:

```text
1 active pilot at a time
pilot drives with wheel + pedals
up to 5 supporter players use smartphones
pilot switch at checkpoints/segments
crew actions that materially affect gameplay
true immersive output, not stretched 16:9
fisheye/domemaster 2048x2048 first target
Syphon output on macOS
Swift app controls sessions and phones
```

Design for multi-environment future:

```text
Gameplay Relay
  -> Immersive Camera Rig
  -> Projection Adapter
  -> Output Adapter
  -> Swift app / venue renderer
```

Dome is the first environment profile, not a hardcoded assumption.

---

## 2. Non-negotiable rules

1. **Keep the fork thin.** Prefer new modules and minimal hooks over core rewrites.
2. **Preserve vanilla SuperTuxKart.** With immersive disabled, the normal game must still build and run.
3. **Separate gameplay, projection, and output.** No Syphon/Spout code inside gameplay logic.
4. **No 16:9 stretch.** Immersive output must come from true 3D rendering, initially cubemap/multi-view to fisheye.
5. **Swift app stays authoritative for venue sessions.** Phones talk to Swift; Swift talks to STK local API.
6. **Pilot uses physical driving controller.** Smartphone driving is fallback/demo only, not primary.
7. **Supporters use atomic actions.** No continuous joystick/drag control for crew.
8. **Crew must be meaningful.** Every role needs unique info, unique actions, timing pressure, visible cupola feedback.
9. **Camera must be comfort-aware.** Stabilize horizon, clamp roll, smooth motion.
10. **macOS first, Windows-ready.** Platform APIs live behind adapters.
11. **All new features behind flags/config.** Do not break upstream behavior.
12. **No unclear assets.** Respect STK licensing and GPL obligations.

---

## 3. Product design rule: not a kart game with phone bonuses

The design target is:

```text
The pilot drives.
The crew makes it possible to drive well.
```

The driver should naturally say things like:

```text
“Open left gate.”
“Boost after this curve.”
“Shield now.”
“Which route?”
“Clear that drone.”
“I need grip.”
```

If the pilot can play almost the same without the crew, the implementation is wrong.

If a supporter spends more than 30 seconds with nothing useful to do, the implementation is wrong.

If a supporter action does not create visible feedback on the immersive output, the implementation is wrong.

---

## 4. Crew roles that must be implemented

The full 6-person setup is:

```text
1 Pilot        wheel + pedals
2 Navigator    phone
3 Engineer     phone
4 Defender     phone
5 Hacker       phone
6 Gunner       phone
```

### Navigator

Purpose:

```text
route choice, pace notes, hazard callouts, shortcut reveal
```

Required actions:

```text
nav_select_route(route_id)
nav_mark_hazard(hazard_id)
nav_call_pace_note(note_id)
nav_reveal_shortcut(shortcut_id)
nav_request_role(role_id, reason)
```

Required visible effects:

```text
route arrows
brief ghost line
hazard ping
shortcut highlight
```

### Engineer

Purpose:

```text
boost, heat, grip, power balancing
```

Required actions:

```text
engineer_charge_boost(amount)
engineer_release_boost(mode)
engineer_dump_heat()
engineer_stabilize_grip(duration)
engineer_rebalance_power(target)
```

Required visible effects:

```text
boost trail
heat warning
grip stabilization pulse
engine overload feedback
```

### Defender

Purpose:

```text
shield, protection, hit mitigation, status cleanse
```

Required actions:

```text
defender_raise_shield(direction)
defender_cleanse_status(status_id)
defender_absorb_hit(threat_id)
defender_repair_armor_small()
defender_emergency_bubble()
```

Required visible effects:

```text
shield arc
projectile/hazard deflection
impact flash
armor pulse
```

### Hacker

Purpose:

```text
gates, barriers, ramps, shortcuts, traps
```

Required actions:

```text
hacker_open_gate(gate_id)
hacker_close_hazard_barrier(barrier_id)
hacker_rotate_ramp(ramp_id, angle_state)
hacker_arm_shortcut(shortcut_id)
hacker_disable_trap(trap_id)
```

Required visible effects:

```text
gate opens
barrier powers down
ramp rotates
shortcut lights up
```

### Gunner / Utility

Purpose:

```text
clear obstacles, target drones/rivals, EMP zones, decoys
```

Required actions:

```text
gunner_lock_target(target_id)
gunner_fire_utility(target_id)
gunner_emp_zone(zone_id)
gunner_drop_decoy()
gunner_tag_rival(rival_id)
```

Required visible effects:

```text
lock-on reticle
utility projectile/EMP
obstacle destroyed
rival slowed/tagged
```

---

## 5. Crew Event Director

Implement a `CrewEventDirector` or equivalent module.

It must create time/track-based support windows. Each support window follows:

```text
telegraph -> assignment -> player communication -> action -> world effect -> driver payoff -> score/feedback
```

Required event types:

```text
route_choice
boost_chain
shield_timing
gate_hack
clear_target
team_sync
```

A 7-minute standard session must include at least:

```text
8 route/navigation prompts
6 boost/engineering prompts
5 defender prompts
5 hacker prompts
5 gunner prompts
2 multi-role combo events
1 final sync event
```

No player should be inactive for more than 20–30 seconds.

---

## 6. Arcade session envelope

Do not build long open-ended races. Build sellable arcade sessions.

Default presets:

```json
{
  "quick_play": {"gameplay_seconds": 300, "slot_seconds": 480},
  "standard_arcade": {"gameplay_seconds": 420, "slot_seconds": 720},
  "premium_ride": {"gameplay_seconds": 660, "slot_seconds": 900}
}
```

Initial target:

```text
standard_arcade = 7 minutes gameplay, 10–12 minutes total slot
```

Session states:

```text
attract
loading
ready_for_players
briefing
countdown
running
handoff
last_minute
final_sprint
finished_gold
finished_silver
finished_bronze
finished_timeout
finished_operator_abort
results
resetting
error_recoverable
error_fatal
```

Operator commands:

```text
start_session
pause_session
resume_session
extend_session(seconds)
force_handoff(player_id)
skip_handoff
force_final_sprint
finish_now
abort_session
reset_session
recenter_camera
set_projection_profile
set_output_active
```

---

## 7. Pilot rotation and wheel handoff

At checkpoint/segment end:

```text
1. kart enters safe handoff zone
2. autopilot stabilizes for 5–8 seconds
3. cupola shows next pilot
4. next pilot phone vibrates/alerts
5. players physically switch wheel position
6. 3–2–1 countdown
7. control returns to wheel
```

Implement policies:

```text
checkpoint_or_timeout      default
checkpoint_only
fixed_time_segments
operator_manual_handoff
single_pilot_demo
```

Never trigger handoff during:

```text
hard turns
jumps
heavy collision sequences
narrow hazard sections
```

---

## 8. Architecture targets

Conceptual modules. Use real repo naming after inspection.

```text
src/immersive/
  ImmersiveRelayManager
  EnvironmentProfile
  ImmersiveCameraRig
  ProjectionAdapter
  OutputAdapter
  ImmersiveRenderGraph
  ImmersiveTelemetry

src/immersive/projections/
  FisheyeDomemasterProjection
  EquirectangularProjection
  CylindricalProjection
  PerspectiveStripProjection
  WallAtlasProjection

src/immersive/outputs/
  SyphonOutputAdapter_macOS
  SpoutOutputAdapter_Windows
  WindowOutputAdapter
  NullOutputAdapter
  NDIOutputAdapter_optional

src/modes/immersive_relay/
  RelayRaceMode
  RelaySessionController
  RelayPilotRotation
  CrewRoleManager
  CrewEventDirector
  CrewActionHandler
  RelayScoring
  RelayAutopilotHandoff
  RelayTrackScript

src/remote/
  LocalControlServer
  RelayCommandRouter
  RelayStatePublisher
```

Do not create a giant generic framework before a playable MVP. Implement a concrete `Dome Relay Crew Mode` first, but with interfaces that allow future projections/outputs.

---

## 9. Local API contract

STK must expose a minimal local API controlled by the Swift app.

Minimum endpoints:

```http
POST /immersive/v1/session/start
POST /immersive/v1/session/stop
POST /immersive/v1/session/pause
POST /immersive/v1/session/resume
POST /immersive/v1/session/reset
POST /immersive/v1/crew/action
POST /immersive/v1/operator/handoff
GET  /immersive/v1/state
GET  /immersive/v1/health
WS   /immersive/v1/events
```

Security:

```text
bind localhost by default
session token required
rate limit actions
reject stale state versions
idempotency keys for actions
never expose broad filesystem/network operations
```

Crew action JSON:

```json
{
  "session_id": "tholus-001",
  "action_id": "uuid",
  "state_version_seen": 1024,
  "player_id": "p4",
  "role": "hacker",
  "type": "hacker_open_gate",
  "payload": {"gate_id": "gate_blue_02"}
}
```

---

## 10. Rendering and output

MVP output:

```text
fisheye/domemaster 2048x2048 via Syphon on macOS
```

Rendering path:

```text
scene -> cubemap/multi-view -> fisheye projection shader -> output texture -> Syphon
```

Future outputs:

```text
equirectangular 2048x1024 or 4096x2048
panoramic wall strip
cylindrical strip
CAVE/wall atlas
Windows Spout
NDI optional
```

Camera must support:

```text
yaw
pitch
roll
FOV
look-ahead
height offset
horizon stabilization
roll clamp
comfort presets
```

Do not hardcode dome orientation. Use JSON profiles.

---

## 11. macOS Syphon requirement

Use Syphon as the first same-machine video transport on macOS.

Requirements:

```text
configurable sender name, default STK_ImmersiveRelay
health metric: output active/inactive
fps/frame counter
window/null fallback
no CPU readback if avoidable
```

Do not implement NDI first unless explicitly requested. Syphon is the target for same-Mac capture by the Swift app.

---

## 12. Windows portability

Keep platform-specific output isolated:

```text
macOS: SyphonOutputAdapter_macOS
Windows: SpoutOutputAdapter_Windows
Network: NDIOutputAdapter_optional
```

All gameplay, crew, camera, projection math and session logic must compile without Syphon/Spout dependencies.

---

## 13. Feature flags

Use actual build system conventions after inspection. Conceptually support:

```text
ENABLE_IMMERSIVE_RELAY
ENABLE_SYphon_OUTPUT
ENABLE_SPOUT_OUTPUT
ENABLE_RELAY_LOCAL_API
ENABLE_RELAY_TEST_TOOLS
```

With immersive disabled, vanilla STK must behave normally.

---

## 14. Testing requirements

At minimum:

```text
vanilla build smoke test
relay session start/reset test
fake local API client
fake phone clients 1–5
each crew action accepted/rejected tests
pilot handoff tests
projection output smoke test
Syphon receiver test
performance 5-minute soak on Mac mini M1
operator abort/reset test
phone reconnect simulation
```

Gameplay acceptance:

```text
each supporter performs >=5 meaningful actions in 7 minutes
pilot asks/uses crew help repeatedly
no role inactive >30 seconds
each action has visible immersive feedback
session always reaches results/reset
new players understand basics in <60 seconds
```

---

## 15. Implementation phases

### Phase 0 — Repo audit

Deliver:

```text
docs/immersive/repo_audit.md
working vanilla build
identified camera/render/input/checkpoint hooks
risk list
```

### Phase 1 — Relay session without immersive rendering

Deliver:

```text
session controller
pilot rotation
handoff autopilot stub
local API start/state/reset
```

### Phase 2 — Crew actions in flat/debug mode

Deliver:

```text
5 roles
CrewActionHandler
real effects on kart/pista/AI/score
fake phone/client tooling
```

### Phase 3 — Crew Event Director

Deliver:

```text
track event script
7-minute standard session
route/boost/shield/gate/target/sync events
score/combo
```

### Phase 4 — Immersive fisheye + Syphon

Deliver:

```text
ImmersiveCameraRig
cubemap/multi-view render
FisheyeDomemasterProjection
SyphonOutputAdapter_macOS
orientation config
```

### Phase 5 — Venue-ready MVP

Deliver:

```text
operator state machine
reset rapid
M1 performance pass
playtests 2/4/6
known issues
```

### Phase 6 — Future profiles

Deliver later:

```text
equirectangular
cylindrical/panoramic
Windows/Spout
CAVE/wall atlas
```

---

## 16. Code quality rules

- Prefer small, reviewable patches.
- Do not rewrite renderer/input/race manager wholesale.
- Add adapters/interfaces rather than platform conditionals everywhere.
- Keep JSON schemas versioned.
- Log action rejects with reason.
- All new public config fields must have defaults.
- All new modules need comments explaining integration points.
- Avoid global mutable state unless matching existing STK architecture.
- Do not block render thread on HTTP/WebSocket.
- Do not trust phone/client input.
- Do not make session reset depend on manual UI interactions.

---

## 17. Licensing rules

SuperTuxKart code is GPL v3.0 or later. STK uses Git for source code and SVN for assets/data. Respect upstream license and asset documentation.

Do not add:

```text
unclear assets
trademarked logos
commercial assets without license file
closed-source dependencies incompatible with GPL/distribution plan
```

Maintain:

```text
LICENSE/NOTICE updates
third-party dependency list
source availability plan if distributing builds
credits for modified STK
```

Ask for legal review before commercial deployment/distribution.

---

## 18. Reporting format for the coding agent

After each substantial change, report:

```text
Summary
Files changed
Build/test commands run
Results
Known risks
Next recommended step
```

Do not claim success unless you ran the relevant build/test or clearly say it is unverified.

---

## 19. Definition of Done for MVP

```text
[ ] macOS build works
[ ] vanilla STK still works
[ ] local API can start/reset relay session
[ ] wheel/pedals drive active pilot
[ ] pilot switch at checkpoint works with autopilot handoff
[ ] 5 crew roles implemented
[ ] crew actions modify real gameplay state
[ ] 7-minute standard arcade session playable
[ ] no role idle >30s in test session
[ ] fisheye/domemaster output is generated from 3D scene
[ ] Syphon sender visible to receiver/Swift app
[ ] camera yaw/pitch/roll configurable
[ ] reset works without manual STK menu
[ ] telemetry logs session metrics
[ ] 2/4/6 player playtests documented
[ ] build/update/licensing docs updated
```
