# Scintilla core regressions

This standalone target compiles the production `scintilla_input.cpp` with its
regressions. It needs a C++11 compiler and CMake 3.12 or newer, with no STK assets,
graphics libraries, local API, running app or hardware. `ENABLE_RELAY_TEST_TOOLS`
is explicitly opt-in and defaults to `OFF`. Root STK builds register the same
target and cases when that option is enabled together with `ENABLE_IMMERSIVE_RELAY`.

These commands are instructions for a later authorized verification. Configuration,
compilation and native test execution were **not performed** for this delivery.
Keep the build directory on the external SSD:

```sh
TASK_BUILD="/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/sc1-core-regression-build"
cmake -S tools/immersive/tests -B "$TASK_BUILD" -DENABLE_RELAY_TEST_TOOLS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "$TASK_BUILD" --target scintilla_input_regression --config Release
ctest --test-dir "$TASK_BUILD" -C Release --output-on-failure -L scintilla
```

With CMake versions before 3.20, enter the build directory before invoking CTest
instead of using `--test-dir`. Multi-configuration generators may place the
executable in `Release/`; CTest resolves its location automatically.

`scintilla_input_cases.inc` is the shared case registry used by both the executable
and CTest. The executable accepts `--list`, `--case NAME`, or no arguments to run
all cases. Unknown cases and invalid arguments fail. A `CHECK` failure prints the
expression, source file and line, then exits unsuccessfully. Checks and their
setup expressions remain active with `NDEBUG`, including Release/STKRelease.

Coverage includes parsing, admission, replay and renewal, lifecycle barriers,
immutable presentation proofs, actual history admission, pending metadata leases,
feedback, per-source quotas and ownership. The five-source case loses/reacquires
one source without altering the other four. The moving-camera case distinguishes
a shot at the old visible pose from a later shot after translation and rotation.

Aim/action sequence spaces remain independent. The live cursor uses acquisition
time in the original host clock domain; an older/equal aim consumes its own aim
sequence without moving or disarming a cursor supplied by an accepted press.
An older valid action still resolves its own event-time ray while retaining a
newer live aim. The adapter preserves aims across actions because only STK can
decide whether a press passes gameplay admission. Three native regression cases
cover same-batch and cross-batch supersession, expired/equal pending aims, replay
protection, older actions with newer aims, and rate-limited presses followed by a
useful aim. Host Swift tests exercise the actual reliable-first SI5 drain and
both message orders. These native/Swift regression sources were reviewed, not run.

Clipping cases exercise the complete consumer grant/proof/action path with an
always-visible visibility oracle. Picking selects the nearest positive sphere
entry before clipping, matching the marker shader. Both use strict near/far
planes in the dominant cubemap face's camera-space depth, rather than radial
distance. Cases include the clipped-nearest/two-marker counterexample, off-axis
near/far contacts, a rotated/translated camera, both projection formats, dome tilt, capture
FOVs of 90/120 degrees, exact plane boundaries and all six cubemap directions.
The oracle deliberately cannot hide a wrong selection. These are native core
regression sources, not executed tests or GPU/shader qualification.

Nearest-target ambiguity is checked only after every positive sphere entry has
been examined, against the two globally smallest contacts. A tie between farther
markers cannot suppress a closer marker registered later. Two native regression
cases exercise every registration permutation of the audit's three-marker case,
both projections, translated/rotated cameras, and genuine nearest ties inside
and outside the existing tolerance. Genuine nearest ties still fail closed;
clipping and historical depth policy remain the same. These are source fixtures,
not executed native/GPU tests.

Target-lifetime regressions use the production current-owner lookup/retirement
helpers with local world/item fixtures. They mutate ownership and eligibility
after capture, without publishing another frame: collected/used-up/type-switched
items, finished/eliminated/player karts, absent owners/slots and replaced pointers.
An observed retirement removes the captured instance; restoring the same pointer
needs a newly registered instance. Core cases cover both an already-ready
visibility result and a pending result completed after retirement. Neither may
confirm a hit or revoke the otherwise valid input grant. These are software
fixture sources, not an executed world/GPU or hardware qualification.

GPU readback cache regressions exercise the production maintenance helper with
synthetic fence readiness. Maintenance polls before retirement; a result signaled
at 90 ms and first observed at 100.2 ms is mapped once and remains reusable. Cache
entries expire only after more than the shared 250 ms consumer window since the
last visibility request, including reuse by another action; maintenance polling
does not renew that idle window. Completed, pending and failed entries all retain
the existing 64-entry active/deferred resource budget. In-flight retirement still
requires proven GPU completion before storage deletion.

Core integration fixtures keep aim fresh and resolve visible/occluded results
observed at 90, 100.2, 150, 249.999 and exactly 250 ms, without reissuing the
query. A result first observed at 250.001 ms still produces `history_timeout`:
cache retention never extends the original shot deadline. These regressions are
source additions, not executed native tests, real OpenGL fence/map tests or GPU
latency measurements.

Depth visibility uses the same conservative scalar rule in the marker shader
and historical resolver. The source RTT is `GL_DEPTH24_STENCIL8`; historical
copies are `GL_DEPTH_COMPONENT24`. The UNORM24 spacing is `1/(2^24-1)`, from
[OpenGL 3.3 section 2.1.5](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf#page=25).
The former additive `0.000002` allowance (about 34 depth levels) is removed.
Both paths compare the stable complement `1-depth = n/(f-n) * (f-z)/z`,
with float inputs and operations. Two D24 steps account conservatively for
scene quantization/conversion, while eight float epsilons guard the short marker
arithmetic chain. Both bounds shrink visibility; neither permits penetration.
Equal or overlapping depth intervals are occluded. This can reject a very close
foreground surface or a marker in the final unresolved far-plane slice, including
against a clear-depth sample. Clear depth is not a special visibility bypass.

Four native depth regressions cover the 100 m marker/99.95 m wall counterexample,
invalid inputs, quantization ambiguity, near/far boundaries, multiple clip ratios,
and the full grant/proof/action path with both projections and translated/rotated
capture cameras. They are registered sources, not executed native tests.
A separate compiler-free check evaluates the actual C++/GLSL scalar expressions
with binary32 rounding after every operation, against independent 60-digit
perspective/D24 fixtures, three quantization modes and deterministic random pairs:

```sh
python3 -B tools/immersive/tests/check_scintilla_depth.py
```

This checks source wiring and arithmetic policy. It does not execute C++/GLSL,
qualify GPU precision/rasterization or prove physical occlusion. Native and GPU
verification remain separate from these source/math checks.

The realistic long-session fixture advances a synthetic monotonic clock through
4800 primary shots, 251 ms apart (about 3.984 Hz), spanning 20 min 4.8 s and 9600
action UUIDs. Clock/lease renewal follows an independent 500 ms schedule. Its
1 ms fire-rate margin accounts for the conservative clock uncertainty at the
exact 250 ms boundary. The separate 6000-edge/30-second stress fixture is retained.
These are deterministic core tests; they do not qualify live throughput, GPU
retention, Syphon transport, physical input or venue operation.

## Local HTTP transport and dispatcher

On POSIX systems the same opt-in build also registers seven `relay_http.*`
socketpair cases and four `relay_dispatcher.*` cases. They use always-active
checks and named `--case` selection. No socket listens on a network address.
The transport target compiles the production framing/poll helper; the dispatcher
target compiles the production queue with a recorder replacing only the final
game router. Its stop case runs last in all-case mode; CTest isolates every case.

Later authorized verification can build `relay_http_transport_regression` and
`relay_game_dispatcher_regression` in the same external build directory, then
select them with `ctest --test-dir "$TASK_BUILD" -C Release --output-on-failure -L http`.
No configure, compiler, socketpair test, native executable or server was run for
this source-only delivery.

Transport cases cover strict header-only framing/authentication, duplicate fields,
unsupported transfer encoding, complete bodies up to 64 KiB independently of the
8 KiB header limit, slow-drip absolute deadlines, partial writes, 1 MiB response
bounds, a nonreading peer, pending async jobs, full-body half-close, finite slots,
wake readiness and healthy traffic beside incomplete clients. Dispatcher cases
cover FIFO, spatial commands staying queued, snapshot reads, cancellation before
drain, reclaimed capacity, cancellation after routing has begun and shutdown.

The production pool holds at most 16 clients, reads/writes at most 16 KiB per peer
per turn, and accepts at most eight new peers per listener turn. Deadlines are
absolute: headers 2 s and full request 5 s from accept; queued game result 30 s;
response 2 s, with a 37 s connection ceiling. The `poll` syscall timeout is capped at 1 ms while game results are pending
(5 ms otherwise). Scheduling and processing can extend the actual interval. These
are configured limits, not measured latency or performance qualification.

Framing follows [RFC 9112 section 6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3):
unsupported transfer codings and ambiguous/invalid lengths are rejected, an
incomplete declared body is never dispatched, and each response closes its
connection. The transport uses [POSIX `poll` readiness](https://pubs.opengroup.org/onlinepubs/9799919799/functions/poll.html),
nonblocking descriptors and steady time.

## SC2 source increment (2026-10-15)

The combat core is registered as an opt-in C++11 regression executable with 15
individually selectable cases. Spatial-input regressions now include combat
historical-window consensus, drain-once delivery, GPU completion ordering,
continuous-beam freshness, query ownership under capacity pressure and rejection
of beam regression from an older reliable press. The dispatcher tests cover the
new combat GET on the game queue. None of these native cases was compiled or run
in the no-build task. `check_scintilla_combat.py` checks source wiring and math;
it is not execution of the C++ engine, shader, physics or host app.

See `docs/immersive/scintilla/sc2-2026-10-15/README.md` for phase scope, API,
parameters, the M01–M44 coverage map and outstanding integrated verification.

The SC2 Ready-gate correction adds two combat cases (17 total), one strict
intent-parser case (60 spatial cases total), and one intent/Ready dispatcher
ordering case (6 dispatcher cases total). The source/math checker now has eight
checks. These additions cover explicit intent before a grant, reset preserving
intent without readiness, same-race intent CAS, and the reset-only Results path.
Native cases remain written/reviewed only under the no-build constraint. See
`docs/immersive/scintilla/sc2-ready-gate-2026-10-15/README.md` for the additive API
and host source evidence.

The SC2 stagger-clearance correction adds four combat cases (21 total), using
the production pure envelope helper with translated/asymmetric physical bounds,
the low-ceiling counterexample, supporting-plane separation and invalid inputs.
They remain registered native sources, not executed tests. The compiler-free
check below evaluates the actual scalar expressions against independent
quaternion/vertex geometry and verifies physics-query source wiring:

```sh
python3 -B tools/immersive/tests/check_scintilla_stagger_clearance.py
```

Its six checks and the eight existing combat source/math checks do not execute
Bullet overlap/sweep callbacks or certify the animated kart in a running game.
See `docs/immersive/scintilla/sc2-stagger-clearance-2026-10-15/README.md` for the
derivation, conservative support rule and source-only verification boundary.

The SC2 beam-loss correction adds four spatial cases (64 total). They drive
miss/changed target/unavailable depth/retired instance followed by a newer hit
between authoritative updates, during acquisition and during breach with a
contribution and a pending shot. They also cover release/press rearming, an
initial miss before any established beam, continuous positives, source isolation
and old-epoch asynchronous failures. The tests are registered native sources,
reviewed but not compiled or executed. The source/math checker now has ten
checks, including evaluation of the actual interruption predicate and inspection
of authoritative close-before-shoot ordering. See
`docs/immersive/scintilla/sc2-beam-loss-2026-10-15/README.md`.

The SC2 tool-feedback correction adds four pure combat cases (25 total) and two
spatial cases (66 total). They cover persistent release-required state after
expiry, negative proof, completion, incompatibility and retirement; distinct
busy ownership without stealing; release, pause, reset and input reprepare;
publication after same-tick effects; primary feedback independence; stale epochs;
and the input interruption barrier winning over a stale core view.
Native cases remain written and reviewed, not compiled or executed.

```sh
python3 -B tools/immersive/tests/check_scintilla_tool_feedback.py
```

The six compiler-free checks cover actual render gates, source/owner mapping,
publication order, status/uniform codes, glyph distinctions and complete badge
boxes fitting both projection domains without overlapping other tool badges.
They do not execute GLSL or establish legibility on the projected dome.
See `docs/immersive/scintilla/sc2-tool-feedback-2026-10-15/README.md` for the
additive source feedback DTO, visual language and outstanding runtime checks.

## SC3 defense (source checkpoint, 15 October 2026)

The opt-in runner also registers `scintilla_defense_regression` (21 pure core
cases) and `scintilla_defense_collision_regression` (6 cases linked to the actual
bundled Bullet). The input registry adds 8 SC3 cases, retaining all66 previous
cases (74 total). New cases cover resource reserve/cooldown/rearm, all-release
arbitration, late/new epochs, optical boundaries, live negative proofs, two AI
identities, two sources with moving historical camera/target, logical same-grant
participant rebind, actual compound offsets/rotation and earliest obstruction.

These native suites were written and source-reviewed without compilation or
execution. The existing CMake opt-in remains required; do not configure/build
while the no-build phase is in effect. Later standalone configuration can build
the actual bundled Bullet in the same external task output directory.

The permitted source-only check is:

```sh
python3 -B tools/immersive/tests/check_scintilla_defense.py
```

It checks source wiring, native case registration, bundled callback ABI, lexical
balance and independent mathematical bounds. It does not parse/typecheck native
code or prove Bullet/GLSL/Swift execution. See the
[SC3 checkpoint](../../../docs/immersive/scintilla/sc3-2026-10-15/README.md) and
[scenario matrix](../../../docs/immersive/scintilla/sc3-2026-10-15/REGRESSIONI.md).

## Optional local GPU depth regression (macOS)

The separate runner below compiles and evaluates the **production GLSL depth
function** in an offscreen OpenGL 4.1 context. It checks 64 independent geometric,
D24-quantized, clipping and invalid-input cases, including the 99.95 m wall / 100 m
marker counterexample. This catches runtime `const` initializers rejected by GLSL
4.10 and verifies the conservative depth arithmetic on the local driver.

It requires Xcode command-line tools and a local OpenGL 4.1 GPU. It is deliberately
separate from the portable CTest suite: neither software rendering nor a skipped
test is reported as a successful GPU qualification. All build outputs and compiler
temporary files use the explicit output directory, which must not already exist.

```sh
python3 -B tools/immersive/tests/run_scintilla_depth_gpu.py \
  --output-dir "/Volumes/SanDisk 2TB/CodexWorkspace/ImmersiveSupertuxKart/depth-gpu-regression"
```

This does not test the full marker compositor, asynchronous historical readback,
Syphon receiver, optical timing, tracking, venue output or physical occlusion.
The dated [SC0–SC3 qualification checkpoint](../../../docs/immersive/scintilla/sc0-sc3-qualification-2026-10-15/README.md)
records the executed native/GPU tests and their separate limits.
