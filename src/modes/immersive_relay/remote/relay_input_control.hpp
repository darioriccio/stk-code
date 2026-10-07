#ifndef HEADER_RELAY_INPUT_CONTROL_HPP
#define HEADER_RELAY_INPUT_CONTROL_HPP

#ifdef ENABLE_RELAY_LOCAL_API
#include <string>

// SDL discovery and gameplay mutations run on the main loop. JSON reads use
// cached values, so the API thread never touches devices or kart controllers.
namespace RelayInputControl
{
struct Configuration
{
    std::string device_id = "keyboard";
    int steering_axis = 0;
    int accelerator_axis = 1;
    int brake_axis = 2;
    int ready_button = 0;
    bool steering_inverted = false;
    bool accelerator_inverted = false;
    bool brake_inverted = false;
    bool accelerator_full_range = true;
    bool brake_full_range = true;
    float deadzone = 0.08f;
};
bool configure(const Configuration& config, std::string* error);
std::string devicesJson();
std::string stateJson();
bool isAvailable();
void update();
// Return true for inputs consumed by the relay, before vanilla dispatch.
bool keyboardInput(int key, int value);
bool joystickInput(int instance_id, bool axis, int index, int value);
float normalizeSteering(int raw, bool inverted, float deadzone);
float normalizePedal(int raw, bool inverted, bool full_range, float deadzone);
}
#endif
#endif
