// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/scintilla_crew_physics.hpp"
#include "modes/immersive_relay/scintilla_crew_boost_math.hpp"
#include "modes/immersive_relay/relay_player_controller.hpp"
#include "graphics/material.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/kart_properties.hpp"
#include "karts/max_speed.hpp"
#include "physics/btKart.hpp"
#include <array>
#include <cmath>
namespace ScintillaCrewPhysics {
namespace {
std::array<btScalar,3> components(const btVector3& v){return {{v.x(),v.y(),v.z()}};}
bool finite(const btVector3& v){return std::isfinite(double(v.x()))&&std::isfinite(double(v.y()))&&std::isfinite(double(v.z()));}
using Preflight=ScintillaCrewBoostMath::Result<btScalar>;
Status read(AbstractKart* kart,Preflight* output) {
    Status status;if(!kart)return status;
    auto* driver=dynamic_cast<RelayPlayerController*>(kart->getController());
    status.driverVeto=driver&&driver->partyDriverVeto();
    for(unsigned category=MaxSpeed::MS_INCREASE_MIN;category<MaxSpeed::MS_INCREASE_MAX;++category)
        status.nativeBoostActive=status.nativeBoostActive||kart->isSpeedIncreaseActive(category);
    auto* body=kart->getBody();auto* vehicle=kart->getVehicle();const auto* properties=kart->getKartProperties();
    if(!driver||!body||!vehicle||!properties||status.driverVeto||status.nativeBoostActive||
       kart->isEliminated()||kart->hasFinishedRace()||kart->getKartAnimation()||kart->isInvulnerable()||
       !kart->isOnGround()||!std::isfinite(double(kart->getSpeed()))||kart->getSpeed()<=0)return status;
    const auto* material=kart->getMaterial();const double floor=vehicle->getMinSpeed();
    if((material&&material->isZipper())||!std::isfinite(floor)||floor>0)return status;
    const btVector3 velocity=body->getLinearVelocity(),normal=kart->getNormal();
    const btVector3 forward=body->getWorldTransform().getBasis().getColumn(2);
    if(!finite(forward)||!finite(body->getAngularVelocity()))return status;
    if(!ScintillaCrewBoostMath::prepare(components(velocity),components(normal),
        properties->getEngineMaxSpeed(),kart->getCurrentMaxSpeed(),output))return status;
    const btVector3 tangent=velocity-normal*normal.dot(velocity);
    if(!finite(tangent)||!std::isfinite(double(tangent.dot(forward)))||tangent.dot(forward)<=0)return status;
    status.eligible=true;return status;
}
}
Status inspect(AbstractKart* kart){Preflight unused;return read(kart,&unused);}
bool apply(AbstractKart* kart,int fps) {
    int burst=0,fade=0;if(!ScintillaCrewBoostMath::durations(fps,&burst,&fade))return false;
    Preflight flight;const auto status=read(kart,&flight);if(!status.eligible)return false;
    auto* driver=static_cast<RelayPlayerController*>(kart->getController());
    const btVector3 velocity(flight.velocity[0],flight.velocity[1],flight.velocity[2]);
    // All validation and btScalar conversion precede these non-failing native
    // writes. No await/interleaving, floor, angular velocity or SC3 API call.
    kart->increaseMaxSpeed(MaxSpeed::MS_INCREASE_ZIPPER,1.0f,0.0f,burst,fade);
    kart->getBody()->setLinearVelocity(velocity);
    driver->beginPartyBrakeLease(burst+fade);
    return true;
}
}
#endif
