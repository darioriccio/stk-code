// SuperTuxKart Immersive Relay - GPLv3 or later.
#ifndef HEADER_THOLUS_MOUSE_DEBUG_HPP
#define HEADER_THOLUS_MOUSE_DEBUG_HPP
#include <string>

namespace TholusMouseDebug
{
    void readEarlyCommandLine();
    void applyWindowSettings();
    const std::string& dataRoot();
    bool configureRaceIfEnabled();
    void updateInput(bool in_game);
}
#endif
