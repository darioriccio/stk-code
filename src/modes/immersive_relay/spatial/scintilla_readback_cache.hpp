// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_READBACK_CACHE_HPP
#define HEADER_SCINTILLA_READBACK_CACHE_HPP
#include "modes/immersive_relay/spatial/scintilla_input.hpp"
#include <utility>

namespace ScintillaSpatial { namespace ReadbackCache {
// Query storage has no independent shot deadline. The consumer owns that
// absolute deadline, including cached/shared queries and all candidate frames.
// Every visibility lookup refreshes lastRequestedUs; maintenance polling does
// not. Therefore an idle entry can only be removed after every action that
// could already have requested it has exhausted its own resolution window.
template<class Queries,class Poll,class Retire>
void collectIdle(Queries& queries,int64_t now,const Poll& poll,const Retire& retire)
{
    for(auto it=queries.begin();it!=queries.end();)
    {
        // Read signaled results before considering age, with no blocking wait.
        poll(it->second);
        if(now-it->second.lastRequestedUs>PendingTimeoutUs)
        {
            retire(std::move(it->second));
            it=queries.erase(it);
        }
        else ++it;
    }
}
// Capacity pressure is distinct from idle expiry. A result owned by an admitted
// action must survive even after its GPU work has completed.
template<class Queries,class Poll,class Owned,class Dispose>
bool reclaimCompletedUnowned(Queries& queries,const Poll& poll,const Owned& owned,const Dispose& dispose)
{
    auto victim=queries.end();
    for(auto it=queries.begin();it!=queries.end();++it){
        const bool complete=poll(it->second);
        if(complete&&!owned(it->second)&&(victim==queries.end()||it->second.lastRequestedUs<victim->second.lastRequestedUs))victim=it;
    }
    if(victim==queries.end())return false;
    dispose(victim->second);queries.erase(victim);return true;
}
}}
#endif
