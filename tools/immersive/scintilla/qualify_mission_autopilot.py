#!/usr/bin/env python3
"""Qualify the SC4 mission anchors with the autopilot, without pointers.

Runs the operator procedure of docs/immersive/scintilla/sc4-implementation-2026-10-16/
OPERATORE.md section 1 end to end: two complete races driven natively by the
autopilot (STK_RELAY_AUTOPILOT=1), one recording the capsules as shells and one
as open capsules, offline validation of both raw traces and, if the geometry
qualifies, installation of report and traces in
data/immersive_relay/scintilla/qualification_v1/. A final preflight in a fresh
race asks the native reader to accept them.

The qualification fingerprint covers the STK executable, assets, shaders,
anchors, camera, projection, resolutions, P/C and laps: rerun this after every
rebuild or configuration change. It proves rendered geometry only, never
optics, pointers or a played mission.
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
import time
import uuid
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parent))
import collect_mission_measurement as collect  # noqa: E402
import crew_simulator as sim  # noqa: E402
import validate_mission_qualification as validate  # noqa: E402

REPO = Path(__file__).resolve().parents[3]
ANCHORS = REPO / 'data/immersive_relay/scintilla/snowtuxpeak_mission_v1.json'
QUALIFICATION = REPO / 'data/immersive_relay/scintilla'
MEASUREMENT = 'tholus.stk-scintilla-measurement/1'
PHASES = ('shell', 'open')


def log(message):
    print(time.strftime('%H:%M:%S ') + message, file=sys.stderr, flush=True)


def configure(api, players):
    """A fresh held pre-race world in the exact native SC4 profile."""
    api.call('/operator/reset', {})
    # Multiplayer sessions only admit lap-based pilot changes. The shortest
    # automatic one pauses the world for 6 s at each lap line: windows break
    # there, as in a real session, and must hold within each lap.
    api.call('/operator/configure', dict(track_id='snowtuxpeak', kart_id='tux', player_count=players, ai_karts=3,
             difficulty='easy', crew_mode='disabled', output='syphon', countdown_seconds=2,
             handoff_policy='lap_based_handoff', handoff_seconds=6, min_handoff_seconds=6,
             handoff_operator_confirm=False, auto_confirm_handoff=True))


def measurement(api, operation, body=None):
    return api.call('/scintilla/mission/measurement/' + operation, body, expect=None)


def request(api, operation, **fields):
    state = measurement(api, 'state')
    body = dict(contract=MEASUREMENT, schemaVersion=1, requestId=str(uuid.uuid4()),
                producerUUID=state['producerUUID'], raceEpoch=state['raceEpoch'], **fields)
    return measurement(api, operation, body)


def until(condition, timeout, what, interval=.5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = condition()
        if value:
            return value
        time.sleep(interval)
    raise RuntimeError(what)


def record(api, players, phase, output, race_timeout, crew_profile=None):
    configure(api, players)
    extra = {'crewProfile': crew_profile} if crew_profile else {}
    # The world loads asynchronously; transient refusals end once it is held.
    started = until(lambda: (lambda r: r if r.get('ok') else log('start: ' + str(r.get('error'))))(
        request(api, 'start', phase=phase, **extra)), 120, 'measurement did not start', interval=2)
    log(f'{phase}: recording {started["recordingID"]} race {started["raceEpoch"]}')
    api.call('/operator/start', {})
    api.call('/operator/countdown', dict(skip=True))
    began = time.monotonic()

    def finished():
        state = api.call('/state')
        if state.get('arcade_state') in ('results', 'finished'):
            return state
        session = state.get('arcade_session') or {}
        if state.get('arcade_state') == 'lap_handoff' and session.get('handoff_countdown', state.get('handoff_countdown')) == 0 \
                and state.get('next_pilot_id'):
            api.call('/operator/ready', {'player_id': state['next_pilot_id']}, expect=None)  # fallback if not automatic
        if int(time.monotonic() - began) % 30 == 0:
            session = state.get('arcade_session') or {}
            log(f'{phase}: {state.get("arcade_state")} lap {session.get("current_lap", state.get("current_lap"))}')
        return None
    until(finished, race_timeout, f'{phase}: race did not finish naturally', interval=1)
    log(f'{phase}: natural finish after {time.monotonic() - began:.0f} s')

    def stopped():
        reply = request(api, 'stop')
        if reply.get('ok') and reply.get('closed'):
            return reply
        log('stop: ' + str(reply.get('error') or 'not closed'))
        return None
    state = until(stopped, 60, f'{phase}: recording did not close', interval=1)
    if state.get('reason'):
        log(f'{phase}: trace reason {state["reason"]}')
    return collect.export(collect.Client(api.base, api.token), str(output))


def preflight(api, players):
    configure(api, players)
    reply = until(lambda: (lambda r: r if r.get('ok') else log('preflight: ' + str(r.get('error'))))(
        request(api, 'preflight')), 300, 'preflight unavailable', interval=2)
    return reply


def install_dir(crew_profile):
    """Where the native reader looks: SC4 alone, or one SC5 crew profile."""
    return QUALIFICATION / ('qualification_v2/' + crew_profile if crew_profile else 'qualification_v1')


def install(staging, report, crew_profile=None):
    """Copy report and traces, never symlinks; keep a previous install aside."""
    INSTALL = install_dir(crew_profile)
    if INSTALL.exists():
        previous = staging / ('previous-' + time.strftime('%Y%m%d-%H%M%S'))
        shutil.move(str(INSTALL), str(previous))
        log(f'previous qualification moved to {previous}')
    INSTALL.mkdir(parents=True)
    for entry in report['traceFiles']:
        target = INSTALL / entry['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(staging / entry['path'], target)
    shutil.copyfile(staging / 'report.json', INSTALL / 'report.json')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--assets-dir', type=Path)
    parser.add_argument('--runtime-dir', type=Path, required=True, help='new directory on the external SSD')
    parser.add_argument('--players', type=int, default=3, choices=range(2, 7), help='P of the session to qualify')
    parser.add_argument('--projection', default='fisheye_domemaster', choices=['fisheye_domemaster', 'equirectangular'])
    parser.add_argument('--race-timeout', type=float, default=900)
    parser.add_argument('--races-per-phase', type=int, default=2, choices=range(1, 5),
                        help='the validator keeps, per lap, the best race of a phase (at most 8 traces)')
    parser.add_argument('--crew-profile', choices=('sc5_crew_standard_v1', 'sc5_crew_party_v1'),
                        help='SC5 layout v3 measurement, installed under qualification_v2/<profile>')
    parser.add_argument('--no-install', action='store_true', help='validate only')
    args = parser.parse_args(argv)
    staging = args.runtime_dir.resolve() / 'qualification'
    staging.mkdir(parents=True, exist_ok=False)
    launch_args = SimpleNamespace(runtime_dir=args.runtime_dir, build_dir=args.build_dir, assets_dir=args.assets_dir,
                                  crew=args.players - 1, projection=args.projection, tuning=None,
                                  telemetry_dir=args.runtime_dir / 'telemetry')
    token, port = 'qualify-' + uuid.uuid4().hex, sim.free_port()
    process, producer_log = sim.launch(launch_args, token, port)
    api = sim.Api(f'http://127.0.0.1:{port}/immersive/v1', token)
    try:
        until(lambda: process.poll() is None and sim_state(api), 300, 'producer API unavailable', interval=1)
        traces = []
        for race in range(1, args.races_per_phase + 1):
            for phase in PHASES:
                name = f'{phase}-{race}.ndjson'
                exported = record(api, args.players, phase, staging / name, args.race_timeout, args.crew_profile)
                log(f'{name}: {exported["byteCount"]} bytes, sha256 {exported["sha256"][:12]}')
                traces.append(staging / name)
        code = validate.main(['--anchors', str(ANCHORS), '--report', str(staging / 'report.json'),
                              *[a for t in traces for a in ('--trace', str(t))]])
        report = json.loads((staging / 'report.json').read_text())
        if report['status'] != 'qualified_geometry':
            log('unqualified: ' + ', '.join(report['reasons']))
            return 1
        if args.no_install:
            return 0
        install(staging, report, args.crew_profile)
        if args.crew_profile:
            # The SC5 preflight reads the report of the latched crew intent;
            # the simulator's crew session exercises it.
            log(f'installed under {install_dir(args.crew_profile)}')
            return code
        reply = preflight(api, args.players)
        log(f'preflight: qualified={reply.get("qualified")} reason={reply.get("reason")!r}')
        return 0 if reply.get('qualified') and code == 0 else 1
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=15)
            except Exception:
                process.kill()
        producer_log.close()


def sim_state(api):
    try:
        return api.call('/state')
    except Exception:
        return None


if __name__ == '__main__':
    sys.exit(main())
