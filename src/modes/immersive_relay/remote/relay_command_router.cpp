//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2026  SuperTuxKart Immersive Relay contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#ifdef ENABLE_RELAY_LOCAL_API

#include "modes/immersive_relay/remote/relay_command_router.hpp"
#include "modes/immersive_relay/spatial/scintilla_engine.hpp"

#include "modes/immersive_relay/relay_session_controller.hpp"
#include "race/race_manager.hpp"
#include "audio/music_manager.hpp"
#include "audio/sfx_manager.hpp"
#include "config/user_config.hpp"
#include "karts/kart_properties_manager.hpp"
#include "karts/kart_properties.hpp"
#include "utils/string_utils.hpp"
#include "modes/immersive_relay/remote/relay_input_control.hpp"
#include "modes/immersive_relay/remote/relay_world_controller.hpp"
#include "modes/immersive_relay/remote/relay_state_publisher.hpp"
#include "modes/immersive_relay/render/immersive_render_pipeline.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
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
    if (!child || child->m_type != JsonValue::JSON_NUMBER ||
        !std::isfinite(child->m_number) ||
        child->m_number < std::numeric_limits<int>::min() ||
        child->m_number > std::numeric_limits<int>::max() ||
        std::floor(child->m_number) != child->m_number)
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
    if (!child || child->m_type != JsonValue::JSON_NUMBER ||
        !std::isfinite(child->m_number) ||
        child->m_number < -std::numeric_limits<float>::max() ||
        child->m_number > std::numeric_limits<float>::max())
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
        config.m_ai_karts = int_value;
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
    float float_value;
    if (getFloat(root, "handoff_seconds", &float_value)) config.m_handoff_seconds = float_value;
    if (getFloat(root, "min_handoff_seconds", &float_value)) config.m_min_handoff_seconds = float_value;
    if (getBool(root, "music_enabled", &bool_value)) config.m_music_enabled = bool_value;
    if (getBool(root, "sfx_enabled", &bool_value)) config.m_sfx_enabled = bool_value;
    if (getFloat(root, "music_volume", &float_value)) config.m_music_volume = float_value;
    if (getFloat(root, "sfx_volume", &float_value)) config.m_sfx_volume = float_value;
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
        error == "invalid_players" ||
        error == "invalid_session_settings" ||
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
                            const std::string& projection,
                            ImmersiveRelay::ImmersiveRenderConfig* config)
{
    const bool debug = profile == "debug" ||
        profile == "fulldome_debug" || profile == "equirectangular_debug";
    const bool target = profile == "target" ||
        profile == "fulldome_target" || profile == "equirectangular_target";
    if (config->m_projection_mode == ImmersiveRelay::IMMERSIVE_PROJECTION_EQUIRECTANGULAR)
        config->m_pitch_deg = 35.0f;
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
    if (projection == "equirectangular" || profile.find("equirectangular_") == 0)
    {
        config->m_projection_mode = ImmersiveRelay::IMMERSIVE_PROJECTION_EQUIRECTANGULAR;
        config->m_projection_width = target ? 4096 : 2048;
        config->m_projection_height = config->m_projection_width / 2;
        config->m_pitch_deg = 0.0f;
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
    out << ",\"application\":\"pending_render_frame\"";
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

    // SC1 stays in the authenticated local API and on the queued game thread.
    // Canonical spatial records use their own closed parser, never the legacy crew parser.
    const std::string crew_prefix="/immersive/v1/scintilla/crew/";
    if(route_path.compare(0,crew_prefix.size(),crew_prefix)==0){
        if(!api_active||!controller->isRuntimeEnabled())return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"relay_runtime_not_enabled\"}");
        using namespace ImmersiveRelay;
        if(route_path==crew_prefix+"state"&&method=="GET"){const auto r=ScintillaEngine::crewState();return RelayHttpResponse(r.status,r.json);}
        for(const auto* operation:{"intent","bindings-confirm","prepare"})if(route_path==crew_prefix+operation&&method=="POST"){
            const auto r=ScintillaEngine::crewCommand(operation,body);return RelayHttpResponse(r.status,r.json);
        }
        return RelayHttpResponse(404,"{\"ok\":false,\"error\":\"unknown_crew_route\"}");
    }
    const std::string mission_prefix="/immersive/v1/scintilla/mission/";
    if(route_path.compare(0,mission_prefix.size(),mission_prefix)==0){
        if(!api_active||!controller->isRuntimeEnabled())return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"relay_runtime_not_enabled\"}");
        using namespace ImmersiveRelay;
        for(const auto* op:{"begin","resume"})if(route_path==mission_prefix+"input-maintenance/"+op&&method=="POST"){
            const auto r=ScintillaEngine::missionInputMaintenanceCommand(op,body);return RelayHttpResponse(r.status,r.json);
        }
        if(route_path==mission_prefix+"measurement/state"&&method=="GET")return RelayHttpResponse(200,ScintillaEngine::missionMeasurementStateJSON());
        if(route_path==mission_prefix+"measurement/trace"&&method=="GET"){
            if(query.size()!=1||!query.count("offset"))return RelayHttpResponse(400,"{\"ok\":false,\"error\":\"measurement_export_offset\"}");
            const auto r=ScintillaEngine::missionMeasurementTrace(query.find("offset")->second);return RelayHttpResponse(r.status,r.json);
        }
        for(const auto* op:{"start","stop","preflight"})if(route_path==mission_prefix+"measurement/"+op&&method=="POST"){
            const auto r=ScintillaEngine::missionMeasurementCommand(op,body);return RelayHttpResponse(r.status,r.json);
        }
        if(route_path==mission_prefix+"state"&&method=="GET")return RelayHttpResponse(200,ScintillaEngine::missionStateJSON());
        if(route_path==mission_prefix+"intent"&&method=="POST"){const auto r=ScintillaEngine::setMissionIntent(body);return RelayHttpResponse(r.status,r.json);}
        if(route_path==mission_prefix+"prepare"&&method=="POST"){const auto r=ScintillaEngine::prepareMission(body);return RelayHttpResponse(r.status,r.json);}
        if(route_path==mission_prefix+"intro-complete"&&method=="POST"){const auto r=ScintillaEngine::completeMissionIntro(body);return RelayHttpResponse(r.status,r.json);}
        return RelayHttpResponse(404,"{\"ok\":false,\"error\":\"unknown_mission_route\"}");
    }
    const std::string defense_prefix="/immersive/v1/scintilla/defense/";
    if(route_path.compare(0,defense_prefix.size(),defense_prefix)==0){
        if(!api_active||!controller->isRuntimeEnabled())return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"relay_runtime_not_enabled\"}");
        using namespace ImmersiveRelay;
        if(route_path==defense_prefix+"state"&&method=="GET")return RelayHttpResponse(200,ScintillaEngine::defenseStateJSON());
        if(route_path==defense_prefix+"intent"&&method=="POST"){const auto r=ScintillaEngine::setDefenseIntent(body);return RelayHttpResponse(r.status,r.json);}
        if(route_path==defense_prefix+"prepare"&&method=="POST"){const auto r=ScintillaEngine::prepareDefense(body);return RelayHttpResponse(r.status,r.json);}
        return RelayHttpResponse(404,"{\"ok\":false,\"error\":\"unknown_defense_route\"}");
    }
    const std::string combat_prefix="/immersive/v1/scintilla/";
    if(route_path.compare(0,combat_prefix.size(),combat_prefix)==0){
        if(!api_active||!controller->isRuntimeEnabled())return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"relay_runtime_not_enabled\"}");
        if(route_path==combat_prefix+"state"&&method=="GET")return RelayHttpResponse(200,ImmersiveRelay::ScintillaEngine::combatStateJSON());
        if(route_path==combat_prefix+"tuning"&&method=="GET")return RelayHttpResponse(200,ImmersiveRelay::ScintillaEngine::tuningStateJSON());
        if(route_path==combat_prefix+"simulation/view"&&method=="GET"){
            const std::string view=ImmersiveRelay::ScintillaEngine::simulationViewJSON();
            return view.empty()?RelayHttpResponse(404,"{\"ok\":false,\"error\":\"simulation_disabled\"}"):RelayHttpResponse(200,view);
        }
        if(route_path==combat_prefix+"tuning/reload"&&method=="POST"){
            const auto reply=ImmersiveRelay::ScintillaEngine::reloadTuning(body);return RelayHttpResponse(reply.status,reply.json);
        }
        if(route_path==combat_prefix+"prepare"&&method=="POST"){
            const auto reply=ImmersiveRelay::ScintillaEngine::prepareCombat(body);return RelayHttpResponse(reply.status,reply.json);
        }
        if(route_path==combat_prefix+"intent"&&method=="POST"){
            const auto reply=ImmersiveRelay::ScintillaEngine::setCombatIntent(body);return RelayHttpResponse(reply.status,reply.json);
        }
        return RelayHttpResponse(404,"{\"ok\":false,\"error\":\"unknown_combat_route\"}");
    }
    const std::string spatial_prefix="/immersive/v1/spatial/";
    if(route_path.compare(0,spatial_prefix.size(),spatial_prefix)==0)
    {
        using namespace ImmersiveRelay;
        if(!api_active || !controller->isRuntimeEnabled())
            return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"relay_runtime_not_enabled\"}");
        const std::string spatial_path="/spatial/"+route_path.substr(spatial_prefix.size());
        if(spatial_path=="/spatial/rebind"){
            if(method!="POST")return RelayHttpResponse(405,"{\"ok\":false,\"error\":\"method_not_allowed\"}");
            const auto r=ScintillaEngine::rebindSpatialParticipants(body);return RelayHttpResponse(r.status,r.json);
        }
        if(spatial_path=="/spatial/enable" || spatial_path=="/spatial/disable") {
            // These opt-in commands contain no configuration or data. Unknown fields fail closed.
            std::string empty; for(char c:body)if(c!=' ' && c!='\t' && c!='\n' && c!='\r')empty+=c;
            if(method!="POST" || empty!="{}")return RelayHttpResponse(400,"{\"ok\":false,\"error\":\"invalid_spatial_switch\"}");
            if(spatial_path=="/spatial/disable") {
                ScintillaEngine::setEnabled(false);
                return RelayHttpResponse(200,"{\"ok\":true,\"accepted\":true}");
            }
            if(!controller->isWorldHeld() || snapshot.m_crew_mode!="disabled" || snapshot.m_player_count<2)
                return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"spatial_requires_prepared_world_and_legacy_crew_disabled\"}");
            ScintillaEngine::setEnabled(true);
            const std::string uuid=ImmersiveRenderPipeline::get()->prepareScintillaOutput();
            if(uuid.empty()) {
                ScintillaEngine::setEnabled(false);
                return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"spatial_requires_syphon_output\"}");
            }
            return RelayHttpResponse(200,"{\"ok\":true,\"accepted\":true,\"serverUUID\":\""+RelayStatePublisher::jsonEscape(uuid)+"\"}");
        }
        if(!ScintillaEngine::enabled())return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"scintilla_input_disabled\"}");
        if(spatial_path=="/spatial/clock" && method=="GET") {
            const int64_t received=ScintillaEngine::nowUs();std::ostringstream out;
            out<<"{\"ok\":true,\"serverReceivedUs\":"<<received<<",\"serverSentUs\":"<<ScintillaEngine::nowUs()<<"}";
            return RelayHttpResponse(200,out.str());
        }
        if(spatial_path=="/spatial/history")
        {
            if(method!="GET")return RelayHttpResponse(405,"{\"ok\":false,\"error\":\"method_not_allowed\"}");
            return RelayHttpResponse(200,ScintillaEngine::historyJSON());
        }
        if((spatial_path=="/spatial/state" && method!="GET") || (spatial_path!="/spatial/state" && method!="POST"))
            return RelayHttpResponse(405,"{\"ok\":false,\"error\":\"method_not_allowed\"}");
        if(spatial_path=="/spatial/prepare"&&ScintillaEngine::missionMeasurementActive())return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"mission_measurement_active\"}");
        if(spatial_path=="/spatial/prepare" && (!controller->isWorldHeld() || snapshot.m_crew_mode!="disabled"))
            return RelayHttpResponse(409,"{\"ok\":false,\"error\":\"spatial_prepare_requires_hold\"}");
        const ImmersiveRenderConfig producer=ImmersiveRenderPipeline::get()->getConfig();
        ScintillaSpatial::Projection projection;projection.kind=projectionKindToString(producer.m_projection_mode);
        projection.width=producer.m_projection_width;projection.height=producer.m_projection_height;projection.domeTiltDeg=producer.m_dome_tilt_deg;
        ScintillaEngine::input().setProducerProjection(projection);
        ScintillaEngine::input().setParticipants(controller->getSpatialCrewIDs());
        ScintillaEngine::refreshCombatInput();
        const RelaySpatialGameplayState gameplay=controller->getSpatialGameplayState();
        const int64_t received=ScintillaEngine::nowUs();
        ScintillaEngine::input().tick(received,gameplay.m_active,gameplay.m_resume_revision,gameplay.m_resume_at_us);
        const ScintillaSpatial::Reply reply=ScintillaEngine::input().route(spatial_path,body,received);
        ScintillaEngine::refreshCombatInput();
        if(spatial_path=="/spatial/prepare"&&reply.status==200)ScintillaEngine::seedPreparedHistory();
        std::string response=reply.json;
        if(spatial_path=="/spatial/update" && reply.status==200 && !response.empty() && response.back()=='}') {
            std::ostringstream clock;clock<<",\"serverReceivedUs\":"<<received<<",\"serverSentUs\":"<<ScintillaEngine::nowUs()<<",\"history\":"<<ScintillaEngine::historyJSON()<<"}";
            response.pop_back();response+=clock.str();
        }
        return RelayHttpResponse(reply.status,response);
    }

    if (method == "GET" && route_path == "/immersive/v1/health")
    {
        return RelayHttpResponse(200,
            RelayStatePublisher::healthJson(snapshot, api_active, api_port));
    }
    if (method == "GET" && route_path == "/immersive/v1/tracks")
        return RelayHttpResponse(200, RelayWorldController::tracksJson());
    if (method == "GET" && route_path == "/immersive/v1/input/devices")
        return RelayHttpResponse(200, RelayInputControl::devicesJson());
    if (method == "GET" && route_path == "/immersive/v1/input/state")
        return RelayHttpResponse(200, RelayInputControl::stateJson());
    if (method == "GET" && route_path == "/immersive/v1/karts")
    {
        if (!kart_properties_manager)
            return RelayHttpResponse(503, RelayStatePublisher::errorJson("kart_inventory_unavailable", snapshot.m_state_version));
        std::ostringstream out;
        out << "{\"ok\":true,\"karts\":[";
        bool first = true;
        for (const std::string& id : kart_properties_manager->getAllAvailableKarts())
        {
            const KartProperties* kart = kart_properties_manager->getKart(id);
            if (!kart) continue;
            if (!first) out << ',';
            first = false;
            out << "{\"id\":\"" << RelayStatePublisher::jsonEscape(id)
                << "\",\"name\":\"" << RelayStatePublisher::jsonEscape(
                    StringUtils::wideToUtf8(kart->getName())) << "\"}";
        }
        out << "]}";
        return RelayHttpResponse(200, out.str());
    }
    if (method == "GET" && route_path == "/immersive/v1/capabilities")
    {
        std::string capabilities=RelayStatePublisher::capabilitiesJson(snapshot,api_active,api_port);
        if(!capabilities.empty() && capabilities.back()=='}') {
            capabilities.pop_back();
            capabilities+=",\"spatial_input_sc1\":{\"contract\":\"tholus.stk-spatial/1\",\"canonical_input\":\"tholus.spatial-input/1\",\"implemented\":true,\"physical_qualification\":\"pending\",\"enabled\":";
            capabilities+=ImmersiveRelay::ScintillaEngine::enabled()?"true":"false";
            capabilities+="},\"scintilla_combat_sc2\":{\"contract\":\"tholus.stk-scintilla-combat/1\",\"profile\":\"crew_standard_sc2_v1\",\"implemented\":true,\"physical_qualification\":\"pending\"}";
            capabilities+=",\"scintilla_mission_sc4\":{\"contract\":\"tholus.stk-scintilla-mission/1\",\"schemaVersion\":1,\"profileId\":\"sc4_cargo_three_slots_v1\",\"profileRevision\":\"1\",\"implemented\":true,\"anchor_qualification\":\"required\",\"layoutProfile\":\"front_rear_cargo_v2\",\"layoutRevision\":\"2\",\"producerUUID\":\""+ImmersiveRelay::ScintillaEngine::producerUUID()+"\"}";
            capabilities+=",\"scintilla_crew_sc5\":{\"contract\":\"tholus.stk-scintilla-crew/1\",\"schemaVersion\":1,\"implemented\":true,\"crewRoles\":true,\"crewHandoffVerification\":true,\"partyBoost\":true,\"guidanceRevision\":\"2\",\"layoutProfile\":\"front_rear_crew_v3\",\"layoutRevision\":\"3\",\"producerUUID\":\""+ImmersiveRelay::ScintillaEngine::producerUUID()+"\",\"profiles\":[\"sc5_crew_standard_v1\",\"sc5_crew_party_v1\"]}}";
        }
        return RelayHttpResponse(200,capabilities);
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

    // The legacy shortcut replaces session/roster while starting immediately.
    // SC2 must use the operator lifecycle with its prepared world and crew.
    if(route_path=="/immersive/v1/session/start"&&ImmersiveRelay::ScintillaEngine::combatRequired())
        return RelayHttpResponse(409,RelayStatePublisher::errorJson("scintilla_combat_requires_operator_start",snapshot.m_state_version));
    std::string next_state;
    if (route_path == "/immersive/v1/operator/next-state")
        getString(root, "state", &next_state);
    const bool next_starts_gameplay = next_state == "briefing" || next_state == "countdown" ||
        next_state == "running" || next_state == "final_lap";
    const bool combat_start=next_starts_gameplay||route_path=="/immersive/v1/operator/start"||route_path=="/immersive/v1/operator/countdown"||
        route_path=="/immersive/v1/operator/resume"||route_path=="/immersive/v1/session/resume"||
        (route_path=="/immersive/v1/operator/ready"&&snapshot.m_arcade_state!=ARCADE_STATE_LAP_HANDOFF&&snapshot.m_arcade_state!=ARCADE_STATE_RESULTS);
    if(combat_start&&!ImmersiveRelay::ScintillaEngine::combatStartPermitted())
        return RelayHttpResponse(409,RelayStatePublisher::errorJson("scintilla_combat_not_prepared",snapshot.m_state_version));

    if ((next_starts_gameplay || route_path == "/immersive/v1/operator/ready" ||
         route_path == "/immersive/v1/operator/resume" ||
         route_path == "/immersive/v1/operator/countdown" ||
         route_path == "/immersive/v1/operator/start" ||
         route_path == "/immersive/v1/operator/handoff/confirm") &&
        !RelayInputControl::isAvailable())
        return RelayHttpResponse(409, RelayStatePublisher::errorJson("input_device_unavailable", snapshot.m_state_version));

    if (route_path == "/immersive/v1/input/config" || route_path == "/immersive/v1/input/configure")
    {
        RelayInputControl::Configuration config;
        std::string error;
        if (objectGet(root, "device_id") && !getString(root, "device_id", &config.device_id))
            error = "invalid_device_id";
        struct IntField { const char* name; int* value; };
        const IntField ints[] = {{"steering_axis", &config.steering_axis},
            {"accelerator_axis", &config.accelerator_axis}, {"brake_axis", &config.brake_axis},
            {"ready_button", &config.ready_button}};
        for (const IntField& field : ints)
            if (objectGet(root, field.name) && !getInt(root, field.name, field.value))
                error = std::string("invalid_") + field.name;
        struct BoolField { const char* name; bool* value; };
        const BoolField bools[] = {{"steering_inverted", &config.steering_inverted},
            {"accelerator_inverted", &config.accelerator_inverted}, {"brake_inverted", &config.brake_inverted},
            {"accelerator_full_range", &config.accelerator_full_range}, {"brake_full_range", &config.brake_full_range}};
        for (const BoolField& field : bools)
            if (objectGet(root, field.name) && !getBool(root, field.name, field.value))
                error = std::string("invalid_") + field.name;
        if (objectGet(root, "deadzone") && !getFloat(root, "deadzone", &config.deadzone))
            error = "invalid_deadzone";
        if (!error.empty() || !RelayInputControl::configure(config, &error))
            return RelayHttpResponse(400, RelayStatePublisher::errorJson(error, snapshot.m_state_version));
        return RelayHttpResponse(200, RelayInputControl::stateJson());
    }
    if (route_path == "/immersive/v1/crew/presence")
    {
        std::string session_id;
        const JsonValue* ids = objectGet(root, "connected_player_ids");
        if (!getString(root, "session_id", &session_id) || !ids ||
            ids->m_type != JsonValue::JSON_ARRAY || ids->m_array.size() > 6)
            return RelayHttpResponse(400, RelayStatePublisher::errorJson("invalid_presence", snapshot.m_state_version));
        std::vector<std::string> connected;
        for (const JsonValue& id : ids->m_array)
        {
            if (id.m_type != JsonValue::JSON_STRING)
                return RelayHttpResponse(400, RelayStatePublisher::errorJson("invalid_connected_player_ids", snapshot.m_state_version));
            connected.push_back(id.m_string);
        }
        std::string error;
        if (!controller->setCrewPresence(session_id, connected, &error))
            return RelayHttpResponse(409, RelayStatePublisher::errorJson(error, snapshot.m_state_version));
        return RelayHttpResponse(200, RelayStatePublisher::okJson(controller->getSnapshot(), "crew_presence_updated"));
    }
    if (route_path == "/immersive/v1/render/readiness")
    {
        bool ready = false;
        if (!getBool(root, "ready", &ready))
            return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                "invalid_presentation_readiness", snapshot.m_state_version));
        controller->setPresentationReady(ready);
        return RelayHttpResponse(200, RelayStatePublisher::okJson(
            controller->getSnapshot(), "presentation_readiness_updated"));
    }
    if (route_path == "/immersive/v1/operator/ready")
    {
        std::string player_id, error;
        if (objectGet(root, "player_id") && !getPlayerId(root, "player_id", &player_id))
            return RelayHttpResponse(400, RelayStatePublisher::errorJson("invalid_player_id", snapshot.m_state_version));
        if (snapshot.m_arcade_state == ARCADE_STATE_RESULTS)
        {
            if (!player_id.empty() && player_id != snapshot.m_rotation.m_active_player_id)
                return RelayHttpResponse(409, RelayStatePublisher::errorJson("not_expected_pilot", snapshot.m_state_version));
            if (!controller->operatorReset(&error))
                return RelayHttpResponse(409, RelayStatePublisher::errorJson(error, snapshot.m_state_version));
            if (!snapshot.m_test_accelerated_progress) RelayWorldController::reset();
            // Results Ready only resets a combat race, even with video held.
            // SC3 retains the physical grant for explicit new-race preparation;
            // ordinary Ready below still enforces both authoritative start gates.
            if (ImmersiveRelay::ScintillaEngine::combatRequired())
                return RelayHttpResponse(200, RelayStatePublisher::okJson(
                    controller->getSnapshot(), "scintilla_combat_reset_requires_prepare"));
            player_id.clear();
        }
        if (!controller->pilotReady(player_id, &error))
            return RelayHttpResponse(409, RelayStatePublisher::errorJson(error, snapshot.m_state_version));
        RelayWorldController::dismissPauseUI();
        return RelayHttpResponse(200, RelayStatePublisher::okJson(controller->getSnapshot(), "pilot_ready"));
    }

    if (route_path == "/immersive/v1/operator/configure")
    {
        const char* integer_fields[] = {"player_count", "ai_karts",
            "countdown_seconds", "briefing_seconds"};
        for (const char* field : integer_fields)
        {
            int value;
            if (objectGet(root, field) && !getInt(root, field, &value))
                return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                    std::string("invalid_") + field, snapshot.m_state_version));
        }
        if (const JsonValue* roster = objectGet(root, "players"))
        {
            if (roster->m_type != JsonValue::JSON_ARRAY || roster->m_array.empty() || roster->m_array.size() > 6)
                return RelayHttpResponse(400, RelayStatePublisher::errorJson("invalid_players", snapshot.m_state_version));
            for (const JsonValue& player : roster->m_array)
            {
                std::string id, name;
                if (player.m_type != JsonValue::JSON_OBJECT || !getString(player, "player_id", &id) || id.empty() ||
                    (objectGet(player, "display_name") && !getString(player, "display_name", &name)))
                    return RelayHttpResponse(400, RelayStatePublisher::errorJson("invalid_players", snapshot.m_state_version));
            }
        }
        const char* float_fields[] = {"handoff_seconds", "min_handoff_seconds", "music_volume", "sfx_volume"};
        for (const char* field : float_fields)
        {
            float value;
            if (objectGet(root, field) && (!getFloat(root, field, &value) || !std::isfinite(value)))
                return RelayHttpResponse(400, RelayStatePublisher::errorJson(std::string("invalid_") + field, snapshot.m_state_version));
        }
        const char* bool_fields[] = {"handoff_operator_confirm", "require_handoff_confirm", "auto_confirm_handoff", "music_enabled", "sfx_enabled"};
        for (const char* field : bool_fields)
        {
            bool value;
            if (objectGet(root, field) && !getBool(root, field, &value))
                return RelayHttpResponse(400, RelayStatePublisher::errorJson(std::string("invalid_") + field, snapshot.m_state_version));
        }
        RelaySessionConfig config = extractSessionConfig(
            root, controller->getDefaultConfig());
        if (!objectGet(root, "session_id"))
            config.m_session_id.clear();
        std::vector<RelayPlayer> players = extractPlayers(root);
        std::string reject_reason;
        if (!RelayWorldController::validate(config, &reject_reason))
            return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                reject_reason, snapshot.m_state_version));
#ifndef ENABLE_SYPHON_OUTPUT
        if (config.m_output == "syphon")
            return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                "immersive_output_unavailable:syphon", snapshot.m_state_version));
#endif
        if (!controller->configureArcadeSession(config, players,
                                                &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        try
        {
            if (!config.m_test_accelerated_progress &&
                !RelayWorldController::load(config, &reject_reason))
                throw std::runtime_error(reject_reason.empty()
                    ? "race_load_failed" : reject_reason);
        }
        catch (const std::exception& error)
        {
            controller->worldLoadFailed(error.what());
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(500, RelayStatePublisher::errorJson(
                "race_load_failed", snapshot.m_state_version));
        }
        UserConfigParams::m_music = config.m_music_enabled;
        UserConfigParams::m_sfx = config.m_sfx_enabled;
        UserConfigParams::m_music_volume = config.m_music_volume;
        UserConfigParams::m_sfx_volume = config.m_sfx_volume;
        if (music_manager) music_manager->setMasterMusicVolume(config.m_music_enabled ? config.m_music_volume : 0.0f);
        if (SFXManager::get())
        {
            SFXManager::get()->toggleSound(config.m_sfx_enabled);
            SFXManager::get()->setMasterSFXVolume(config.m_sfx_volume);
        }
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_configured"));
    }

    if (route_path == "/immersive/v1/operator/start")
    {
        std::string reject_reason;
        if (snapshot.m_arcade_state == ARCADE_STATE_RESULTS)
        {
            if (!controller->operatorReset(&reject_reason))
                return RelayHttpResponse(409, RelayStatePublisher::errorJson(
                    reject_reason, snapshot.m_state_version));
            if (!snapshot.m_test_accelerated_progress)
                RelayWorldController::reset();
        }
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
        RelayWorldController::dismissPauseUI();
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
        RelayWorldController::dismissPauseUI();
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
        ImmersiveRelay::ScintillaEngine::terminateMission("operator_abort");
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
        if (!snapshot.m_test_accelerated_progress)
            RelayWorldController::reset();
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
        ImmersiveRelay::ScintillaEngine::terminateMission("operator_force_finish");
        snapshot = controller->getSnapshot();
        return RelayHttpResponse(200,
            RelayStatePublisher::okJson(snapshot, "operator_force_finished"));
    }

    if (route_path == "/immersive/v1/operator/next-state")
    {
        std::string requested_state;
        if (objectGet(root, "state") && !getString(root, "state", &requested_state))
            return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                "invalid_state", snapshot.m_state_version));
        std::string reject_reason;
        if (!controller->operatorNextState(requested_state, &reject_reason))
        {
            snapshot = controller->getSnapshot();
            return RelayHttpResponse(statusForOperatorError(reject_reason),
                RelayStatePublisher::errorJson(reject_reason,
                                               snapshot.m_state_version));
        }
        if (snapshot.m_arcade_state == ARCADE_STATE_RESULTS &&
            controller->getSnapshot().m_arcade_state == ARCADE_STATE_READY_FOR_PLAYERS &&
            !snapshot.m_test_accelerated_progress)
            RelayWorldController::reset();
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
        std::string projection;
        if (!getString(root, "render_profile", &profile) &&
            !getString(root, "render_preset", &profile))
            getString(root, "profile", &profile);
        getString(root, "output", &output);
        getString(root, "projection", &projection);
        if (projection.empty()) projection = profile.find("equirectangular_") == 0 ? "equirectangular" : "fisheye_domemaster";
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
        if (!snapshot.m_paused && (snapshot.m_arcade_state == ARCADE_STATE_RUNNING ||
            snapshot.m_arcade_state == ARCADE_STATE_FINAL_LAP || snapshot.m_arcade_state == ARCADE_STATE_COUNTDOWN ||
            snapshot.m_arcade_state == ARCADE_STATE_LAP_HANDOFF || snapshot.m_arcade_state == ARCADE_STATE_FINISH_SEQUENCE))
            return RelayHttpResponse(409, RelayStatePublisher::errorJson(
                "render_reconfiguration_requires_pause", snapshot.m_state_version));
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
                               output, projection, &render_config);
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
        const ImmersiveRelay::ImmersiveProjectionMode previous_projection = render_config.m_projection_mode;
        // Reject malformed camera values before changing even the candidate.
        // JSON numbers can be finite doubles but overflow the float camera math.
        const JsonValue* camera_objects[] = { &root, getObject(root, "camera") };
        const char* camera_numbers[] = { "yaw_deg", "pitch_deg", "roll_deg",
            "dome_tilt_deg", "roll_limit_deg", "forward_offset",
            "vertical_offset", "smoothing_factor" };
        for (const JsonValue* object : camera_objects)
        {
            if (!object) continue;
            for (const char* field : camera_numbers)
            {
                float parsed = 0.0f;
                if (objectGet(*object, field) && !getFloat(*object, field, &parsed))
                    return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                        std::string("invalid_immersive_camera_") + field,
                        snapshot.m_state_version));
            }
        }
        const JsonValue* requested_projection = getObject(root, "projection");
        const JsonValue* dimension_objects[] = { &root, requested_projection };
        const char* dimension_keys[][4] = {
            { "projection_width", "projection_height", "domemaster_size", "cubemap_size" },
            { "width", "height", "domemaster_size", NULL }
        };
        bool dimensions_explicit = false;
        for (unsigned int i=0; i<2; i++)
        {
            if (!dimension_objects[i]) continue;
            for (unsigned int j=0; j<4 && dimension_keys[i][j]; j++)
            {
                const JsonValue* field = objectGet(*dimension_objects[i], dimension_keys[i][j]);
                if (!field) continue;
                if (j<2) dimensions_explicit = true;
                if (field->m_type != JsonValue::JSON_NUMBER || !std::isfinite(field->m_number) ||
                    field->m_number < 1 || field->m_number > 8192 ||
                    field->m_number != std::floor(field->m_number))
                    return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                        "invalid_immersive_dimensions", snapshot.m_state_version));
            }
        }
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
        if (render_config.m_projection_mode == ImmersiveRelay::IMMERSIVE_PROJECTION_EQUIRECTANGULAR)
        {
            if (previous_projection != render_config.m_projection_mode && !dimensions_explicit)
            { render_config.m_projection_width=2048; render_config.m_projection_height=1024; }
            if (!ImmersiveRelay::validEquirectDimensions(render_config.m_projection_width, render_config.m_projection_height))
                return RelayHttpResponse(400, RelayStatePublisher::errorJson(
                    "equirectangular_requires_supported_2_to_1_projection", snapshot.m_state_version));
        }
        else if (previous_projection == ImmersiveRelay::IMMERSIVE_PROJECTION_EQUIRECTANGULAR &&
                 render_config.m_projection_mode == ImmersiveRelay::IMMERSIVE_PROJECTION_FISHEYE_DOMEMASTER && !dimensions_explicit)
        {
            render_config.m_projection_width=render_config.m_domemaster_size;
            render_config.m_projection_height=render_config.m_domemaster_size;
        }
        if (previous_projection != render_config.m_projection_mode)
        {
            const JsonValue* camera_request = getObject(root, "camera");
            if (!objectGet(root, "pitch_deg") && (!camera_request || !objectGet(*camera_request, "pitch_deg")))
                render_config.m_pitch_deg = render_config.m_projection_mode == ImmersiveRelay::IMMERSIVE_PROJECTION_EQUIRECTANGULAR ? 0.0f : 35.0f;
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
        const ImmersiveRelay::ImmersiveRenderConfig previous_config =
            ImmersiveRelay::ImmersiveRenderPipeline::get()->getConfig();
        const bool mapping_changed = previous_config.m_projection_mode != render_config.m_projection_mode ||
            previous_config.m_projection_width != render_config.m_projection_width ||
            previous_config.m_projection_height != render_config.m_projection_height ||
            previous_config.m_cubemap_size != render_config.m_cubemap_size ||
            previous_config.m_output_mode != render_config.m_output_mode ||
            previous_config.m_syphon_server_name != render_config.m_syphon_server_name ||
            previous_config.m_yaw_deg != render_config.m_yaw_deg || previous_config.m_pitch_deg != render_config.m_pitch_deg ||
            previous_config.m_roll_deg != render_config.m_roll_deg || previous_config.m_dome_tilt_deg != render_config.m_dome_tilt_deg;
        if (mapping_changed && !snapshot.m_paused &&
            (snapshot.m_arcade_state == ARCADE_STATE_RUNNING || snapshot.m_arcade_state == ARCADE_STATE_FINAL_LAP ||
             snapshot.m_arcade_state == ARCADE_STATE_COUNTDOWN || snapshot.m_arcade_state == ARCADE_STATE_LAP_HANDOFF ||
             snapshot.m_arcade_state == ARCADE_STATE_FINISH_SEQUENCE))
            return RelayHttpResponse(409, RelayStatePublisher::errorJson(
                "render_reconfiguration_requires_pause", snapshot.m_state_version));
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
        ImmersiveRelay::ScintillaEngine::terminateMission("api_stop");
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
        if (!snapshot.m_test_accelerated_progress)
            RelayWorldController::reset();
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
        if(controller->getMissionResult().valid)
            ImmersiveRelay::ScintillaEngine::terminateMission("test_accelerated_progress");
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
