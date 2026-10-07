#!/usr/bin/env python3
"""Input/API regression probe, written for the next authorized runtime phase.

Use a dedicated, paused or unconfigured producer. Does not build or launch STK.
Real steering/pedals, button edges and disconnect recovery need physical checks.
"""
import argparse
import json
import urllib.error
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='http://127.0.0.1:18890/immersive/v1')
    parser.add_argument('--token', required=True)
    args = parser.parse_args()

    def call(path, body=None, expected=200, headers=None):
        request = urllib.request.Request(args.base.rstrip('/') + path,
            data=None if body is None else json.dumps(body).encode(),
            headers=headers if headers is not None else {
                'X-STK-Relay-Token': args.token, 'Content-Type': 'application/json'},
            method='GET' if body is None else 'POST')
        try:
            response = urllib.request.urlopen(request, timeout=40)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            result = json.load(response)
            assert response.code == expected, (path, response.code, result.get('error'))
            return result

    devices = call('/input/devices')
    assert any(device['id'] == 'keyboard' and device['connected'] for device in devices['devices'])
    original = call('/input/state')['configuration']
    # Exact headers only: a token prefix, unrelated header or body must not authenticate.
    call('/input/state', expected=401, headers={'X-STK-Relay-Token': args.token + '-suffix'})
    call('/input/state', expected=401, headers={'X-Unrelated': 'x-stk-relay-token: ' + args.token})
    call('/input/config', {'note': 'x-stk-relay-token: ' + args.token}, 401, headers={'Content-Type': 'application/json'})
    assert call('/input/state', headers={'Authorization': 'Bearer ' + args.token})['ok']

    keyboard = dict(original, device_id='keyboard')
    try:
        applied = call('/input/config', keyboard)
        assert applied['configuration'] == keyboard and applied['connected']
        for change, reason in [({'deadzone': 0.8}, 'invalid_input_deadzone'),
                               ({'deadzone': -0.01}, 'invalid_input_deadzone'),
                               ({'ready_button': 'zero'}, 'invalid_ready_button'),
                               ({'accelerator_inverted': 1}, 'invalid_accelerator_inverted'),
                               ({'device_id': 'missing-device'}, 'input_device_unavailable')]:
            rejected = call('/input/config', dict(keyboard, **change), 400)
            assert rejected['error'] == reason, rejected['error']
            assert call('/input/state')['configuration'] == keyboard
        for device in devices['devices']:
            if device['kind'] != 'joystick' or not device['connected']:
                continue
            rejected = call('/input/config', dict(keyboard, device_id=device['id'],
                            steering_axis=device['axis_count']), 400)
            assert rejected['error'] == 'invalid_input_binding'
            rejected = call('/input/config', dict(keyboard, device_id=device['id'],
                            steering_axis=0, accelerator_axis=0), 400)
            assert rejected['error'] == 'invalid_input_binding'
    finally:
        restored = call('/input/config', original)
        assert restored['configuration'] == original
    print('Input configuration and exact authentication checks passed; physical acceptance still required.')


if __name__ == '__main__':
    main()
