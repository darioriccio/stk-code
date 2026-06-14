//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_IMMERSIVE_RELAY

#include "modes/immersive_relay/crew_event_director.hpp"

#include "io/file_manager.hpp"
#include "utils/log.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
struct JsonValue
{
    enum Type
    {
        JSON_NULL,
        JSON_BOOL,
        JSON_NUMBER,
        JSON_STRING,
        JSON_OBJECT,
        JSON_ARRAY
    };

    Type m_type;
    bool m_bool;
    double m_number;
    std::string m_string;
    std::map<std::string, JsonValue> m_object;
    std::vector<JsonValue> m_array;

    JsonValue() : m_type(JSON_NULL), m_bool(false), m_number(0.0) {}
};

class JsonParser
{
private:
    const std::string& m_text;
    std::string::size_type m_pos;
    std::string m_error;

    void skipSpace()
    {
        while (m_pos < m_text.size() &&
               (m_text[m_pos] == ' ' || m_text[m_pos] == '\t' ||
                m_text[m_pos] == '\n' || m_text[m_pos] == '\r'))
        {
            m_pos++;
        }
    }

    bool consume(char c)
    {
        skipSpace();
        if (m_pos >= m_text.size() || m_text[m_pos] != c)
            return false;
        m_pos++;
        return true;
    }

    bool parseLiteral(const std::string& literal, JsonValue::Type type,
                      JsonValue* out)
    {
        if (m_text.compare(m_pos, literal.size(), literal) != 0)
            return false;
        m_pos += literal.size();
        out->m_type = type;
        out->m_bool = literal == "true";
        return true;
    }

    bool parseString(std::string* out)
    {
        if (!consume('"'))
            return false;
        std::string value;
        while (m_pos < m_text.size())
        {
            const char c = m_text[m_pos++];
            if (c == '"')
            {
                *out = value;
                return true;
            }
            if ((unsigned char)c < 0x20)
            {
                m_error = "control_character_in_string";
                return false;
            }
            if (c != '\\')
            {
                value.push_back(c);
                continue;
            }
            if (m_pos >= m_text.size())
            {
                m_error = "unterminated_escape";
                return false;
            }
            const char esc = m_text[m_pos++];
            switch (esc)
            {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u':
                if (m_pos + 4 > m_text.size())
                {
                    m_error = "bad_unicode_escape";
                    return false;
                }
                value.push_back('?');
                m_pos += 4;
                break;
            default:
                m_error = "bad_string_escape";
                return false;
            }
        }
        m_error = "unterminated_string";
        return false;
    }

    bool parseNumber(JsonValue* out)
    {
        skipSpace();
        const char* start = m_text.c_str() + m_pos;
        char* end_ptr = NULL;
        const double parsed = std::strtod(start, &end_ptr);
        if (end_ptr == start)
            return false;
        m_pos += (std::string::size_type)(end_ptr - start);
        out->m_type = JsonValue::JSON_NUMBER;
        out->m_number = parsed;
        return true;
    }

    bool parseArray(JsonValue* out)
    {
        if (!consume('['))
            return false;
        out->m_type = JsonValue::JSON_ARRAY;
        skipSpace();
        if (consume(']'))
            return true;
        while (m_pos < m_text.size())
        {
            JsonValue item;
            if (!parseValue(&item))
                return false;
            out->m_array.push_back(item);
            skipSpace();
            if (consume(']'))
                return true;
            if (!consume(','))
            {
                m_error = "expected_array_comma";
                return false;
            }
        }
        m_error = "unterminated_array";
        return false;
    }

    bool parseObject(JsonValue* out)
    {
        if (!consume('{'))
            return false;
        out->m_type = JsonValue::JSON_OBJECT;
        skipSpace();
        if (consume('}'))
            return true;
        while (m_pos < m_text.size())
        {
            std::string key;
            if (!parseString(&key))
            {
                if (m_error.empty())
                    m_error = "expected_object_key";
                return false;
            }
            if (!consume(':'))
            {
                m_error = "expected_object_colon";
                return false;
            }
            JsonValue value;
            if (!parseValue(&value))
                return false;
            out->m_object[key] = value;
            skipSpace();
            if (consume('}'))
                return true;
            if (!consume(','))
            {
                m_error = "expected_object_comma";
                return false;
            }
        }
        m_error = "unterminated_object";
        return false;
    }

public:
    JsonParser(const std::string& text) : m_text(text), m_pos(0) {}

    bool parseValue(JsonValue* out)
    {
        skipSpace();
        if (m_pos >= m_text.size())
        {
            m_error = "empty_json";
            return false;
        }
        const char c = m_text[m_pos];
        if (c == '{')
            return parseObject(out);
        if (c == '[')
            return parseArray(out);
        if (c == '"')
        {
            out->m_type = JsonValue::JSON_STRING;
            return parseString(&out->m_string);
        }
        if (c == '-' || (c >= '0' && c <= '9'))
            return parseNumber(out);
        if (parseLiteral("true", JsonValue::JSON_BOOL, out))
            return true;
        if (parseLiteral("false", JsonValue::JSON_BOOL, out))
            return true;
        if (parseLiteral("null", JsonValue::JSON_NULL, out))
            return true;
        m_error = "unexpected_json_token";
        return false;
    }

    bool parseRoot(JsonValue* out, std::string* error)
    {
        if (!parseValue(out))
        {
            if (error)
                *error = m_error.empty() ? "malformed_json" : m_error;
            return false;
        }
        skipSpace();
        if (m_pos != m_text.size())
        {
            if (error)
                *error = "trailing_json";
            return false;
        }
        if (out->m_type != JsonValue::JSON_OBJECT)
        {
            if (error)
                *error = "json_root_must_be_object";
            return false;
        }
        return true;
    }
};

const JsonValue* objectGet(const JsonValue& object, const std::string& key)
{
    if (object.m_type != JsonValue::JSON_OBJECT)
        return NULL;
    std::map<std::string, JsonValue>::const_iterator found =
        object.m_object.find(key);
    return found == object.m_object.end() ? NULL : &found->second;
}

bool getString(const JsonValue& object, const std::string& key,
               std::string* value)
{
    const JsonValue* child = objectGet(object, key);
    if (!child || child->m_type != JsonValue::JSON_STRING)
        return false;
    *value = child->m_string;
    return true;
}

bool getBool(const JsonValue& object, const std::string& key, bool* value)
{
    const JsonValue* child = objectGet(object, key);
    if (!child || child->m_type != JsonValue::JSON_BOOL)
        return false;
    *value = child->m_bool;
    return true;
}

bool getInt(const JsonValue& object, const std::string& key, int* value)
{
    const JsonValue* child = objectGet(object, key);
    if (!child || child->m_type != JsonValue::JSON_NUMBER)
        return false;
    *value = (int)child->m_number;
    return true;
}

bool getFloat(const JsonValue& object, const std::string& key, float* value)
{
    const JsonValue* child = objectGet(object, key);
    if (!child || child->m_type != JsonValue::JSON_NUMBER)
        return false;
    *value = (float)child->m_number;
    return true;
}

std::vector<std::string> getStringArray(const JsonValue& object,
                                        const std::string& key)
{
    std::vector<std::string> values;
    const JsonValue* array = objectGet(object, key);
    if (!array || array->m_type != JsonValue::JSON_ARRAY)
        return values;
    for (unsigned int i = 0; i < array->m_array.size(); i++)
    {
        if (array->m_array[i].m_type == JsonValue::JSON_STRING)
            values.push_back(array->m_array[i].m_string);
    }
    return values;
}

std::vector<int> getIntArray(const JsonValue& object, const std::string& key)
{
    std::vector<int> values;
    const JsonValue* array = objectGet(object, key);
    if (!array || array->m_type != JsonValue::JSON_ARRAY)
        return values;
    for (unsigned int i = 0; i < array->m_array.size(); i++)
    {
        if (array->m_array[i].m_type == JsonValue::JSON_NUMBER)
            values.push_back((int)array->m_array[i].m_number);
    }
    return values;
}

std::map<std::string, std::string> getStringMap(const JsonValue& object,
                                                const std::string& key)
{
    std::map<std::string, std::string> values;
    const JsonValue* map = objectGet(object, key);
    if (!map || map->m_type != JsonValue::JSON_OBJECT)
        return values;
    for (std::map<std::string, JsonValue>::const_iterator i =
             map->m_object.begin(); i != map->m_object.end(); i++)
    {
        if (i->second.m_type == JsonValue::JSON_STRING)
            values[i->first] = i->second.m_string;
    }
    return values;
}

std::string lowerCopy(std::string value)
{
    for (std::string::size_type i = 0; i < value.size(); i++)
        value[i] = (char)std::tolower((unsigned char)value[i]);
    return value;
}

float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

std::string readTextFile(const std::string& path)
{
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in)
        return "";
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool vectorContains(const std::vector<std::string>& values,
                    const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool roleVectorContains(const std::vector<RelayCrewRole>& values,
                        RelayCrewRole value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}
}

CrewEncounterTriggerContext::CrewEncounterTriggerContext()
{
    m_current_lap = 1;
    m_total_laps = 6;
    m_lap_progress = 0.0f;
    m_checkpoint_id = -1;
    m_distance = 0.0f;
    m_active_driver_index = 0;
    m_player_count = 1;
    m_race_phase = "running_lap";
}

RelayCrewPhoneAction::RelayCrewPhoneAction()
{
    m_role = RELAY_CREW_ROLE_UNKNOWN;
    m_amount = 0;
    m_available = true;
    m_cooldown_ms = 0;
}

RelayCrewPhoneState::RelayCrewPhoneState()
{
    m_role = RELAY_CREW_ROLE_UNKNOWN;
    m_lap = 1;
    m_total_laps = 6;
    m_active_driver_index = 0;
    m_remaining_ms = 0;
    m_primary = false;
}

RelayCrewEncounterSnapshot::RelayCrewEncounterSnapshot()
{
    m_active = false;
    m_remaining_ms = 0;
    m_required_actions = 0;
    m_completed_actions = 0;
}

CrewEventDirector::Trigger::Trigger()
{
    m_lap = 0;
    m_lap_every = 0;
    m_lap_offset = 0;
    m_checkpoint = -1;
    m_progress = 0.0f;
    m_preview_progress = 0.08f;
    m_duration_progress = 0.14f;
}

CrewEventDirector::Step::Step()
{
    m_start_ms = 0;
    m_end_ms = 0;
    m_role = RELAY_CREW_ROLE_UNKNOWN;
    m_required = true;
    m_completed = false;
}

CrewEventDirector::Encounter::Encounter()
{
    m_enabled = true;
    m_player_count_min = 1;
    m_player_count_max = 6;
    m_duration_ms = 14000;
    m_fail_score = 0;
    m_success_score = 0;
    m_combo_score = 0;
    m_state = ENCOUNTER_PENDING;
    m_runtime_lap = -1;
    m_active_started_ms = -1;
    m_started_logged = false;
    m_preview_logged = false;
    m_payoff_logged = false;
    m_combo_awarded = false;
}

CrewEventDirector::CrewEventDirector()
{
    m_total_laps = 6;
    m_player_count = 1;
    m_now_ms = 0;
    m_last_active_index = -1;
    m_encounters_started = 0;
    m_encounters_succeeded = 0;
    m_encounters_failed = 0;
    m_combo_count = 0;
}

RelayCrewPhoneAction CrewEventDirector::makeAction(
    const std::string& type, const std::string& label,
    const std::string& target_key, const std::string& target_id,
    const std::string& mode_key, const std::string& mode, int amount)
{
    RelayCrewPhoneAction action;
    action.m_type = type;
    action.m_label = label.empty() ? type : label;
    const RelayCrewActionDefinition* definition =
        CrewRoleManager::getActionDefinition(type);
    if (definition)
        action.m_role = definition->m_role;
    action.m_target_key = target_key;
    action.m_target_id = target_id;
    action.m_mode_key = mode_key;
    action.m_mode = mode;
    action.m_amount = amount;
    return action;
}

CrewEventDirector::Step CrewEventDirector::makeStep(
    const std::string& id, int start_ms, int end_ms, RelayCrewRole role,
    const std::string& title, const std::string& message,
    const std::string& dome_callout,
    const std::vector<RelayCrewPhoneAction>& actions, bool required) const
{
    Step step;
    step.m_id = id;
    step.m_start_ms = start_ms;
    step.m_end_ms = end_ms > start_ms ? end_ms : start_ms + 8000;
    step.m_role = role;
    step.m_title = title;
    step.m_message = message;
    step.m_dome_callout = dome_callout;
    step.m_actions = actions;
    step.m_required = required;
    return step;
}

void CrewEventDirector::configure(const std::string& track_id, int total_laps,
                                  int player_count)
{
    m_track_id = track_id.empty() ? "default" : track_id;
    m_total_laps = total_laps > 0 ? total_laps : 6;
    m_player_count = player_count > 0 ? player_count : 1;
    reset();
    m_track_id = track_id.empty() ? "default" : track_id;
    m_total_laps = total_laps > 0 ? total_laps : 6;
    m_player_count = player_count > 0 ? player_count : 1;
    loadEncounterScripts();
    Log::info("ImmersiveRelay",
              "Crew Event Director loaded %lu encounter definition(s) for track '%s'.",
              (unsigned long)m_encounters.size(), m_track_id.c_str());
}

void CrewEventDirector::reset()
{
    m_encounters.clear();
    m_now_ms = 0;
    m_last_active_index = -1;
    m_context = CrewEncounterTriggerContext();
    m_pending_dome_callout.clear();
    m_pending_track_effects.clear();
    m_role_ready_at_ms.clear();
    m_encounter_ready_at_ms.clear();
    m_combo_ready_at_ms.clear();
    m_encounters_started = 0;
    m_encounters_succeeded = 0;
    m_encounters_failed = 0;
    m_combo_count = 0;
}

bool CrewEventDirector::loadEncounterScripts()
{
    if (!m_track_id.empty() && m_track_id != "default")
    {
        const std::string candidate =
            "immersive_relay/encounters/" + m_track_id +
            "_relay_encounters.json";
        std::string path;
        if (file_manager)
            path = file_manager->getAsset(candidate);
        if (path.empty())
            path = "data/" + candidate;
        if (loadEncounterScriptFile(path))
            return true;
    }

    const std::string fallback =
        "immersive_relay/encounters/default_relay_encounters.json";
    std::string path;
    if (file_manager)
        path = file_manager->getAsset(fallback);
    if (path.empty())
        path = "data/" + fallback;
    return loadEncounterScriptFile(path);
}

bool CrewEventDirector::loadEncounterScriptFile(const std::string& path)
{
    const std::string text = readTextFile(path);
    if (text.empty())
        return false;

    JsonValue root;
    std::string error;
    JsonParser parser(text);
    if (!parser.parseRoot(&root, &error))
    {
        Log::warn("ImmersiveRelay",
                  "Could not parse relay encounter script '%s': %s.",
                  path.c_str(), error.c_str());
        return false;
    }

    std::string script_track_id;
    getString(root, "track_id", &script_track_id);
    if (!script_track_id.empty() && script_track_id != "*" &&
        script_track_id != "default" && script_track_id != m_track_id)
    {
        return false;
    }

    const JsonValue* encounters = objectGet(root, "encounters");
    if (!encounters || encounters->m_type != JsonValue::JSON_ARRAY)
        return false;

    unsigned int added = 0;
    for (unsigned int i = 0; i < encounters->m_array.size(); i++)
    {
        const JsonValue& entry = encounters->m_array[i];
        if (entry.m_type != JsonValue::JSON_OBJECT)
            continue;

        Encounter encounter;
        getString(entry, "id", &encounter.m_id);
        getString(entry, "title", &encounter.m_title);
        if (encounter.m_title.empty())
            encounter.m_title = encounter.m_id;
        getBool(entry, "enabled", &encounter.m_enabled);
        getInt(entry, "player_count_min", &encounter.m_player_count_min);
        getInt(entry, "player_count_max", &encounter.m_player_count_max);

        const JsonValue* trigger = objectGet(entry, "trigger");
        if (trigger && trigger->m_type == JsonValue::JSON_OBJECT)
        {
            getString(*trigger, "type", &encounter.m_trigger.m_type);
            getInt(*trigger, "lap", &encounter.m_trigger.m_lap);
            encounter.m_trigger.m_laps = getIntArray(*trigger, "laps");
            getString(*trigger, "lap_pattern",
                      &encounter.m_trigger.m_lap_pattern);
            getInt(*trigger, "lap_every", &encounter.m_trigger.m_lap_every);
            getInt(*trigger, "lap_offset", &encounter.m_trigger.m_lap_offset);
            getInt(*trigger, "checkpoint", &encounter.m_trigger.m_checkpoint);
            getFloat(*trigger, "progress", &encounter.m_trigger.m_progress);
            getFloat(*trigger, "lap_progress",
                     &encounter.m_trigger.m_progress);
            getFloat(*trigger, "preview_progress",
                     &encounter.m_trigger.m_preview_progress);
            getFloat(*trigger, "duration_progress",
                     &encounter.m_trigger.m_duration_progress);
        }

        int duration_seconds = 0;
        if (getInt(entry, "duration_seconds", &duration_seconds))
            encounter.m_duration_ms = duration_seconds * 1000;

        const std::vector<std::string> role_names =
            getStringArray(entry, "roles");
        for (unsigned int r = 0; r < role_names.size(); r++)
        {
            RelayCrewRole role =
                CrewRoleManager::roleFromString(lowerCopy(role_names[r]));
            if (CrewRoleManager::isCrewRole(role) &&
                !roleVectorContains(encounter.m_roles, role))
            {
                encounter.m_roles.push_back(role);
            }
        }

        const JsonValue* stages = objectGet(entry, "stages");
        if (stages && stages->m_type == JsonValue::JSON_ARRAY)
        {
            for (unsigned int s = 0; s < stages->m_array.size(); s++)
            {
                const JsonValue& stage = stages->m_array[s];
                if (stage.m_type != JsonValue::JSON_OBJECT)
                    continue;

                std::string stage_id;
                std::string role_name;
                std::string title;
                std::string message;
                std::string dome_callout;
                int starts_at = 0;
                int ends_at = 0;
                bool required = true;
                getString(stage, "id", &stage_id);
                getString(stage, "role", &role_name);
                getInt(stage, "starts_at_seconds", &starts_at);
                getInt(stage, "ends_at_seconds", &ends_at);
                getBool(stage, "required", &required);

                const JsonValue* phone = objectGet(stage, "phone");
                if (phone && phone->m_type == JsonValue::JSON_OBJECT)
                {
                    getString(*phone, "title", &title);
                    getString(*phone, "message", &message);
                }
                const JsonValue* debug = objectGet(stage, "dome_debug");
                if (debug && debug->m_type == JsonValue::JSON_OBJECT)
                    getString(*debug, "message", &dome_callout);

                std::vector<RelayCrewPhoneAction> actions;
                const JsonValue* action_array =
                    phone ? objectGet(*phone, "actions") : NULL;
                if (action_array &&
                    action_array->m_type == JsonValue::JSON_ARRAY)
                {
                    for (unsigned int a = 0; a < action_array->m_array.size();
                         a++)
                    {
                        const JsonValue& action = action_array->m_array[a];
                        if (action.m_type != JsonValue::JSON_OBJECT)
                            continue;
                        std::string type;
                        std::string label;
                        std::string target_key;
                        std::string target_id;
                        std::string mode_key;
                        std::string mode;
                        int amount = 0;
                        getString(action, "type", &type);
                        if (type.empty())
                            getString(action, "id", &type);
                        getString(action, "label", &label);
                        getString(action, "target_key", &target_key);
                        getString(action, "target_id", &target_id);
                        getString(action, "mode_key", &mode_key);
                        getString(action, "mode", &mode);
                        getInt(action, "amount", &amount);
                        if (!type.empty())
                        {
                            actions.push_back(makeAction(type, label,
                                target_key, target_id, mode_key, mode,
                                amount));
                        }
                    }
                }

                RelayCrewRole role =
                    CrewRoleManager::roleFromString(lowerCopy(role_name));
                if (CrewRoleManager::isCrewRole(role))
                {
                    encounter.m_steps.push_back(makeStep(stage_id,
                        starts_at * 1000, ends_at * 1000, role, title,
                        message, dome_callout, actions, required));
                    if (!roleVectorContains(encounter.m_roles, role))
                        encounter.m_roles.push_back(role);
                }
            }
        }

        const JsonValue* success = objectGet(entry, "success");
        if (success && success->m_type == JsonValue::JSON_OBJECT)
        {
            encounter.m_required_actions =
                getStringArray(*success, "required_actions");
            getInt(*success, "score", &encounter.m_success_score);
            getInt(*success, "combo_score", &encounter.m_combo_score);
            getString(*success, "combo_tag", &encounter.m_combo_tag);
            encounter.m_success_effects =
                getStringMap(*success, "track_effects");
        }

        const JsonValue* fail = objectGet(entry, "fail");
        if (fail && fail->m_type == JsonValue::JSON_OBJECT)
        {
            getInt(*fail, "score", &encounter.m_fail_score);
            getString(*fail, "soft_penalty",
                      &encounter.m_fail_soft_penalty);
            encounter.m_fail_effects = getStringMap(*fail, "track_effects");
        }

        if (!encounter.m_id.empty() && encounter.m_enabled &&
            !encounter.m_steps.empty())
        {
            m_encounters.push_back(encounter);
            added++;
        }
    }

    if (added > 0)
    {
        Log::info("ImmersiveRelay",
                  "Loaded %u relay encounter(s) from '%s'.",
                  added, path.c_str());
    }
    return added > 0;
}

std::string CrewEventDirector::makeRuntimeEncounterId(
    const Encounter& encounter) const
{
    if (encounter.m_runtime_lap > 0)
    {
        return encounter.m_id + "_lap" +
               std::to_string(encounter.m_runtime_lap);
    }
    return encounter.m_id;
}

std::string CrewEventDirector::stateToString(EncounterState state) const
{
    switch (state)
    {
    case ENCOUNTER_PENDING: return "pending";
    case ENCOUNTER_PREVIEW: return "preview";
    case ENCOUNTER_ACTIVE: return "active";
    case ENCOUNTER_RESOLVED_SUCCESS: return "resolved_success";
    case ENCOUNTER_RESOLVED_FAIL: return "resolved_fail";
    case ENCOUNTER_EXPIRED: return "expired";
    }
    return "pending";
}

bool CrewEventDirector::lapMatches(const Encounter& encounter) const
{
    const int lap = m_context.m_current_lap;
    const Trigger& trigger = encounter.m_trigger;
    if (trigger.m_type == "final_lap" ||
        trigger.m_type == "final_stretch" ||
        trigger.m_lap_pattern == "final")
    {
        return lap == m_context.m_total_laps;
    }
    if (trigger.m_lap > 0)
        return lap == trigger.m_lap;
    if (!trigger.m_laps.empty())
    {
        return std::find(trigger.m_laps.begin(), trigger.m_laps.end(), lap) !=
               trigger.m_laps.end();
    }
    if (trigger.m_lap_every > 0)
    {
        const int offset = trigger.m_lap_offset > 0 ? trigger.m_lap_offset : 1;
        return lap >= offset && ((lap - offset) % trigger.m_lap_every) == 0;
    }
    if (trigger.m_lap_pattern == "odd")
        return (lap % 2) == 1;
    if (trigger.m_lap_pattern == "even")
        return (lap % 2) == 0;
    return true;
}

void CrewEventDirector::resetEncounterRuntime(Encounter* encounter, int lap)
{
    encounter->m_state = ENCOUNTER_PENDING;
    encounter->m_runtime_lap = lap;
    encounter->m_active_started_ms = -1;
    encounter->m_started_logged = false;
    encounter->m_preview_logged = false;
    encounter->m_payoff_logged = false;
    encounter->m_combo_awarded = false;
    encounter->m_accepted_actions.clear();
    for (unsigned int i = 0; i < encounter->m_steps.size(); i++)
        encounter->m_steps[i].m_completed = false;
}

CrewEventDirector::EncounterState CrewEventDirector::evaluateState(
    const Encounter& encounter) const
{
    if (!encounter.m_enabled ||
        m_context.m_player_count < encounter.m_player_count_min ||
        m_context.m_player_count > encounter.m_player_count_max ||
        !lapMatches(encounter))
    {
        return ENCOUNTER_PENDING;
    }

    const Trigger& trigger = encounter.m_trigger;
    const std::string type = trigger.m_type.empty()
        ? "lap_progress" : trigger.m_type;
    float progress = trigger.m_progress;
    if (type == "lap_start")
        progress = 0.02f;
    else if (type == "lap_end")
        progress = 0.88f;
    else if (type == "final_lap")
        progress = 0.06f;
    else if (type == "final_stretch")
        progress = progress > 0.0f ? progress : 0.72f;

    const float preview_start = clamp01(progress - trigger.m_preview_progress);
    const float active_end = clamp01(progress + trigger.m_duration_progress);

    if (type == "lap_checkpoint")
    {
        if (m_context.m_checkpoint_id == trigger.m_checkpoint)
            return ENCOUNTER_ACTIVE;
        if (m_context.m_checkpoint_id == trigger.m_checkpoint - 1 ||
            (m_context.m_lap_progress >= preview_start &&
             m_context.m_lap_progress < progress))
        {
            return ENCOUNTER_PREVIEW;
        }
        if (m_context.m_checkpoint_id > trigger.m_checkpoint ||
            m_context.m_lap_progress > active_end)
        {
            return ENCOUNTER_EXPIRED;
        }
        return ENCOUNTER_PENDING;
    }

    if (m_context.m_lap_progress >= progress &&
        m_context.m_lap_progress <= active_end)
    {
        return ENCOUNTER_ACTIVE;
    }
    if (m_context.m_lap_progress >= preview_start &&
        m_context.m_lap_progress < progress)
    {
        return ENCOUNTER_PREVIEW;
    }
    if (m_context.m_lap_progress > active_end)
        return ENCOUNTER_EXPIRED;
    return ENCOUNTER_PENDING;
}

void CrewEventDirector::queueEffects(
    const std::map<std::string, std::string>& effects)
{
    for (std::map<std::string, std::string>::const_iterator i =
             effects.begin(); i != effects.end(); i++)
    {
        m_pending_track_effects.push_back(
            RelayCrewTrackEffect(i->first, i->second));
    }
}

void CrewEventDirector::updateEncounter(Encounter* encounter)
{
    if (encounter->m_runtime_lap != m_context.m_current_lap &&
        lapMatches(*encounter))
    {
        resetEncounterRuntime(encounter, m_context.m_current_lap);
    }

    if (encounter->m_state == ENCOUNTER_RESOLVED_SUCCESS ||
        encounter->m_state == ENCOUNTER_RESOLVED_FAIL)
    {
        return;
    }

    const EncounterState next_state = evaluateState(*encounter);
    if (next_state == encounter->m_state)
        return;

    if (next_state == ENCOUNTER_PREVIEW)
    {
        encounter->m_state = ENCOUNTER_PREVIEW;
        if (!encounter->m_preview_logged)
        {
            encounter->m_preview_logged = true;
            m_pending_dome_callout = "PREVIEW " + encounter->m_title;
            Log::info("ImmersiveRelay", "Encounter preview %s.",
                      makeRuntimeEncounterId(*encounter).c_str());
        }
    }
    else if (next_state == ENCOUNTER_ACTIVE)
    {
        encounter->m_state = ENCOUNTER_ACTIVE;
        if (encounter->m_active_started_ms < 0)
            encounter->m_active_started_ms = m_now_ms;
        if (!encounter->m_started_logged)
        {
            encounter->m_started_logged = true;
            m_encounters_started++;
            m_pending_dome_callout = "ACTIVE " + encounter->m_title;
            Log::info("ImmersiveRelay", "Encounter start %s.",
                      makeRuntimeEncounterId(*encounter).c_str());
        }
    }
    else if (next_state == ENCOUNTER_EXPIRED)
    {
        encounter->m_state = ENCOUNTER_RESOLVED_FAIL;
        m_encounters_failed++;
        queueEffects(encounter->m_fail_effects);
        m_pending_dome_callout = "MISSED " + encounter->m_title;
        Log::info("ImmersiveRelay", "Encounter fail %s.",
                  makeRuntimeEncounterId(*encounter).c_str());
    }
    else
    {
        encounter->m_state = next_state;
    }
}

bool CrewEventDirector::update(
    const CrewEncounterTriggerContext& context, int now_ms)
{
    const int previous_best = m_last_active_index;
    const int previous_started = m_encounters_started;
    const int previous_success = m_encounters_succeeded;
    const int previous_failed = m_encounters_failed;

    m_context = context;
    m_now_ms = now_ms;

    for (unsigned int i = 0; i < m_encounters.size(); i++)
        updateEncounter(&m_encounters[i]);

    m_last_active_index = getBestEncounterIndex();
    return previous_best != m_last_active_index ||
           previous_started != m_encounters_started ||
           previous_success != m_encounters_succeeded ||
           previous_failed != m_encounters_failed ||
           !m_pending_dome_callout.empty() ||
           !m_pending_track_effects.empty();
}

int CrewEventDirector::getBestEncounterIndex() const
{
    int preview_index = -1;
    for (unsigned int i = 0; i < m_encounters.size(); i++)
    {
        if (m_encounters[i].m_state == ENCOUNTER_ACTIVE)
            return (int)i;
        if (preview_index < 0 &&
            m_encounters[i].m_state == ENCOUNTER_PREVIEW)
        {
            preview_index = (int)i;
        }
    }
    return preview_index;
}

std::string CrewEventDirector::getStageForEncounter(
    const Encounter& encounter) const
{
    if (encounter.m_state == ENCOUNTER_PREVIEW)
        return "encounter_preview";
    if (encounter.m_state == ENCOUNTER_RESOLVED_SUCCESS)
        return "encounter_payoff_success";
    if (encounter.m_state == ENCOUNTER_RESOLVED_FAIL)
        return "encounter_payoff_fail";
    if (encounter.m_state != ENCOUNTER_ACTIVE)
        return "pending";

    const int elapsed = encounter.m_active_started_ms >= 0
        ? m_now_ms - encounter.m_active_started_ms : 0;
    for (unsigned int i = 0; i < encounter.m_steps.size(); i++)
    {
        if (elapsed >= encounter.m_steps[i].m_start_ms &&
            elapsed <= encounter.m_steps[i].m_end_ms)
        {
            return encounter.m_steps[i].m_id;
        }
    }
    return "encounter_active";
}

const CrewEventDirector::Step* CrewEventDirector::getCurrentStepForRole(
    const Encounter& encounter, RelayCrewRole role) const
{
    if (encounter.m_state != ENCOUNTER_ACTIVE)
        return NULL;
    const int elapsed = encounter.m_active_started_ms >= 0
        ? m_now_ms - encounter.m_active_started_ms : 0;
    for (unsigned int i = 0; i < encounter.m_steps.size(); i++)
    {
        const Step& step = encounter.m_steps[i];
        if (step.m_role == role &&
            elapsed >= step.m_start_ms && elapsed <= step.m_end_ms)
        {
            return &step;
        }
    }
    return NULL;
}

CrewEventDirector::Step* CrewEventDirector::getCurrentStepForRole(
    Encounter* encounter, RelayCrewRole role)
{
    if (!encounter || encounter->m_state != ENCOUNTER_ACTIVE)
        return NULL;
    const int elapsed = encounter->m_active_started_ms >= 0
        ? m_now_ms - encounter->m_active_started_ms : 0;
    for (unsigned int i = 0; i < encounter->m_steps.size(); i++)
    {
        Step& step = encounter->m_steps[i];
        if (step.m_role == role &&
            elapsed >= step.m_start_ms && elapsed <= step.m_end_ms)
        {
            return &step;
        }
    }
    return NULL;
}

std::string CrewEventDirector::actionIdentity(
    const RelayCrewActionRequest& action) const
{
    if (!action.m_target_id.empty())
        return action.m_type + ":" + action.m_target_id;
    if (!action.m_mode.empty())
        return action.m_type + ":" + action.m_mode;
    return action.m_type;
}

std::string CrewEventDirector::actionIdentity(
    const RelayCrewPhoneAction& action) const
{
    if (!action.m_target_id.empty())
        return action.m_type + ":" + action.m_target_id;
    if (!action.m_mode.empty())
        return action.m_type + ":" + action.m_mode;
    return action.m_type;
}

bool CrewEventDirector::actionMatchesStep(
    const Step& step, const RelayCrewActionRequest& action) const
{
    for (unsigned int i = 0; i < step.m_actions.size(); i++)
    {
        const RelayCrewPhoneAction& candidate = step.m_actions[i];
        if (candidate.m_type != action.m_type)
            continue;
        if (!candidate.m_target_id.empty() &&
            candidate.m_target_id != action.m_target_id)
            continue;
        if (!candidate.m_mode.empty() && candidate.m_mode != action.m_mode)
            continue;
        return true;
    }
    return false;
}

bool CrewEventDirector::encounterIncludesAction(
    const Encounter& encounter, const RelayCrewActionRequest& action) const
{
    for (unsigned int i = 0; i < encounter.m_steps.size(); i++)
    {
        for (unsigned int a = 0; a < encounter.m_steps[i].m_actions.size();
             a++)
        {
            if (encounter.m_steps[i].m_actions[a].m_type == action.m_type)
                return true;
        }
    }
    return false;
}

bool CrewEventDirector::hasAcceptedAction(
    const Encounter& encounter, const std::string& identity) const
{
    if (vectorContains(encounter.m_accepted_actions, identity))
        return true;
    const std::string::size_type colon = identity.find(':');
    const std::string type = colon == std::string::npos
        ? identity : identity.substr(0, colon);
    if (vectorContains(encounter.m_accepted_actions, type))
        return true;
    for (unsigned int i = 0; i < encounter.m_accepted_actions.size(); i++)
    {
        if (encounter.m_accepted_actions[i].find(type + ":") == 0)
            return true;
    }
    return false;
}

bool CrewEventDirector::requiredActionsSatisfied(
    const Encounter& encounter) const
{
    if (encounter.m_required_actions.empty())
        return countCompletedActions(encounter) >= countRequiredActions(encounter);
    for (unsigned int i = 0; i < encounter.m_required_actions.size(); i++)
    {
        if (!hasAcceptedAction(encounter, encounter.m_required_actions[i]))
            return false;
    }
    return true;
}

bool CrewEventDirector::validateAction(
    const RelayCrewActionRequest& action, std::string* reject_reason) const
{
    bool relevant_expired = false;
    bool relevant_waiting = false;
    bool relevant_already_used = false;

    for (unsigned int i = 0; i < m_encounters.size(); i++)
    {
        const Encounter& encounter = m_encounters[i];
        if (encounter.m_state != ENCOUNTER_ACTIVE)
            continue;
        if (!encounterIncludesAction(encounter, action))
            continue;

        if (!roleVectorContains(encounter.m_roles, action.m_role))
        {
            if (reject_reason)
                *reject_reason = "wrong_role";
            return false;
        }

        const Step* step = getCurrentStepForRole(encounter, action.m_role);
        if (!step)
        {
            if (reject_reason)
                *reject_reason = "wrong_window";
            return false;
        }

        if (!actionMatchesStep(*step, action))
        {
            if (reject_reason)
                *reject_reason = "wrong_window";
            return false;
        }

        if (hasAcceptedAction(encounter, actionIdentity(action)))
        {
            if (reject_reason)
                *reject_reason = "already_used";
            return false;
        }

        const std::string role_key =
            std::string(CrewRoleManager::roleToString(action.m_role));
        std::map<std::string, int>::const_iterator role_ready =
            m_role_ready_at_ms.find(role_key);
        if (role_ready != m_role_ready_at_ms.end() &&
            role_ready->second > m_now_ms)
        {
            if (reject_reason)
                *reject_reason = "cooldown";
            return false;
        }

        const std::string encounter_key = makeRuntimeEncounterId(encounter);
        std::map<std::string, int>::const_iterator encounter_ready =
            m_encounter_ready_at_ms.find(encounter_key);
        if (encounter_ready != m_encounter_ready_at_ms.end() &&
            encounter_ready->second > m_now_ms)
        {
            if (reject_reason)
                *reject_reason = "cooldown";
            return false;
        }

        if (!encounter.m_combo_tag.empty())
        {
            std::map<std::string, int>::const_iterator combo_ready =
                m_combo_ready_at_ms.find(encounter.m_combo_tag);
            if (combo_ready != m_combo_ready_at_ms.end() &&
                combo_ready->second > m_now_ms)
            {
                if (reject_reason)
                    *reject_reason = "cooldown";
                return false;
            }
        }

        return true;
    }

    for (unsigned int i = 0; i < m_encounters.size(); i++)
    {
        const Encounter& encounter = m_encounters[i];
        if (!encounterIncludesAction(encounter, action))
            continue;
        if (hasAcceptedAction(encounter, actionIdentity(action)))
        {
            relevant_already_used = true;
            continue;
        }
        if (encounter.m_state == ENCOUNTER_RESOLVED_FAIL ||
            encounter.m_state == ENCOUNTER_EXPIRED)
        {
            relevant_expired = true;
            continue;
        }
        if (encounter.m_state == ENCOUNTER_PREVIEW ||
            encounter.m_state == ENCOUNTER_PENDING)
        {
            relevant_waiting = true;
            continue;
        }
    }

    if (reject_reason)
    {
        if (relevant_already_used)
            *reject_reason = "already_used";
        else if (relevant_expired && !relevant_waiting)
            *reject_reason = "encounter_expired";
        else
            *reject_reason = "wrong_window";
    }
    return false;
}

int CrewEventDirector::countRequiredActions(const Encounter& encounter) const
{
    if (!encounter.m_required_actions.empty())
        return (int)encounter.m_required_actions.size();
    int count = 0;
    for (unsigned int i = 0; i < encounter.m_steps.size(); i++)
    {
        if (encounter.m_steps[i].m_required)
            count++;
    }
    return count;
}

int CrewEventDirector::countCompletedActions(const Encounter& encounter) const
{
    if (!encounter.m_required_actions.empty())
    {
        int count = 0;
        for (unsigned int i = 0; i < encounter.m_required_actions.size(); i++)
        {
            if (hasAcceptedAction(encounter, encounter.m_required_actions[i]))
                count++;
        }
        return count;
    }
    int count = 0;
    for (unsigned int i = 0; i < encounter.m_steps.size(); i++)
    {
        if (encounter.m_steps[i].m_required &&
            encounter.m_steps[i].m_completed)
        {
            count++;
        }
    }
    return count;
}

int CrewEventDirector::markActionAccepted(
    const RelayCrewActionRequest& action, std::string* effect_suffix)
{
    if (effect_suffix)
        effect_suffix->clear();

    for (unsigned int i = 0; i < m_encounters.size(); i++)
    {
        Encounter& encounter = m_encounters[i];
        if (encounter.m_state != ENCOUNTER_ACTIVE ||
            !encounterIncludesAction(encounter, action))
        {
            continue;
        }

        Step* step = getCurrentStepForRole(&encounter, action.m_role);
        if (!step || !actionMatchesStep(*step, action))
            continue;

        const std::string identity = actionIdentity(action);
        if (!vectorContains(encounter.m_accepted_actions, identity))
            encounter.m_accepted_actions.push_back(identity);
        step->m_completed = true;

        m_role_ready_at_ms[
            std::string(CrewRoleManager::roleToString(action.m_role))] =
                m_now_ms + 250;

        int bonus = 25;
        std::string effect = "encounter_action:" +
            makeRuntimeEncounterId(encounter) + ":" + identity;
        if (!step->m_dome_callout.empty())
            m_pending_dome_callout = step->m_dome_callout;

        Log::info("ImmersiveRelay",
                  "Encounter action accepted encounter=%s action=%s.",
                  makeRuntimeEncounterId(encounter).c_str(), identity.c_str());

        if (requiredActionsSatisfied(encounter))
        {
            encounter.m_state = ENCOUNTER_RESOLVED_SUCCESS;
            m_encounters_succeeded++;
            bonus += encounter.m_success_score;
            queueEffects(encounter.m_success_effects);
            m_pending_dome_callout = "SUCCESS " + encounter.m_title;
            effect = "encounter_success:" + makeRuntimeEncounterId(encounter);
            if (!encounter.m_combo_tag.empty() &&
                !encounter.m_combo_awarded)
            {
                encounter.m_combo_awarded = true;
                m_combo_count++;
                bonus += encounter.m_combo_score;
                m_combo_ready_at_ms[encounter.m_combo_tag] = m_now_ms + 1000;
                effect += "|combo:" + encounter.m_combo_tag;
                m_pending_dome_callout =
                    "COMBO " + encounter.m_combo_tag + "!";
            }
            Log::info("ImmersiveRelay", "Encounter success %s payoff=%d.",
                      makeRuntimeEncounterId(encounter).c_str(), bonus);
        }

        if (effect_suffix)
            *effect_suffix = effect;
        return bonus;
    }
    return 0;
}

std::string CrewEventDirector::consumeDomeCallout()
{
    std::string callout = m_pending_dome_callout;
    m_pending_dome_callout.clear();
    return callout;
}

std::vector<RelayCrewTrackEffect> CrewEventDirector::consumeTrackEffects()
{
    std::vector<RelayCrewTrackEffect> effects = m_pending_track_effects;
    m_pending_track_effects.clear();
    return effects;
}

RelayCrewEncounterSnapshot CrewEventDirector::getSnapshot() const
{
    RelayCrewEncounterSnapshot snapshot;
    const int index = getBestEncounterIndex();
    if (index < 0 || index >= (int)m_encounters.size())
        return snapshot;

    const Encounter& encounter = m_encounters[index];
    snapshot.m_active = encounter.m_state == ENCOUNTER_ACTIVE ||
                        encounter.m_state == ENCOUNTER_PREVIEW;
    snapshot.m_id = makeRuntimeEncounterId(encounter);
    snapshot.m_title = encounter.m_title;
    snapshot.m_stage = getStageForEncounter(encounter);
    snapshot.m_state = stateToString(encounter.m_state);
    snapshot.m_trigger_type = encounter.m_trigger.m_type;
    snapshot.m_combo_tag = encounter.m_combo_tag;
    snapshot.m_dome_callout = m_pending_dome_callout;
    snapshot.m_remaining_ms = 0;
    if (encounter.m_state == ENCOUNTER_ACTIVE &&
        encounter.m_active_started_ms >= 0)
    {
        snapshot.m_remaining_ms = std::max(0,
            encounter.m_duration_ms - (m_now_ms -
                                       encounter.m_active_started_ms));
    }
    snapshot.m_required_actions = countRequiredActions(encounter);
    snapshot.m_completed_actions = countCompletedActions(encounter);
    snapshot.m_roles = encounter.m_roles;
    return snapshot;
}

std::vector<RelayCrewPhoneState> CrewEventDirector::getPhoneStates(
    const std::vector<RelayCrewRoleAssignment>& assignments,
    int boost_charge, int heat, int armor) const
{
    std::vector<RelayCrewPhoneState> states;
    const int best_index = getBestEncounterIndex();
    const Encounter* encounter = best_index >= 0 &&
        best_index < (int)m_encounters.size() ? &m_encounters[best_index] : NULL;

    for (unsigned int i = 0; i < assignments.size(); i++)
    {
        const RelayCrewRoleAssignment& assignment = assignments[i];
        if (assignment.m_is_pilot)
            continue;

        for (unsigned int r = 0; r < assignment.m_roles.size(); r++)
        {
            const RelayCrewRole role = assignment.m_roles[r];
            RelayCrewPhoneState state;
            state.m_player_id = assignment.m_player_id;
            state.m_role = role;
            state.m_lap = m_context.m_current_lap;
            state.m_total_laps = m_context.m_total_laps;
            state.m_active_driver_index = m_context.m_active_driver_index;
            state.m_active_driver_id = m_context.m_active_driver_id;
            state.m_title = std::string(CrewRoleManager::roleToString(role)) +
                " console";
            state.m_message = "Stand by";
            state.m_resource_text =
                "boost=" + std::to_string(boost_charge) +
                " heat=" + std::to_string(heat) +
                " armor=" + std::to_string(armor);

            if (encounter)
            {
                state.m_next_encounter_id = makeRuntimeEncounterId(*encounter);
                state.m_encounter_id = makeRuntimeEncounterId(*encounter);
                if (roleVectorContains(encounter->m_roles, role))
                {
                    state.m_primary = true;
                    state.m_remaining_ms = getSnapshot().m_remaining_ms;
                    const Step* step = getCurrentStepForRole(*encounter, role);
                    if (encounter->m_state == ENCOUNTER_PREVIEW)
                    {
                        state.m_title = encounter->m_title;
                        state.m_message = "Incoming: " + encounter->m_title;
                        for (unsigned int s = 0; s < encounter->m_steps.size();
                             s++)
                        {
                            if (encounter->m_steps[s].m_role != role)
                                continue;
                            state.m_title = encounter->m_steps[s].m_title;
                            state.m_message = encounter->m_steps[s].m_message;
                            state.m_actions = encounter->m_steps[s].m_actions;
                            for (unsigned int a = 0; a < state.m_actions.size();
                                 a++)
                            {
                                state.m_actions[a].m_available = false;
                                state.m_actions[a].m_unavailable_reason =
                                    "wrong_window";
                            }
                            break;
                        }
                    }
                    else if (step)
                    {
                        state.m_title = step->m_title.empty()
                            ? encounter->m_title : step->m_title;
                        state.m_message = step->m_message;
                        state.m_actions = step->m_actions;
                        for (unsigned int a = 0; a < state.m_actions.size();
                             a++)
                        {
                            if (hasAcceptedAction(*encounter,
                                    actionIdentity(state.m_actions[a])))
                            {
                                state.m_actions[a].m_available = false;
                                state.m_actions[a].m_unavailable_reason =
                                    "already_used";
                            }
                        }
                    }
                    else
                    {
                        state.m_message = "Hold for your window";
                    }
                }
                else
                {
                    state.m_message = "Stand by - " + encounter->m_title;
                }
            }

            states.push_back(state);
        }
    }
    return states;
}

#endif // ENABLE_IMMERSIVE_RELAY
