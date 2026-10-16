//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2012-2015 Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "karts/explosion_animation.hpp"

#include "audio/sfx_manager.hpp"
#include "graphics/camera/camera.hpp"
#include "graphics/stars.hpp"
#include "guiengine/engine.hpp"
#include "items/attachment.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/kart_properties.hpp"
#include "modes/follow_the_leader.hpp"
#include "network/network_string.hpp"
#include "network/protocols/client_lobby.hpp"
#include "race/race_manager.hpp"
#include "tracks/track.hpp"
#include "mini_glm.hpp"

#include <cstring>
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/scintilla_combat.hpp"
#include "modes/immersive_relay/spatial/scintilla_engine.hpp"
#include "network/network_config.hpp"
#include "network/rewind_manager.hpp"
#include "physics/physics.hpp"
#include <btBulletDynamicsCommon.h>
#include <cmath>
#include <algorithm>
#include <limits>
#endif

#ifdef ENABLE_IMMERSIVE_RELAY
namespace {
bool relaySupportingContact(const btVector3& normal,const btVector3& point,
                            const btVector3& origin,const btVector3& up,btScalar bottom)
{
    const btScalar length=normal.length();
    return std::isfinite(double(length))&&length>0&&
        ScintillaCombat::StaggerEnvelope::supportingFace(double(normal.dot(up)/length),
            double((point-origin).dot(up)),double(bottom));
}
// Support contacts below the actual body's bottom may be left behind. An
// upward-facing obstacle elsewhere is not automatically supporting ground.
struct RelayOverlap : btCollisionWorld::ContactResultCallback {
    const btCollisionObject* probe;
    const btCollisionObject* self;
    btVector3 origin,up;
    btScalar bottom;
    bool blocked=false;
    RelayOverlap(const btCollisionObject* p,const btCollisionObject* body,const btVector3& o,const btVector3& n,btScalar b)
      :probe(p),self(body),origin(o),up(n),bottom(b){}
    btScalar addSingleResult(btManifoldPoint& point,const btCollisionObject* a,int,int,
                             const btCollisionObject* b,int,int) override {
        const bool first=a==probe;
        const auto* other=first?b:a;
        const btVector3 normal=first?point.m_normalWorldOnB:-point.m_normalWorldOnB;
        const btVector3 hit=first?point.getPositionWorldOnB():point.getPositionWorldOnA();
        if(other!=self&&point.getDistance()<=0&&!relaySupportingContact(normal,hit,origin,up,bottom))blocked=true;
        return 0;
    }
};
struct RelayClearance : btCollisionWorld::ClosestConvexResultCallback {
    const btCollisionObject* self;
    btVector3 origin,up;
    btScalar bottom;
    RelayClearance(const btVector3& from,const btVector3& to,const btCollisionObject* body,
                   const btVector3& o,const btVector3& normal,btScalar b)
      :btCollisionWorld::ClosestConvexResultCallback(from,to),self(body),origin(o),up(normal),bottom(b){}
    btScalar addSingleResult(btCollisionWorld::LocalConvexResult& r,bool worldNormal) override {
        if(r.m_hitCollisionObject==self)return 1;
        const btVector3 n=worldNormal?r.m_hitNormalLocal:r.m_hitCollisionObject->getWorldTransform().getBasis()*r.m_hitNormalLocal;
        // Bullet's ClosestConvexResultCallback also takes this point as world space.
        if(relaySupportingContact(n,r.m_hitPointLocal,origin,up,bottom))return 1;
        return btCollisionWorld::ClosestConvexResultCallback::addSingleResult(r,worldNormal);
    }
};
bool relayStaggerClearance(AbstractKart* kart,btCollisionWorld* world,float* pitch)
{
    const auto* shape=kart->getBody()->getCollisionShape();
    if(!shape)return false;
    btTransform identity;identity.setIdentity();btVector3 lower,upper;
    shape->getAabb(identity,lower,upper);
    const double lo[3]={double(lower.x()),double(lower.y()),double(lower.z())};
    const double hi[3]={double(upper.x()),double(upper.y()),double(upper.z())};
    ScintillaCombat::StaggerEnvelope bounds;
    if(!ScintillaCombat::StaggerEnvelope::create(lo,hi,0.25,8.0*3.14159265358979323846/180.0,&bounds))return false;
    btVector3 center(0,0,0),half(0,0,0);
    for(unsigned i=0;i<3;++i){
        center[i]=btScalar(bounds.center[i]);
        // Account for center conversion, and round the outer extent up.
        half[i]=std::nextafter(btScalar(bounds.halfExtents[i]+std::abs(double(center[i])-bounds.center[i])),
                               std::numeric_limits<btScalar>::infinity());
        if(!std::isfinite(double(center[i]))||!std::isfinite(double(half[i]))||half[i]<=0)return false;
    }
    btBoxShape envelope(half);
    const btTransform body=kart->getTrans();
    btTransform from=body;from.setOrigin(body(center));btTransform to=from;
    const btVector3 normal=body.getBasis().getColumn(1).normalized();
    to.setOrigin(from.getOrigin()+normal*0.25f);
    for(unsigned i=0;i<3;++i)
        if(!std::isfinite(double(from.getOrigin()[i]))||!std::isfinite(double(to.getOrigin()[i])))return false;
    btCollisionObject probe;probe.setCollisionShape(&envelope);probe.setWorldTransform(from);
    RelayOverlap overlap(&probe,kart->getBody(),body.getOrigin(),normal,lower.y());
    world->contactTest(&probe,overlap);
    RelayClearance clearance(from.getOrigin(),to.getOrigin(),kart->getBody(),body.getOrigin(),normal,lower.y());
    world->convexSweepTest(&envelope,from,to,clearance);
    // The whole pitch interval is enclosed at both ends of this translation.
    // Never use a partial sweep as permission to rotate through an obstacle.
    if(overlap.blocked||clearance.hasHit())return false;
    *pitch=std::nextafter(float(bounds.pitch),0.0f);
    return true;
}
}
ExplosionAnimation* ExplosionAnimation::createRelayStagger(AbstractKart* kart,int64_t ticks)
{
    World* world=World::getWorld();
    if(!ImmersiveRelay::ScintillaEngine::combatPolicyActive()||!world||!world->isRacePhase()||world->isFinishPhase()||
       !kart||kart==world->getPlayerKart(0)||kart->isEliminated()||kart->hasFinishedRace()||kart->getKartAnimation()||
       kart->isInvulnerable()||kart->isShielded()||ticks<1||ticks>10000||
       NetworkConfig::get()->isNetworking()||RewindManager::isEnabled()||RaceManager::get()->isWatchingReplay()||RaceManager::get()->isRecordingRace())return nullptr;
    float height=0,pitch=0;
    auto* physics=Physics::get();
    if(!physics||!physics->getPhysicsWorld()||!kart->getBody())return nullptr;
    if(relayStaggerClearance(kart,physics->getPhysicsWorld(),&pitch))height=0.25f;
    auto* animation=new ExplosionAnimation(kart,int(ticks),height,pitch);
    kart->playCustomSFX(SFXManager::CUSTOM_EXPLODE);
    return animation;
}
ExplosionAnimation::ExplosionAnimation(AbstractKart* kart,int ticks,float height,float pitch)
  :AbstractKartAnimation(kart,"RelayStagger")
{
    m_direct_hit=true;m_reset_ticks=-1;m_velocity=0;
    memset(m_reset_trans_compressed,0,sizeof(m_reset_trans_compressed));
    m_relay_stagger=true;m_relay_ticks=ticks;m_relay_height=height;m_relay_pitch=pitch;
    m_end_ticks=m_created_ticks+ticks;
    m_normal=m_created_transform.getBasis().getColumn(1).normalized();
    kart->getBody()->setLinearVelocity(btVector3(0,0,0));
    kart->getBody()->setAngularVelocity(btVector3(0,0,0));
}
#endif

/** A static create function that does only create an explosion if
 *  the explosion happens to be close enough to affect the kart.
 *  Otherwise, NULL is returned.
 *  \param kart The kart that is exploded.
 *  \param pos The position where the explosion happened.
 *  \param direct_hit If the kart was hit directly.
 */
ExplosionAnimation *ExplosionAnimation::create(AbstractKart *kart,
                                               const Vec3 &pos,
                                               bool direct_hit)
{
    // When goal phase is happening karts is made stationary, so no animation
    // will be created
    if (kart->isInvulnerable() || World::getWorld()->isGoalPhase())
        return NULL;

    float r = kart->getKartProperties()->getExplosionRadius();

    // Ignore explosion that are too far away.
    if(!direct_hit && pos.distance2(kart->getXYZ())>r*r) return NULL;

    if(kart->isShielded())
    {
        kart->decreaseShieldTime();
        return NULL;
    }

    if (RaceManager::get()->isFollowMode())
    {
        FollowTheLeaderRace *ftl_world =
            dynamic_cast<FollowTheLeaderRace*>(World::getWorld());
        if(ftl_world->isLeader(kart->getWorldKartId()))
            ftl_world->leaderHit();
    }

    return new ExplosionAnimation(kart, direct_hit);
}   // create

// ----------------------------------------------------------------------------
/** A static create function that does only create an explosion if
 *  the explosion happens to be close enough to affect the kart.
 *  Otherwise, NULL is returned. */
ExplosionAnimation *ExplosionAnimation::create(AbstractKart *kart)
{
    if (kart->isInvulnerable() || World::getWorld()->isGoalPhase())
        return NULL;
    else if (kart->isShielded())
    {
        kart->decreaseShieldTime();
        return NULL;
    }
    return new ExplosionAnimation(kart, /*direct hit*/true);
}   // create

// ----------------------------------------------------------------------------
ExplosionAnimation::ExplosionAnimation(AbstractKart* kart, bool direct_hit)
                  : AbstractKartAnimation(kart, "ExplosionAnimation")
{
    memset(m_reset_trans_compressed, 0, 16);
    Vec3 normal = m_created_transform.getBasis().getColumn(1).normalized();
    // Put the kart back to its own flag base like rescue if direct hit in CTF
    bool reset = RaceManager::get()->getMinorMode() ==
        RaceManager::MINOR_MODE_CAPTURE_THE_FLAG && direct_hit;
    if (reset)
    {
        btTransform prev_trans = m_kart->getTrans();
        World::getWorld()->moveKartAfterRescue(m_kart);
        btTransform reset_trans = m_kart->getTrans();
        m_kart->getBody()->setCenterOfMassTransform(prev_trans);
        m_kart->setTrans(prev_trans);
        MiniGLM::compressbtTransform(reset_trans,
            m_reset_trans_compressed);
        init(direct_hit, normal, reset_trans);
    }
    else
    {
        init(direct_hit, normal,
             btTransform(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f)));
    }

    float t = m_kart->getKartProperties()->getExplosionInvulnerabilityTime();
    m_kart->setInvulnerableTicks(stk_config->time2Ticks(t));
    m_kart->playCustomSFX(SFXManager::CUSTOM_EXPLODE);
    m_kart->getAttachment()->clear();
    // Clear powerups when direct hit in CTF
    if (reset)
        resetPowerUp();
}   // ExplosionAnimation

//-----------------------------------------------------------------------------
ExplosionAnimation::ExplosionAnimation(AbstractKart* kart, BareNetworkString* b)
                  : AbstractKartAnimation(kart, "ExplosionAnimation")
{
    restoreBasicState(b);
    restoreData(b);
}   // RescueAnimation

//-----------------------------------------------------------------------------
void ExplosionAnimation::restoreData(BareNetworkString* b)
{
    bool direct_hit = b->getUInt8() == 1;
    Vec3 normal = m_created_transform.getBasis().getColumn(1).normalized();
    btTransform reset_transform =
        btTransform(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f));

    if (RaceManager::get()->getMinorMode() ==
        RaceManager::MINOR_MODE_CAPTURE_THE_FLAG && direct_hit)
    {
        m_reset_trans_compressed[0] = b->getInt24();
        m_reset_trans_compressed[1] = b->getInt24();
        m_reset_trans_compressed[2] = b->getInt24();
        m_reset_trans_compressed[3] = b->getUInt32();
        reset_transform =
            MiniGLM::decompressbtTransform(m_reset_trans_compressed);
    }
    init(direct_hit, normal, reset_transform);
}   // restoreData

//-----------------------------------------------------------------------------
ExplosionAnimation::~ExplosionAnimation()
{
    // Only play with physics and camera if the object is getting destroyed
    // because its time is up. If there is still time left when this gets
    // called, it means that the world is getting destroyed so we don't touch
    // these settings.
    if (m_end_ticks != std::numeric_limits<int>::max())
    {
        m_kart->getBody()->setLinearVelocity(btVector3(0,0,0));
        m_kart->getBody()->setAngularVelocity(btVector3(0,0,0));
#ifdef ENABLE_IMMERSIVE_RELAY
        if(m_relay_stagger)return; // base destructor still restores the physics body
#endif
        // Don't reset spectate camera
        auto cl = LobbyProtocol::get<ClientLobby>();
        if (!GUIEngine::isNoGraphics() && (!cl || !cl->isSpectator()))
        {
            for (unsigned i = 0; i < Camera::getNumCameras(); i++)
            {
                Camera *camera = Camera::getCamera(i);
                if (camera->getType() != Camera::CM_TYPE_END)
                    camera->setMode(Camera::CM_NORMAL);
            }
        }
    }
}   // ~ExplosionAnimation

// ----------------------------------------------------------------------------
void ExplosionAnimation::init(bool direct_hit, const Vec3& normal,
                              const btTransform& reset_trans)
{
    m_direct_hit = direct_hit;
    m_reset_ticks = -1;
    float timer = m_kart->getKartProperties()->getExplosionDuration();
    m_normal = normal;

    // Non-direct hits will be only affected half as much.
    if (!direct_hit)
    {
        timer *= 0.5f;
    }

    // Put the kart back to its own flag base like rescue if direct hit in CTF
    if (RaceManager::get()->getMinorMode() ==
        RaceManager::MINOR_MODE_CAPTURE_THE_FLAG && direct_hit)
    {
        m_reset_ticks = m_created_ticks +
            stk_config->time2Ticks(timer * 0.8f);
    }
    m_end_ticks = m_created_ticks + stk_config->time2Ticks(timer);

    if (m_reset_ticks != -1)
        m_reset_trans = reset_trans;
    else
        m_reset_trans = btTransform(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f));

    // Half of the overall time is spent in raising, so only use
    // half of the explosion time here.
    // Velocity after t seconds is:
    // v(t) = m_velocity + t*gravity
    // Since v(explosion_time*0.5) = 0, the following forumla computes
    // the right initial velocity for a kart to land back after
    // the specified time.
    m_velocity = 0.5f * timer * Track::getCurrentTrack()->getGravity();

    // From moveable::updatePosition
    Vec3 forw_vec = m_created_transform.getBasis().getColumn(2);
    float heading = atan2f(forw_vec.getX(), forw_vec.getZ());
    Vec3 up = m_created_transform.getBasis().getColumn(1);
    float pitch = atan2(up.getZ(), fabsf(up.getY()));
    float roll = atan2(up.getX(), up.getY());

    m_curr_rotation.setHeading(heading);
    m_curr_rotation.setPitch(pitch);
    m_curr_rotation.setRoll(roll);

    const int max_rotation = direct_hit ? 2 : 1;
    // To get rotations in both directions for each axis we determine a
    // number calculated by world created ticks between
    // -(max_rotation-1) and +(max_rotation-1)
    float f = 2.0f * M_PI / timer;
    m_add_rotation.setHeading(
        ((m_created_ticks / 9) % (2 * max_rotation + 1) - max_rotation) * f);
    m_add_rotation.setPitch(
        ((m_created_ticks / 10) % (2 * max_rotation + 1) - max_rotation) * f);
    m_add_rotation.setRoll(
        ((m_created_ticks / 11) % (2 * max_rotation + 1) - max_rotation) * f);
}   // init

// ----------------------------------------------------------------------------
/** Updates the kart animation.
 *  \param ticks Number of time steps - should be 1.
 */
void ExplosionAnimation::update(int ticks)
{
#ifdef ENABLE_IMMERSIVE_RELAY
    if(m_relay_stagger){
        const int elapsed=World::getWorld()->getTicksSinceStart()-m_created_ticks;
        const double w=ScintillaCombat::Profile::staggerCurve(double(elapsed)/m_relay_ticks);
        m_kart->setXYZ(m_created_transform.getOrigin()+m_normal*(m_relay_height*w));
        m_kart->setRotation((m_created_transform.getRotation()*btQuaternion(btVector3(1,0,0),m_relay_pitch*w)).normalized());
        if(elapsed>=m_relay_ticks){
            m_kart->setXYZ(m_created_transform.getOrigin());m_kart->setRotation(m_created_transform.getRotation());
            m_end_ticks=World::getWorld()->getTicksSinceStart();
        }
        AbstractKartAnimation::update(ticks);return; // may delete this
    }
#endif
    float dur = stk_config->ticks2Time(
        World::getWorld()->getTicksSinceStart() - m_created_ticks);

    float velocity = m_velocity -
        dur * 0.5f * Track::getCurrentTrack()->getGravity();
    Vec3 xyz = m_created_transform.getOrigin() + dur * velocity * m_normal;
    btQuaternion q = m_created_transform.getRotation();

    // Make sure the kart does not end up under the track
    if ((xyz - m_created_transform.getOrigin()).dot(m_normal) < 0.0f)
    {
        xyz = m_created_transform.getOrigin();
        m_end_ticks = World::getWorld()->getTicksSinceStart();
    }
    else if (getAnimationTimer() != 0.0f)
    {
        Vec3 curr_rotation = m_curr_rotation + dur * m_add_rotation;
        q = btQuaternion(curr_rotation.getHeading(), curr_rotation.getPitch(),
            curr_rotation.getRoll());
    }

    if (m_reset_ticks != -1 &&
        World::getWorld()->getTicksSinceStart() > m_reset_ticks)
    {
        Vec3 reset_xyz;
        const Vec3 reset_up = m_reset_trans.getBasis().getColumn(1);
        reset_xyz = m_reset_trans.getOrigin() + dur * velocity * reset_up;
        if ((reset_xyz - m_reset_trans.getOrigin()).dot(reset_up) < 0.0f)
            reset_xyz = m_reset_trans.getOrigin();
        m_kart->setXYZ(reset_xyz);
        m_kart->setRotation(m_reset_trans.getRotation());
    }
    else
    {
        m_kart->setXYZ(xyz);
        m_kart->setRotation(q);
    }

    AbstractKartAnimation::update(ticks);
}   // update

// ----------------------------------------------------------------------------
void ExplosionAnimation::updateGraphics(float dt)
{
#ifdef ENABLE_IMMERSIVE_RELAY
    if(m_relay_stagger){AbstractKartAnimation::updateGraphics(dt);return;}
#endif
    if (m_kart->getStarsEffect() && !m_kart->getStarsEffect()->isEnabled())
    {
        // Set graphical effects for invulnerable time in updateGraphics
        // to avoid issue with rewind
        float t =
            m_kart->getKartProperties()->getExplosionInvulnerabilityTime();
        m_kart->showStarEffect(t);
    }
    AbstractKartAnimation::updateGraphics(dt);
}   // updateGraphics

// ----------------------------------------------------------------------------
bool ExplosionAnimation::hasResetAlready() const
{
    return m_reset_ticks != -1 &&
        World::getWorld()->getTicksSinceStart() > m_reset_ticks;
}   // update

// ----------------------------------------------------------------------------
void ExplosionAnimation::saveState(BareNetworkString* buffer)
{
    AbstractKartAnimation::saveState(buffer);
    buffer->addUInt8(m_direct_hit ? 1 : 0);
    if (RaceManager::get()->getMinorMode() ==
        RaceManager::MINOR_MODE_CAPTURE_THE_FLAG && m_direct_hit)
    {
        buffer->addInt24(m_reset_trans_compressed[0])
            .addInt24(m_reset_trans_compressed[1])
            .addInt24(m_reset_trans_compressed[2])
            .addUInt32(m_reset_trans_compressed[3]);
    }
}   // saveState

// ----------------------------------------------------------------------------
void ExplosionAnimation::restoreState(BareNetworkString* buffer)
{
    AbstractKartAnimation::restoreState(buffer);
    restoreData(buffer);
}   // restoreState
