# Immersive Relay Phase 4.6 Projection Generalization

Phase 4.6 is a controlled architecture pass. It does not add a production
equirectangular, cylindrical, CAVE, LED wall, VR, or stereo feature. Its purpose
is to stop the working fisheye/Syphon path from becoming hardcoded everywhere.

## 1. Why This Phase Exists

Phase 4.5 proved the current render path:

```text
ImmersiveCameraRig
-> six cubemap-equivalent render targets
-> forward-facing fisheye/domemaster projection
-> window or Syphon output
```

The future venue stack needs more projection profiles, but fisheye/domemaster is
still the production target. Phase 4.6 therefore formalizes:

```text
FullSphereCapture -> ProjectionAdapter -> OutputAdapter
```

## 2. What It Implements

- `FullSphereCapture` wraps the existing six-face cubemap-equivalent capture.
- `ProjectionAdapter` defines the projection contract.
- `FisheyeDomemasterProjection` is the only production projection adapter.
- `DebugWindowOutputAdapter` previews a generic projected frame and preserves
  aspect ratio.
- `SyphonOutputAdapterMac` publishes a generic projected frame and no longer
  assumes every projection is square.
- `/immersive/v1/state` exposes structured `capture`, `projection`, `output`,
  `camera`, `stats`, and `supported_projections`.
- `/immersive/v1/render/config` accepts legacy flat fields and the new nested
  `capture` / `projection` / `output` / `camera` shape.
- Unsupported projection kinds are recognized as stubs and fail clearly.

## 3. What It Does Not Implement

This phase does not implement Phase 5 arcade/operator state machine,
equirectangular production output, cylindrical production output, CAVE/cube
production output, LED wall production output, Windows/Spout, VR/OpenXR, stereo
rendering, a Swift companion app, PWA, new crew mechanics, gameplay modes, or
lap/handoff changes.

## 4. Architecture

```text
ImmersiveCameraRig
  -> FullSphereCapture
      -> CubemapRenderTarget (+X, -X, +Y, -Y, +Z, -Z)
  -> ProjectionAdapter
      -> FisheyeDomemasterProjection [production]
      -> equirectangular/cylindrical/cube_faces/wall_atlas [stub]
  -> OutputAdapter
      -> DebugWindowOutputAdapter
      -> SyphonOutputAdapterMac
```

`FullSphereCapture` does not know the target projection. It produces a reusable
monoscopic full-sphere/cubemap-equivalent representation. Projection adapters
own projection-specific shader logic and output descriptions. Output adapters
receive only the final projected texture metadata: texture, texture target,
width, height, projection id, and server name.

## 5. Projection Status

| projection | status | output size | notes |
| --- | --- | --- | --- |
| `fisheye_domemaster` | production | 1024x1024, 1536x1536, 2048x2048 | front-facing 180 degree circular fisheye |
| `equirectangular` | stub | future 2:1 | capture is ready; shader/config are future Phase 6 |
| `cylindrical` | stub | future variable | for panoramic/cylinder profiles |
| `cube_faces` | stub | future atlas/faces | for CAVE/cube-style layouts |
| `wall_atlas` | stub | future layout-defined | for LED/wall environments |

Stub projections are not selectable as working output. API requests return
`immersive_projection_unavailable:<projection>`.

## 6. Runtime Config

Legacy CLI flags remain valid. The local API now also accepts:

```json
{
  "capture": {
    "type": "full_sphere_cubemap",
    "cubemap_size": 1024
  },
  "projection": {
    "type": "fisheye_domemaster",
    "width": 2048,
    "height": 2048,
    "debug_grid": true
  },
  "output": {
    "type": "syphon",
    "server_name": "SuperTuxKart Immersive Relay Domemaster"
  },
  "camera": {
    "mode": "stabilized_chase",
    "yaw_deg": 0,
    "pitch_deg": 35,
    "roll_deg": 0,
    "dome_tilt_deg": 0,
    "forward_offset": 0.25,
    "vertical_offset": 0.40,
    "smoothing_factor": 0.16,
    "horizon_stabilization": true,
    "roll_limit_deg": 12
  }
}
```

If legacy flat fields and nested fields are both present, the nested fields win
inside one API request. CLI sets startup defaults; runtime API changes override
the running render config until STK exits or another API request changes it.

## 7. State Contract

`/immersive/v1/state` includes:

```json
{
  "immersive_render": {
    "capture": {
      "type": "full_sphere_cubemap",
      "cubemap_size": 512,
      "faces": 6
    },
    "projection": {
      "type": "fisheye_domemaster",
      "status": "production",
      "width": 1024,
      "height": 1024,
      "aspect": "1:1",
      "coverage": "front-facing 180"
    },
    "output": {
      "type": "window",
      "available": true,
      "active": true
    },
    "stats": {
      "capture_ms": 7.5,
      "projection_ms": 0.2,
      "output_ms": 0.1
    }
  }
}
```

The actual JSON also keeps older top-level fields for existing tools.

## 8. TholusOS Contract

TholusOS/Swift should not guess the incoming video shape. It should first read
`/capabilities`, then read `/state.immersive_render` and use:

- `projection.type`;
- `projection.width` and `projection.height`;
- `projection.aspect`;
- `projection.coverage`;
- `projection.orientation`;
- `output.type`;
- `output.server_name`;
- `output.active`;
- `fallback_reason`.

Phase 5.3 freezes the default base URL as
`http://127.0.0.1:8890/immersive/v1`. The only production projection remains
`fisheye_domemaster`; equirectangular, cylindrical, cube faces, and wall atlas
are discoverable as stubs only.

STK outputs the final projected frame. TholusOS receives it via Syphon, routes
it to the venue pipeline, applies projector warp/blend/layout, and shows
receiver diagnostics. Phones remain separate and talk to Swift/TholusOS, not to
Syphon.

Future environment profiles:

```json
{"profile_id":"fulldome_180","projection":"fisheye_domemaster","preferred_resolution":"2048x2048","output":"syphon","status":"production"}
{"profile_id":"equirect_360","projection":"equirectangular","preferred_resolution":"2048x1024","output":"syphon","status":"future"}
{"profile_id":"cylindrical_270","projection":"cylindrical","status":"future"}
{"profile_id":"cave_cube","projection":"wall_atlas_or_cube_faces","status":"future"}
```

## 9. Debug Grid

Fisheye remains the only projection with a working grid in this phase:
concentric rings, radials, dome edge, forward marker, horizon/tilt markers, and
side/up markers. Stub projections do not draw fake grids.

## 10. Benchmark Smoke

Short local smoke on MacBookPro18,3 / Apple M1 Pro / 16 GB / macOS 26.4.1.
These are not final Mac mini M1 venue numbers, but they verify that timing is
split into capture, projection, and output.

| mode | projection | size | output | duration | avg fps | p95 | p99 | capture ms | projection ms | output ms | dropped | notes |
| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| relay_16x9_no_immersive | none | 16:9 | main | 20s | not measured | not measured | not measured | not measured | not measured | not measured | not measured | regression only; no immersive timing |
| fisheye_1024_window | fisheye_domemaster | 1024x1024 | window | 20s | 175.34 | 7.00 | 14.00 | 5.56 | 0.08 | 0.06 | 0 | debug preset |
| fisheye_1536_window | fisheye_domemaster | 1536x1536 | window | 20s | 153.87 | 9.00 | 19.00 | 6.35 | 0.09 | 0.06 | 0 | safe/test preset |
| fisheye_2048_window | fisheye_domemaster | 2048x2048 | window | 20s | 144.96 | 11.00 | 21.00 | 6.75 | 0.09 | 0.06 | 0 | target preset |
| fisheye_1024_syphon | fisheye_domemaster | 1024x1024 | syphon | 20s | 155.87 | 8.00 | 13.00 | 6.28 | 0.07 | 0.06 | 0 | receiver PASS, 1024x1024 |
| fisheye_1536_syphon | fisheye_domemaster | 1536x1536 | syphon | 20s | 139.83 | 9.00 | 16.00 | 7.03 | 0.08 | 0.04 | 0 | receiver PASS, 1536x1536 |
| fisheye_2048_syphon | fisheye_domemaster | 2048x2048 | syphon | 20s | 113.96 | 12.00 | 19.00 | 8.62 | 0.08 | 0.07 | 0 | receiver PASS, 2048x2048 |

Run controlled 60 second tests on the Mac mini M1 before venue acceptance.

## 11. Known Limits

- Equirectangular is a stub, not a working output.
- Cylindrical, cube faces, and wall atlas are stubs.
- The current capture is monoscopic. Stereo/VR needs per-eye rendering later.
- Projection timing often rounds to 0.0ms with millisecond timers.
- The full-sphere capture still renders six separate faces.

## 12. Handoff

Phase 5 can focus on arcade/operator session control, reset flow, playtest, and
Swift/TholusOS integration. Phase 6 can add equirectangular production output.
Phase 7+ can add cylindrical, wall atlas/CAVE, Windows/Spout, or VR-specific
paths.
