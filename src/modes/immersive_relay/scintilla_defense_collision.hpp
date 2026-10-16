// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_DEFENSE_COLLISION_HPP
#define HEADER_SCINTILLA_DEFENSE_COLLISION_HPP
#include "btBulletCollisionCommon.h"
namespace ScintillaDefenseCollision {
btTransform interpolate(const btTransform&,const btTransform&,btScalar);
// Physical socket relocation, including both endpoint overlaps.
bool socketPathClear(btCollisionWorld&,const btVector3& from,const btVector3& to,const btCollisionObject* source,const btCollisionObject* human);
struct Result {double human=2,obstacle=2;bool valid=true;};
void castShape(const btCollisionShape*,const btTransform& local,const btTransform& from,const btTransform& to,bool human,double epsilon,Result*,const btVector3& pulse0,const btVector3& pulse1,unsigned depth=0);
}
#endif
