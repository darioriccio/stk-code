//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_RELAY_LOCAL_API

#include "modes/immersive_relay/remote/relay_command_router.hpp"

#include "modes/immersive_relay/relay_session_controller.hpp"
#include "race/race_manager.hpp"
#include "modes/immersive_relay/remote/relay_state_publisher.hpp"
#include "modes/immersive_relay/render/immersive_render_pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

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
        if (literal == "true")
            out->m_bool = true;
        return true;
    }

    bool parseStringValue(std::string* out)
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
            if (!parseStringValue(&key))
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
            return parseStringValue(&out->m_string);
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

    bool parseRootObject(JsonValue* out, std::string* error)
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
                *error = "json_body_must_be_object";
            return false;
        }
        return true;
    }
};

bool parseJsonBody(const std::string& body, JsonValue* root,
                   std::string* error)
{
    if (body.empty())
    {
        root->m_type = JsonValue::JSON_OBJECT;
        return true;
    }
    JsonParser parser(body);
    return parser.parseRootObject(root, error);
}

const JsonValue* objectGet(const JsonValue& object, const std::string& key)
{
    if (object.m_type != JsonValue::JSON_OBJECT)
        return NULL;
    std::map<std::string, JsonValue>::const_iterator found =
        object.m_object.find(key);
    if (found == object.m_object.end())
        return NULL;
    return &found->second;
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

bool getInt(const JsonValue& object, const std::string& key, int* value)
{
    const JsonValue* child = objectGet(object, key);
    if (!child || child->m_type != JsonValue::JSON_NUMBER)
        return false;
    *value = (int)child->m_number;
    return true;
}

bool getPlayerId(const JsonValue& object, const std::string& key,
                 std::string* value)
{
    if (getString(object, key, value))
        return true;
    int int_value = 0;
    if (!getInt(object, key, &int_value))
        return false;
    if (int_value <= 0)
        return false;
    *value = "p" + std::to_string(int_value);
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

bool getBool(const JsonValue& object, const std::string& key, bool* value)
{
    const JsonValue* child = objectGet(object, key);
    if (!child || child->m_type != JsonValue::JSON_BOOL)
        return false;
    *value = child->m_bool;
    return true;
}

const JsonValue* getObject(const JsonValue& object, const std::string& key)
{
    const JsonValue* child = objectGet(object, key);
    if (!child || child->m_type != JsonValue::JSON_OBJECT)
        return NULL;
    return child;
}

bool getBoolFlexible(const JsonValue& object, const std::string& key,
                     bool* value)
{
    if (getBool(object, key, value))
        return true;
    std::string raw;
    if (getString(object, key, &raw))
        return ImmersiveRelay::parseOnOff(raw, value);
    return false;
}

void extractAdaptiveDifficultyConfig(const JsonValue& root,
                                     RelayDifficultyConfig* config)
{
    std::string value;
    int int_value = 0;
    bool bool_value = false;

    const JsonValue* nested = objectGet(root, "adaptive_difficulty");
    if (nested && nested->m_type == JsonValue::JSON_OBJECT)
    {
        if (getBool(*nested, "enabled", &bool_value))
            config->m_enabled = bool_value;
        if (getString(*nested, "profile", &value))
            config->m_profile = value;
        if (getString(*nested, "visibility", &value))
            config->m_visibility = value;
        if (getInt(*nested, "min_intensity", &int_value))
            config->m_min_intensity = int_value;
        if (getInt(*nested, "max_intensity", &int_value))
            config->m_max_intensity = int_value;
    }

    if (getBool(root, "adaptive_difficulty_enabled", &bool_value))
        config->m_enabled = bool_value;
    if (getString(root, "adaptive_difficulty_profile", &value))
        config->m_profile = value;
    if (getString(root, "adaptive_difficulty_visibility", &value))
        config->m_visibility = value;
    if (getInt(root, "adaptive_difficulty_min_intensity", &int_value))
        config->m_min_intensity = int_value;
    if (getInt(root, "adaptive_difficulty_max_intensity", &int_value))
        config->m_max_intensity = int_value;

    *config = RelayDifficultyDirector::sanitizeConfig(*config);
}

bool getStringInObjectOrPayload(const JsonValue& object,
                                const std::string& key,
                                std::string* value)
{
    if (getString(object, key, value))
        return true;
    const JsonValue* payload = objectGet(object, "payload");
    return payload && getString(*payload, key, value);
}

bool applyCrewActionAlias(RelayCrewActionRequest* action)
{
    if (!action || !action->m_type.empty())
        return false;

    if (CrewRoleManager::getActionDefinition(action->m_action_id))
    {
        action->m_type = action->m_action_id;
        return true;
    }

    if (action->m_action_id == "navigator_mark_left_route")
    {
        action->m_type = "nav_select_route";
        if (action->m_target_id.empty())
            action->m_target_id = "ridge_left";
        return true;
    }

    return false;
}

bool getIntInObjectOrPayload(const JsonValue& object,
                             const std::string& key, int* value)
{
    if (getInt(object, key, value))
        return true;
    const JsonValue* payload = objectGet(object, "payload");
    return payload && getInt(*payload, key, value);
}

std::vector<RelayPlayer> extractPlayers(const JsonValue& root)
{
    std::vector<RelayPlayer> players;
    const JsonValue* array = objectGet(root, "players");
    if (!array || array->m_type != JsonValue::JSON_ARRAY)
        return players;

    for (unsigned int i = 0; i < array->m_array.size(); i++)
    {
        const JsonValue& entry = array->m_array[i];
        std::string player_id;
        std::string display_name;
        if (!getString(entry, "player_id", &player_id))
            continue;
        getString(entry, "display_name", &display_name);
        if (display_name.empty())
            display_name = player_id;
        players.push_back(RelayPlayer(player_id, display_name));
    }
    return players;
}

RelaySessionConfig extractSessionConfig(const JsonValue& root,
                                        RelaySessionConfig config)
{
    std::string value;
    int int_value = 0;
    bool bool_value = false;

    if (getString(root, "session_id", &value))
        config.m_session_id = value;
    if (getString(root, "preset", &value))
        config.m_preset = value;
    if (getString(root, "track_id", &value))
        config.m_track_id = value;
    if (getString(root, "kart_id", &value))
        config.m_kart_id = value;
    if (getString(root, "difficulty", &value))
        config.m_difficulty = value;
    if (getInt(root, "ai_karts", &int_value))
        config.m_ai_karts = std::max(0, int_value);
    if (getString(root, "lap_policy", &value))
        config.m_lap_policy = value;
    if (getString(root, "handoff_policy", &value))
    {
        RelayHandoffPolicy policy;
        if (RelaySessionController::parseHandoffPolicy(value, &policy))
            config.m_handoff_policy = policy;
    }
    if (getString(root, "crew_mode", &value))
        config.m_crew_mode = value;
    if (getString(root, "encounter_script", &value))
        config.m_encounter_script = value;
    if (getString(root, "render_profile", &value) ||
        getString(root, "render_preset", &value) ||
        getString(root, "profile", &value))
        config.m_render_profile = value;
    if (getString(root, "output", &value))
        config.m_output = value;
    if (getString(root, "projection", &value))
        config.m_projection = value;
    if (getString(root, "operator_mode", &value))
        config.m_operator_mode = value;
    if (getBool(root, "allow_pause", &bool_value))
        config.m_allow_pause = bool_value;
    if (getBool(root, "allow_force_finish", &bool_value))
        config.m_allow_force_finish = bool_value;
    if (getBool(root, "allow_abort", &bool_value))
        config.m_allow_abort = bool_value;
    if (getBool(root, "handoff_operator_confirm", &bool_value) ||
        getBool(root, "require_handoff_confirm", &bool_value))
        config.m_require_handoff_confirm = bool_value;
    if (getBool(root, "auto_confirm_handoff", &bool_value))
        config.m_auto_confirm_handoff = bool_value;
    if (getInt(root, "briefing_seconds", &int_value))
        config.m_briefing_seconds = std::max(0, int_value);
    if (getInt(root, "countdown_seconds", &int_value))
        config.m_countdown_seconds = std::max(0, int_value);
    if (getInt(root, "gameplay_seconds", &int_value))
        config.m_gameplay_seconds = int_value;
    if (getInt(root, "slot_seconds", &int_value))
        config.m_slot_seconds = int_value;
    if (getInt(root, "player_count", &int_value))
        config.m_player_count = int_value;
    if (getBool(root, "arcade_ui", &bool_value))
        config.m_arcade_ui = bool_value;
#ifdef ENABLE_RELAY_TEST_TOOLS
    if (getBool(root, "test_accelerated_progress", &bool_value))
        config.m_test_accelerated_progress = bool_value;
#endif
    extractAdaptiveDifficultyConfig(root, &config.m_adaptive_difficulty);
    return config;
}

RelayCrewActionRequest extractCrewAction(const JsonValue& root,
                                         const std::string& body)
{
    RelayCrewActionRequest action;
    std::string value;
    int int_value = 0;

    if (getString(root, "session_id", &value))
        action.m_session_id = value;
    if (getString(root, "action_id", &value))
        action.m_action_id = value;
    if (getString(root, "idempotency_key", &value))
        action.m_idempotency_key = value;
    if (getPlayerId(root, "player_id", &value))
        action.m_player_id = value;
    if (getString(root, "role", &value))
    {
        action.m_role_name = value;
        action.m_role = CrewRoleManager::roleFromString(value);
    }
    if (getString(root, "type", &value) ||
        getString(root, "action_type", &value))
        action.m_type = value;
    if (getInt(root, "state_version_seen", &int_value) ||
        getInt(root, "state_version", &int_value))
        action.m_state_version_seen = (unsigned int)std::max(0, int_value);
    if (getIntInObjectOrPayload(root, "amount", &int_value))
        action.m_amount = int_value;
    if (getIntInObjectOrPayload(root, "duration_ms", &int_value))
        action.m_duration_ms = int_value;

    const char* target_keys[] = {
        "route_id", "hazard_id", "note_id", "shortcut_id", "role_id",
        "gate_id", "barrier_id", "ramp_id", "trap_id", "target_id",
        "zone_id", "rival_id", "status_id", "threat_id", "direction"
    };
    for (unsigned int i = 0; i < sizeof(target_keys) / sizeof(target_keys[0]);
         i++)
    {
        if (getStringInObjectOrPayload(root, target_keys[i], &value))
        {
            action.m_target_id = value;
            break;
        }
    }

    const char* mode_keys[] = {
        "mode", "target", "angle_state", "reason"
    };
    for (unsigned int i = 0; i < sizeof(mode_keys) / sizeof(mode_keys[0]);
         i++)
    {
        if (getStringInObjectOrPayload(root, mode_keys[i], &value))
        {
            action.m_mode = value;
            break;
        }
    }

    applyCrewActionAlias(&action);
    action.m_payload_json = body;
    return action;
}

std::string percentDecode(const std::string& in)
{
    std::string out;
    for (std::string::size_type i = 0; i < in.size(); i++)
    {
        if (in[i] == '+')
        {
            out.push_back(' ');
        }
        else if (in[i] == '%' && i + 2 < in.size() &&
                 std::isxdigit((unsigned char)in[i + 1]) &&
                 std::isxdigit((unsigned char)in[i + 2]))
        {
            const std::string hex = in.substr(i + 1, 2);
            out.push_back((char)std::strtol(hex.c_str(), NULL, 16));
            i += 2;
        }
        else
        {
            out.push_back(in[i]);
        }
    }
    return out;
}

std::map<std::string, std::string> parseQuery(const std::string& query)
{
    std::map<std::string, std::string> params;
    std::string::size_type pos = 0;
    while (pos < query.size())
    {
        std::string::size_type amp = query.find('&', pos);
        if (amp == std::string::npos)
            amp = query.size();
        const std::string part = query.substr(pos, amp - pos);
        const std::string::size_type eq = part.find('=');
        if (eq != std::string::npos)
            params[percentDecode(part.substr(0, eq))] =
                percentDecode(part.substr(eq + 1));
        else if (!part.empty())
            params[percentDecode(part)] = "";
        pos = amp + 1;
    }
    return params;
}

std::string splitPathAndQuery(const std::string& raw_path,
                              std::map<std::string, std::string>* query)
{
    const std::string::size_type q = raw_path.find('?');
    if (q == std::string::npos)
        return raw_path;
    *query = parseQuery(raw_path.substr(q + 1));
    return raw_path.substr(0, q);
}

int statusForCrewError(const std::string& error)
{
    if (error == "missing_session_id" ||
        error == "missing_player_id" ||
        error == "missing_action_type" ||
        error == "missing_idempotency_key" ||
        error == "missing_state_version" ||
        error == "future_state_version" ||
        error == "stale_state" ||
        error == "stale_state_version" ||
        error == "invalid_payload" ||
        error == "unknown_crew_action" ||
        error == "unknown_crew_role" ||
        error == "wrong_window" ||
        error == "already_used" ||
        error == "encounter_expired" ||
        error == "role_action_mismatch" ||
        error == "idempotency_key_reused")
    {
        return 400;
    }
    if (error == "wrong_role")
        return 403;
    if (error == "crew_action_rate_limited" || error == "cooldown")
        return 429;
    return 409;
}

int statusForOperatorError(const std::string& error)
{
    if (error == "invalid_player_count" ||
        error == "invalid_render_preset" ||
        error == "invalid_projection" ||
        error == "invalid_output" ||
        error == "invalid_track" ||
        error == "invalid_lap_policy" ||
        error == "invalid_handoff_policy" ||
        error == "invalid_state")
        return 400;
    if (error == "invalid_token")
        return 401;
    if (error == "reset_in_progress")
        return 409;
    return 409;
}

void applyVenueRenderPreset(const std::string& profile,
                            const std::string& output,
                            ImmersiveRelay::ImmersiveRenderConfig* config)
{
    const bool debug = profile == "debug" ||
        profile == "fulldome_debug";
    const bool target = profile == "target" ||
        profile == "fulldome_target";
    config->m_projection_mode =
        ImmersiveRelay::IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER;
    if (debug)
    {
        config->m_domemaster_size = 1024;
        config->m_projection_width = 1024;
        config->m_projection_height = 1024;
        config->m_cubemap_size = 512;
    }
    else if (target)
    {
        config->m_domemaster_size = 2048;
        config->m_projection_width = 2048;
        config->m_projection_height = 2048;
        config->m_cubemap_size = 1024;
    }
    else
    {
        config->m_domemaster_size = 1536;
        config->m_projection_width = 1536;
        config->m_projection_height = 1536;
        config->m_cubemap_size = 768;
    }
    if (!output.empty())
    {
        if (!ImmersiveRelay::parseOutputMode(output, &config->m_output_mode))
            config->m_output_mode = ImmersiveRelay::IMMERSIVE_OUTPUT_NONE;
    }
}

std::string renderConfigJson(
    const RelaySessionSnapshot& snapshot,
    const ImmersiveRelay::ImmersiveRenderConfig& config)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "{";
    out << "\"ok\":true";
    out << ",\"event\":\"render_config_updated\"";
    out << ",\"state_version\":" << snapshot.m_state_version;
    out << ",\"immersive_render_config\":{";
    out << "\"output_mode\":\""
        << ImmersiveRelay::outputModeToString(config.m_output_mode) << "\"";
    out << ",\"projection\":\""
        << ImmersiveRelay::projectionModeToString(config.m_projection_mode)
        << "\"";
    out << ",\"projection_type\":\""
        << ImmersiveRelay::projectionKindToString(config.m_projection_mode)
        << "\"";
    out << ",\"projection_status\":\""
        << ImmersiveRelay::projectionStatusToString(config.m_projection_mode)
        << "\"";
    out << ",\"camera_mode\":\""
        << ImmersiveRelay::cameraModeToString(config.m_camera_mode) << "\"";
    out << ",\"domemaster_size\":" << config.m_domemaster_size;
    out << ",\"projection_width\":" << config.m_projection_width;
    out << ",\"projection_height\":" << config.m_projection_height;
    out << ",\"cubemap_size\":" << config.m_cubemap_size;
    out << ",\"yaw_deg\":" << config.m_yaw_deg;
    out << ",\"pitch_deg\":" << config.m_pitch_deg;
    out << ",\"roll_deg\":" << config.m_roll_deg;
    out << ",\"dome_tilt_deg\":" << config.m_dome_tilt_deg;
    out << ",\"horizon_stabilization\":"
        << (config.m_horizon_stabilization ? "true" : "false");
    out << ",\"roll_limit_deg\":" << config.m_roll_limit_deg;
    out << ",\"forward_offset\":" << config.m_forward_offset;
    out << ",\"vertical_offset\":" << config.m_vertical_offset;
    out << ",\"smoothing_factor\":" << config.m_smoothing_factor;
    out << ",\"syphon_server_name\":\""
        << RelayStatePublisher::jsonEscape(config.m_syphon_server_name)
        << "\"";
    out << ",\"capture\":{";
    out << "\"type\":\"full_sphere_cubemap\"";
    out << ",\"cubemap_size\":" << config.m_cubemap_size;
    out << ",\"faces\":6";
    out << "}";
    out << ",\"projection_settings\":{";
    out << "\"type\":\""
        << ImmersiveRelay::projectionKindToString(config.m_projection_mode)
        << "\"";
    out << ",\"status\":\""
        << ImmersiveRelay::projectionStatusToString(config.m_projection_mode)
        << "\"";
    out << ",\"width\":" << config.m_projection_width;
    out << ",\"height\":" << config.m_projection_height;
    out << ",\"aspect\":\""
        << ImmersiveRelay::projectionAspectToString(config.m_projection_mode)
        << "\"";
    out << ",\"coverage\":\""
        << ImmersiveRelay::projectionCoverageToString(config.m_projection_mode)
        << "\"";
    out << ",\"debug_grid\":"
        << (config.m_debug_grid ? "true" : "false");
    out << "}";
    out << ",\"output\":{";
    out << "\"type\":\""
        << ImmersiveRelay::outputModeToString(config.m_output_mode) << "\"";
    out << ",\"server_name\":\""
        << RelayStatePublisher::jsonEscape(config.m_syphon_server_name)
        << "\"";
    out << "}";
    out << ",\"camera\":{";
    out << "\"mode\":\""
        << ImmersiveRelay::cameraModeToString(config.m_camera_mode) << "\"";
    out << ",\"yaw_deg\":" << config.m_yaw_deg;
    out << ",\"pitch_deg\":" << config.m_pitch_deg;
    out << ",\"roll_deg\":" << config.m_roll_deg;
    out << ",\"dome_tilt_deg\":" << config.m_dome_tilt_deg;
    out << ",\"forward_offset\":" << config.m_forward_offset;
    out << ",\"vertical_offset\":" << config.m_vertical_offset;
    out << ",\"smoothing_factor\":" << config.m_smoothing_factor;
    out << ",\"horizon_stabilization\":"
        << (config.m_horizon_stabilization ? "true" : "false");
    out << ",\"roll_limit_deg\":" << config.m_roll_limit_deg;
    out << "}";
    out << "}}";
    return out.str();
}
}

RelayHttpResponse::RelayHttpResponse()
{
    m_status_code = 200;
}

RelayHttpResponse::RelayHttpResponse(int status_code, const std::string& body)
{
    m_status_code = status_code;
    m_body = body;
}

RelayHttpResponse RelayCommandRouter::route(const std::string& method,
                                            const std::string& path,
                                            const std::string& body,
                                            bool api_active,
                                            int api_port)
{
    RelaySessionController* controller = RelaySessionController::get();
    RelaySessionSnapshot snapshot = controller->getSnapshot();
    std::map<std::string, std::string> query;
    const std::string route_path = splitPathAndQuery(path, &query);

    if (method == "GET" && route_path == "/immersive/v1/health")
    {
        return RelayHttpResponse(200,
            RelayStatePublisher::healthJson(snapshot, api_active, api_port));
    }
    if (method == "GET" && route_path == "/immersive/v1/capabilities")
    {
        return RelayHttpResponse(200,
            RelayStatePublisher::capabilitiesJson(snapshot, api_active,
                                                  api_port));
    }
    if (method == "GET" && route_path == "/immersive/v1/state")
    {
        const std::string player_filter =
            query.find("player_id") == query.end() ? "" : query["player_id"];
        const std::string role_filter =
            query.find("role") == query.end() ? "" : query["role"];
        return RelayHttpResponse(200,
            RelayStatePublisher::stateJson(snapshot, player_filter,
                                           role_filter));
    }
    if (method == "GET" && route_path == "/immersive/v1/operator/state")
    {
        return RelayHttpResponse(200,
            RelayStatePublisher::stateJson(snapshot));
    }
    if (method == "GET" && route_path == "/immersive/v1/events")
    {
        return RelayHttpResponse(501,
            RelayStatePublisher::errorJson("websocket_events_not_implemented_phase_1",
                                           snapshot.m_state_version));
    }
    if (method != "POST")
    {
        return RelayHttpResponse(405,
            RelayStatePublisher::errorJson("method_not_allowed",
                                           snapshot.m_state_version));
    }

    JsonValue root;
    std::string parse_error;
    if (!parseJsonBody(body, &root, &parse_error))
    {
        if (route_path == "/immersive/v1/crew/action")
        {
            return RelayHttpResponse(400,
                RelayStatePublisher::errorJson("invalid_payload",
                                               snapshot.m_state_version));
        }
        return RelayHttpResponse(400,
            RelayStatePublisher::errorJson("malformed_json:" + parse_error,
                                           snapshot.m_state_version));
    }

    if (route_path == "/immersive/v1/operator/configure")
    {
        RelaySessionConfig config = extractSessionConfig(
            root, controller->getDefaultConfig());
        std::vector<RelayPlayer> players = extractPlayers(root);
        std::string reject_reason;
        if (!controller->configureArcadeSession(config, players,
                                                &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_configured"));
    }

    if (route_path == "/immersive/v1/operator/start")
    {
        std::string reject_reason;
        if (!controller->operatorStart(&reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_started"));
    }

    if (route_path == "/immersive/v1/operator/countdown")
    {
        bool skip = false;
        getBool(root, "skip", &skip);
        bool bool_value = false;
        if (getBool(root, "auto_start", &bool_value))
            skip = bool_value;
        std::string reject_reason;
        if (!controller->operatorCountdown(skip, &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_countdown"));
    }

    if (route_path == "/immersive/v1/operator/pause")
    {
        std::string reject_reason;
        if (!controller->operatorPause(&reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_paused"));
    }

    if (route_path == "/immersive/v1/operator/resume")
    {
        std::string reject_reason;
        if (!controller->operatorResume(&reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_resumed"));
    }

    if (route_path == "/immersive/v1/operator/abort")
    {
        std::string reject_reason;
        if (!controller->operatorAbort(&reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_aborted"));
    }

    if (route_path == "/immersive/v1/operator/reset")
    {
        std::string reject_reason;
        if (!controller->operatorReset(&reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_reset"));
    }

    if (route_path == "/immersive/v1/operator/force-finish")
    {
        std::string reject_reason;
        if (!controller->operatorForceFinish(&reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_force_finished"));
    }

    if (route_path == "/immersive/v1/operator/next-state")
    {
        std::string requested_state;
        getString(root, "state", &requested_state);
        std::string reject_reason;
        if (!controller->operatorNextState(requested_state, &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_next_state"));
    }

    if (route_path == "/immersive/v1/operator/handoff/confirm")
    {
        std::string reject_reason;
        if (!controller->confirmHandoff(&reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_handoff_confirmed"));
    }

    if (route_path == "/immersive/v1/operator/render-preset")
    {
        std::string profile;
        std::string output;
        std::string projection = "fisheye_domemaster";
        if (!getString(root, "render_profile", &profile) &&
            !getString(root, "render_preset", &profile))
            getString(root, "profile", &profile);
        getString(root, "output", &output);
        getString(root, "projection", &projection);
#ifndef ENABLE_SYPHON_OUTPUT
        if (output == "syphon")
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(400,
                RelayStatePublisher::errorJson(
                    "immersive_output_unavailable:syphon",
                    snapshot.m_state_version));
        }
#endif
        std::string reject_reason;
        if (!controller->operatorRenderPreset(profile, output, projection,
                                              &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        ImmersiveRelay::ImmersiveRenderConfig render_config =
            ImmersiveRelay::ImmersiveRenderPipeline::get()->getConfig();
        applyVenueRenderPreset(profile.empty() ? "fulldome_safe" : profile,
                               output, &render_config);
        ImmersiveRelay::ImmersiveRenderPipeline::get()->setConfig(
            render_config);
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            renderConfigJson(snapshot, render_config));
    }

    if (route_path == "/immersive/v1/render/config")
    {
        ImmersiveRelay::ImmersiveRenderConfig render_config =
            ImmersiveRelay::ImmersiveRenderPipeline::get()->getConfig();
        std::string value;
        int int_value = 0;
        float float_value = 0.0f;
        bool bool_value = false;

        if (getString(root, "output_mode", &value) ||
            getString(root, "immersive_output_mode", &value) ||
            getString(root, "output", &value))
        {
            if (!ImmersiveRelay::parseOutputMode(
                    value, &render_config.m_output_mode))
            {
                return RelayHttpResponse(400,
                    RelayStatePublisher::errorJson(
                        "unknown_immersive_output_mode",
                        snapshot.m_state_version));
            }
        }
        if (getString(root, "projection", &value) ||
            getString(root, "immersive_projection", &value))
        {
            if (!ImmersiveRelay::parseProjectionMode(
                    value, &render_config.m_projection_mode))
            {
                return RelayHttpResponse(400,
                    RelayStatePublisher::errorJson(
                        "unknown_immersive_projection",
                        snapshot.m_state_version));
            }
        }
        if (getString(root, "camera_mode", &value) ||
            getString(root, "camera", &value))
        {
            if (!ImmersiveRelay::parseCameraMode(
                    value, &render_config.m_camera_mode))
            {
                return RelayHttpResponse(400,
                    RelayStatePublisher::errorJson(
                        "unknown_immersive_camera",
                        snapshot.m_state_version));
            }
        }
        if (getInt(root, "domemaster_size", &int_value))
        {
            render_config.m_domemaster_size =
                ImmersiveRelay::sanitizeDomemasterSize(int_value);
            render_config.m_projection_width = render_config.m_domemaster_size;
            render_config.m_projection_height = render_config.m_domemaster_size;
        }
        if (getInt(root, "projection_width", &int_value))
            render_config.m_projection_width = std::max(1, int_value);
        if (getInt(root, "projection_height", &int_value))
            render_config.m_projection_height = std::max(1, int_value);
        if (getInt(root, "cubemap_size", &int_value))
            render_config.m_cubemap_size =
                ImmersiveRelay::sanitizeCubemapSize(int_value);
        if (getFloat(root, "yaw_deg", &float_value))
            render_config.m_yaw_deg = float_value;
        if (getFloat(root, "pitch_deg", &float_value))
            render_config.m_pitch_deg = float_value;
        if (getFloat(root, "roll_deg", &float_value))
            render_config.m_roll_deg = float_value;
        if (getFloat(root, "dome_tilt_deg", &float_value))
            render_config.m_dome_tilt_deg = float_value;
        if (getBoolFlexible(root, "horizon_stabilization", &bool_value))
            render_config.m_horizon_stabilization = bool_value;
        if (getFloat(root, "roll_limit_deg", &float_value))
            render_config.m_roll_limit_deg = float_value;
        if (getFloat(root, "forward_offset", &float_value))
            render_config.m_forward_offset = float_value;
        if (getFloat(root, "vertical_offset", &float_value))
            render_config.m_vertical_offset = float_value;
        if (getFloat(root, "smoothing_factor", &float_value))
            render_config.m_smoothing_factor = float_value;
        if (getString(root, "syphon_server_name", &value) ||
            getString(root, "server_name", &value))
            render_config.m_syphon_server_name = value;
        if (getBoolFlexible(root, "debug_grid", &bool_value))
            render_config.m_debug_grid = bool_value;

        const JsonValue* capture = getObject(root, "capture");
        if (capture)
        {
            if (getString(*capture, "type", &value) &&
                value != "full_sphere_cubemap")
            {
                return RelayHttpResponse(400,
                    RelayStatePublisher::errorJson(
                        "unknown_immersive_capture",
                        snapshot.m_state_version));
            }
            if (getInt(*capture, "cubemap_size", &int_value))
                render_config.m_cubemap_size =
                    ImmersiveRelay::sanitizeCubemapSize(int_value);
        }

        const JsonValue* projection = getObject(root, "projection");
        if (projection)
        {
            if (getString(*projection, "type", &value) ||
                getString(*projection, "kind", &value))
            {
                if (!ImmersiveRelay::parseProjectionMode(
                        value, &render_config.m_projection_mode))
                {
                    return RelayHttpResponse(400,
                        RelayStatePublisher::errorJson(
                            "unknown_immersive_projection",
                            snapshot.m_state_version));
                }
            }
            if (getInt(*projection, "width", &int_value))
                render_config.m_projection_width = std::max(1, int_value);
            if (getInt(*projection, "height", &int_value))
                render_config.m_projection_height = std::max(1, int_value);
            if (getInt(*projection, "domemaster_size", &int_value))
            {
                render_config.m_domemaster_size =
                    ImmersiveRelay::sanitizeDomemasterSize(int_value);
                render_config.m_projection_width =
                    render_config.m_domemaster_size;
                render_config.m_projection_height =
                    render_config.m_domemaster_size;
            }
            if (getBoolFlexible(*projection, "debug_grid", &bool_value))
                render_config.m_debug_grid = bool_value;
        }

        const JsonValue* output = getObject(root, "output");
        if (output)
        {
            if (getString(*output, "type", &value))
            {
                if (!ImmersiveRelay::parseOutputMode(
                        value, &render_config.m_output_mode))
                {
                    return RelayHttpResponse(400,
                        RelayStatePublisher::errorJson(
                            "unknown_immersive_output_mode",
                            snapshot.m_state_version));
                }
            }
            if (getString(*output, "server_name", &value) ||
                getString(*output, "syphon_server_name", &value))
                render_config.m_syphon_server_name = value;
        }

        const JsonValue* camera = getObject(root, "camera");
        if (camera)
        {
            if (getString(*camera, "mode", &value) ||
                getString(*camera, "camera_mode", &value))
            {
                if (!ImmersiveRelay::parseCameraMode(
                        value, &render_config.m_camera_mode))
                {
                    return RelayHttpResponse(400,
                        RelayStatePublisher::errorJson(
                            "unknown_immersive_camera",
                            snapshot.m_state_version));
                }
            }
            if (getFloat(*camera, "yaw_deg", &float_value))
                render_config.m_yaw_deg = float_value;
            if (getFloat(*camera, "pitch_deg", &float_value))
                render_config.m_pitch_deg = float_value;
            if (getFloat(*camera, "roll_deg", &float_value))
                render_config.m_roll_deg = float_value;
            if (getFloat(*camera, "dome_tilt_deg", &float_value))
                render_config.m_dome_tilt_deg = float_value;
            if (getBoolFlexible(*camera, "horizon_stabilization",
                                &bool_value))
                render_config.m_horizon_stabilization = bool_value;
            if (getFloat(*camera, "roll_limit_deg", &float_value))
                render_config.m_roll_limit_deg = float_value;
            if (getFloat(*camera, "forward_offset", &float_value))
                render_config.m_forward_offset = float_value;
            if (getFloat(*camera, "vertical_offset", &float_value))
                render_config.m_vertical_offset = float_value;
            if (getFloat(*camera, "smoothing_factor", &float_value))
                render_config.m_smoothing_factor = float_value;
        }

        if (render_config.m_output_mode !=
                ImmersiveRelay::IMMERSIVE_OUTPUT_NONE &&
            render_config.m_projection_mode ==
                ImmersiveRelay::IMMERSIVE_PROJECTION_NONE)
        {
            render_config.m_projection_mode =
                ImmersiveRelay::IMMERSIVE_PROJECTION_FISHEYE;
        }
        if (render_config.m_projection_mode ==
                ImmersiveRelay::IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER)
        {
            if (render_config.m_projection_width !=
                    render_config.m_projection_height)
            {
                return RelayHttpResponse(400,
                    RelayStatePublisher::errorJson(
                        "fisheye_domemaster_requires_square_projection",
                        snapshot.m_state_version));
            }
            render_config.m_domemaster_size =
                ImmersiveRelay::sanitizeDomemasterSize(
                    (int)render_config.m_projection_width);
            render_config.m_projection_width = render_config.m_domemaster_size;
            render_config.m_projection_height = render_config.m_domemaster_size;
        }
        ImmersiveRelay::syncLegacyDomemasterSize(&render_config);
        if (render_config.m_projection_mode !=
                ImmersiveRelay::IMMERSIVE_PROJECTION_NONE &&
            !ImmersiveRelay::projectionIsAvailable(
                render_config.m_projection_mode))
        {
            return RelayHttpResponse(400,
                RelayStatePublisher::errorJson(
                    std::string("immersive_projection_unavailable:") +
                        ImmersiveRelay::projectionKindToString(
                            render_config.m_projection_mode),
                    snapshot.m_state_version));
        }
#ifndef ENABLE_SYPHON_OUTPUT
        if (render_config.m_output_mode ==
            ImmersiveRelay::IMMERSIVE_OUTPUT_SYPHON)
        {
            return RelayHttpResponse(400,
                RelayStatePublisher::errorJson(
                    "immersive_output_unavailable:syphon",
                    snapshot.m_state_version));
        }
#endif
        ImmersiveRelay::ImmersiveRenderPipeline::get()->setConfig(
            render_config);
        return RelayHttpResponse(200,
            renderConfigJson(snapshot, render_config));
    }

    if (route_path == "/immersive/v1/session/start")
    {
        RelaySessionConfig config = extractSessionConfig(
            root, controller->getDefaultConfig());
        std::vector<RelayPlayer> players = extractPlayers(root);

        std::string reject_reason;
        if (!controller->startSession(config, players, &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(409,
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "session_started"));
    }

    if (route_path == "/immersive/v1/session/stop")
    {
        controller->stopSession("api_stop");
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "session_stopped"));
    }
    if (route_path == "/immersive/v1/session/pause")
    {
        controller->pauseSession();
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "session_paused"));
    }
    if (route_path == "/immersive/v1/session/resume")
    {
        controller->resumeSession();
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "session_resumed"));
    }
    if (route_path == "/immersive/v1/session/reset")
    {
        controller->resetSession();
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "session_reset"));
    }
    if (route_path == "/immersive/v1/operator/handoff")
    {
        std::string command;
        getString(root, "command", &command);
        if (command == "skip_handoff")
        {
            if (!controller->skipHandoff())
            {
                snapshot = controller->getSnapshot();
                return RelayHttpResponse(409,
                    RelayStatePublisher::errorJson("handoff_not_ready",
                                                   snapshot.m_state_version));
            }
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(200,
                RelayStatePublisher::okJson(snapshot, "handoff_skipped"));
        }

        std::string player_id;
        getString(root, "player_id", &player_id);
        std::string reject_reason;
        if (!controller->forceHandoff(player_id, &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(409,
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "handoff_forced"));
    }
    if (route_path == "/immersive/v1/crew/action")
    {
        RelayCrewActionRequest action = extractCrewAction(root, body);
        if (action.m_session_id.empty())
            action.m_session_id = snapshot.m_session_id;
        RelayCrewActionAck ack;
        std::string reject_reason;
        if (!controller->submitCrewAction(action, &ack, &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForCrewError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version,
                                               ack.m_cooldown_ms));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::crewActionJson(snapshot, ack));
    }
#ifdef ENABLE_RELAY_TEST_TOOLS
    if (route_path == "/immersive/v1/test/progress")
    {
        int lap = 1;
        int checkpoint = -1;
        int progress_x1000 = 0;
        getInt(root, "lap", &lap);
        getInt(root, "checkpoint", &checkpoint);
        if (!getInt(root, "progress_x1000", &progress_x1000))
        {
            int progress_percent = 0;
            if (getInt(root, "progress_percent", &progress_percent))
                progress_x1000 = progress_percent * 10;
        }
        const float progress =
            std::max(0, std::min(1000, progress_x1000)) / 1000.0f;
        std::string reject_reason;
        if (!controller->applyTestProgress(lap, progress, checkpoint,
                                           &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(409,
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "test_progress_applied"));
    }
#endif

    return RelayHttpResponse(404,
        RelayStatePublisher::errorJson("unknown_endpoint",
                                       snapshot.m_state_version));
}

#endif // ENABLE_RELAY_LOCAL_API
