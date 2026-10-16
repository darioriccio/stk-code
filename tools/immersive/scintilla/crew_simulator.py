#!/usr/bin/env python3
"""Software crew for Missione Scintilla (diagnostic only).

Drives a running STK Relay producer through its authenticated /spatial API with
synthetic controllers that see the dome through a modelled display latency and
aim like people: reaction time, smooth pursuit, tremor and click delay. It also
stands in for TholusOS's presentation proofs, with a declared display latency.

Requires a producer started with STK_RELAY_SIMULATION=1 (the diagnostic view);
--launch starts one with the native autopilot driving the relay kart. Results
are telemetry for balancing, never optical, hardware or venue evidence.
"""
import argparse
import json
import math
import os
import random
import secrets
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
import uuid
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

SPATIAL = 'tholus.stk-spatial/1'
INPUT = 'tholus.spatial-input/1'
SESSION, GENERATION, CONTEXT, ROUTE = 'software-session', 'software-generation', 'software-projection', 'software-route'
RAY = {'originM': [0, 0, 0], 'direction': [0, 0, -1], 'basis': 'room.rhs-y-up-front-minus-z/1'}
SECTOR_NODES = {0: 'front', 1: 'right', 2: 'rear', 3: 'left'}


def host_us():
    return time.monotonic_ns() // 1000


# ---- Projection: the same equations as ScintillaSpatial::Runtime. ----
def direction(projection, u, v):
    kind = projection['kind']
    if kind == 'equirectangular':
        lon, lat = (u - .5) * 2 * math.pi, (.5 - v) * math.pi
        return (math.sin(lon) * math.cos(lat), math.sin(lat), math.cos(lon) * math.cos(lat))
    if kind == 'fisheye_domemaster':
        x, y = 2 * u - 1, 1 - 2 * v
        r = math.hypot(x, y)
        if r > 1:
            return None
        theta = r * math.pi / 2
        d = (0.0, 0.0, 1.0) if r < 1e-5 else (math.sin(theta) * x / r, math.sin(theta) * y / r, math.cos(theta))
        a = math.radians(projection.get('domeTiltDeg', 0))
        c, s = math.cos(a), math.sin(a)
        return (d[0], c * d[1] - s * d[2], s * d[1] + c * d[2])
    raise ValueError(kind)


def uv(projection, d):
    n = math.sqrt(sum(x * x for x in d))
    x, y, z = (c / n for c in d)
    if projection['kind'] == 'equirectangular':
        return math.atan2(x, z) / (2 * math.pi) + .5, .5 - math.asin(max(-1, min(1, y))) / math.pi
    a = math.radians(projection.get('domeTiltDeg', 0))
    c, s = math.cos(a), math.sin(a)
    lx, ly, lz = x, c * y + s * z, -s * y + c * z
    r = min(1.0, math.acos(max(-1, min(1, lz))) / (math.pi / 2))
    planar = math.hypot(lx, ly)
    px, py = (0, 0) if planar < 1e-12 else (lx / planar * r, ly / planar * r)
    return (px + 1) / 2, (1 - py) / 2


def angle_deg(a, b):
    dot = sum(x * y for x, y in zip(a, b)) / (math.sqrt(sum(x * x for x in a)) * math.sqrt(sum(y * y for y in b)))
    return math.degrees(math.acos(max(-1.0, min(1.0, dot))))


def slerp(a, b, t):
    omega = math.radians(angle_deg(a, b))
    if omega < 1e-9:
        return b
    sa, sb = math.sin((1 - t) * omega) / math.sin(omega), math.sin(t * omega) / math.sin(omega)
    return tuple(sa * x + sb * y for x, y in zip(a, b))


def rotate(d, yaw_deg, pitch_deg):
    """Small angular offset of a direction (tremor), in its own tangent frame."""
    x, y, z = d
    up = (0, 1, 0) if abs(y) < .95 else (1, 0, 0)
    right = (up[1] * z - up[2] * y, up[2] * x - up[0] * z, up[0] * y - up[1] * x)
    n = math.sqrt(sum(c * c for c in right))
    right = tuple(c / n for c in right)
    true_up = (y * right[2] - z * right[1], z * right[0] - x * right[2], x * right[1] - y * right[0])
    a, b = math.radians(yaw_deg), math.radians(pitch_deg)
    return tuple(d[i] + right[i] * a + true_up[i] * b for i in range(3))


# ---- Presentation proofs, standing in for TholusOS. ----
LOOKBACK_US = 65_000
# The producer drops the grant 1 s after the last accepted renewal, checked
# when a request is processed: renew often enough that one stalled request
# (a long STK frame, a slow reply) still leaves room for the next.
RENEW_EVERY_S = .25
RENEW_INLINE_AFTER_S = .5
SLOW_CALL_MS = 250
MIN_PROOF_US = 30_000  # as TholusOS: the consumer only resolves events of the last 60 ms


def presentation_runs(frames, offset_us, latency_us, view_server_us, now_host, previous_from=None):
    """Proof (from, runs): frame i is on screen from capture_i+L to capture_{i+1}+L.

    The newest frame is claimed only until view time+L, since a later frame cannot
    have been captured before the view. The proof starts at now-lookback, never
    before the previous one, and runs are clipped to it rather than dropped: the
    producer rejects a proof whose overlap with the last one changes any frame.
    """
    frames = sorted(frames, key=lambda f: f['id'])
    start_bound = max(now_host - LOOKBACK_US, previous_from or 0)
    runs = []
    for i, frame in enumerate(frames):
        start = frame['captureUs'] - offset_us + latency_us
        if i == 0:
            # Its predecessor is not listed: at the exact boundary both frames
            # were plausible, and an earlier proof said so. Start just after it.
            start += 1
        end = frames[i + 1]['captureUs'] - offset_us + latency_us if i + 1 < len(frames) \
            else view_server_us - offset_us + latency_us
        end = min(end, now_host - 1000)
        if start > end:
            break
        if end < start_bound:
            continue
        runs.append({'frameId': frame['id'], 'presentedFromUs': max(start, start_bound), 'presentedUntilUs': end})
    if not runs or runs[0]['presentedFromUs'] >= runs[-1]['presentedUntilUs']:
        return None, []
    return runs[0]['presentedFromUs'], runs


# ---- People. ----
@dataclass
class Human:
    """Pointing model: reaction, first-order pursuit with partial lead, tremor."""
    rng: random.Random
    reaction_s: float = .30
    pursuit_tau_s: float = .12
    lead_s: float = .06
    tremor_deg: float = .35
    tremor_tau_s: float = .25
    click_delay_s: float = .06
    hand: tuple = (0.0, 0.0, 1.0)
    tremor: list = field(default_factory=lambda: [0.0, 0.0])

    def step(self, goal, dt):
        if goal is not None:
            self.hand = slerp(self.hand, goal, 1 - math.exp(-dt / self.pursuit_tau_s))
        # Ornstein-Uhlenbeck tremor with stationary deviation tremor_deg.
        k = math.exp(-dt / self.tremor_tau_s)
        for i in range(2):
            self.tremor[i] = self.tremor[i] * k + self.rng.gauss(0, self.tremor_deg * math.sqrt(1 - k * k))
        return rotate(self.hand, *self.tremor)


@dataclass
class Member:
    participant: str
    source: str
    human: Human
    role: str
    aim_seq: int = 0
    action_seq: int = 0
    events: int = 0
    tool: bool = False
    primary: bool = False
    tool_since: float = 0.0
    release_primary_at: float = 0.0
    last_shot: float = -10.0
    goal_since: float = 0.0
    goal_key: object = None
    rearm: bool = False
    poses: deque = field(default_factory=lambda: deque(maxlen=400))


class Simulation:
    def __init__(self, api, args):
        self.api, self.args = api, args
        self.rng = random.Random(args.seed)
        self.lock = threading.Lock()
        self.views = deque(maxlen=120)
        self.offset_us = 0
        # Proof boundaries use one frozen offset: a later re-estimate would move
        # them by microseconds and the producer would reject a rewritten history.
        self.proof_offset_us = None
        self.proof_from = self.proof_until = 0
        self.stop = threading.Event()
        self.stats = {'updates': 0, 'rejected': 0, 'reasons': {}}
        self.update_ms = []
        self.sent_proofs = deque(maxlen=4)
        # Light /spatial/clock probes taken by the view thread: the tightest
        # recent ones feed the next in-band renewal. Heavy update replies read
        # late by this process would widen the producer's offset interval.
        self.exchanges = deque(maxlen=9)
        self.next_probe = 0.0
        self.last_renew = 0.0
        self.capture_us = []
        self.target_motion = []  # (deg/s across the dome, angular radius deg)

    # Clock samples measured like the TholusOS pump; the three tightest of eight
    # keep the producer's offset interval (and so its pose ageing) narrow.
    def clock_samples(self):
        samples = []
        for _ in range(8):
            t0 = host_us()
            c = self.api.call('/spatial/clock')
            t3 = host_us()
            if 0 <= t3 - t0 <= 50000 and t3 - t0 >= c['serverSentUs'] - c['serverReceivedUs']:
                samples.append({'hostSentUs': t0, 'serverReceivedUs': c['serverReceivedUs'],
                                'serverSentUs': c['serverSentUs'], 'hostReceivedUs': t3})
        if len(samples) < 3:
            raise RuntimeError('three valid clock measurements unavailable')
        samples = sorted(sorted(samples, key=lambda s: s['hostReceivedUs'] - s['hostSentUs'])[:3], key=lambda s: s['hostSentUs'])
        mids = sorted((s['serverReceivedUs'] + s['serverSentUs']) / 2 - (s['hostSentUs'] + s['hostReceivedUs']) / 2 for s in samples)
        self.offset_us = int(mids[1])
        if self.proof_offset_us is None:
            self.proof_offset_us = self.offset_us
        return samples

    def identity(self, source):
        return {'sourceId': source, 'deviceBindingRevision': source + '-binding', 'trackingSpaceEpoch': 'software-space',
                'poseContinuityId': source + '-continuity', 'roomCalibrationRevision': 'software-room',
                'gripCalibrationRevision': 'software-grip'}

    def message(self, member, fields, point=None):
        body = {'contract': SPATIAL, 'message': {'contract': INPUT, 'inputSessionID': SESSION, 'routingGeneration': GENERATION,
                                                 **self.identity(member.source), **fields}}
        if point is not None:
            body['point'] = {'u': point[0], 'v': point[1], 'viewId': 'main', 'sourceId': member.source,
                             'sequence': fields['sequence'], 'projectionContextId': CONTEXT}
        return body

    def setup(self):
        a = self.args
        self.api.call('/operator/reset', {})
        self.api.call('/operator/configure', dict(track_id=a.track, player_count=a.crew + 1, ai_karts=a.ai_karts,
                      difficulty=a.difficulty, crew_mode='disabled', output='syphon', countdown_seconds=2,
                      handoff_policy='lap_based_handoff', handoff_seconds=6, min_handoff_seconds=6,
                      handoff_operator_confirm=True, auto_confirm_handoff=False))
        self.api.call('/spatial/enable', {})
        d = self.api.call('/scintilla/defense/state')
        self.api.call('/scintilla/defense/intent', dict(contract='tholus.stk-scintilla-defense/1', raceEpoch=d['raceEpoch'],
                                                        intentRevision=d['intentRevision'], required=True))
        state = self.api.call('/state')
        pilot = state.get('active_pilot_id', 'p1')
        self.players = [f'p{i}' for i in range(1, a.crew + 2)]
        crew = [p for p in self.players if p != pilot][:a.crew]
        roles = ['solo'] if a.crew == 1 else ['opener', 'shooter', 'shooter', 'defender', 'support'][:a.crew]
        if a.crew == 2:
            roles = ['opener', 'shooter']
        self.members = []
        for index, (participant, role) in enumerate(zip(crew, roles)):
            human = Human(rng=random.Random(self.rng.random()), reaction_s=a.reaction_ms / 1000 * self.rng.uniform(.8, 1.2),
                          tremor_deg=a.tremor_deg * self.rng.uniform(.8, 1.25), pursuit_tau_s=a.pursuit_ms / 1000)
            self.members.append(Member(participant=participant, source=f'sim-wand-{index + 1}', human=human, role=role))
        view = self.wait_view()
        projection = dict(view['projection'])
        projection.pop('type', None)
        self.projection = projection
        # The grant must reach the producer within 60 ms of its clock samples.
        samples = self.clock_samples()
        # Issued just before sending: within 60 ms of receipt, never in the future.
        issued = host_us() - 15000
        roster = dict(contract=INPUT, type='roster', inputSessionID=SESSION, routingGeneration=GENERATION,
                      installationID='software-crew-simulator-no-physical-evidence', consumerID='external.immersive_supertuxkart',
                      capabilities=['roomRay6dof', 'primaryAction', 'secondaryAction', 'eventTimePose'],
                      sources=[self.identity(m.source) for m in self.members], projectionContextId=CONTEXT,
                      expiresAtHostUs=issued + 5_000_000)
        self.api.call('/spatial/prepare', dict(contract=SPATIAL, roster=roster, issuedAtHostUs=issued, inputUncertaintyUs=0,
                      bindings=[{'sourceId': m.source, 'participantId': m.participant} for m in self.members],
                      clockSamples=samples, routeRevision=ROUTE, projection=projection))
        self.post([self.message(m, {'type': 'sourceState', 'sequence': 0, 'state': 'tracked', 'reason': 'prepared'})
                   for m in self.members], presentation=False)
        self.prepare_gameplay()

    def prepare_gameplay(self):
        c = self.api.call('/scintilla/state')
        i = self.api.call('/spatial/state')
        self.api.call('/scintilla/prepare', dict(contract='tholus.stk-scintilla-combat/1', profile='crew_standard_sc2_v1',
                      expectedRaceEpoch=c['raceEpoch'], expectedInputSessionId=SESSION, expectedRosterRevision=c['rosterRevision']))
        d = self.api.call('/scintilla/defense/state')
        self.api.call('/scintilla/defense/prepare', dict(contract='tholus.stk-scintilla-defense/1', profile='sc3_sector_rear_readonly_v1',
                      inputSessionID=SESSION, inputEpoch=str(i['inputEpoch']), raceEpoch=d['raceEpoch'], rosterRevision=d['rosterRevision'],
                      combatProfile='crew_standard_sc2_v1', layoutProfile=d['layoutProfile'], layoutRevision=d['layoutRevision'],
                      routeRevision=ROUTE, projectionContextId=CONTEXT,
                      visibilityEvidenceID='software-crew-simulator-no-physical-evidence',
                      visibleRegions=[dict(left=0, top=0, right=1, bottom=1)]))

    def rearm(self):
        inputs = []
        for member in self.members:
            for control in ('primary', 'secondary'):
                member.primary = member.tool = True
                self.edge(member, control, 'released', inputs, host_us())
        self.post(inputs)

    def wait_active(self, timeout=20):
        deadline = time.monotonic() + timeout
        while not self.api.call('/spatial/state').get('active'):
            if time.monotonic() > deadline:
                raise RuntimeError('gameplay did not become active')
            self.post([])
            time.sleep(.05)

    def handoff(self, state):
        """Lap-based pilot change: confirm, rebind the same sources, prepare, rearm."""
        pilot = state.get('next_pilot_id')
        if not pilot:
            return
        self.api.call('/operator/ready', {'player_id': pilot}, expect=None)
        deadline = time.monotonic() + 15
        while self.api.call('/state').get('active_pilot_id') != pilot:
            if time.monotonic() > deadline:
                raise RuntimeError('pilot change not confirmed')
            self.post([])
            time.sleep(.1)
        crew = [p for p in self.players if p != pilot]
        for member, participant in zip(self.members, crew):
            member.participant = participant
        i = self.api.call('/spatial/state')
        d = self.api.call('/scintilla/defense/state')
        self.api.call('/spatial/rebind', dict(contract=SPATIAL, inputSessionID=SESSION, routingGeneration=GENERATION,
                      expectedInputEpoch=str(i['inputEpoch']), expectedRosterRevision=d['rosterRevision'],
                      bindings=[{'sourceId': m.source, 'participantId': m.participant} for m in self.members]))
        self.prepare_gameplay()
        current = self.api.call('/state')
        if current.get('arcade_state') not in ('running', 'final_lap'):
            self.api.call('/operator/resume', {}, expect=None)
        self.wait_active()
        self.rearm()
        self.stats['handoffs'] = self.stats.get('handoffs', 0) + 1

    def wait_view(self, timeout=20):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            view = self.api.call('/scintilla/simulation/view')
            if view.get('frame') and view.get('projection'):
                return view
            time.sleep(.1)
        raise RuntimeError('no published frame: is STK_RELAY_SIMULATION=1 set and immersive output running?')

    def renewal(self):
        if len(self.exchanges) < 3:
            return None
        samples = sorted(sorted(self.exchanges, key=lambda s: s['hostReceivedUs'] - s['hostSentUs'])[:3],
                         key=lambda s: s['hostSentUs'])
        return dict(contract=SPATIAL, inputSessionID=SESSION, routingGeneration=GENERATION,
                    expiresAtHostUs=samples[-1]['hostSentUs'] + 5_000_000, clockSamples=samples)

    def poll_views(self):
        while not self.stop.is_set():
            if time.monotonic() >= self.next_probe:
                self.next_probe = time.monotonic() + .06
                try:
                    t0 = host_us()
                    c = self.api.call('/spatial/clock')
                    t3 = host_us()
                    if t3 - t0 > 20000:  # TholusOS rejects a clock probe slower than 20 ms
                        self.stats['clockProbesOver20Ms'] = self.stats.get('clockProbesOver20Ms', 0) + 1
                    # STK's own limit. The tightest three are used; a stricter cut,
                    # as in native TholusOS, starves renewals here because this
                    # thread shares the GIL with the crew loop.
                    if 0 <= t3 - t0 <= 50000:
                        with self.lock:
                            self.exchanges.append({'hostSentUs': t0, 'serverReceivedUs': c['serverReceivedUs'],
                                                   'serverSentUs': c['serverSentUs'], 'hostReceivedUs': t3})
                except Exception as error:
                    self.stats['reasons']['clock_error:' + type(error).__name__] = 1
            try:
                view = self.api.call('/scintilla/simulation/view')
                if view.get('frame'):
                    with self.lock:
                        if not self.views or view['frame']['id'] != self.views[-1]['frame']['id']:
                            self.views.append(view)
                            self.capture_us.append(view['frame']['captureUs'])
            except Exception as error:  # keep the crew alive; the update loop reports failures
                self.stats['reasons']['view_error:' + type(error).__name__] = 1
            time.sleep(.004)

    def displayed(self, now_host):
        """The newest frame already on screen for the crew at host time now."""
        latency = self.args.display_latency_ms * 1000
        with self.lock:
            for view in reversed(self.views):
                if view['frame']['captureUs'] - self.proof_offset_us + latency <= now_host:
                    return view
        return None

    def drawn_reticle(self, member, view):
        """Reticle STK drew into that frame: the newest pose sent before its capture."""
        capture_host = view['frame']['captureUs'] - self.proof_offset_us
        for sent, point in reversed(member.poses):
            if sent <= capture_host:
                return point
        return None

    def post(self, inputs, presentation=True):
        body = {'contract': SPATIAL, 'inputs': inputs}
        latest = None
        if presentation:
            with self.lock:
                latest = self.views[-1] if self.views else None
            if latest:
                start, runs = presentation_runs(latest['frames'][-24:], self.proof_offset_us, self.args.display_latency_ms * 1000,
                                                latest['nowUs'], host_us(), self.proof_from)
                # The producer maps the window through its clock interval: a proof
                # shorter than that interval is invalid and ends the session.
                if runs and runs[-1]['presentedUntilUs'] >= self.proof_until and \
                        runs[-1]['presentedUntilUs'] - start >= MIN_PROOF_US:
                    self.proof_from, self.proof_until = start, runs[-1]['presentedUntilUs']
                    body['presentation'] = dict(contract=SPATIAL, inputSessionID=SESSION, routingGeneration=GENERATION,
                                                frameStream=latest['stream'], timingRevision='software-display',
                                                proofFromUs=start, proofUntilUs=self.proof_until, runs=runs)
        if presentation and time.monotonic() - self.last_renew > RENEW_EVERY_S:
            with self.lock:
                renew = self.renewal()
            if renew is None and self.last_renew and time.monotonic() - self.last_renew > RENEW_INLINE_AFTER_S:
                # Fallback well before the producer's 1 s clock limit: probe inline.
                try:
                    samples = self.clock_samples()
                    renew = dict(contract=SPATIAL, inputSessionID=SESSION, routingGeneration=GENERATION,
                                 expiresAtHostUs=samples[-1]['hostSentUs'] + 5_000_000, clockSamples=samples)
                    self.stats['inlineRenewals'] = self.stats.get('inlineRenewals', 0) + 1
                except RuntimeError:
                    renew = None
            if renew:
                body['renew'] = renew
                if self.last_renew:
                    self.stats['maxRenewGapS'] = max(self.stats.get('maxRenewGapS', 0), round(time.monotonic() - self.last_renew, 3))
                self.stats['renewals'] = self.stats.get('renewals', 0) + 1
                self.last_renew = time.monotonic()
                with self.lock:
                    self.exchanges.clear()  # each renewal needs newer, distinct samples
        started = time.monotonic()
        try:
            reply = self.api.call('/spatial/update', body, expect=(200,))
        except RuntimeError:
            with self.lock:
                history = list(self.sent_proofs)
            Path(self.args.runtime_dir, 'rejected-update.json').write_text(json.dumps(
                {'body': body, 'previousProofs': history, 'offsetUs': self.offset_us, 'proofOffsetUs': self.proof_offset_us,
                 'viewFrames': latest['frames'] if presentation and latest else None, 'viewNowUs': latest['nowUs'] if presentation and latest else None,
                 'hostNow': host_us()}, indent=1))
            raise
        if 'presentation' in body:
            self.sent_proofs.append({k: body['presentation'][k] for k in ('proofFromUs', 'proofUntilUs', 'runs')})
        self.update_ms.append((time.monotonic() - started) * 1000)
        if self.update_ms[-1] > 50 and 'firstUpdateOver50MsS' not in self.stats and getattr(self, 'race_started', None):
            self.stats['firstUpdateOver50MsS'] = round(time.monotonic() - self.race_started, 1)
        self.stats['updates'] += 1
        for envelope, result in zip(inputs, reply.get('results', [])):
            if result.get('accepted') is False:
                self.stats['rejected'] += 1
                reason = result.get('reason', '?')
                self.stats['reasons'][reason] = self.stats['reasons'].get(reason, 0) + 1
                # A rejected sample or edge disarms that source: like a person told
                # to release, the member lets go of both controls next tick.
                source = envelope['message'].get('sourceId')
                for member in getattr(self, 'members', []):
                    if member.source == source:
                        member.rearm = True
        return reply

    # ---- Behaviour ----
    @staticmethod
    def pick(member, candidates, kind):
        """Keep the current target while it stays a candidate, as a person would;
        otherwise take the largest one. Switching every tick between two similar
        rivals would restart the reaction time and never press at all."""
        if member.goal_key and member.goal_key[0] == kind:
            for candidate in candidates:
                if candidate['id'] == member.goal_key[1]:
                    return candidate
        return max(candidates, key=lambda t: t['angularRadiusDeg'])

    def goal_for(self, member, view, now):
        targets = view.get('targets', [])
        rivals = [t for t in targets if t['kind'] == 'rival' and t.get('eligible')]
        defense = view.get('defense', {})
        nodes = {n['name']: n for n in view.get('nodes', [])}
        others_hold = any(m.tool and m is not member and m.role == 'opener' for m in self.members)
        threat = defense.get('phase') == 'telegraph'
        defender = member.role in ('defender', 'solo') or (member.role == 'shooter' and len(self.members) <= 3 and not any(
            m.role == 'defender' for m in self.members))
        if threat and defender and nodes:
            node = nodes.get(SECTOR_NODES.get(defense.get('sector'), ''))
            if node:
                return ('node', node['name'], node, 'tool')
        capsules = [t for t in targets if t['kind'] == 'capsule' and t.get('eligible')]
        if capsules and member.role in ('support', 'solo', 'defender'):
            capsule = self.pick(member, capsules, 'capsule')
            return ('capsule', capsule['id'], capsule, 'primary' if capsule['phase'] == 'shell' else 'tool')
        if not rivals:
            if defense.get('boostReady') and member.role in ('support', 'defender', 'solo') and nodes.get('boost') \
                    and self.rng.random() < .02:
                return ('node', 'boost', nodes['boost'], 'tool')
            return None
        if member.role == 'opener':
            target = self.pick(member, rivals, 'rival')
            return ('rival', target['id'], target, 'tool')
        if member.role == 'solo':
            target = self.pick(member, rivals, 'rival')
            return ('rival', target['id'], target, 'primary')
        open_targets = [t for t in rivals if t.get('phase') == 'breached' and t.get('opener') != member.participant]
        if open_targets:
            target = self.pick(member, open_targets, 'rival')
            return ('rival', target['id'], target, 'primary')
        # Pre-aim at the opener's choice so the breach can be hit quickly.
        target = self.pick(member, rivals, 'rival')
        return ('rival', target['id'], target, None) if others_hold or member.role == 'shooter' else None

    def target_dir(self, goal, view):
        kind, ident, item, _ = goal
        if kind == 'node':
            return direction(self.projection, item['u'], item['v']), max(.5, math.degrees(item['radiusU'] * 2 * math.pi) * .8)
        return direction(self.projection, item['u'], item['v']), item['angularRadiusDeg']

    def update_member(self, member, now, dt, inputs):
        host = host_us()
        if member.rearm:
            member.rearm = False
            member.primary = member.tool = True
            self.edge(member, 'primary', 'released', inputs, host)
            self.edge(member, 'secondary', 'released', inputs, host)
            member.goal_key = None
        view = self.displayed(host)
        if view is None:
            return
        goal = self.goal_for(member, view, now)
        key = goal[:2] if goal else None
        if key != member.goal_key:
            member.goal_key, member.goal_since = key, now
            if member.tool and (goal is None or goal[3] != 'tool' or key != getattr(member, 'tool_key', None)):
                self.edge(member, 'secondary', 'released', inputs, host)
        reacting = goal is not None and now - member.goal_since >= member.human.reaction_s
        desired = None
        if reacting:
            desired, radius = self.target_dir(goal, view)
            # How fast the chosen target moves across the dome, and how large it is.
            previous = getattr(member, 'tracked', None)
            if previous and previous[0] == key and previous[1] != view['frame']['id'] and now > previous[3]:
                self.target_motion.append((angle_deg(previous[2], desired) / (now - previous[3]), radius))
            member.tracked = (key, view['frame']['id'], desired, now)
        pointing = member.human.step(desired, dt)
        point = uv(self.projection, pointing)
        member.aim_seq += 1
        sample = host - 1000
        inputs.append(self.message(member, {'type': 'aim', 'sequence': member.aim_seq,
                                            'time': {'evaluatedAtHostUs': sample, 'uncertaintyUs': 2000},
                                            'projectionContextId': CONTEXT, 'ray': RAY}, point))
        member.poses.append((sample, point))
        if member.primary and now >= member.release_primary_at:
            self.edge(member, 'primary', 'released', inputs, host)
        if not reacting or goal[3] is None:
            return
        drawn = self.drawn_reticle(member, view)
        if drawn is None:
            return
        on_target = angle_deg(direction(self.projection, *drawn), desired) <= radius
        # How often a reacting member's drawn reticle is on its target.
        self.stats['aimTicks'] = self.stats.get('aimTicks', 0) + 1
        self.stats['onTargetTicks'] = self.stats.get('onTargetTicks', 0) + on_target
        if goal[3] == 'primary' and on_target and not member.primary and now - member.last_shot >= self.args.fire_interval_ms / 1000:
            if self.rng.random() < dt / member.human.click_delay_s:  # motor delay, memoryless
                self.edge(member, 'primary', 'pressed', inputs, host, point)
                member.last_shot, member.release_primary_at = now, now + .08
        if goal[3] == 'tool':
            if not member.tool and on_target:
                self.edge(member, 'secondary', 'pressed', inputs, host, point)
                member.tool_since, member.tool_key = now, key
            elif member.tool and goal[0] == 'node' and goal[1] == 'boost' and now - member.tool_since > .7:
                self.edge(member, 'secondary', 'released', inputs, host)
            elif member.tool and goal[0] == 'rival' and now - member.tool_since > .8 and \
                    goal[2].get('phase') not in ('acquiring', 'breached'):
                # The beam did not take (missed, occluded, ambiguous): let go and
                # press again once back on target, as a person would.
                self.edge(member, 'secondary', 'released', inputs, host)
                self.stats['beamRetries'] = self.stats.get('beamRetries', 0) + 1

    def edge(self, member, control, edge, inputs, host, point=None):
        member.action_seq += 1
        member.events += 1
        fields = {'type': 'action', 'sequence': member.action_seq, 'eventId': f'{member.source}-{member.events}-{secrets.token_hex(3)}',
                  'control': control, 'edge': edge, 'time': {'evaluatedAtHostUs': host - 3000, 'uncertaintyUs': 2000},
                  'projectionContextId': CONTEXT}
        if edge == 'pressed':
            fields['rayAtEvent'] = RAY
        if point is None and member.poses:
            point = member.poses[-1][1]
        inputs.append(self.message(member, fields, point))
        if control == 'primary':
            member.primary = edge == 'pressed'
        else:
            member.tool = edge == 'pressed'

    def run(self, seconds):
        poller = threading.Thread(target=self.poll_views, daemon=True)
        poller.start()
        self.wait_view()
        self.post([])
        self.api.call('/operator/start', {})
        self.api.call('/operator/countdown', dict(skip=True))
        # Rearming counts only once gameplay is active: every source starts with
        # an UP edge of both controls after the start, as a person would.
        self.wait_active(15)
        self.rearm()
        start = last = self.last_state = self.last_renew = self.race_started = time.monotonic()
        try:
            while time.monotonic() - start < seconds:
                now = time.monotonic()
                dt, last = now - last, now
                self.stats['maxLoopGapS'] = max(self.stats.get('maxLoopGapS', 0), round(dt, 3))
                inputs = []
                for member in self.members:
                    self.update_member(member, now, dt, inputs)
                self.post(inputs)
                if now - self.last_state > .5:
                    self.last_state = now
                    state = self.api.call('/state')
                    if state.get('arcade_state') in ('results', 'finished'):
                        break
                    session = state.get('arcade_session') or {}
                    if state.get('arcade_state') == 'lap_handoff' and session.get('handoff_countdown', state.get('handoff_countdown')) == 0:
                        self.handoff(state)
                        last = time.monotonic()
        finally:
            self.stop.set()
            poller.join(timeout=5)


class Api:
    def __init__(self, base, token):
        self.base, self.token = base.rstrip('/'), token
        # Per-route round trips: a slow reply is the usual cause of a lapsed grant.
        self.max_ms, self.slow = {}, []

    def call(self, path, body=None, expect=(200,)):
        data = None if body is None else json.dumps(body).encode()
        request = urllib.request.Request(self.base + path, data=data, method='GET' if body is None else 'POST',
                                         headers={'Authorization': 'Bearer ' + self.token, 'Content-Type': 'application/json'})
        started = time.monotonic()
        try:
            response = urllib.request.urlopen(request, timeout=20)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            status, raw = response.status, response.read()
        elapsed = (time.monotonic() - started) * 1000
        route = path.split('?')[0]
        self.max_ms[route] = max(self.max_ms.get(route, 0), round(elapsed, 1))
        if elapsed > SLOW_CALL_MS:
            self.slow.append((round(time.monotonic(), 3), route, round(elapsed)))
        value = json.loads(raw or b'{}')
        if expect and status not in expect:
            raise RuntimeError(f'{path} -> {status}: {value}')
        return value


def free_port():
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


def launch(args, token, port):
    repo = Path(__file__).resolve().parents[3]
    runtime = Path(args.runtime_dir)
    runtime.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, RELAY_BUILD_DIR=str(args.build_dir), SUPERTUXKART_SAVEDIR=str(runtime),
               RELAY_API_TOKEN=token, RELAY_API_PORT=str(port), RELAY_PLAYER_COUNT=str(args.crew + 1),
               RELAY_PROJECTION=args.projection, STK_RELAY_SIMULATION='1', STK_RELAY_AUTOPILOT='1',
               STK_RELAY_TELEMETRY_DIR=str(args.telemetry_dir), TMPDIR=str(runtime))
    if args.assets_dir:
        env['SUPERTUXKART_ASSETS_DIR'] = str(args.assets_dir)
    if args.tuning:
        env['STK_RELAY_SCINTILLA_TUNING'] = str(Path(args.tuning).resolve())
    log = open(runtime / 'producer.log', 'w')
    command = ['zsh', str(repo / 'tools/immersive/RunRelayTholusOS.command'), '-N', '--no-sound',
               f'--syphon-server-name=Scintilla Crew Simulator {port}']
    return subprocess.Popen(command, cwd=repo, env=env, stdout=log, stderr=subprocess.STDOUT), log


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument('--base-url', help='attach to a running producer, e.g. http://127.0.0.1:8890/immersive/v1')
    target.add_argument('--launch', action='store_true', help='start a producer with autopilot and simulation view')
    parser.add_argument('--token', default=os.environ.get('STK_RELAY_TOKEN', ''))
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--assets-dir', type=Path)
    parser.add_argument('--runtime-dir', type=Path, default=Path(os.environ.get('TMPDIR', '/tmp')) / 'scintilla-crew-sim')
    parser.add_argument('--telemetry-dir', type=Path, default=Path(os.environ.get('TMPDIR', '/tmp')) / 'scintilla-crew-sim' / 'telemetry')
    parser.add_argument('--tuning', type=Path, help='tuning profile for the launched producer')
    parser.add_argument('--projection', default='fisheye_domemaster', choices=['fisheye_domemaster', 'equirectangular'])
    parser.add_argument('--track', default='snowtuxpeak')
    parser.add_argument('--crew', type=int, default=2, choices=range(1, 6))
    parser.add_argument('--ai-karts', type=int, default=3)
    parser.add_argument('--difficulty', default='easy')
    parser.add_argument('--seconds', type=float, default=180)
    parser.add_argument('--display-latency-ms', type=float, default=45)
    parser.add_argument('--reaction-ms', type=float, default=300)
    parser.add_argument('--pursuit-ms', type=float, default=120)
    parser.add_argument('--tremor-deg', type=float, default=.35)
    parser.add_argument('--fire-interval-ms', type=float, default=260)
    parser.add_argument('--seed', type=int, default=1)
    args = parser.parse_args(argv)
    process = log = None
    if args.launch:
        if not args.build_dir:
            parser.error('--launch needs --build-dir')
        token, port = 'sim-' + uuid.uuid4().hex, free_port()
        process, log = launch(args, token, port)
        base = f'http://127.0.0.1:{port}/immersive/v1'
    else:
        token, base = args.token, args.base_url
    api = Api(base, token)
    try:
        deadline = time.monotonic() + 120
        while True:
            if process and process.poll() is not None:
                raise RuntimeError(f'producer exited with {process.returncode}')
            try:
                api.call('/state')
                break
            except (urllib.error.URLError, ConnectionError, TimeoutError):
                if time.monotonic() > deadline:
                    raise RuntimeError('producer API unavailable')
                time.sleep(.5)
        simulation = Simulation(api, args)
        simulation.setup()
        failure = None
        try:
            simulation.run(args.seconds)
        except RuntimeError as error:
            failure = str(error)  # still report what the session measured
        telemetry = api.call('/scintilla/tuning').get('telemetry', {})
        # Finish, then reset: the new race generation closes the telemetry file
        # with its footer before the producer is stopped.
        for path in ('/operator/force-finish', '/operator/reset'):
            try:
                api.call(path, {}, expect=None)
            except Exception:
                pass
        def q(values, f):
            values = sorted(values)
            return round(values[min(len(values) - 1, int(len(values) * f))], 1) if values else None
        captures = simulation.capture_us
        span = (captures[-1] - captures[0]) / 1e6 if len(captures) > 1 else 0
        gaps = [(b - a) / 1000 for a, b in zip(captures, captures[1:])]
        print(json.dumps({'failure': failure, 'updateRoundTripMsMax': round(max(simulation.update_ms), 1) if simulation.update_ms else None,
                          'producerFramesPerSecondSeen': round((len(captures) - 1) / span, 1) if span else None,
                          'frameIntervalMsP50': q(gaps, .5), 'frameIntervalMsP90': q(gaps, .9),
                          'updateRoundTripMsP50': q(simulation.update_ms, .5), 'updateRoundTripMsP90': q(simulation.update_ms, .9),
                          # TholusOS treats an update reply slower than 50 ms as invalid and drops the session.
                          'updatesOver50Ms': sum(1 for v in simulation.update_ms if v > 50),
                          'clockProbesOver20Ms': simulation.stats.get('clockProbesOver20Ms', 0),
                          'firstUpdateOver50MsS': simulation.stats.get('firstUpdateOver50MsS'),
                          'renewals': simulation.stats.get('renewals'), 'maxRenewGapS': simulation.stats.get('maxRenewGapS'),
                          'handoffs': simulation.stats.get('handoffs', 0),
                          'inlineRenewals': simulation.stats.get('inlineRenewals', 0),
                          'frameIntervalMsMax': round(max(gaps), 1) if gaps else None,
                          'telemetry': telemetry, 'crew': [m.participant for m in simulation.members],
                          'updates': simulation.stats['updates'], 'rejectedInputs': simulation.stats['rejected'],
                          'rejectionReasons': simulation.stats['reasons'],
                          'maxLoopGapS': simulation.stats.get('maxLoopGapS'),
                          'beamRetries': simulation.stats.get('beamRetries', 0),
                          'aimTicks': simulation.stats.get('aimTicks', 0), 'onTargetTicks': simulation.stats.get('onTargetTicks', 0),
                          'targetSpeedDegSP50': q([m[0] for m in simulation.target_motion], .5),
                          'targetSpeedDegSP90': q([m[0] for m in simulation.target_motion], .9),
                          'targetRadiusDegP50': q([m[1] for m in simulation.target_motion], .5),
                          'apiMaxMs': api.max_ms, 'slowCalls': len(api.slow), 'slowCallsFirst': api.slow[:10]}, indent=2))
        return 1 if failure else 0
    finally:
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                process.kill()
        if log:
            log.close()


if __name__ == '__main__':
    sys.exit(main())
