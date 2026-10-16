#!/usr/bin/env python3
"""Summarise Missione Scintilla session telemetry (tholus.stk-scintilla-telemetry/1).

Reads one or more NDJSON files written by STK and prints a Markdown report in
Italian; --json writes the same metrics for comparing tuning profiles. The
records are balancing observations, never optical, hardware or venue proof.
"""
import argparse
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

SCHEMA = 'tholus.stk-scintilla-telemetry/1'
US = 1_000_000


class TelemetryError(ValueError):
    pass


def read_session(path):
    records = []
    with open(path, encoding='utf-8') as stream:
        for number, line in enumerate(stream, 1):
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as error:
                raise TelemetryError(f'{path}:{number}: invalid JSON ({error.msg})') from None
            if not isinstance(record, dict) or not isinstance(record.get('type'), str):
                raise TelemetryError(f'{path}:{number}: record without type')
            records.append(record)
    if not records or records[0]['type'] != 'header' or records[0].get('schema') != SCHEMA:
        raise TelemetryError(f'{path}: missing {SCHEMA} header')
    return records


def quantile(values, q):
    values = sorted(v for v in values if v is not None and math.isfinite(v))
    if not values:
        return None
    position = (len(values) - 1) * q
    low, high = math.floor(position), math.ceil(position)
    return values[low] + (values[high] - values[low]) * (position - low)


def share(part, whole):
    return part / whole if whole else None


def summarise(records, path=''):
    header = records[0]
    by_type = defaultdict(list)
    for record in records:
        by_type[record['type']].append(record)
    times = [r['t'] for r in records if isinstance(r.get('t'), int)]
    duration = (max(times) - min(times)) / US if len(times) > 1 else 0.0
    footer = by_type['footer'][-1] if by_type['footer'] else None
    closes = by_type['close']

    # Aim: resolved presses (rival/capsule, primary) and admission notes.
    presses = [r for r in by_type['input'] if r.get('control') == 'primary']
    results = Counter(r.get('result', '') for r in presses)
    per_participant = Counter(r.get('participant', '') for r in presses)
    automatic = sum(1 for r in presses if r.get('automatic'))
    notes = Counter((n.get('kind', ''), n.get('reason', '')) for n in by_type['input_note'])
    hits, misses = [], []
    for record in presses:
        nearest = record.get('nearest')
        if not isinstance(nearest, dict):
            continue
        (hits if record.get('result') == 'hit' else misses).append(nearest)
    near_miss = sum(1 for n in misses if n.get('edgeDeg') is not None and n['edgeDeg'] <= n.get('angularRadiusDeg', 0))
    bands = {}
    for label, low, high in (('0-10 m', 0, 10), ('10-20 m', 10, 20), ('20+ m', 20, 1e9)):
        band_hits = [n for n in hits if low <= n.get('distanceM', -1) < high]
        band_misses = [n for n in misses if low <= n.get('distanceM', -1) < high]
        bands[label] = {
            'hits': len(band_hits), 'misses': len(band_misses),
            'hitRate': share(len(band_hits), len(band_hits) + len(band_misses)),
            'medianAngularRadiusDeg': quantile([n.get('angularRadiusDeg') for n in band_hits + band_misses], .5),
        }
    aim = {
        'presses': len(presses), 'automatic': automatic,
        # Secondary (beam/tool) presses resolved on a target.
        'toolPresses': sum(1 for r in by_type['input'] if r.get('control') == 'secondary'),
        'perParticipant': dict(per_participant),
        'results': dict(results),
        'hitRate': share(results.get('hit', 0), len(presses)),
        'admissionRejections': {f'{k}:{r}': c for (k, r), c in notes.items()},
        'missEdgeDegP50': quantile([n.get('edgeDeg') for n in misses], .5),
        'missEdgeDegP90': quantile([n.get('edgeDeg') for n in misses], .9),
        'nearMissShare': share(near_miss, len(misses)),
        'poseLagDegP50': quantile([r.get('poseLagDeg') for r in presses], .5),
        'poseLagDegP90': quantile([r.get('poseLagDeg') for r in presses], .9),
        'byDistance': bands,
    }

    # Combat: core outcomes and target phase transitions.
    shot_reasons = Counter(r.get('reason', '') for r in by_type['combat_shot'])
    phases = Counter(r.get('phase', '') for r in by_type['combat'])
    breaches = expired = 0
    last_phase = {}
    for record in by_type['combat']:
        target, phase = record.get('target'), record.get('phase')
        previous = last_phase.get(target)
        if phase == 'breached' and previous != 'breached':
            breaches += 1
        if previous == 'breached' and phase == 'shielded':
            expired += 1
        last_phase[target] = phase
    staggers = sum(1 for r in by_type['combat_shot'] if r.get('effectApplied'))
    combat = {
        'shotOutcomes': dict(shot_reasons), 'staggers': staggers,
        'staggersPerMinute': staggers / (duration / 60) if duration else None,
        'breachesOpened': breaches, 'breachesExpired': expired,
        'acquisitions': phases.get('acquiring', 0),
    }

    # Defense: threats from warning to outcome.
    started = sum(1 for r in by_type['defense'] if r.get('phase') == 'await_warning')
    telegraphs = sum(1 for r in by_type['defense'] if r.get('phase') == 'telegraph')
    outcomes = Counter(r.get('reason', '') for r in by_type['defense'] if r.get('phase') == 'cooldown')
    samples = by_type['sample']
    energy = [s.get('energy') for s in samples]
    defense = {
        'threatsStarted': started, 'telegraphs': telegraphs, 'outcomes': dict(outcomes),
        'blocked': outcomes.get('blocked', 0), 'impacts': outcomes.get('impact_applied', 0),
        'cancelled': sum(c for reason, c in outcomes.items()
                         if reason not in ('blocked', 'impact_applied', 'miss', 'world_blocked')),
        'threatsPerMinute': started / (duration / 60) if duration else None,
        'boosts': len(by_type['boost']),
        'energyMin': min((e for e in energy if e is not None), default=None),
        'energyMean': (sum(e for e in energy if e is not None) / len(energy)) if energy else None,
        'humanTransientShare': share(sum(1 for s in samples if s.get('humanTransient')), len(samples)),
    }

    # Mission: capsules, tethers and the frozen outcome.
    entity_phases = defaultdict(set)
    for record in by_type['mission_entity']:
        entity_phases[(record.get('kind'), record.get('id'), record.get('instance'))].add(record.get('phase'))
    capsules = [p for (kind, _, _), p in entity_phases.items() if kind == 'capsule']
    tethers = [p for (kind, _, _), p in entity_phases.items() if kind == 'tether']
    # A detached module ends as secured again (recovered) or empty (lost).
    recovered = lost = 0
    previous_slots = None
    for record in by_type['cargo']:
        slots = record.get('slots') or []
        if previous_slots:
            for before, after in zip(previous_slots, slots):
                recovered += before == 'tethered' and after == 'secured'
                lost += before == 'tethered' and after == 'empty'
        previous_slots = slots
    result = by_type['mission_result'][-1] if by_type['mission_result'] else None
    mission = {
        'capsulesOffered': len(capsules),
        'capsulesOpened': sum(1 for p in capsules if 'open' in p),
        'capsulesCollected': sum(1 for p in capsules if 'collected' in p),
        'capsulesExpired': sum(1 for p in capsules if 'expired' in p and 'collected' not in p),
        'tethers': len(tethers), 'tethersRecovered': recovered, 'tethersLost': lost,
        'shotStatuses': dict(Counter(r.get('status', '') for r in by_type['mission_shot'])),
        'result': {k: result.get(k) for k in ('outcome', 'position', 'secured', 'natural', 'termination')} if result else None,
    }

    # Crew activity: is there something useful to do?
    active = [s for s in samples if s.get('gameplay')]
    def busy(s):
        return (s.get('attackable', 0) > 0 or s.get('capsules', 0) > 0
                or s.get('defense') not in ('idle', 'cooldown', None) or s.get('boostReady'))
    activity = {
        'samples': len(active),
        'attackableShare': share(sum(1 for s in active if s.get('attackable', 0) > 0), len(active)),
        'capsuleShare': share(sum(1 for s in active if s.get('capsules', 0) > 0), len(active)),
        'threatShare': share(sum(1 for s in active if s.get('defense') not in ('idle', 'cooldown', None)), len(active)),
        'nothingToDoShare': share(sum(1 for s in active if not busy(s)), len(active)),
        'pressesPerMinute': {p: c / (duration / 60) for p, c in per_participant.items()} if duration else {},
        # Pilot crashes and falls: each rescue stops the kart for about a second.
        'rescues': len(by_type['rescue']),
        'rescuesPerMinute': len(by_type['rescue']) / (duration / 60) if duration else None,
        'rescueTrackM': sorted(round(r['trackM']) for r in by_type['rescue'] if isinstance(r.get('trackM'), (int, float))),
    }

    # Latency probe: pose -> STK receipt -> capture -> presented by TholusOS.
    presented = by_type['latency_presented']
    latency = {
        'probes': len(by_type['latency_flash']), 'presented': len(presented),
        'poseToReceivedMsP50': quantile([(r['receivedUs'] - r['poseUs']) / 1000 for r in presented], .5),
        'receivedToCaptureMsP50': quantile([(r['captureUs'] - r['receivedUs']) / 1000 for r in presented], .5),
        'captureToPresentedMsP50': quantile([(r['presentedLowUs'] - r['captureUs']) / 1000 for r in presented], .5),
        'poseToPresentedMsP50': quantile([(r['presentedLowUs'] - r['poseUs']) / 1000 for r in presented], .5),
        'poseToPresentedMsP90': quantile([(r['presentedLowUs'] - r['poseUs']) / 1000 for r in presented], .9),
        'framePresentationP50Ms': quantile([r.get('captureToPresentedP50Ms') for r in by_type['presentation']], .5),
        'framePresentationP90Ms': quantile([r.get('captureToPresentedP90Ms') for r in by_type['presentation']], .9),
    }
    # Main-loop stalls: STK serves relay input between frames, so a long frame
    # delays aims and clock renewals too (the input grant lapses after 1 s).
    frames = [s['frameMaxMs'] for s in samples if isinstance(s.get('frameMaxMs'), (int, float))]
    latency.update({
        'frameMaxMsP50': quantile(frames, .5), 'frameMaxMs': max(frames) if frames else None,
        'stallsOver250Ms': sum(1 for f in frames if f > 250), 'stallsOver600Ms': sum(1 for f in frames if f > 600),
    })
    latency['depthSkipped'] = sum(s.get('depthSkippedPinned', 0) + s.get('depthSkippedBudget', 0) + s.get('depthInvalid', 0)
                                  for s in samples)
    # Explicit stall rows are written every frame, also after the grant lapsed.
    stalls = [r['ms'] for r in by_type['stall'] if isinstance(r.get('ms'), (int, float))]
    if stalls:
        latency['frameMaxMs'] = max([latency['frameMaxMs'] or 0] + stalls)
        latency['stallsOver250Ms'] = max(latency['stallsOver250Ms'], len(stalls))
        latency['stallsOver600Ms'] = max(latency['stallsOver600Ms'], sum(1 for f in stalls if f > 600))
    return {
        'file': str(path), 'track': header.get('track'), 'raceEpoch': header.get('raceEpoch'),
        'startedAtUtc': header.get('startedAtUtc'), 'crew': header.get('crew'),
        'tuningProfile': header.get('tuningProfile'), 'tuningSha256': header.get('tuningSha256'),
        'durationS': duration, 'records': len(records),
        'dropped': footer.get('dropped') if footer else None, 'complete': footer is not None,
        'closeReason': closes[-1].get('reason') if closes else None,
        'aim': aim, 'combat': combat, 'defense': defense, 'mission': mission,
        'activity': activity, 'latency': latency,
    }


def percent(value):
    return '—' if value is None else f'{value * 100:.0f}%'


def decimal(value, digits=1):
    return '—' if value is None else f'{value:.{digits}f}'


def counts(mapping):
    return ', '.join(f'{k or "?"} {v}' for k, v in sorted(mapping.items(), key=lambda kv: -kv[1])) or '—'


def markdown(summary):
    a, c, d, m, x, l = (summary[k] for k in ('aim', 'combat', 'defense', 'mission', 'activity', 'latency'))
    crew = ', '.join(summary.get('crew') or []) or '—'
    out = [
        f"## {summary['file']}",
        '',
        f"Pista **{summary['track']}**, gara {summary['raceEpoch']}, durata {decimal(summary['durationS'], 0)} s, "
        f"equipaggio {crew}. Profilo **{summary['tuningProfile']}** "
        f"(`{(summary['tuningSha256'] or 'builtin')[:12]}`). "
        + ('' if summary['complete'] else '**File incompleto (manca il footer).** ')
        + (f"Righe perse: {summary['dropped']}." if summary['dropped'] else ''),
        '',
        '| Area | Valori |',
        '|---|---|',
        f"| Colpi | {a['presses']} (di cui automatici {a['automatic']}), centrati {percent(a['hitRate'])}; esiti: {counts(a['results'])}; fascio/attrezzo premuto {a['toolPresses']} volte |",
        f"| Rifiuti all'ingresso | {counts(a['admissionRejections'])} |",
        f"| Mancati | scarto dal bordo mediano {decimal(a['missEdgeDegP50'])}°, p90 {decimal(a['missEdgeDegP90'])}°; quasi centrati {percent(a['nearMissShare'])} |",
        f"| Ritardo reticolo | posa vs reticolo mostrato: mediana {decimal(a['poseLagDegP50'])}°, p90 {decimal(a['poseLagDegP90'])}° |",
        f"| Combattimento | brecce aperte {c['breachesOpened']}, scadute {c['breachesExpired']}, sobbalzi {c['staggers']} ({decimal(c['staggersPerMinute'])}/min); esiti: {counts(c['shotOutcomes'])} |",
        f"| Difesa | minacce {d['threatsStarted']} ({decimal(d['threatsPerMinute'])}/min), parate {d['blocked']}, impatti {d['impacts']}, annullate {d['cancelled']}; esiti: {counts(d['outcomes'])} |",
        f"| Energia | minimo {decimal(d['energyMin'], 0)}, media {decimal(d['energyMean'], 0)}; turbo {d['boosts']}; kart in boost/salto {percent(d['humanTransientShare'])} del tempo |",
        f"| Carico | capsule offerte {m['capsulesOffered']}, aperte {m['capsulesOpened']}, raccolte {m['capsulesCollected']}, scadute {m['capsulesExpired']}; moduli sganciati {m['tethers']}, recuperati {m['tethersRecovered']}, persi {m['tethersLost']} |",
        f"| Esito | {m['result'] or '—'} |",
        f"| Attività crew | rivali colpibili {percent(x['attackableShare'])}, capsule {percent(x['capsuleShare'])}, minacce {percent(x['threatShare'])}, **niente da fare {percent(x['nothingToDoShare'])}** |",
        f"| Pilota | salvataggi {x['rescues']} ({decimal(x['rescuesPerMinute'])}/min) |",
        f"| Latenza (lampi) | {l['presented']}/{l['probes']} presentati; posa→schermo mediana {decimal(l['poseToPresentedMsP50'])} ms, p90 {decimal(l['poseToPresentedMsP90'])} ms (posa→STK {decimal(l['poseToReceivedMsP50'])}, STK→cattura {decimal(l['receivedToCaptureMsP50'])}, cattura→schermo {decimal(l['captureToPresentedMsP50'])}) |",
        f"| Latenza (tutti i fotogrammi) | cattura→presentazione mediana {decimal(l['framePresentationP50Ms'])} ms, p90 {decimal(l['framePresentationP90Ms'])} ms |",
        f"| Fluidità STK | frame più lungo per campione: mediana {decimal(l['frameMaxMsP50'])} ms, massimo {decimal(l['frameMaxMs'])} ms; blocchi >250 ms {l['stallsOver250Ms']}, >600 ms {l['stallsOver600Ms']}; fotogrammi senza profondità {l['depthSkipped']} |",
        '',
        '| Distanza | Centrati | Mancati | % | Raggio angolare mediano |',
        '|---|---:|---:|---:|---:|',
    ]
    for band, values in a['byDistance'].items():
        out.append(f"| {band} | {values['hits']} | {values['misses']} | {percent(values['hitRate'])} | {decimal(values['medianAngularRadiusDeg'])}° |")
    out.append('')
    return '\n'.join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('files', nargs='+', type=Path)
    parser.add_argument('--json', type=Path, help='write all metrics as JSON')
    args = parser.parse_args(argv)
    summaries = []
    for path in args.files:
        try:
            summaries.append(summarise(read_session(path), path))
        except (OSError, TelemetryError) as error:
            print(f'error: {error}', file=sys.stderr)
            return 1
    print('# Missione Scintilla — rapporto di telemetria\n')
    print('Osservazioni del gioco per il bilanciamento: non sono prove ottiche, hardware o di accettazione in sala.\n')
    for summary in summaries:
        print(markdown(summary))
    if args.json:
        args.json.write_text(json.dumps({'schema': 'tholus.stk-scintilla-telemetry-report/1', 'sessions': summaries},
                                        indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
