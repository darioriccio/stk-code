#!/usr/bin/env python3
"""Exercise a dedicated running MVP producer; no compilation or simulated progress.

Launch it on a spare port first. This probe resets its session and loads three
real worlds. It checks operator completion; natural driving remains a playtest.
"""
import argparse
import json
import time
import urllib.error
import urllib.request


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--base', default='http://127.0.0.1:18890/immersive/v1')
    parser.add_argument('--token', required=True)
    args = parser.parse_args()

    def call(path, body=None, status=200):
        request = urllib.request.Request(
            args.base.rstrip('/') + path,
            data=None if body is None else json.dumps(body).encode(),
            headers={'X-STK-Relay-Token': args.token, 'Content-Type': 'application/json'},
            method='GET' if body is None else 'POST')
        try:
            response = urllib.request.urlopen(request, timeout=40)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            result = json.load(response)
            assert response.code == status, (path, response.code, result)
            return result

    state = lambda: call('/state')
    assert call('/capabilities')['player_count']['min'] == 1
    tracks = {track['id'] for track in call('/tracks')['tracks']}
    assert tracks == {'snowtuxpeak', 'lighthouse', 'olivermath'}, tracks
    call('/operator/reset', {})
    for track in ('snowtuxpeak', 'lighthouse', 'olivermath'):
        config = dict(track_id=track, player_count=1, ai_karts=3, difficulty='easy',
                      crew_mode='enabled', output='syphon', countdown_seconds=2)
        call('/operator/configure', config)
        ready = state()
        assert ready['loaded_track_id'] == track and ready['race_attached'], ready
        assert ready['arcade_state'] == 'ready_for_players', ready
        assert ready['arcade_session']['input_locked'], ready
        time.sleep(0.4)
        assert state()['world_elapsed_ms'] == ready['world_elapsed_ms'] == 0
        for bad in (dict(track_id='missing-map'), dict(player_count=0),
                    dict(player_count=7), dict(player_count=1.5), dict(ai_karts=-1)):
            error = call('/operator/configure', dict(config, **bad), status=400)
            assert not error['ok']
            assert state()['loaded_track_id'] == track
        call('/crew/presence', {'session_id': ready['session_id'], 'connected_player_ids': ['p1']})
        call('/operator/start', {})
        briefing = state()
        assert briefing['arcade_state'] == 'briefing', briefing
        assert briefing['arcade_session']['input_locked'], briefing
        call('/operator/countdown', {'skip': False})
        assert state()['arcade_state'] == 'countdown'
        deadline = time.monotonic() + 10
        while state()['arcade_state'] == 'countdown' and time.monotonic() < deadline:
            time.sleep(0.1)
        running = state()
        assert running['arcade_state'] in ('running', 'final_lap'), running
        assert running['player_count'] == 1 and not running['arcade_session']['handoff_active']
        assert any(phone['player_id'] == 'p1' for phone in running['phone_states'])
        # A phone is optional, but its p1 support action must really be accepted.
        available = [(phone, action) for phone in running['phone_states']
                     for action in phone['actions'] if action['available']]
        assert available, running['phone_states']
        phone, action = available[0]
        payload = {'amount': action['amount']}
        if action['target_key']:
            payload[action['target_key']] = action['target_id']
        if action['mode_key']:
            payload[action['mode_key']] = action['mode']
        crew = dict(session_id=running['session_id'], player_id='p1',
                    role=action['role'], type=action['type'], payload=payload,
                    state_version_seen=running['state_version'],
                    idempotency_key=track + '-crew', action_id=track + '-crew')
        ack = call('/crew/action', crew)
        assert ack['accepted'] and ack['score_delta'] > 0, ack
        duplicate = call('/crew/action', crew)
        assert duplicate['duplicate'] and duplicate['score_delta'] == 0, duplicate
        deadline = time.monotonic() + 10
        while state()['world_elapsed_ms'] < 100 and time.monotonic() < deadline:
            time.sleep(0.1)
        assert state()['world_elapsed_ms'] >= 100, state()
        call('/operator/pause', {})
        paused = state()['world_elapsed_ms']
        time.sleep(0.4)
        assert state()['world_elapsed_ms'] == paused
        call('/operator/resume', {})
        call('/operator/force-finish', {})
        assert state()['arcade_state'] == 'results', state()
        # Direct replay must reset the physical world before briefing.
        call('/operator/start', {})
        replay = state()
        assert replay['arcade_state'] == 'briefing' and replay['world_elapsed_ms'] == 0, replay
        call('/operator/reset', {})
        assert state()['world_elapsed_ms'] == 0 and state()['current_lap'] == 1
        print(f'[ok] {track}: real load, held preparation, countdown, race, pause, completion, replay')
    call('/operator/configure', dict(config, player_count=2, crew_mode='disabled'))
    call('/operator/start', {})
    call('/operator/countdown', {'skip': True})
    assert state()['player_count'] == 2 and state()['phone_states'] == []
    call('/operator/reset', {})
    print('[ok] two pilots can start without phones; crew disabled publishes no actions')


if __name__ == '__main__':
    main()
