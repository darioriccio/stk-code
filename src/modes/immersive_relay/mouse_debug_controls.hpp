// SuperTuxKart Immersive Relay - GPLv3 or later.
#ifndef HEADER_THOLUS_MOUSE_DEBUG_CONTROLS_HPP
#define HEADER_THOLUS_MOUSE_DEBUG_CONTROLS_HPP

#include <algorithm>

namespace TholusMouseDebug
{
struct Controls
{
    float steering = 0.0f;
    bool accelerate = false;
    bool brake = false;
    bool fire = false;
};

inline Controls controlsForCursor(int x, int width, bool focused,
                                 bool left, bool right, bool middle)
{
    Controls result;
    if (!focused || width < 2)
        return result;
    result.steering = std::max(-1.0f, std::min(1.0f,
        2.0f * (float)x / (float)(width - 1) - 1.0f));
    result.accelerate = left && !right;
    result.brake = right;
    result.fire = middle;
    return result;
}
}
#endif
