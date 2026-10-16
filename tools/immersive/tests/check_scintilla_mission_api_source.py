#!/usr/bin/env python3
"""SC4 API integration source guards; no compiler, server, or native execution."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
IR = ROOT / "src/modes/immersive_relay"
engine = (IR / "spatial/scintilla_engine.cpp").read_text()
router = (IR / "remote/relay_command_router.cpp").read_text()
dispatcher = (IR / "remote/relay_game_dispatcher.cpp").read_text()


def body(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda match: " " * len(match.group()), source, flags=re.S)
    depth = 1
    for index in range(opening + 1, len(masked)):
        depth += (masked[index] == "{") - (masked[index] == "}")
        if depth == 0:
            return source[opening + 1:index]
    raise AssertionError(signature)


checks = []
def check(name, condition):
    assert condition, name
    checks.append(name)


check("mission routes precede broad combat dispatch",
      router.index('const std::string mission_prefix=') < router.index('const std::string combat_prefix='))
check("mission snapshots and commands use the existing authoritative dispatcher",
      'std::string("/immersive/v1/scintilla/")' in dispatcher and
      all('mission_prefix+"' + path + '"' in router for path in ("state", "intent", "prepare", "intro-complete")))
intent = body(engine, "Reply setMissionIntent(")
check("all three requirements are preflighted before atomic publication",
      all(s in intent for s in ("auto nextMission=mission", "auto nextCombat=combat", "auto nextDefense=defense")) and
      intent.index("setScintillaStartPermitted(false)") < intent.index("mission=std::move(nextMission)") and
      intent.index("defense=std::move(nextDefense)") < intent.index("missionRequests.commit(request)"))
check("same-value held refresh is possible after a race reset",
      "request.required!=mission.required()" in intent and "loadMissionIntro(&reason)" in intent)
gate = body(engine, "ScintillaSpatial::Reply missionRequestGate(")
check("recent exact retries are resolved before changed identity rejection",
      gate.index("Receipts::Duplicate") < gate.index("request.expected==missionIdentity()"))
prepare = body(engine, "Reply prepareMission(")
preparation_clock = re.search(r'const auto (\w+)=nowUs\(\);', prepare)
check("preparation requires real fresh held presentation and complete SC1-3",
      preparation_clock is not None and
      all(s in prepare for s in ("consumer.prepared()", "combat.configured()", "!defenseBound()",
          "mission_fresh_presentation_required", "missionIntroComplete()")) and
      all(f"consumer.{method}({preparation_clock[1]},true)" in prepare
          for method in ("defenseVideoReady", "presentedDefenseFrames")))
check("unqualified authored anchors cannot prepare public SC4",
      'anchorStatus!="qualified"' in prepare and "mission_anchors_unqualified" in prepare)
check("terminal preparation restores resources before returning, without entering core prepare",
      prepare.index("prepareMissionPresentationAssets(") < prepare.index("if(missionTerminal())") < prepare.index("mission.prepare("))
binding = body(engine, "bool missionPreparationBound()")
check("binding covers all immutable runtime authorities",
      all(s in binding for s in ("missionPreparedIdentity==now", "mission.race()==raceEpoch",
          "mission.inputEpoch()==consumer.inputEpoch()", "mission.crew()==lastRoster", "mission.humanInstance()==humanInstance",
          "mission.profile().fps==stk_config->getPhysicsFPS()", "missionNativeProfileReason(true)",
          "measurementFingerprint(ImmersiveRenderPipeline::get()->getConfig())!=qualificationFingerprint")))
check("binding is rechecked at start and before every native physics step",
      "missionPreparationBound()" in body(engine, "bool missionStartPermitted()") and
      "!missionPreparationBound()" in body(engine, "bool beginMissionStep(int ticks)"))
profile = body(engine, "std::string missionNativeProfileReason(")
check("profile validates actual world and session, beyond labels",
      all(s in profile for s in ('s.m_track_id!="snowtuxpeak"', 's.m_kart_id!="tux"', "s.m_ai_karts!=3",
          's.m_difficulty!="easy"', "world->getNumKarts()!=4", "rm->getReverseTrack()", "DIFFICULTY_EASY",
          "s.m_test_accelerated_progress", "s.m_adaptive_difficulty_config.m_enabled")))
state = body(engine, "std::string missionStateJSON()")
check("state serializes immutable result and never performs preparation or file access",
      "ScintillaMission::writeResultJSON(out,result)" in state and
      all(s not in state for s in ("readMissionFile(", "loadMissionIntro(", "mission.prepare(", "mission.setRequired(", "refreshRoster(")))
check("mission qualification snapshot requires current cheap fingerprint without mutation",
      'anchorQualified=anchorStatus=="qualified"&&!qualificationFingerprint.empty()&&' in state and
      'measurementFingerprint(ImmersiveRenderPipeline::get()->getConfig())==qualificationFingerprint' in state and
      'publishedAnchorStatus=anchorStatus=="qualified"&&!anchorQualified?"preflight_required":anchorStatus' in state and
      'quote(publishedAnchorStatus)' in state and
      state.index("#ifndef SERVER_ONLY") < state.index('anchorQualified=anchorStatus=="qualified"') < state.index("#endif") and
      all(s not in state for s in ("loadMeasurementFingerprints(", "loadMissionQualification(", "refreshMissionQualification(")) and
      re.search(r"\b(?:anchorStatus|qualificationFingerprint)\s*=(?!=)", state) is None)
receipt = body(engine, "Reply completeMissionIntro(")
check("intro receipt is explicit, pre-race, held, and bound to the selected local media",
      all(s in receipt for s in ("isWorldHeld()", "world->getTimeTicks()>0", "missionIntroAvailable",
          "request.variantId", "introReceiptKey()", "request.completion", "request.playbackID")))

measurement = (IR / "spatial/scintilla_measurement_native.hpp").read_text()
qualification = body(measurement, "bool refreshMissionQualification(")
check("public preparation uses cached qualification and no massive file reads",
      "fingerprint==qualificationFingerprint" in qualification and
      "mission_qualification_preflight_required" in qualification and
      all(s not in qualification for s in ("readMissionFile(", "loadMeasurementFingerprints(",
          "loadMissionQualification(", "evidence.frame(")))
diagnostic = body(engine, "Reply missionMeasurementCommand(")
preflight = body(diagnostic, 'if(operation=="preflight"){')
check("expensive evidence preflight is held and cannot expire an active spatial grant",
      diagnostic.index("isWorldHeld()") < diagnostic.index('if(operation=="preflight"){') and
      preflight.index("consumer.hasPreparedGrant()") < preflight.index("loadMeasurementFingerprints(") and
      "consumer.inputMaintenanceMatches(" in preflight and
      "loadMissionQualification(" in preflight)

invalidation = body(measurement, "void invalidateMissionQualificationCache()")
check("actual content reload invalidates qualification without resetting cargo or doing GL",
      all(s in invalidation for s in ("qualificationFingerprint.clear()", "measurementAssetsHash.clear()",
          "measurementShaderHash.clear()", "measurementExecutableHash.clear()", "missionAPIPrepared=false",
          "measurementProofsInvalidated=true")) and
      all(s not in invalidation for s in ("glDelete", "mission.reset(", "consumer.reset(", "consumer.invalidate(")))
new_world = body(engine, "void newWorld(bool preserveSpatialGrant)")
textures = (ROOT / "src/graphics/stk_tex_manager.cpp").read_text()
check("same-world reset and individual font eviction retain qualification",
      "if(!preserveSpatialGrant)invalidateMissionQualificationCache()" in new_world and
      "if(remove_all)ImmersiveRelay::ScintillaEngine::invalidateMissionContent()" in textures and
      "invalidateMissionQualificationCache(" not in body(engine, "void releaseMissionPresentationAssets("))

evidence = (IR / "scintilla_mission_measurement.hpp").read_text()
poll = body(measurement, "void pollMissionMeasurement()")
check("measurement records actual availability after reading query results",
      poll.index("GL_QUERY_RESULT,&o.pixels[i]") < poll.index("f.readbackUs=nowUs()") < poll.index("measurementFrameJSON(f)") and
      "f.readbackTick=world?" in poll and
      all(field in measurement for field in ("readbackUs", "readbackTick", "activationUs")))
check("capsule runtime and evidence share the same closed proof interval",
      "Measure::capsuleProofAvailable(proof.captureUs,proof.readbackUs,nowUs())" in body(measurement, "bool measurementPlacementPermitted(") and
      "capsuleProofsOverlap(lastUs,readbackUs)" in evidence and
      "if(continuous&&geometry&&proofAt(key,activationUs))activationProven[key]=true;" in evidence and
      "return latest&&latest->visible&&capsuleProofAvailable(latest->captureUs,latest->readbackUs,at);" in evidence and
      "activationUs<=lastUs||activationUs>us" in evidence)
check("qualification separates capsule availability from tether geometry",
      "tether?us:readbackUs" in evidence and "tether?tick:readbackTick" in evidence and
      "const bool available=tether||capsuleProofAvailable(" in evidence and
      "us-a.availableUs" in evidence and "tick-a.availableTick" in evidence)
check("trace writer and native verifier require the bounded availability policy",
      "Measure::LiveProofMaxAgeUs" in diagnostic and
      'closed(limits,{"maxFrames","maxPending","maxGapUs","liveProofMaxAgeUs"})' in evidence)

spatial = (IR / "spatial/scintilla_input.cpp").read_text()
maintenance = body(spatial, "Reply Runtime::inputMaintenance(")
wrapper = body(engine, "Reply missionInputMaintenanceCommand(")
check("input maintenance is authenticated through the normal mission dispatcher",
      'mission_prefix+"input-maintenance/"+op' in router and
      "ScintillaEngine::missionInputMaintenanceCommand(op,body)" in router and
      all(s in wrapper for s in ("producerID", "std::to_string(raceEpoch)", "isWorldHeld()", "world->getTimeTicks()!=0", "missionNativeProfileReason(false)")))
check("maintenance transaction preserves raw grant and deduplication state",
      all(s in maintenance for s in ("inputMaintenanceFrom=m->inputEpoch", "++m->inputEpoch", "m->historyReady=false", "m->participantBindingsValid=false")) and
      all(s not in maintenance for s in ("usedSessions.clear", "seenEvents.clear", "sources.clear", "actionSeq=", 'route("/spatial/prepare"', 'route("/spatial/release"')))
ordinary = body(spatial, "Reply Runtime::route(")
clock_tick = body(spatial, "void Runtime::tick(")
check("maintenance blocks batches before any ordinary renewal or malformed invalidation",
      ordinary.index('if(m->inputMaintenance)return error(409,"input_maintenance_suspended")') < ordinary.index("const J j=Parser(body).parse()") and
      clock_tick.index("if(m->inputMaintenance){m->active=false;return;}") < clock_tick.index('m->invalidate("lease_or_clock_expired")') and
      "!m->inputMaintenance&&m->participantBindingsValid" in spatial)
check("resume requires fresh clocks and SI5 expiry, completed retry cannot extend",
      all(s in maintenance for s in ('arr(at(j,"clockSamples"),3,3)', 'm->clocks(at(j,"clockSamples"),now,low,high)', "until<=now||until>SAFE||until-now>10000000", 'if(m->inputMaintenanceCompleted&&!m->inputMaintenance)')) and
      maintenance.index("if(m->inputMaintenanceCompleted&&!m->inputMaintenance)") < maintenance.index("m->leaseUntil=until"))
check("preflight permits only matching suspended raw grant and remains prerace held",
      'closed(j,{"contract","schemaVersion","requestId","producerUUID","raceEpoch"},{"inputMaintenanceOperationID"})' in diagnostic and
      'consumer.hasPreparedGrant()&&!ticket' in preflight and 'consumer.inputMaintenanceMatches(text(j,"inputMaintenanceOperationID"))' in preflight and
      'world->getTimeTicks()!=0' in preflight)
check("maintenance closes both start paths independently of required policies",
      "if(consumer.inputMaintenanceActive())return false" in body(engine, "bool missionStartPermitted()") and
      "if(consumer.inputMaintenanceActive())return false" in body(engine, "bool combatStartPermitted()") and
      'mission.reset(' not in wrapper and 'consumer.reset(' not in wrapper)

# Evidence lookup: builds reach data/ through a bundle link, and getPath()
# drops the trailing separator; traces must join the resolved directory.
reader = body(measurement, "bool loadMissionQualification(")
located = body(measurement, "std::string measurementEvidenceDirectory()")
check("qualification evidence resolves the linked asset root and joins traces correctly",
      "realpath(root.c_str(),resolved)" in located and "qualificationReportAsset()" in located and
      'StringUtils::getPath(relative)+"/"' in located and
      'evidenceDirectory+"report.json"' in reader and "const std::string path=evidenceDirectory+rel;" in reader and
      "getPath(reportPath)+rel" not in reader and "measurementNoSymlink(reportPath)" in reader and
      "measurementNoSymlink(path)" in reader)

print(f"SC4 API: {len(checks)} source checks passed; HTTP/runtime remains unexecuted.")
