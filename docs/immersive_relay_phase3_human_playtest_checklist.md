# Immersive Relay Phase 3 Human Playtest Checklist

This checklist is for human Phase 3/3.5 playtests in 16:9 debug mode. It is not
a Phase 4 fulldome, fisheye, equirectangular, Syphon, Spout, VR, PWA, or real
Swift companion app checklist.

## Setup

- Build: relay build with `ENABLE_IMMERSIVE_RELAY=ON` and local API enabled.
- Track: `snowtuxpeak`.
- Session: lap-based, `lap_based_handoff`.
- Participants: 2, 3, 4, 5, or 6.
- Driver: 1 active driver at a time.
- Crew: 1 to 5 supporters, using fake phones or a UI mock fed from
  `phone_states`.
- Lap counts:
  - 2 or 3 participants: 6 laps.
  - 4 participants: 8 laps.
  - 5 participants: 10 laps.
  - 6 participants: 12 laps.
- Rotation: driver changes every lap; every participant must drive at least
  twice.

## Player Count Load Targets

| player_count | crew layout per lap | max actions visible per phone | target average actions per lap | overload risk |
| ---: | --- | ---: | ---: | --- |
| 2 | 1 driver, 1 Mission Control crew with combined roles | 2 | 4-8 | Medium if UI shows all role history |
| 3 | 1 driver, 2 combined crew phones | 2-3 | 4-8 | Medium |
| 4 | 1 driver, 3 crew phones, light role combining | 3 | 5-8 | Low |
| 5 | 1 driver, 4 crew phones, one combined/rotating role | 2-3 | 6-8 | Low |
| 6 | 1 driver, 5 role-focused crew phones | 2 | 7-9 | Low |

Hard rule: no phone should show five complete roles or a wall of ten buttons.
If the real UI needs to show unavailable actions, show them disabled with a
short reason and keep the primary available actions visually dominant.

## Observe

Use this during live runs:

- Does the active driver talk to the crew without prompting?
- Does the crew look at the track/debug HUD, or only at the phone?
- Does any player remain inactive for more than one full lap?
- Are late, wrong, or cooldown errors understandable?
- Is the lap handoff visible and socially clear?
- Are combined roles too heavy for 2 or 3 players?
- Does the final sprint feel different from normal laps?
- Does the team understand why they scored well or poorly?
- Does score/combo feedback make sense?
- Are preview, active, success, fail, boost, shield, gate, target, and route
  effects visible in 16:9 debug state/HUD?

## Manual Metrics

Record after each run:

- participants and total laps;
- final result and final score;
- laps completed;
- pilot turns by player;
- max inactivity time per role;
- number of spontaneous voice callouts;
- moments of confusion;
- memorable moments;
- actions players did not understand;
- encounters that felt too easy;
- encounters that felt too hard;
- phones with too many simultaneous actions;
- any crash, stuck session, or incoherent state.

## Role Audit Prompts

| role | unique information | unique power | timing pressure | visible payoff |
| --- | --- | --- | --- | --- |
| Navigator | route/callout text before the driver reaches a split | marks route or pace note | choose while the line is still useful | HUD route marker, handoff line, final apex |
| Engineer | boost, heat, grip, charge state | primes boost or stabilizes grip | boost/grip window can be missed | boost ready, heat/grip text, combo payoff |
| Defender | hazard timing and shield direction | temporary shield/armor | shield must overlap danger window | shield active/missed state |
| Hacker | gate/shortcut target | opens gate or arms shortcut | gate must be opened before arrival | gate/shortcut track effect |
| Gunner | target priority | locks/clears target | target window can expire | target clear/active state |

Every role should produce at least one moment per lap where the player can say:
"that changed the run because of me."

## Encounter Fail-Soft Checklist

| encounter_id | success | late | wrong | missing_role | visible feedback |
| --- | --- | --- | --- | --- | --- |
| route_choice_ridge | left route marked, score awarded | route unclear, small score | safe/decoy choice avoids combo-level payoff | combined crew may choose one route only | `route:ridge_left`, `hud:route`, phone message |
| blue_gate_hack | blue gate open | gate closed, longer route | red decoy does not satisfy required action | Mission Control can still open one gate | `gate_blue_02`, `hud:gate` |
| engineer_boost_window | boost primed | boost missed | grip choice helps but does not satisfy boost | combined crew may prioritize route/gate first | `boost_window`, `hud:boost`, boost charge |
| combo_shortcut | shortcut combo and score bonus | shortcut missed | partial actions give lower payoff | simplified by available combined-role actions | `shortcut:ice_cut`, `combo:shortcut` |
| danger_tunnel_defense | shield plus grip combo | wobble soft penalty | missing one side loses combo | one combined crew can still save one part | `shield`, `combo:defense`, armor/grip text |
| drone_target_clear | target locked/cleared | target stays active | wrong/late shot misses payoff | lower player counts can skip without blocking | `target:drone_ridge`, `hud:target` |
| lap_end_handoff_call | safe handoff line marked | handoff unclear | wrong timing gives low score | Navigator fallback can still mark line | `handoff_line` |
| final_sprint_sync | all-role final combo | partial final sprint | missing final action lowers rank/score | available roles contribute partial finish | `final_sprint`, `combo:final_sprint` |

Failure must never block race progress. It should produce lower score, missed
combo, missed debug/track effect, and clear phone/HUD feedback.

## Timeline Review

Check that the lap script does not front-load all activity:

- 6 laps: intro, normal rhythm, combo, defense/target variation, final sprint.
- 8 laps: at least one strong encounter per lap and at least two combo moments.
- 10 laps: repeated archetypes must vary timing, role, target, or payoff.
- 12 laps: no role should feel irrelevant for more than one full lap in a
  normal 6-player run.

For each run, write a short row per lap:

```text
lap | driver | encounter ids | primary roles | expected crew load | notes
```

## Repeatable Phase 3.5 Audit

Automated audit:

```sh
tools/immersive/RunRelayPhase35Audit.sh
```

Faster rerun after builds are already current:

```sh
SKIP_BUILDS=1 tools/immersive/RunRelayPhase35Audit.sh
```

Reports are written to:

```text
/tmp/relay_phase35/*.json
```

Minimum human-playability pass criteria:

- perfect mode completes;
- mistakes, random, or silent-role modes produce lower score or soft fails;
- every player drives at least two laps;
- no phone exceeds the action load target;
- no role depends on hidden debug fields;
- all encounters have fail-soft outcomes;
- final sprint is visible in 16:9 debug state.

## Phase 5 Venue Playtest Checklist

This section is for arcade/venue MVP tests after Phase 5. Use the local
operator API or `tools/immersive/FakeArcadeOperator.py`; the Swift app is still
future work.

### Required Groups

Run at least:

- 2 participants, 6 laps.
- 4 participants, 8 laps.
- 6 participants, 12 laps.

### Required Hardware/Output

Check:

- one wheel plus pedals;
- one active pilot at a time;
- up to five fake phones or phone UI mocks;
- Syphon output if available;
- dome receiver or 16:9/debug receiver;
- operator reset without app restart.

### Handoff Observation

At every lap boundary, record:

- did participants understand who drives next?
- was "Cambio pilota" visible/audible enough?
- did `/state.arcade_session.handoff_active` become true during the swap?
- did `/state.arcade_session.input_locked` become true while the wheel was
  being handed over?
- did `/state.arcade_session.next_driver` match the person taking the wheel?
- did the kart stay stable and avoid crashing?
- did the camera avoid violent jumps?
- was the physical swap possible without rushing dangerously?
- was the handoff pause too long, too short, or about right?
- did the operator need to confirm, and was that clear?

### Crew Observation

For each group size, record:

- does the crew look at the dome/receiver, not only the phone?
- does the crew understand what to do?
- does anyone stay inactive for more than one lap?
- does the pilot ask the crew for help?
- do crew actions produce visible dome/debug feedback?
- do combined roles overload 2p or 3p groups?
- does final sprint read as different from normal laps?

### Operator Observation

Record:

- can the operator configure player count, track, kart, and preset?
- can the operator start briefing/countdown without STK menus?
- can the operator pause and resume confidently?
- can the operator abort a bad run?
- can the operator force-finish when needed?
- can the operator reset during running and during handoff?
- is reset fast enough for back-to-back guests?
- does `/state.arcade_session.last_error` explain invalid commands?
- does the operator know what to do if Syphon/output fails?

### Results Observation

At results:

- is the final rank understandable?
- are driver turns understandable?
- are role contributions understandable?
- do players understand why score was high or low?
- is "ready for reset" clear to the operator?

### Repeatability

Run:

```text
start -> run partial -> reset -> start again -> finish -> reset -> start again
```

Minimum pass:

- three consecutive sessions in the same process;
- different session id per configured run;
- no score leak;
- no old active driver leak;
- no active encounter leak;
- no API hang;
- Syphon/window output remains alive or recovers cleanly.

## Phase 5.1 Venue Acceptance Checklist

Before declaring the arcade MVP ready for a public venue playtest, run:

```sh
tools/immersive/RunRelayPhase51VenueAcceptance.sh
```

Review:

- hardware inventory says whether the machine is the Mac mini M1 base target;
- 2p, 4p, and 6p sessions reach `results` with FakeCrewPhones active;
- 6p mistakes and 6p silent-role hacker sessions still finish coherently;
- report score and `crew_actions_accepted` are positive when fake phones act;
- operator-only/disconnected-phone report explicitly shows no crew actions;
- `arcade_session.report` is null after reset while `last_report` remains a
  previous-session reference;
- reset loop keeps API alive and score/cooldown/encounter state clean;
- Syphon receiver probe passes when Syphon is available;
- handoff confirm test shows `handoff_active`, `input_locked`, and correct
  `next_driver`;
- invalid token, wrong state, invalid preset/projection, abort, force-finish,
  and reset during handoff return coherent JSON and do not crash.

If the test machine is not a Mac mini M1 base, do not close Phase 5 from this
checklist alone. Treat it as hardware-acceptance prep and run the 60 second M1
benchmark on the target machine before the venue verdict.
