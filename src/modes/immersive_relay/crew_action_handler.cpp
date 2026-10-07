//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/crew_action_handler.hpp"

#include "config/stk_config.hpp"
#include "items/attachment.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/max_speed.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "states_screens/race_gui_base.hpp"
#include "utils/string_utils.hpp"

#include <SColor.h>
#include <algorithm>
#include <cstdlib>

CrewActionHandler::CrewActionHandler()
{
}

int CrewActionHandler::timeToTicks(float seconds) const
{
    return stk_config ? stk_config->time2Ticks(seconds)
        : (int)(seconds * 60.0f);
}

void CrewActionHandler::addRelayMessage(RaceGUIBase* gui,
                                        const AbstractKart* kart,
                                        const std::string& text,
                                        int red, int green, int blue) const
{
    if (!gui)
        return;

    gui->addMessage(StringUtils::utf8ToWide(text), kart, 2.2f,
        video::SColor(255, red, green, blue), true, true, true);
}

void CrewActionHandler::giveBubbleShield(AbstractKart* kart,
                                         float seconds) const
{
    if (!kart || !kart->getAttachment())
        return;

    kart->getAttachment()->set(Attachment::ATTACH_BUBBLEGUM_SHIELD,
                               timeToTicks(seconds), NULL);
}

void CrewActionHandler::applyPilotBoost(AbstractKart* kart,
                                        float add_max_speed,
                                        float speed_boost,
                                        float duration_seconds) const
{
    if (!kart || kart->isEliminated())
        return;

    kart->instantSpeedIncrease(MaxSpeed::MS_INCREASE_ZIPPER,
        add_max_speed, speed_boost, 180.0f,
        timeToTicks(duration_seconds), timeToTicks(1.2f));
    kart->showZipperFire();
    kart->showStarEffect(0.8f);
}

AbstractKart* CrewActionHandler::findRivalKart(
    const RelayCrewActionRequest& action, const AbstractKart* pilot_kart) const
{
    World* world = World::getWorld();
    if (!world)
        return NULL;

    int requested_index = -1;
    for (std::string::size_type i = 0; i < action.m_target_id.size(); i++)
    {
        if (action.m_target_id[i] >= '0' && action.m_target_id[i] <= '9')
        {
            requested_index =
                std::atoi(action.m_target_id.c_str() + i);
            break;
        }
    }

    if (requested_index > 0)
    {
        const int kart_index = requested_index - 1;
        AbstractKart* kart = world->getKart(kart_index);
        if (kart && kart != pilot_kart && !kart->isEliminated())
            return kart;
    }

    const unsigned int local_players = RaceManager::get()->getNumPlayers();
    for (unsigned int i = 0; i < world->getNumKarts(); i++)
    {
        AbstractKart* kart = world->getKart((int)i);
        if (kart && kart != pilot_kart && !kart->isEliminated() &&
            i >= local_players)
        {
            return kart;
        }
    }

    for (unsigned int i = 0; i < world->getNumKarts(); i++)
    {
        AbstractKart* kart = world->getKart((int)i);
        if (kart && kart != pilot_kart && !kart->isEliminated())
            return kart;
    }
    return NULL;
}

void CrewActionHandler::slowRival(const RelayCrewActionRequest& action,
                                  const AbstractKart* pilot_kart,
                                  float seconds,
                                  float speed_factor) const
{
    AbstractKart* rival = findRivalKart(action, pilot_kart);
    if (!rival)
        return;

    rival->setSquash(seconds, 0.55f);
    rival->adjustSpeed(speed_factor);
    rival->showStarEffect(0.7f);
}

void CrewActionHandler::slowAllRivals(const AbstractKart* pilot_kart,
                                      float seconds,
                                      float speed_factor) const
{
    World* world = World::getWorld();
    if (!world)
        return;

    const unsigned int local_players = RaceManager::get()->getNumPlayers();
    for (unsigned int i = 0; i < world->getNumKarts(); i++)
    {
        AbstractKart* kart = world->getKart((int)i);
        if (!kart || kart == pilot_kart || kart->isEliminated() ||
            i < local_players)
        {
            continue;
        }
        kart->setSquash(seconds, 0.6f);
        kart->adjustSpeed(speed_factor);
    }
}

std::string CrewActionHandler::displayTarget(
    const RelayCrewActionRequest& action, const std::string& fallback) const
{
    if (!action.m_target_id.empty())
        return action.m_target_id;
    if (!action.m_mode.empty())
        return action.m_mode;
    return fallback;
}

void CrewActionHandler::applyAction(const RelayCrewActionRequest& action,
                                    AbstractKart* pilot_kart,
                                    RaceGUIBase* gui) const
{
    if (!pilot_kart)
        return;

    const std::string role =
        CrewRoleManager::roleToString(action.m_role);
    const std::string target = displayTarget(action, "ready");
    std::string message = "CREW ";
    message += role;
    message += ": ";

    if (action.m_type == "nav_select_route")
    {
        applyPilotBoost(pilot_kart, 1.2f, 0.8f, 1.0f);
        addRelayMessage(gui, pilot_kart, message + "route " + target,
                        80, 210, 255);
    }
    else if (action.m_type == "nav_mark_hazard")
    {
        pilot_kart->setInvulnerableTicks(timeToTicks(1.0f));
        pilot_kart->showStarEffect(0.7f);
        addRelayMessage(gui, pilot_kart, message + "hazard ping " + target,
                        80, 210, 255);
    }
    else if (action.m_type == "nav_call_pace_note")
    {
        applyPilotBoost(pilot_kart, 0.7f, 0.4f, 0.7f);
        pilot_kart->beep();
        addRelayMessage(gui, pilot_kart, message + "pace note " + target,
                        80, 210, 255);
    }
    else if (action.m_type == "nav_reveal_shortcut")
    {
        applyPilotBoost(pilot_kart, 1.0f, 0.5f, 0.8f);
        addRelayMessage(gui, pilot_kart, message + "bonus segnalato",
                        80, 210, 255);
    }
    else if (action.m_type == "nav_request_role")
    {
        pilot_kart->beep();
        addRelayMessage(gui, pilot_kart, message + "request " + target,
                        80, 210, 255);
    }
    else if (action.m_type == "engineer_charge_boost")
    {
        pilot_kart->setEnergy(std::min(10.0f,
            pilot_kart->getEnergy() + std::max(1, action.m_amount) / 40.0f));
        addRelayMessage(gui, pilot_kart, message + "boost charge",
                        120, 255, 120);
    }
    else if (action.m_type == "engineer_release_boost")
    {
        const float strength = std::max(20, action.m_amount) / 100.0f;
        applyPilotBoost(pilot_kart, 3.0f + 3.5f * strength,
                        2.0f + 3.0f * strength, 1.6f + strength);
        addRelayMessage(gui, pilot_kart, message + "boost release",
                        120, 255, 120);
    }
    else if (action.m_type == "engineer_dump_heat")
    {
        pilot_kart->showStarEffect(0.6f);
        addRelayMessage(gui, pilot_kart, message + "heat dumped",
                        120, 255, 120);
    }
    else if (action.m_type == "engineer_stabilize_grip")
    {
        pilot_kart->adjustSpeed(1.04f);
        pilot_kart->showStarEffect(1.2f);
        addRelayMessage(gui, pilot_kart, message + "grip stable",
                        120, 255, 120);
    }
    else if (action.m_type == "engineer_rebalance_power")
    {
        if (action.m_mode == "speed")
            applyPilotBoost(pilot_kart, 2.0f, 1.2f, 1.2f);
        else
            pilot_kart->adjustSpeed(1.03f);
        addRelayMessage(gui, pilot_kart, message + "power " + target,
                        120, 255, 120);
    }
    else if (action.m_type == "defender_raise_shield")
    {
        giveBubbleShield(pilot_kart, 4.0f);
        addRelayMessage(gui, pilot_kart, message + "shield " + target,
                        120, 170, 255);
    }
    else if (action.m_type == "defender_cleanse_status")
    {
        pilot_kart->unsetSquash();
        pilot_kart->setInvulnerableTicks(timeToTicks(1.2f));
        pilot_kart->showStarEffect(1.0f);
        addRelayMessage(gui, pilot_kart, message + "cleanse " + target,
                        120, 170, 255);
    }
    else if (action.m_type == "defender_absorb_hit")
    {
        pilot_kart->setInvulnerableTicks(timeToTicks(2.0f));
        addRelayMessage(gui, pilot_kart, message + "hit absorbed",
                        120, 170, 255);
    }
    else if (action.m_type == "defender_repair_armor_small")
    {
        pilot_kart->showStarEffect(0.8f);
        addRelayMessage(gui, pilot_kart, message + "armor pulse",
                        120, 170, 255);
    }
    else if (action.m_type == "defender_emergency_bubble")
    {
        giveBubbleShield(pilot_kart, 7.0f);
        pilot_kart->setInvulnerableTicks(timeToTicks(1.0f));
        addRelayMessage(gui, pilot_kart, message + "emergency bubble",
                        120, 170, 255);
    }
    else if (action.m_type == "hacker_open_gate")
    {
        applyPilotBoost(pilot_kart, 1.8f, 1.1f, 1.1f);
        addRelayMessage(gui, pilot_kart, message + "impulso sincronizzato",
                        255, 220, 90);
    }
    else if (action.m_type == "hacker_close_hazard_barrier")
    {
        pilot_kart->setInvulnerableTicks(timeToTicks(1.5f));
        addRelayMessage(gui, pilot_kart, message + "impulso protettivo",
                        255, 220, 90);
    }
    else if (action.m_type == "hacker_rotate_ramp")
    {
        applyPilotBoost(pilot_kart, 2.4f, 1.6f, 1.4f);
        addRelayMessage(gui, pilot_kart, message + "impulso potenza",
                        255, 220, 90);
    }
    else if (action.m_type == "hacker_arm_shortcut")
    {
        applyPilotBoost(pilot_kart, 2.0f, 1.0f, 1.2f);
        addRelayMessage(gui, pilot_kart, message + "bonus squadra",
                        255, 220, 90);
    }
    else if (action.m_type == "hacker_disable_trap")
    {
        pilot_kart->setInvulnerableTicks(timeToTicks(1.2f));
        addRelayMessage(gui, pilot_kart, message + "trap off " + target,
                        255, 220, 90);
    }
    else if (action.m_type == "gunner_lock_target")
    {
        pilot_kart->beep();
        addRelayMessage(gui, pilot_kart, message + "lock " + target,
                        255, 120, 120);
    }
    else if (action.m_type == "gunner_fire_utility")
    {
        slowRival(action, pilot_kart, 2.5f, 0.68f);
        addRelayMessage(gui, pilot_kart, message + "utility fired " + target,
                        255, 120, 120);
    }
    else if (action.m_type == "gunner_emp_zone")
    {
        slowAllRivals(pilot_kart, 2.0f, 0.72f);
        addRelayMessage(gui, pilot_kart, message + "EMP " + target,
                        255, 120, 120);
    }
    else if (action.m_type == "gunner_drop_decoy")
    {
        pilot_kart->setInvulnerableTicks(timeToTicks(1.0f));
        slowRival(action, pilot_kart, 1.4f, 0.82f);
        addRelayMessage(gui, pilot_kart, message + "decoy dropped",
                        255, 120, 120);
    }
    else if (action.m_type == "gunner_tag_rival")
    {
        slowRival(action, pilot_kart, 1.8f, 0.78f);
        addRelayMessage(gui, pilot_kart, message + "tag " + target,
                        255, 120, 120);
    }
}

void CrewActionHandler::applyActions(
    const std::vector<RelayCrewActionRequest>& actions,
    AbstractKart* pilot_kart,
    RaceGUIBase* gui) const
{
    for (unsigned int i = 0; i < actions.size(); i++)
        applyAction(actions[i], pilot_kart, gui);
}

#endif // ENABLE_IMMERSIVE_RELAY
