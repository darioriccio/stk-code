// SuperTuxKart Immersive Relay - GPLv3 or later.
#include "modes/immersive_relay/mouse_debug_controls.hpp"
#include <cstdlib>
#include <initializer_list>
#include <iostream>

static void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main()
{
    using TholusMouseDebug::controlsForCursor;
    require(controlsForCursor(0, 101, true, false, false, false).steering == -1.0f,
            "Left window edge must steer fully left");
    require(controlsForCursor(50, 101, true, false, false, false).steering == 0.0f,
            "Window centre must drive straight");
    require(controlsForCursor(100, 101, true, false, false, false).steering == 1.0f,
            "Right window edge must steer fully right");
    require(controlsForCursor(-50, 101, true, false, false, false).steering == -1.0f &&
            controlsForCursor(150, 101, true, false, false, false).steering == 1.0f,
            "Out-of-window coordinates must stay within the steering range");
    const auto accelerating = controlsForCursor(50, 101, true, true, false, true);
    require(accelerating.accelerate && !accelerating.brake && accelerating.fire,
            "Left and middle mouse buttons must accelerate and use an item");
    const auto braking = controlsForCursor(50, 101, true, true, true, false);
    require(!braking.accelerate && braking.brake, "Braking must override acceleration");
    for (int width : {0, 1, 101})
    {
        const auto inactive = controlsForCursor(100, width, false, true, true, true);
        require(inactive.steering == 0.0f && !inactive.accelerate && !inactive.brake && !inactive.fire,
                "Focus or menu loss must release every driving input");
    }
    for (int width : {0, 1})
    {
        const auto invalid = controlsForCursor(100, width, true, true, true, true);
        require(invalid.steering == 0.0f && !invalid.accelerate && !invalid.brake && !invalid.fire,
                "An unavailable window must never generate driving input");
    }
    return 0;
}
