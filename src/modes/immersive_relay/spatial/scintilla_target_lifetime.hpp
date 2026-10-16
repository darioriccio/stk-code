// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_TARGET_LIFETIME_HPP
#define HEADER_SCINTILLA_TARGET_LIFETIME_HPP
#include <cstdint>
#include <map>

namespace ScintillaSpatial { namespace TargetLifetime {
// Confined to the existing game/render thread. Historical pointers are identity
// tokens only; resolve through the current owner before reading any object.
struct Registered
{
    const void* pointer=nullptr;
    uint64_t instance=0;
    bool present=false;
};
struct Current
{
    const void* pointer;
    bool eligible;
    Current(const void* object=nullptr,bool ready=false):pointer(object),eligible(ready) {}
};
template<class Kart>
bool eligibleKart(Kart* kart,Kart* player)
{return kart&&!kart->isEliminated()&&!kart->hasFinishedRace()&&kart!=player;}
template<class Item,class ItemType>
bool eligibleItem(Item* item,ItemType bonusBox)
{return item&&item->getType()==bonusBox&&item->isAvailable()&&!item->isUsedUp();}
template<class World>
Current currentKart(World* world,uint64_t index)
{
    if(!world||index>=world->getNumKarts())return Current();
    auto* kart=world->getKart(static_cast<int>(index));
    if(!kart)return Current();
    return Current(kart,eligibleKart(kart,world->getPlayerKart(0)));
}
template<class Items,class ItemType>
Current currentItem(Items* items,uint64_t index,ItemType bonusBox)
{
    if(!items||index>=items->getNumberOfItems())return Current();
    auto* item=items->getItem(static_cast<unsigned>(index));
    return Current(item,eligibleItem(item,bonusBox));
}
template<class Lookup>
bool isLive(std::map<uint64_t,Registered>& records,uint64_t id,uint64_t instance,const Lookup& lookup)
{
    auto found=records.find(id);
    if(found==records.end()||!found->second.present||found->second.instance!=instance)return false;
    const Current current=lookup(id);
    if(!current.eligible||!current.pointer||current.pointer!=found->second.pointer)
    {
        // Retirement is sticky: a same-address respawn needs registration with
        // a fresh instance before it can accept another historical contact.
        records.erase(found);return false;
    }
    return true;
}
}}
#endif
