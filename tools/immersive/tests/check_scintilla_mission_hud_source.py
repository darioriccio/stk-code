#!/usr/bin/env python3
"""SC4 HUD/resource cross-file source contracts. No native or GPU execution."""
from pathlib import Path
import struct
ROOT=Path(__file__).resolve().parents[3]
IR=ROOT/'src/modes/immersive_relay'
e=(IR/'spatial/scintilla_engine.cpp').read_text()
l=(IR/'scintilla_layout.hpp').read_text()
s=(ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()
t=(ROOT/'src/graphics/stk_tex_manager.cpp').read_text()
p=(IR/'render/projection/fisheye_domemaster_projection.cpp').read_text()
checks=[]
def check(name,condition):
    assert condition,name
    checks.append(name)
for name in ['scintilla_mission_hud_enabled','scintilla_mission_result','scintilla_mission_result_text','scintilla_mission_slots','scintilla_mission_beacons','scintilla_mission_nucleus','scintilla_mission_beacon_states']:
    check('producer/shader uniform '+name,name in s and '"'+name+('[' if name in ['scintilla_mission_slots','scintilla_mission_beacons','scintilla_mission_nucleus'] else '"') in e)
check('SC3 keeps revision1 and SC4 advertises revision2','Revision=1' in l and 'MissionRevision=2' in l and 'l.profile()' in e and 'l.revision()' in e)
check('HUD is required before preparation and persists with frozen results','current.missionHUDEnabled=mission.required()' in e and 'current.missionResult.valid?current.missionResult.slots:mission.slots()' in e)
check('first presentation has no fictional zero-second recovery window','double missionTetherSeconds=-1' in (IR/'spatial/scintilla_input.hpp').read_text() and 'TetherRecoverable&&mission.profile().fps>0' in e)
check('result plate suppresses interactive feedback','if(scintilla_mission_result.x<.5)color=scintillaFeedback(' in s and 'color=scintillaMissionHUD(color,uvGL,dimensions)' in s)
asset=(ROOT/'data/immersive_relay/scintilla/mission_results_it_v1.png').read_bytes()
check('final local atlas has exact dimensions',asset[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',asset[16:24])==(2048,800))
check('atlas unit15 is restored after both projection branches',e.index('missionSavedAtlasTexture')<e.index('void finishProjectionUniforms()') and p.rindex('ScintillaEngine::finishProjectionUniforms()')>p.rindex('->drawFullScreenEffect('))
check('texture invalidation precedes driver destruction',t.index('releaseMissionPresentationAssets(p->second)')<t.index('p->second->drop()'))
release=e[e.index('void releaseMissionPresentationAssets('):e.index('bool prepareMissionAnchors(')]
check('asset failure cannot erase a permanent gameplay clock failure','missionAssetsInvalidated=mission.required()' in release and 'missionStepFailure=' not in release)
check('cleanup closes admission without creating a controller','getExisting()' in release and 'setScintillaStartPermitted(false)' in release)
prepare=e[e.index('bool prepareMissionPresentationAssets('):e.index('void releaseMissionPresentationAssets(')]
check('successful held load alone clears the separate asset invalidation','isWorldHeld()' in prepare and 'missionAssetsInvalidated=false' in prepare and 'missionStepFailure.clear()' not in prepare)
check('terminal outcomes do not become a beacon victory condition','result.securedCount' in e and 'result.officialTeamRacePosition' in e and 'current.missionSlots[0].beaconLit' in e)
print(f'SC4 HUD: {len(checks)} source/asset contracts passed; native rendering remains unexecuted.')
