# Immersive Relay Adaptive Difficulty

Immersive Relay now has a deterministic `RelayDifficultyDirector` under:

```text
src/modes/immersive_relay/
```

The director is runtime-gated by the relay session config. Vanilla builds do
not include relay sources, and relay sessions keep adaptive difficulty disabled
unless Swift or a local test explicitly enables it.

## Session Config

`POST /immersive/v1/session/start` accepts either the nested form:

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

or the flat compatibility fields:

```json
{
  "adaptive_difficulty_enabled": true,
  "adaptive_difficulty_profile": "arcade_balanced",
  "adaptive_difficulty_min_intensity": 0,
  "adaptive_difficulty_max_intensity": 100
}
```

`profile` is currently telemetry/config only. The implemented tuning is the
initial `arcade_balanced` profile.

## Runtime Signals

The director samples every 250 ms or more and smooths over race time instead of
reacting to single frames. Current signals:

- human race position;
- estimated time gap from leader, or negative gap when the human leads;
- lap and lap progress;
- on-road/off-road state;
- rescue animation starts;
- handoff activity and completed handoff count;
- final lap/final stretch.

States exposed as `adaptive_pressure`:

```text
neutral
recovering
struggling
dominating
final_lap_lock
handoff_grace
```

## Effects

Effects are deliberately small and physical:

- human assist can add tiny nitro reserves, brief max-speed recovery, and short
  post-rescue/off-road invulnerability when the team is clearly behind;
- AI recovery pacing uses small speed caps/direct speed trims when the team is
  struggling;
- domination pressure marks AI as boosted so existing AI item/nitro skill paths
  become cleaner, then restores original boost flags when pressure ends.

The director does not teleport karts, guarantee wins, patch global physics, or
change vanilla behavior when disabled.

## API State

`GET /immersive/v1/state` includes flat Swift-friendly fields:

```text
adaptive_difficulty_enabled
adaptive_difficulty_profile
adaptive_difficulty_intensity
adaptive_pressure
human_gap_ms
human_position
assist_level
ai_pace_level
leaderboard_bucket
```

and a nested `adaptive_difficulty` object with config, lock flags and the same
runtime telemetry.

Leaderboard bucket policy:

```text
adaptive enabled  -> arcade_adaptive
adaptive disabled -> arcade_fixed
profile solo_no_assist with adaptive disabled -> solo_no_assist
```

## Tuning Risks

- Gap is estimated from track distance and current max speed, not authored
  per-track baselines yet.
- AI slowdown is intentionally subtle because obvious rubber-banding damages
  trust.
- Crew/event intensity is exposed through telemetry but not yet used to select
  alternate encounter scripts.
- Playtests should tune thresholds per venue preset before this is treated as
  final production balancing.
