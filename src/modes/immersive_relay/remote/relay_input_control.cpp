#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/relay_input_control.hpp"
#include "modes/immersive_relay/remote/relay_game_dispatcher.hpp"
#include "modes/immersive_relay/remote/relay_state_publisher.hpp"
#include "modes/immersive_relay/relay_session_controller.hpp"
#include "input/input_manager.hpp"
#include "input/device_manager.hpp"
#include "input/gamepad_device.hpp"
#include "input/keyboard_device.hpp"
#include "input/device_config.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/player_controller.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include <Keycodes.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>
#ifndef SERVER_ONLY
#include <SDL.h>
#endif

namespace
{
struct Device
{
    std::string id, name;
    int instance = -1;
    int axes = 0, buttons = 0;
    bool connected = true;
};
std::mutex state_mutex;
RelayInputControl::Configuration configuration;
std::vector<Device> devices;
std::vector<int> raw_axes;
std::vector<bool> raw_buttons;
float steering = 0, accelerator = 0, brake = 0;
bool connected = true;
bool ready_down = false, enter_down = false;
bool recovery_required = false;

std::string quoted(const std::string& value)
{
    return "\"" + RelayStatePublisher::jsonEscape(value) + "\"";
}

bool needsReady(const RelaySessionSnapshot& s)
{
    bool recovering;
    { std::lock_guard<std::mutex> lock(state_mutex); recovering = recovery_required; }
    return s.m_arcade_state == ARCADE_STATE_READY_FOR_PLAYERS ||
        s.m_arcade_state == ARCADE_STATE_BRIEFING ||
        s.m_arcade_state == ARCADE_STATE_LAP_HANDOFF ||
        s.m_arcade_state == ARCADE_STATE_RESULTS ||
        (s.m_arcade_state == ARCADE_STATE_PAUSED && recovering);
}

void resetKartInput()
{
    World* world = World::getWorld();
    if (!world || RaceManager::get()->getNumPlayers() < 1) return;
    AbstractKart* kart = world->getPlayerKart(0);
    if (kart)
    {
        PlayerController* controller = dynamic_cast<PlayerController*>(kart->getController());
        if (controller) controller->resetInputState();
    }
}

void writeConfiguration(std::ostream& out, const RelayInputControl::Configuration& c)
{
    out << "{\"device_id\":" << quoted(c.device_id)
        << ",\"steering_axis\":" << c.steering_axis
        << ",\"accelerator_axis\":" << c.accelerator_axis
        << ",\"brake_axis\":" << c.brake_axis
        << ",\"ready_button\":" << c.ready_button
        << ",\"steering_inverted\":" << (c.steering_inverted ? "true" : "false")
        << ",\"accelerator_inverted\":" << (c.accelerator_inverted ? "true" : "false")
        << ",\"brake_inverted\":" << (c.brake_inverted ? "true" : "false")
        << ",\"accelerator_full_range\":" << (c.accelerator_full_range ? "true" : "false")
        << ",\"brake_full_range\":" << (c.brake_full_range ? "true" : "false")
        << ",\"deadzone\":" << c.deadzone << '}';
}
}

float RelayInputControl::normalizeSteering(int raw, bool inverted, float deadzone)
{
    float value = std::max(-1.0f, std::min(1.0f, raw / 32768.0f));
    if (inverted) value = -value;
    const float magnitude = std::fabs(value);
    if (magnitude <= deadzone) return 0.0f;
    return std::copysign((magnitude - deadzone) / (1.0f - deadzone), value);
}

float RelayInputControl::normalizePedal(int raw, bool inverted, bool full_range, float deadzone)
{
    float value = std::max(-1.0f, std::min(1.0f, raw / 32768.0f));
    if (inverted) value = -value;
    value = full_range ? (value + 1.0f) * 0.5f : std::max(0.0f, value);
    if (value <= deadzone) return 0.0f;
    return std::min(1.0f, (value - deadzone) / (1.0f - deadzone));
}

bool RelayInputControl::configure(const Configuration& config, std::string* error)
{
    const RelaySessionSnapshot s = RelaySessionController::get()->getSnapshot();
    if (s.m_arcade_state == ARCADE_STATE_RUNNING ||
        s.m_arcade_state == ARCADE_STATE_FINAL_LAP ||
        s.m_arcade_state == ARCADE_STATE_LAP_HANDOFF ||
        s.m_arcade_state == ARCADE_STATE_COUNTDOWN)
    {
        *error = "input_configuration_requires_pause";
        return false;
    }
    if (!std::isfinite(config.deadzone) || config.deadzone < 0.0f || config.deadzone > 0.5f)
    {
        *error = "invalid_input_deadzone";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (config.device_id != "keyboard")
        {
            const auto device = std::find_if(devices.begin(), devices.end(),
                [&](const Device& d) { return d.id == config.device_id && d.connected; });
            if (device == devices.end()) { *error = "input_device_unavailable"; return false; }
            if (config.steering_axis < 0 || config.steering_axis >= device->axes ||
                config.accelerator_axis < 0 || config.accelerator_axis >= device->axes ||
                config.brake_axis < 0 || config.brake_axis >= device->axes ||
                config.ready_button < 0 || config.ready_button >= device->buttons ||
                config.steering_axis == config.accelerator_axis ||
                config.steering_axis == config.brake_axis)
            { *error = "invalid_input_binding"; return false; }
        }
        configuration = config;
        // Require a release before a held confirmation button can start a race.
        ready_down = true;
    }
    resetKartInput();
    update();
    return true;
}

std::string RelayInputControl::devicesJson()
{
    std::lock_guard<std::mutex> lock(state_mutex);
    std::ostringstream out;
    out << "{\"ok\":true,\"selected_device_id\":" << quoted(configuration.device_id)
        << ",\"devices\":[";
    for (unsigned i = 0; i < devices.size(); i++)
    {
        const Device& d = devices[i];
        if (i) out << ',';
        out << "{\"id\":" << quoted(d.id) << ",\"name\":" << quoted(d.name)
            << ",\"kind\":" << quoted(d.id == "keyboard" ? "keyboard" : "joystick")
            << ",\"connected\":" << (d.connected ? "true" : "false")
            << ",\"axis_count\":" << d.axes << ",\"button_count\":" << d.buttons << '}';
    }
    out << "]}";
    return out.str();
}

std::string RelayInputControl::stateJson()
{
    std::lock_guard<std::mutex> lock(state_mutex);
    std::ostringstream out;
    out << "{\"ok\":true,\"configuration\":";
    writeConfiguration(out, configuration);
    out << ",\"connected\":" << (connected ? "true" : "false")
        << ",\"recovery_required\":" << (recovery_required ? "true" : "false") << ",\"axes\":[";
    for (unsigned i = 0; i < raw_axes.size(); i++) { if (i) out << ','; out << raw_axes[i]; }
    out << "],\"buttons\":[";
    for (unsigned i = 0; i < raw_buttons.size(); i++)
    { if (i) out << ','; out << (raw_buttons[i] ? "true" : "false"); }
    out << "],\"steering\":" << steering << ",\"accelerator\":" << accelerator
        << ",\"brake\":" << brake << '}';
    return out.str();
}

bool RelayInputControl::isAvailable()
{
    std::lock_guard<std::mutex> lock(state_mutex);
    return connected;
}

void RelayInputControl::update()
{
    if (!RelaySessionController::get()->isRuntimeEnabled()) return;
    Configuration c;
    { std::lock_guard<std::mutex> lock(state_mutex); c = configuration; }
    std::vector<Device> found;
    Device keyboard; keyboard.id = "keyboard"; keyboard.name = "Tastiera";
    found.push_back(keyboard);
    std::vector<int> axes;
    std::vector<bool> buttons;
    bool available = c.device_id == "keyboard";
#ifndef SERVER_ONLY
    std::map<std::string, int> occurrences;
    SDL_Joystick* selected = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
    {
        const int instance = SDL_JoystickGetDeviceInstanceID(i);
        SDL_Joystick* joystick = SDL_JoystickFromInstanceID(instance);
        if (!joystick || !SDL_JoystickGetAttached(joystick)) continue;
        char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
        const std::string signature(guid);
        Device d;
        d.id = "sdl:" + signature + ":" + std::to_string(occurrences[signature]++);
        const char* name = SDL_JoystickName(joystick);
        d.name = name ? name : "Dispositivo SDL";
        d.instance = instance;
        d.axes = SDL_JoystickNumAxes(joystick);
        d.buttons = SDL_JoystickNumButtons(joystick);
        found.push_back(d);
        if (d.id == c.device_id) selected = joystick;
    }
    if (selected)
    {
        available = true;
        for (int i = 0; i < SDL_JoystickNumAxes(selected); i++) axes.push_back(SDL_JoystickGetAxis(selected, i));
        for (int i = 0; i < SDL_JoystickNumButtons(selected); i++) buttons.push_back(SDL_JoystickGetButton(selected, i) != 0);
    }
#endif
    float steer = 0, accel = 0, braking = 0;
    if (available && c.device_id != "keyboard" && c.steering_axis < (int)axes.size() &&
        c.accelerator_axis < (int)axes.size() && c.brake_axis < (int)axes.size())
    {
        steer = normalizeSteering(axes[c.steering_axis], c.steering_inverted, c.deadzone);
        accel = normalizePedal(axes[c.accelerator_axis], c.accelerator_inverted, c.accelerator_full_range, c.deadzone);
        braking = normalizePedal(axes[c.brake_axis], c.brake_inverted, c.brake_full_range, c.deadzone);
    }
    bool unplugged = false, confirm = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        unplugged = connected && !available;
        if (unplugged) recovery_required = true;
        if (!available)
        {
            const auto old = std::find_if(devices.begin(), devices.end(), [&](const Device& d) { return d.id == c.device_id; });
            if (old != devices.end()) { Device missing = *old; missing.connected = false; found.push_back(missing); }
        }
        devices = found;
        raw_axes = axes; raw_buttons = buttons;
        connected = available;
        steering = steer; accelerator = accel; brake = braking;
        const bool pressed = available && c.device_id != "keyboard" && c.ready_button >= 0 &&
            c.ready_button < (int)buttons.size() && buttons[c.ready_button];
        confirm = pressed && !ready_down;
        ready_down = pressed;
    }
    if (!available)
    {
        resetKartInput();
        if (unplugged)
        {
            std::string ignored;
            RelaySessionController::get()->operatorPause(&ignored);
        }
        return;
    }
    const RelaySessionSnapshot s = RelaySessionController::get()->getSnapshot();
    if (s.m_arcade_state != ARCADE_STATE_PAUSED && !RelaySessionController::get()->isWorldHeld())
    { std::lock_guard<std::mutex> lock(state_mutex); recovery_required = false; }
    if (confirm && needsReady(s)) RelayGameDispatcher::enqueueLocalReady();
    if (RelaySessionController::get()->isHumanInputLocked())
    {
        resetKartInput();
        return;
    }
    if (c.device_id == "keyboard") return;
    World* world = World::getWorld();
    if (!world || RaceManager::get()->getNumPlayers() < 1) return;
    AbstractKart* kart = world->getPlayerKart(0);
    if (!kart || !kart->getController()) return;
    Controller* controller = kart->getController();
    controller->action(PA_STEER_LEFT, (int)(std::max(0.0f, -steer) * Input::MAX_VALUE));
    controller->action(PA_STEER_RIGHT, (int)(std::max(0.0f, steer) * Input::MAX_VALUE));
    controller->action(PA_ACCEL, (int)(accel * Input::MAX_VALUE));
    controller->action(PA_BRAKE, (int)(braking * Input::MAX_VALUE));
}

bool RelayInputControl::keyboardInput(int key, int value)
{
    if (!RelaySessionController::get()->isRuntimeEnabled()) return false;
    std::string selected;
    bool confirm = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        selected = configuration.device_id;
        if (key == irr::IRR_KEY_RETURN)
        {
            confirm = value != 0 && !enter_down;
            enter_down = value != 0;
        }
    }
    if (key == irr::IRR_KEY_RETURN && selected == "keyboard")
    {
        const RelaySessionSnapshot s = RelaySessionController::get()->getSnapshot();
        if (needsReady(s))
        {
            if (confirm) RelayGameDispatcher::enqueueLocalReady();
            return true;
        }
    }
    if (selected == "keyboard" || !input_manager) return false;
    KeyboardDevice* device = input_manager->getDeviceManager()->getKeyboardFromBtnID(key);
    return device && device->getConfiguration()->hasBindingFor(key, PA_STEER_LEFT, PA_LOOK_BACK);
}

bool RelayInputControl::joystickInput(int instance_id, bool axis, int index, int value)
{
    if (!RelaySessionController::get()->isRuntimeEnabled()) return false;
    bool confirm = false, consumed = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        const auto selected = std::find_if(devices.begin(), devices.end(),
            [&](const Device& d) { return d.id == configuration.device_id && d.connected; });
        if (selected == devices.end() || selected->instance != instance_id) return true;
        if (!axis && index == configuration.ready_button)
        {
            confirm = value != 0 && !ready_down;
            ready_down = value != 0;
            consumed = true;
        }
        else if (axis)
            consumed = index == configuration.steering_axis || index == configuration.accelerator_axis ||
                index == configuration.brake_axis;
    }
    if (confirm && needsReady(RelaySessionController::get()->getSnapshot()))
        RelayGameDispatcher::enqueueLocalReady();
    return consumed;
}
#endif
