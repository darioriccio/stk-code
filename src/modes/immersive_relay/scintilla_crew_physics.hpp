// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_CREW_PHYSICS_HPP
#define HEADER_SCINTILLA_CREW_PHYSICS_HPP
class AbstractKart;
namespace ScintillaCrewPhysics {
struct Status {bool eligible=false,driverVeto=false,nativeBoostActive=false;};
Status inspect(AbstractKart*);
// Game-thread only, after Bullet. The caller owns mission/zone/energy gates.
// A false result has no physical or controller side effects.
bool apply(AbstractKart*,int fps);
}
#endif
