#!/usr/bin/env python3
"""Prepared live API regressions; never starts/builds STK. Not run during source delivery.

Requires an explicitly authorized, already running local test session. Toggles
presentation readiness while preserving its previous value. Does not start/reset
any race or change a valid render configuration.
"""
import argparse
import json
import os
from urllib.error import HTTPError
from urllib.request import Request, urlopen


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute',action='store_true',help='Explicitly execute against the running test producer')
    parser.add_argument('--base-url',default='http://127.0.0.1:8890/immersive/v1')
    args=parser.parse_args()
    if not args.execute:
        parser.error('Live probe is opt-in: use --execute only after runtime verification is authorized')
    token=os.environ.get('RELAY_API_TOKEN','immersive-dev-token')
    def call(path,payload=None):
        request=Request(args.base_url+path,data=None if payload is None else json.dumps(payload).encode(),
                        headers={'Authorization':'Bearer '+token,'Content-Type':'application/json'})
        try:
            with urlopen(request,timeout=10) as response:
                return response.status,json.load(response)
        except HTTPError as error:
            return error.code,json.load(error)
    code,capabilities=call('/capabilities')
    assert code==200 and 'presentation_readiness_gate' in capabilities['feature_flags']
    assert any(p['type']=='equirectangular' and p['status']=='production' for p in capabilities['supported_projections'])
    code,initial=call('/state')
    assert code==200 and 'presentation_ready' in initial
    restore=initial['presentation_ready']
    try:
        assert call('/render/readiness',{'ready':False})[0]==200
        assert call('/state')[1]['presentation_ready'] is False
        for endpoint in ['/operator/ready','/operator/start','/operator/countdown','/operator/resume','/operator/handoff/confirm']:
            code,reply=call(endpoint,{})
            assert code==409 and reply['error']=='presentation_not_ready',(endpoint,code,reply)
        for projection in [
            {'type':'equirectangular','width':2048,'height':2048},
            {'type':'equirectangular','width':3000,'height':1500},
            {'type':'equirectangular','width':2048,'height':0},
            {'type':'equirectangular','width':2048.5,'height':1024},
        ]:
            code,reply=call('/render/config',{'projection':projection})
            assert code==400 and not reply['ok'],(projection,code,reply)
        # Valid JSON numbers that overflow float32 must never poison camera
        # matrices or turn subsequent render diagnostics into invalid JSON.
        camera_fields = ['yaw_deg', 'pitch_deg', 'roll_deg', 'dome_tilt_deg',
                         'roll_limit_deg', 'forward_offset', 'vertical_offset',
                         'smoothing_factor']
        for field in camera_fields:
            for invalid in [1e300, -1e300, '35', None, True]:
                for payload in [{field: invalid}, {'camera': {field: invalid}}]:
                    code,reply=call('/render/config',payload)
                    assert code==400 and reply['error']=='invalid_immersive_camera_'+field,(payload,code,reply)
        code,final=call('/state')
        assert code==200
        assert final['session_id']==initial['session_id']
        assert final['arcade_state']==initial['arcade_state']
        assert final['immersive_render']['projection']==initial['immersive_render']['projection']
        assert final['immersive_render']['camera']==initial['immersive_render']['camera']
        print('Live readiness and invalid projection regressions passed. GPU/Syphon/venue qualification remains separate.')
    finally:
        code,_=call('/render/readiness',{'ready':restore})
        assert code==200,'Failed to restore presentation readiness'


if __name__=='__main__':
    main()
