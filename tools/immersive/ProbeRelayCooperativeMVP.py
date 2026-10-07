#!/usr/bin/env python3
"""Future live acceptance probe for a dedicated cooperative MVP producer.

Does not build or launch STK. It mutates the supplied spare producer's session.
Run only after runtime verification is authorized. Natural driving, physical
input and dome projection still require the venue playtest.
"""
import argparse
import json
import time
import urllib.error
import urllib.request
import uuid

ROLES = {'navigator', 'engineer', 'defender', 'hacker', 'gunner'}
ORDINARY = {'nav_select_route', 'nav_call_pace_note', 'engineer_charge_boost',
            'engineer_release_boost', 'engineer_dump_heat', 'defender_raise_shield',
            'defender_cleanse_status', 'hacker_open_gate', 'hacker_disable_trap',
            'gunner_fire_utility', 'gunner_emp_zone', 'gunner_drop_decoy'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='http://127.0.0.1:18890/immersive/v1')
    parser.add_argument('--token', required=True)
    args = parser.parse_args()

    def call(path, body=None, status=200):
        req = urllib.request.Request(args.base.rstrip('/') + path,
            data=None if body is None else json.dumps(body).encode(),
            headers={'X-STK-Relay-Token': args.token, 'Content-Type': 'application/json'},
            method='GET' if body is None else 'POST')
        try:
            response = urllib.request.urlopen(req, timeout=40)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            result = json.load(response)
            assert response.code == status, (path, response.code, result)
            return result

    def state():
        return call('/state')

    def wait_for(predicate, seconds=8):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            value = state()
            if predicate(value):
                return value
            time.sleep(0.1)
        raise AssertionError(state())

    def presence(ids):
        return call('/crew/presence', {'session_id': state()['session_id'],
                                     'connected_player_ids': ids})

    def roles_by_player(value):
        return {item['player_id']: set(item['roles']) for item in value['crew_assignments']}

    def crew_action(player, action_type):
        value = state()
        item = next(action for phone in value['phone_states'] if phone['player_id'] == player
                    for action in phone['actions'] if action['type'] == action_type)
        payload = {'amount': item['amount']}
        if item['target_key']:
            payload[item['target_key']] = item['target_id']
        if item['mode_key']:
            payload[item['mode_key']] = item['mode']
        nonce = str(uuid.uuid4())
        body = dict(session_id=value['session_id'], player_id=player,
                    role=item['role'], type=action_type, payload=payload,
                    state_version_seen=value['state_version'],
                    action_id=nonce, idempotency_key=nonce)
        return call('/crew/action', body), body

    features = set(call('/capabilities')['feature_flags'])
    assert {'crew_presence', 'pilot_ready', 'continuous_crew_actions', 'input_devices'} <= features
    assert any(kart['id'] == 'tux' for kart in call('/karts')['karts'])
    assert call('/input/devices')['ok'] and call('/input/state')['ok']
    call('/operator/reset', {})

    for count in range(1, 7):
        ids = [f'p{i}' for i in range(1, count + 1)]
        config = dict(track_id='snowtuxpeak', kart_id='tux', difficulty='easy',
            ai_karts=3, player_count=count, crew_mode='enabled', output='syphon',
            handoff_operator_confirm=True, auto_confirm_handoff=False,
            handoff_seconds=10, min_handoff_seconds=6, countdown_seconds=3,
            music_enabled=True, sfx_enabled=True, music_volume=0.7, sfx_volume=0.8)
        call('/operator/configure', config)
        value = state()
        assert value['phone_states'] == []
        assert not any(roles_by_player(value).values())
        for key in ('handoff_seconds', 'min_handoff_seconds', 'music_enabled',
                    'sfx_enabled', 'music_volume', 'sfx_volume'):
            assert abs(value['arcade_session']['config'][key] - config[key]) < 0.001
        call('/crew/presence', {'session_id': 'obsolete', 'connected_player_ids': ids}, 409)
        call('/crew/presence', {'session_id': value['session_id'], 'connected_player_ids': ['p7']}, 409)
        presence(['p1'])
        value = state()
        assert set.union(set(), *roles_by_player(value).values()) == (ROLES if count == 1 else set())
        presence(ids)
        value = state()
        assigned = roles_by_player(value)
        assert set.union(set(), *assigned.values()) == ROLES
        assert sum(map(len, assigned.values())) == 5
        if count > 1:
            assert not assigned['p1']
            presence(['p2'])
            assigned = roles_by_player(state())
            assert assigned['p2'] == ROLES
            assert sum(map(len, assigned.values())) == 5
            presence(ids)
        exposed = {action['type'] for phone in state()['phone_states'] for action in phone['actions']}
        assert ORDINARY <= exposed
        call('/operator/ready', {'player_id': 'p2' if count == 1 else 'p3'}, 409)
        call('/operator/ready', {'player_id': 'p1'})
        assert state()['arcade_state'] == 'countdown'
        running = wait_for(lambda value: value['arcade_state'] == 'running')
        engineer = next(player for player, roles in roles_by_player(running).items() if 'engineer' in roles)
        before_encounters = running['encounters_succeeded']
        accepted, body = crew_action(engineer, 'engineer_charge_boost')
        assert accepted['accepted'] and not accepted['duplicate']
        duplicate = call('/crew/action', body)
        assert duplicate['duplicate'] and duplicate['score_delta'] == 0
        assert state()['engineer_boost_charge'] > 0
        time.sleep(0.3)
        accepted, _ = crew_action(engineer, 'engineer_release_boost')
        assert accepted['accepted'] and state()['engineer_boost_charge'] == 0
        assert state()['encounters_succeeded'] == before_encounters
        presence([])
        assert state()['phone_states'] == []
        presence(ids)
        # Reconnection must not replenish the role's action cooldown.
        release = next(action for phone in state()['phone_states'] if phone['player_id'] == engineer
                       for action in phone['actions'] if action['type'] == 'engineer_release_boost')
        assert release['cooldown_ms'] > 0
        if count > 1:
            call('/operator/handoff', {'player_id': 'p2'})
            value = wait_for(lambda value: value['arcade_state'] == 'lap_handoff')
            frozen = value['world_elapsed_ms']
            time.sleep(13)
            value = state()
            assert value['arcade_state'] == 'lap_handoff' and value['world_elapsed_ms'] == frozen
            call('/operator/ready', {'player_id': 'p1'}, 409)
            call('/operator/ready', {'player_id': 'p2'})
            time.sleep(1)
            assert state()['arcade_state'] == 'lap_handoff'
            value = wait_for(lambda value: value['arcade_state'] == 'running')
            assert value['active_pilot_id'] == 'p2'
            assert not roles_by_player(value)['p2']
        call('/operator/pause', {})
        paused = state()
        old_score = paused['crew_score']
        call('/operator/ready', {})
        assert state()['arcade_state'] == 'countdown'
        resumed = wait_for(lambda value: value['arcade_state'] == 'running')
        assert resumed['crew_score'] == old_score and resumed['current_lap'] == paused['current_lap']
        call('/operator/force-finish', {})
        assert state()['arcade_state'] == 'results'
        call('/operator/ready', {})
        assert state()['arcade_state'] == 'countdown' and state()['world_elapsed_ms'] == 0
        call('/operator/reset', {})
        print(f'[ok] {count} participants: presence, roles, turbo, readiness, recovery, replay')


if __name__ == '__main__':
    main()
