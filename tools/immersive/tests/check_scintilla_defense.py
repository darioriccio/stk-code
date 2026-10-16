#!/usr/bin/env python3
"""SC3 source/mathematical checks only. Never compiles or executes native code."""
from pathlib import Path
from itertools import product
import math
import random
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
IR = ROOT / 'src/modes/immersive_relay'
CORE = (IR/'scintilla_defense.cpp').read_text()
HEADER = (IR/'scintilla_defense.hpp').read_text()
COLLISION = (IR/'scintilla_defense_collision.cpp').read_text()
PHYSICS = (IR/'scintilla_defense_physics.cpp').read_text()
ENGINE = (IR/'spatial/scintilla_engine.cpp').read_text()
INPUT = (IR/'spatial/scintilla_input.cpp').read_text()
CONTROLLER = (IR/'relay_session_controller.cpp').read_text()
SHADER = (ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()


class DefenseSourceAndMathChecks(unittest.TestCase):
    def test_native_fixtures_registered_once_with_actual_bullet(self):
        # SC4 extends the shared input registry. Retain the original minimum
        # and critical SC3 cases while admitting additional registered cases.
        required = {
            'scintilla_defense': {'impactIdempotency', 'preserveAcrossReprepare',
                'candidatePriorityWaitsForVisibility', 'exactBoostReserveBoundary'},
            'scintilla_input': {'defenseWarningDoesNotInterruptWorldOffense',
                'defenseRaceResetReusesGrantAndRearmsNewRace',
                'defenseRaceResetNeverRevivesAnExpiredOrReleasedGrant',
                'defenseParticipantRebindIsAtomicAndIdempotent',
                'defenseProtocolClosedAndEpochChecked'},
        }
        for stem, macro, expected in (('scintilla_defense','DEFENSE_CASE',24),('scintilla_input','SCINTILLA_CASE',80)):
            names = re.findall(r'^'+macro+r'\((\w+)\)$',(ROOT/f'tools/immersive/tests/{stem}_cases.inc').read_text(),re.M)
            self.assertGreaterEqual(len(names),expected)
            self.assertEqual(len(set(names)),len(names))
            self.assertTrue(required[stem] <= set(names), 'SC3 regression cases must remain registered')
            text = (ROOT/f'tools/immersive/tests/{stem}_regression.cpp').read_text()
            for name in names:self.assertEqual(len(re.findall(r'void '+name+r'\(',text)),1,name)
        cmake=(ROOT/'tools/immersive/tests/CMakeLists.txt').read_text()
        self.assertIn('bulletcollision bulletmath',cmake)
        self.assertIn('scintilla_defense_collision.cpp',cmake)
        self.assertIn('CXX_STANDARD 11',cmake)
        self.assertIn('separatedObstacleIsNotAnUncertainContact',cmake)

    def test_rear_uses_the_same_camera_face_sampling_as_main_view(self):
        rear=SHADER.split('if(surface==4){',1)[1].split('// Same picking policy',1)[0]
        self.assertIn('color=use_multiview!=0?sampleMultiview(direction).rgb:sampleEnvironment(direction).rgb;',rear)
        for name in ('immersive_equirectangular.frag','immersive_fisheye_domemaster.frag'):
            main=(ROOT/'data/shaders'/name).read_text()
            self.assertRegex(main,r'use_multiview\s*!=\s*0\s*\? sampleMultiview\(dir\)\s*: sampleEnvironment\(dir\)')

    def test_fixed_point_rates_and_ceiling_windows(self):
        self.assertIn('p.q=int64_t(1000)*fps',CORE)
        self.assertIn('p.reserve=5*p.q+20000*(p.warning+p.margin)',CORE)
        for f,c in product((60,120,144),(1,2,5)):
            q=1000*f; warning=(int(3e6 if c==1 else 2.5e6)*f+999999)//1000000
            margin=(250000*f+999999)//1000000
            self.assertEqual((5*q+20000*(warning+margin))/q,70 if c==1 else 60)
            for rate in (2,5,20,25):self.assertEqual(1000*rate*(2*f),rate*2*q)
        self.assertIn('m_heat=std::min(100*m_profile.q,m_heat+heat)',CORE)
        self.assertIn('m_energy==0||m_heat==100*m_profile.q',CORE)

    def test_release_and_new_epoch_precede_admission_and_deadline(self):
        release=CORE.index('std::vector<const Beam*> current')
        arbitrate=CORE.index('for(size_t i:order)')
        deadline=CORE.index('if(m_now>=m_deadline)')
        self.assertLess(release,arbitrate);self.assertLess(arbitrate,deadline)
        reset=CORE[CORE.index('if(t.epoch!=b->epoch)'):arbitrate]
        self.assertIn('if(m_owner==int(i))m_owner=-1',reset)
        self.assertIn('current[a]->admitted<current[b]->admitted',CORE)
        self.assertIn('m_ownerSince<=m_deadline-m_profile.hold',CORE)
        self.assertIn('b->serial<t.serial',CORE)

    def test_optical_proof_not_a_warning_seen_boolean(self):
        self.assertIn('o.presentedHigh<=nowUs',CORE)
        self.assertIn('m_deadline=m_now+m_profile.warning',CORE)
        self.assertIn('nowUs-m_waitStarted>500000',CORE)
        self.assertIn('consumer.presentedDefenseFrames(now)',ENGINE)
        self.assertIn('if(threat&&o.visibilityLost)',CORE)
        self.assertIn('o.visibilityLost=true;o.videoContinuous=false',ENGINE)
        self.assertIn('before.phase!=Defense::Phase::Flying||defense.view().phase!=Defense::Phase::Flying)ScintillaDefensePhysics::clear()',ENGINE)
        self.assertIn('lastDefenseVisibleLow=0;clearDefenseEvidence();return o',ENGINE)
        self.assertIn('now-saved->second.low<=150000',ENGINE)
        self.assertIn('now-low<=60000',INPUT)
        self.assertIn('hi>now||lo<m->issuedAt||now-lo>150000',INPUT)

    def test_same_grant_rebind_preserves_physical_authority_and_replay(self):
        block=INPUT.split('Reply Runtime::rebindParticipants',1)[1].split('uint64_t Runtime::inputEpoch',1)[0]
        for field in ('expectedInputEpoch','expectedRosterRevision','bindings','inputSessionID','routingGeneration'):
            self.assertIn(field,block)
        self.assertIn('now>=m->leaseUntil',block)
        self.assertIn('participants!=m->expectedParticipants',block)
        self.assertIn('m->sourceIndex(id)',block)
        self.assertIn('++m->inputEpoch',block)
        for forbidden in ('seenEvents.clear','sources.clear','leaseUntil=','clockLow=','session=','generation='):
            self.assertNotIn(forbidden,block)
        canonical=INPUT.index('if(type=="sourceState"||type=="cancel")')
        self.assertLess(canonical,INPUT.index('if(!m->participantBindingsValid)return error'))

    def test_race_reset_retains_physical_authority_but_clears_gameplay(self):
        reset=INPUT.split('bool Runtime::resetForRace()',1)[1].split('Reply Runtime::validateCombatPrepare',1)[0]
        for fragment in ('m->stop("race_rebind_required")','m->frames.clear()','m->presented.clear()',
                         'm->participantBindingsValid=false','m->resumeRevision=0','m->historyReady=false'):
            self.assertIn(fragment,reset)
        for forbidden in ('m->session=','m->leaseUntil=','m->clockLow=','m->seenEvents.clear()',
                          'm->sources.clear()','m->stream.clear()'):
            self.assertNotIn(forbidden,reset)
        self.assertIn('r.id<=m->retiredThrough',INPUT)
        race=(IR/'relay_race_mode.cpp').read_text()
        self.assertIn('void RelayRaceMode::reset(bool restart)\n{\n    ImmersiveRelay::ScintillaEngine::newWorld(true);',race)
        self.assertEqual(race.count('newWorld(true)'),1)
        self.assertIn('if(retained)current.projection=projection;',ENGINE)
        self.assertIn('else if(active)changeStream();',ENGINE)

    def test_world_offense_is_independent_of_defense_threat(self):
        self.assertIn('contextThreat=surface.context==ScintillaDefense::Context::WorldOffense?0:f->defense.threat',INPUT)
        self.assertIn('p.threat!=contextThreat',INPUT)
        self.assertIn('p.threat!=source.pressThreat',INPUT) # SC3 still binds warning identity
        for old,new in ((0,1),(1,2),(2,2)):
            for world in (True,False):
                normalized=lambda threat:0 if world else threat
                self.assertEqual(normalized(old)==normalized(new),world or old==new)

    def test_socket_sweep_covers_the_relocated_segment(self):
        shift=PHYSICS.index('origin=origin+b.direction*radius')
        call=PHYSICS.index('ScintillaDefenseCollision::socketPathClear(*world,a,bv(origin)')
        self.assertLess(shift,call)
        self.assertIn('world.convexSweepTest(&sphere,a,b,sweep,0)',COLLISION)
        self.assertIn('world.contactTest(&socket,contact)',COLLISION)
        self.assertNotIn('struct SocketSweep',PHYSICS)
        end=(1.0,0.0,math.sqrt(3.0)); obstacle=tuple(x*.5 for x in end)
        self.assertGreater(abs(obstacle[0]),.1+.122) # center ray is clear
        self.assertGreater(math.dist(end,obstacle),.1+.122) # final overlap is clear
        self.assertEqual(math.dist(tuple(x*.5 for x in end),obstacle),0) # swept path blocked

    def test_candidate_selection_respects_priority_across_pending_gpu_queries(self):
        choose=CORE.split('bool Runtime::chooseCandidate(',1)[1].split('bool Runtime::readyForCandidate()',1)[0]
        self.assertLess(choose.index('std::sort('),choose.index('const auto proof=visible(c)'))
        self.assertIn('if(proof==CandidateVisibility::Pending)return false',choose)
        self.assertIn('aa!=bb?aa<bb:a.distance!=b.distance?a.distance<b.distance:a.id<b.id',choose)
        self.assertIn('defense.chooseCandidate(std::move(candidates)',ENGINE)
        selection=ENGINE.split('std::vector<Defense::Candidate> candidates',1)[1].split('consumer.setDefenseContext',1)[0]
        self.assertLess(selection.index('candidates.push_back(c)'),selection.index('candidateVisibility(c,now)'))
        self.assertNotIn('candidateVisibility(c,now)!=Visibility::Visible',selection)
        # Oracle of the specified ordering, independent of GPU completion order.
        rows=[(-1440,24,1),(-1440,16,2)]
        for supplied in (rows,list(reversed(rows))):
            ordered=sorted(supplied)
            self.assertEqual(ordered[0][2],2)
            for near_state,expected in (('pending',None),('visible',2),('rejected',1)):
                selected=None
                for _,_,identity in ordered:
                    state=near_state if identity==2 else 'visible'
                    if state=='pending':break
                    if state=='visible':selected=identity;break
                self.assertEqual(selected,expected)

    def test_native_physics_step_is_bracketed_and_missing_trace_cancels(self):
        native=(ROOT/'src/physics/physics.cpp').read_text()
        self.assertLess(native.index('beforeDefensePhysics()'),native.index('stepSimulation('))
        self.assertLess(native.index('stepSimulation('),native.index('afterDefensePhysics()'))
        self.assertIn('collision_step_missing',ENGINE)
        self.assertIn('result.available=true;if(!eligibleHuman',PHYSICS)
        self.assertIn('ScintillaDefenseCollision::castShape',PHYSICS)
        self.assertIn('convexSweepTest',PHYSICS)

    def test_bundled_bullet_abi_and_actual_shape_lifetime(self):
        bullet=(ROOT/'lib/bullet/src/BulletCollision/CollisionDispatch/btCollisionWorld.h').read_text()
        self.assertIn('const btCollisionObject* colObj0',bullet)
        for source in (PHYSICS,(ROOT/'src/karts/explosion_animation.cpp').read_text()):
            self.assertNotIn('btCollisionObjectWrapper',source)
        self.assertNotIn('old->second.shape->',PHYSICS)
        for evidence in ('b.lower,b.upper','sameShapes(old->second.shapes,shapes)','shape->getLocalScaling()','shape->getMargin()','extent.setMax(upper.absolute())'):
            self.assertIn(evidence,PHYSICS)
        self.assertIn('humanAtContact',PHYSICS)
        self.assertIn('trace.humanForward,trace.humanUp',ENGINE)

    def test_compound_arc_error_and_separation_bound(self):
        rho=1;phi=math.radians(30)
        n=math.ceil(phi/(2*math.acos(1-.001/rho)))
        self.assertEqual(n,6);self.assertLessEqual(rho*(1-math.cos(phi/(2*n))),.001)
        rng=random.Random(20303)
        for _ in range(1000):
            radius=rng.uniform(.01,10);angle=rng.uniform(0,math.pi);t=rng.uniform(-.5,.5)
            # Any point of the convex child within radius of the body pivot.
            actual=math.hypot(radius*math.cos(angle*t)-radius,radius*math.sin(angle*t))
            bound=2*radius*math.sin(angle/4)
            self.assertLessEqual(actual,bound+1e-12)
        self.assertIn('rho*(1-std::cos(phi/(2*n)))',COLLISION)
        self.assertIn('human?-1:1',COLLISION)
        self.assertIn('narrowOverlap(convex,realBody,realPulse,.0005)',COLLISION)
        self.assertIn('separatedInterval',COLLISION)
        self.assertIn('translation+rotation+epsilon',COLLISION)

    def test_asymmetric_socket_aabb_radius_contains_every_corner(self):
        lower=(-5,-1,-.1);upper=(1,5,.1)
        radius=math.sqrt(sum(max(abs(a),abs(b))**2 for a,b in zip(lower,upper)))+.15
        for corner in product(*zip(lower,upper)):
            self.assertGreater(radius,math.sqrt(sum(v*v for v in corner))+.12)
        self.assertLess(max(math.dist(lower,(0,0,0)),math.dist(upper,(0,0,0))),math.sqrt(50))
        self.assertIn('ScintillaDefenseCollision::socketPathClear',PHYSICS)
        self.assertIn('world.convexSweepTest(&sphere,a,b,sweep,0)',COLLISION)
        self.assertIn('world.contactTest(&socket,contact)',COLLISION)

    def test_impact_is_tangential_idempotent_and_boost_commits_cost_last(self):
        self.assertIn('normal+tangent*.85',PHYSICS)
        impact=PHYSICS.split('bool applyImpact(',1)[1].split('bool applyBoost(',1)[0]
        self.assertNotIn('adjustSpeed(',impact)
        self.assertIn('m_kart', (ROOT/'src/physics/btKart.hpp').read_text())
        self.assertIn('m_impactEvent.race=m_race',CORE)
        self.assertIn('m_protectedUntil=m_now+m_profile.protection',CORE)
        self.assertIn('if(applied){m_energy-=Profile::BoostCost*m_profile.q',CORE)
        self.assertIn('defense.confirmBoost(defense.canApplyBoost()&&ScintillaDefensePhysics::applyBoost',ENGINE)
        self.assertIn('MS_INCREASE_ZIPPER,2,2,0',PHYSICS)
        self.assertIn('if(k->isSpeedIncreaseActive(i))return false',PHYSICS)
        self.assertIn('m_max_speed->isSpeedIncreaseActive(category)',(ROOT/'src/karts/kart.cpp').read_text())

    def test_post_physics_boost_commits_native_floor_before_success(self):
        boost=PHYSICS.split('bool applyBoost(',1)[1]
        self.assertLess(boost.index('instantSpeedIncrease('),boost.index('vehicle->adjustSpeed(vehicle->getMinSpeed(),-1)'))
        self.assertLess(boost.index('vehicle->adjustSpeed('),boost.index('return true;'))
        self.assertLess(boost.index('fuzzyZero()'),boost.index('instantSpeedIncrease('))
        native=(ROOT/'src/karts/max_speed.cpp').read_text().split('void MaxSpeed::instantSpeedIncrease(',1)[1].split('// instantSpeedIncrease',1)[0]
        self.assertIn('m_kart->getVehicle()->setMinSpeed(speed)',native)
        vehicle=(ROOT/'src/physics/btKart.cpp').read_text().split('void btKart::adjustSpeed(',1)[1].split('// adjustSpeed',1)[0]
        self.assertIn('velocity - upright_component',vehicle)
        self.assertIn('v*velocity_ratio',vehicle)
        self.assertNotIn('setAngularVelocity',vehicle)
        race=(IR/'relay_race_mode.cpp').read_text().split('void RelayRaceMode::update(',1)[1]
        self.assertLess(race.index('StandardRace::update(ticks)'),race.index('updateCombat(ticks,'))
        # Independent lifecycle oracle: a deferred floor disappears at the next
        # kart update. Committing it now changes the body, which survives reset.
        for initial,ceiling,expected in ((10,30,12),(29,30,30)):
            floor=min(initial+2,ceiling)
            deferred=initial
            committed=max(initial,floor)
            floor=0 # native Kart::update reset
            self.assertEqual(max(deferred,floor),initial)
            self.assertEqual(max(committed,floor),expected)
        # Native minimum-speed path leaves the component along the normal intact.
        v=(10,-2,0);normal=(0,1,0);floor=12
        vn=tuple(a*sum(x*y for x,y in zip(v,normal)) for a in normal)
        tangent=tuple(x-y for x,y in zip(v,vn));length=math.sqrt(sum(x*x for x in tangent))
        result=tuple(n+x*floor/length for n,x in zip(vn,tangent))
        self.assertEqual(result,(12,-2,0))

    def test_additive_routes_and_all_server_start_gates(self):
        router=(IR/'remote/relay_command_router.cpp').read_text()
        self.assertTrue('/immersive/v1/spatial/' in router and 'spatial_path=="/spatial/rebind"' in router)
        self.assertIn('ScintillaEngine::rebindSpatialParticipants(body)',router)
        self.assertIn('/immersive/v1/scintilla/defense/',router)
        for method in ('pilotReady','operatorStart','operatorCountdown','operatorResume','confirmHandoff'):
            body=CONTROLLER.split('bool RelaySessionController::'+method+'(',1)[1].split('\n}',1)[0]
            self.assertIn('m_scintilla_start_permitted',body,method)
        self.assertIn('(!defense.required()||defenseBound())',ENGINE)
        self.assertIn('!required&&defense.required()',ENGINE)
        self.assertIn('setScintillaStartPermitted(!combat.required()&&!defense.required()&&!mission.required())',ENGINE)
        route=router.split('input().route(spatial_path,body,received)',1)[1]
        self.assertTrue(route.index('refreshCombatInput()')<route.index('return RelayHttpResponse(reply.status,response)'))

    def test_boost_cost_is_shared_by_accounting_api_and_visible_hud(self):
        self.assertIn('enum {BoostCost=25}',HEADER)
        self.assertNotIn('25*m_profile.q',CORE+HEADER)
        self.assertIn('Profile::BoostCost*m_profile.q',CORE)
        self.assertIn('"scintilla_boost_cost"),Defense::Profile::BoostCost',ENGINE)
        self.assertIn('boostCost\\\":\"<<Defense::Profile::BoostCost',ENGINE)
        self.assertIn('uniform float scintilla_boost_cost',SHADER)
        self.assertIn('scintillaBoostCostGlyph(q)',SHADER)
        self.assertIn('scintilla_defense_resources.x-scintilla_boost_cost',SHADER)
        self.assertIn('color=mix(color,vec3(1.0),cost)',SHADER)
        # Complete numeric glyph footprint, including smoothing, remains inside
        # the .65 acquisition ring and the original interactive disc at any size.
        self.assertIn('(q-vec2(.32,.25))/vec2(.26,.21)',SHADER)
        right=.32+(.4+.12)*.26;top=.25-(.8+.12)*.21;bottom=.25+(.8+.12)*.21
        self.assertLess(math.hypot(right,bottom),.65)
        self.assertLess(math.hypot(-.44-.03,.25+.03),.65)
        self.assertGreater(top,0)
        for w,h in ((256,256),(1024,1024),(2048,1024),(2048,2048)):
            radius=.03*min(w,h)
            self.assertLess(math.hypot(right*radius,bottom*radius),radius)
        for energy,reserve,after,allowed in ((100,70,75,True),(95,70,70,True),(94,70,69,False),(85,60,60,True),(80,60,55,False)):
            self.assertEqual(max(0,energy-25),after)
            self.assertEqual(energy-25>=reserve,allowed)

    def test_single_rear_composite_read_only_and_opaque_hud(self):
        self.assertIn('color=use_multiview!=0?sampleMultiview(direction).rgb:sampleEnvironment(direction).rgb',SHADER)
        self.assertIn('-(2.0*st.x-1.0)',SHADER)
        self.assertIn('non_targetable_surface',INPUT)
        self.assertIn('ambiguous_layout_context',INPUT)
        self.assertIn('proofInterrupted=true',INPUT)
        self.assertIn('WorldOffense));b.target',INPUT)
        self.assertIn('worldRectFree',ENGINE)
        self.assertIn('commissionedRegion(u-du,v-dv,u+du,v+dv)',ENGINE)
        self.assertIn('defenseVisibleRegions=a.regions',ENGINE)
        self.assertIn('defense_visibility_evidence_changed',ENGINE)
        self.assertIn('rear_targetable_view\\\":false',ENGINE)
        for projection in ('immersive_equirectangular.frag','immersive_fisheye_domemaster.frag'):
            text=(ROOT/'data/shaders'/projection).read_text()
            self.assertIn('utils/immersive_scintilla.frag',text)
            self.assertIn('scintillaAlpha',text)

    def test_native_source_delimiters_are_balanced(self):
        # Lexical sanity only, deliberately not a C++/GLSL parser/typecheck.
        for path in [IR/'scintilla_defense.cpp',IR/'scintilla_defense.hpp',IR/'scintilla_layout.hpp',IR/'scintilla_defense_collision.cpp',IR/'scintilla_defense_physics.cpp',IR/'spatial/scintilla_engine.cpp',IR/'spatial/scintilla_input.cpp',ROOT/'data/shaders/utils/immersive_scintilla.frag']:
            source=path.read_text()
            source=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', '',source,flags=re.S)
            stack=[];pairs={')':'(',']':'[','}':'{'}
            for c in source:
                if c in '([{':stack.append(c)
                elif c in pairs:self.assertTrue(stack and stack.pop()==pairs[c],str(path))
            self.assertFalse(stack,str(path))


if __name__=='__main__':unittest.main(verbosity=2)
