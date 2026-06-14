# Immersive Relay Phase 4 Fulldome/Syphon

Phase 4 adds a real square fisheye/domemaster render path for Dome Relay Crew
Mode. It keeps the normal 16:9/debug path available and keeps the relay gameplay
logic from Phase 3.5 unchanged.

## 1. What Phase 4 Implements

- Relay sessions can still run in the existing 16:9/debug mode.
- A new immersive render path can publish a 1:1 fisheye/domemaster texture.
- Target domemaster sizes are 1024, 1536, and 2048.
- The render path is built as camera rig -> cubemap-equivalent faces ->
  fisheye projection -> output adapter.
- macOS Syphon output is isolated behind `ENABLE_SYPHON_OUTPUT`.
- A window preview output is available for debug without Syphon.
- The local `/immersive/v1/state` payload now includes `immersive_render`
  stats when a relay build is used.
- Debug grid and render stats can be enabled from the command line.

## 2. What Phase 4 Does Not Implement

This phase does not implement equirectangular output, panoramic/cylindrical
output, CAVE/cube output, Windows, Spout, VR/OpenXR, a real Swift companion app,
payment/session arcade state machines, new crew mechanics, or changes to the
lap-based handoff policy.

Equirectangular, Windows, and Spout remain future placeholders only.

## 3. Render Architecture

New code is kept under:

```text
src/modes/immersive_relay/render/
src/modes/immersive_relay/render/camera/
src/modes/immersive_relay/render/projection/
src/modes/immersive_relay/output/
src/modes/immersive_relay/output/debug_window/
src/modes/immersive_relay/output/syphon_mac/
```

Main modules:

- `ImmersiveRenderConfig`: command-line and runtime render configuration.
- `ImmersiveCameraRig`: comfort-aware camera attached to the active relay driver.
- `CubemapRenderTarget`: six square render targets around the immersive camera.
- `FisheyeDomemasterProjection`: GLSL projection pass from six faces to square
  fisheye/domemaster.
- `ProjectionDebugGrid`: calibration grid metadata.
- `ImmersiveRenderStats`: rolling FPS/frame/projection/output stats.
- `DebugWindowOutputAdapter`: blits the domemaster to the main window.
- `SyphonOutputAdapterMac`: publishes the domemaster texture through Syphon on
  macOS when the Syphon framework is present.

Core STK edits are limited to CMake, CLI help/parsing, tiny render hooks, and an
RTT color-texture accessor used by the immersive projection path.

## 4. Cubemap-Equivalent To Fisheye

The current implementation renders six square views around the immersive
camera:

```text
+X, -X, +Y, -Y, +Z, -Z
```

The camera faces follow the OpenGL cube-map face convention, with Irrlicht's
left-handed look-at handled in `ImmersiveCameraRig`. Each face currently uses a
small overscan FOV so the projection shader can blend across face edges. The
shader maps each pixel of the output disc to a forward-facing 180 degree fisheye
lens direction: the center of the disc is the active driver's forward view, the
upper half is camera-up/sky, and the lower half is camera-down/track/kart. It
then samples the six rendered faces and writes a square fisheye texture. Pixels
outside the dome disc are black.

This is a cubemap-equivalent multi-render path. It is not sampling or warping
the normal 16:9 framebuffer.

## 5. Why It Is Not A 16:9 Warp

The main window render is not the source for the fisheye image. Phase 4 creates
separate square render targets from the immersive camera, renders six directions
from the world, and only then projects those rendered views into fisheye.

The debug window adapter shows the resulting domemaster texture. If output is
`window`, the preview replaces the window contents for calibration; if output is
`none`, the existing 16:9/debug path remains unchanged.

## 6. CLI Flags

Relay builds expose:

```text
--immersive-output=none|window|syphon
--immersive-projection=none|fisheye
--immersive-domemaster-size=1024|1536|2048
--immersive-cubemap-size=512|768|1024|1536
--immersive-camera=pov|chase|stabilized_chase|cockpit
--immersive-yaw-deg=<float>
--immersive-pitch-deg=<float>
--immersive-roll-deg=<float>
--immersive-dome-tilt-deg=<float>
--immersive-horizon-stabilization=on|off
--immersive-roll-limit-deg=<float>
--immersive-forward-offset=<float>
--immersive-vertical-offset=<float>
--immersive-smoothing-factor=<float>
--immersive-debug-grid=on|off
--immersive-stats=on|off
--syphon-server-name=<string>
```

If `--immersive-output` is `window` or `syphon` and no projection is provided,
the projection defaults to `fisheye`.

Vanilla builds do not show relay or immersive flags in `--help`.

## 7. Mac Mini M1 Configuration

Recommended first-pass presets:

- Debug: `--immersive-domemaster-size=1024 --immersive-cubemap-size=512`
- Test: `--immersive-domemaster-size=1536 --immersive-cubemap-size=768`
- Target: `--immersive-domemaster-size=2048 --immersive-cubemap-size=1024`

Keep default camera mode `stabilized_chase` for venue testing. The current
default camera preset is a semi-cockpit lens inside the kart volume, close to a
first-person view but still horizon-stabilized:

```text
--immersive-pitch-deg=35
--immersive-forward-offset=0.25
--immersive-vertical-offset=0.40
--immersive-smoothing-factor=0.16
```

Positive pitch looks up. Use POV/cockpit only for supervised experiments because
they are more intense.

## 8. Camera Modes

Default:

```text
--immersive-camera=stabilized_chase
```

Supported modes:

- `stabilized_chase`: semi-cockpit lens inside/just behind the kart, smoothed,
  horizon-stabilized, default comfort mode.
- `chase`: similar offset but less opinionated.
- `pov`: near-driver experimental camera.
- `cockpit`: closer kart-mounted experimental camera.

The camera target is the relay `active_driver`. On lap handoff, the rig logs the
new active driver and updates the target.

## 9. Comfort Settings

Defaults avoid full kart roll:

```text
--immersive-horizon-stabilization=on
--immersive-roll-limit-deg=12
```

Orientation calibration:

```text
--immersive-yaw-deg=<float>
--immersive-pitch-deg=<float>
--immersive-roll-deg=<float>
--immersive-dome-tilt-deg=<float>
--immersive-forward-offset=<float>
--immersive-vertical-offset=<float>
--immersive-smoothing-factor=<float>
```

Positive pitch looks up. Lower `--immersive-vertical-offset` moves the lens
closer to the ground; lower `--immersive-forward-offset` moves it closer to the
kart. The default `forward_offset=0.25` deliberately leaves almost no space
between camera and kart, so other racers cannot pass between the view and the
active kart. These values are also exposed in `/immersive/v1/state` and can be
updated at runtime through `/immersive/v1/render/config` in relay local API
builds.

Do not use aggressive shake for domemaster output. If the kart crashes or spins,
the default mode keeps the horizon more stable than a raw kart-mounted camera.

## 10. Debug Grid

Enable:

```text
--immersive-debug-grid=on
```

The shader overlays:

- concentric rings;
- radial lines;
- dome edge;
- horizon marker;
- forward marker;
- side/up markers;
- dome tilt marker.

This is intended for cupola calibration and receiver alignment.

## 11. Syphon Output

Build flag:

```text
-DENABLE_SYPHON_OUTPUT=ON
```

Rules:

- macOS only;
- requires `-DENABLE_IMMERSIVE_RELAY=ON`;
- fails at CMake configure time on non-macOS;
- fails at CMake configure time if `Syphon.framework` is not found;
- Syphon code is not compiled into vanilla or relay-without-Syphon builds.

Local development verification uses an official Syphon framework installed at:

```text
~/Library/Frameworks/Syphon.framework
```

The checked local framework is a universal macOS build with `arm64` and
`x86_64` slices.

Default server name:

```text
SuperTuxKart Immersive Relay Domemaster
```

Runtime:

```text
--immersive-output=syphon
--syphon-server-name="SuperTuxKart Immersive Relay Domemaster"
```

The adapter publishes the final projected texture with its configured width and
height. Fisheye/domemaster remains square; Phase 4.6 removes the output-layer
assumption that every future projection must be square.

## 12. Swift Receiver Contract

The future Swift/macOS dome app should:

- discover available Syphon servers;
- prefer or select `SuperTuxKart Immersive Relay Domemaster`;
- capture the square domemaster texture/frame;
- present it through the dome/projector pipeline;
- apply projector blend/warp if needed;
- show receiver diagnostics to an operator;
- keep phone/session authority separate from STK.

Phones do not receive Syphon. The local API carries state and commands. Syphon
is only video from STK to the dome app on the same machine.

Phase 5.3 TholusOS integration uses the `fulldome_safe` venue preset by
default: 1536x1536 domemaster, 768 cubemap, `fisheye_domemaster`, Syphon server
`SuperTuxKart Immersive Relay Domemaster`, and local API
`http://127.0.0.1:8890/immersive/v1`.

## 13. Generic Receiver Test

With a Syphon-capable build:

```bash
cmake --build cmake_build_phase4_syphon --target supertuxkart --parallel 8
cmake_build_phase4_syphon/bin/supertuxkart.app/Contents/MacOS/supertuxkart \
  --race-now \
  --immersive-relay \
  --immersive-output=syphon \
  --immersive-projection=fisheye \
  --immersive-domemaster-size=1024 \
  --immersive-cubemap-size=512 \
  --immersive-debug-grid=on \
  --immersive-stats=on \
  --track=snowtuxpeak \
  --kart=tux \
  --numkarts=6 \
  --difficulty=0 \
  --unlock-all
```

Open a generic Syphon receiver and select:

```text
SuperTuxKart Immersive Relay Domemaster
```

Expected image: square black background with a circular fisheye/domemaster,
debug grid if enabled, and correct square dimensions.

## 14. Benchmark Notes

The Phase 4 smoke script can run graphical benchmarks with:

```bash
RUN_GRAPHICS=1 tools/immersive/RunRelayPhase4RenderSmoke.sh
```

Short local smoke samples from Apple Silicon development hardware:

| mode | resolution | output | avg fps | avg ms | p95 ms | notes |
| --- | ---: | --- | ---: | ---: | ---: | --- |
| vanilla 16:9 | window | main | not measured | not measured | not measured | build/run smoke only; native STK run did not emit FPS |
| relay 16:9 | window | main | not measured | not measured | not measured | build/run smoke only; no immersive output |
| relay fisheye | 1024 | window | 63.6 | 15.7 | 42.0 | debug grid on, cubemap 512, active-driver smoke |
| relay fisheye | 1536 | window | 58.8 | 17.0 | 44.0 | debug grid on, cubemap 768 |
| relay fisheye | 2048 | window | 67.6 | 14.8 | 33.0 | debug grid on, cubemap 1024; short warm-up sample |
| relay fisheye | 1024 | Syphon | not measured | not measured | not measured | runtime smoke published `SuperTuxKart Immersive Relay Domemaster`; receiver-side visual check still pending |
| relay fisheye | 2048 | Syphon | not measured | not measured | not measured | build/link verified; long target-hardware benchmark still pending |

These are smoke numbers, not a final Mac mini M1 acceptance benchmark. Run a
longer controlled benchmark on the target Mac mini M1 before venue tuning.

## 15. Known Limits

- Phase 4.5 projection correction on 2026-06-09: the previous zenith-centered
  image showed a sky disc in the middle and track around the edge. The shader now
  maps the disc as a forward-facing 180 degree fisheye lens and samples the six
  rendered face textures with camera-screen orientation. Local 1024 window
  captures no longer show the hard sky disc or inverted ring. This still needs
  venue validation at 1536/2048 and on tracks with different skyboxes before
  calling it final projector tuning.
- Local Syphon framework used for validation:
  `~/Library/Frameworks/Syphon.framework`, universal `arm64`/`x86_64`.
- CMake can be pointed explicitly at the framework:
  `-DSYPHON_FRAMEWORK_PATH=$HOME/Library/Frameworks/Syphon.framework`.
- The current Phase 4.5 projection keeps the six face color textures for the
  main multiview fisheye projection and also copies the faces into
  `GL_TEXTURE_CUBE_MAP` for validation/fallback. Direct samplerCube fallback is
  currently more seam-prone on Snowtuxpeak than the face-texture path.
- The adapter uses `SyphonOpenGLServer` for real macOS publish.
- Projection timing is very small in current logs and often rounds to `0.0ms`
  because stats use millisecond timing.
- The current cubemap-equivalent path uses six separate face renders copied into
  a cube texture, not a hardware cubemap array or single-pass multiview.
- 2048 performance needs a controlled Mac mini M1 test run.
- The window output is a calibration preview, not the final venue presentation.

Phase 4.6 receiver/benchmark evidence on MacBookPro18,3 / Apple M1 Pro /
16 GB / macOS 26.4.1:

```text
1024 Syphon: SYPHON_PROBE PASS received=1024x1024 expected=1024x1024 frames=1129 duration=20.0054 fps=56.4347 orientation_grid=pass
1536 Syphon: SYPHON_PROBE PASS received=1536x1536 expected=1536x1536 frames=1050 duration=20.0145 fps=52.4618 orientation_grid=pass
2048 Syphon: SYPHON_PROBE PASS received=2048x2048 expected=2048x2048 frames=941 duration=20.0075 fps=47.0325 orientation_grid=pass
```

The matching STK-side 20s benchmark rows are stored in
`/tmp/stk_phase46_benchmark_smoke/phase45_benchmark.csv`. Run a 60s pass on the
Mac mini M1 before venue acceptance.

Historical visual captures of the fixed cubemap-orientation defect:

```text
/tmp/stk_phase45_cube_sampler_direct_1024.png
/tmp/stk_phase45_cube_sampler_nogrid_1024.png
```

Forward-facing low camera comparison captures:

```text
/tmp/stk_phase45_forward_fisheye_face_mapping_fix/domemaster.png
/tmp/stk_phase46_camera_default_semicockpit/domemaster.png
```

## 16. Phase 5 Handoff

Phase 5 can consume:

- domemaster output mode;
- active-driver camera tracking;
- local API render stats;
- debug grid;
- Syphon server contract.

Phase 5 should focus on the actual Swift/macOS authority and receiver app,
including operator diagnostics and projector presentation.

## 17. Phase 6 Equirect

Equirectangular output remains future work. Do not treat the Phase 4 fisheye
shader as an equirectangular implementation.

Equirectangular output is not expected to be cheaper on the GPU for this
renderer. With rasterized STK content it would still need six cubemap-equivalent
scene renders, then a projection pass. A common 4096x2048 equirectangular target
also has about twice the pixels of a 2048x2048 domemaster, so it is a format
choice for later pipelines, not a performance shortcut.

## 18. Smoke Script

Primary script:

```bash
tools/immersive/RunRelayPhase4RenderSmoke.sh
```

Useful modes:

```bash
SKIP_BUILDS=1 RUN_GRAPHICS=0 tools/immersive/RunRelayPhase4RenderSmoke.sh
SKIP_BUILDS=1 RUN_GRAPHICS=1 tools/immersive/RunRelayPhase4RenderSmoke.sh
```

If Syphon is unavailable, the script reports `SKIPPED` with the reason instead
of pretending success.
