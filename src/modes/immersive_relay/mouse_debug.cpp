// SuperTuxKart Immersive Relay - GPLv3 or later.
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/mouse_debug.hpp"
#include "modes/immersive_relay/mouse_debug_controls.hpp"
#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/screen_keyboard.hpp"
#include "input/input.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "states_screens/state_manager.hpp"
#include "utils/command_line.hpp"
#include "utils/log.hpp"
#include <cstdlib>
#include <stdexcept>
#ifndef WIN32
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifndef SERVER_ONLY
#include <SDL.h>
#include <IrrlichtDevice.h>
#endif

namespace TholusMouseDebug
{
namespace
{
bool enabled = false;
std::string private_data_root;
}

void readEarlyCommandLine()
{
    enabled = CommandLine::has("--tholus-mouse-debug");
    private_data_root.clear();
    if (!enabled)
        return;
#ifdef WIN32
    throw std::runtime_error("Native mouse debug data isolation is unavailable on this platform.");
#else
    const char* root = std::getenv("THOLUS_MOUSE_DEBUG_DATA_ROOT");
    struct stat attributes;
    char canonical[PATH_MAX];
    if (!root || root[0] != '/' || !realpath(root, canonical) ||
        std::string(root) != canonical || lstat(root, &attributes) != 0 ||
        !S_ISDIR(attributes.st_mode) || attributes.st_uid != getuid() ||
        (attributes.st_mode & 077) != 0 || access(root, W_OK | X_OK) != 0)
    {
        throw std::runtime_error("Mouse debug requires THOLUS_MOUSE_DEBUG_DATA_ROOT to name an existing canonical private writable folder owned by your account.");
    }
    private_data_root = canonical;
#endif
}

void applyWindowSettings()
{
    if (enabled)
        UserConfigParams::m_fullscreen = false;
}

const std::string& dataRoot()
{
    return private_data_root;
}

bool configureRaceIfEnabled()
{
    if (!enabled)
        return false;
    UserConfigParams::m_no_start_screen = true;
    UserConfigParams::m_race_now = true;
    UserConfigParams::m_default_keyboard = 0;
    UserConfigParams::m_default_gamepad = -1;
    RaceManager::get()->setNumPlayers(1);
    RaceManager::get()->setNumKarts(4);
    RaceManager::get()->setTrack("snowtuxpeak");
    RaceManager::get()->setNumLaps(3);
    RaceManager::get()->setMinorMode(RaceManager::MINOR_MODE_NORMAL_RACE);
    RaceManager::get()->setDifficulty(RaceManager::DIFFICULTY_EASY);
#ifndef SERVER_ONLY
    irr_driver->getDevice()->setWindowCaption(
        L"SuperTuxKart - Mouse debug | Move: steer | Hold left: accelerate | Hold right: brake | Middle: item");
#endif
    Log::info("TholusMouseDebug", "Standalone mouse driving; relay, API and immersive output are not enabled.");
    return true;
}

void updateInput(bool in_game)
{
#ifndef SERVER_ONLY
    if (!enabled || !World::getWorld())
        return;
    AbstractKart* kart = World::getWorld()->getPlayerKart(0);
    if (!kart || !kart->getController())
        return;
    SDL_Window* window = SDL_GetKeyboardFocus();
    const bool active = in_game && window && SDL_GetMouseFocus() == window &&
        StateManager::get()->getGameState() == GUIEngine::GAME &&
        !GUIEngine::ModalDialog::isADialogActive() && !GUIEngine::ScreenKeyboard::isActive() &&
        !RaceManager::get()->isWatchingReplay();
    int width = 0, height = 0, x = 0, y = 0;
    const Uint32 buttons = SDL_GetMouseState(&x, &y);
    if (window)
        SDL_GetWindowSize(window, &width, &height);
    const Controls controls = controlsForCursor(x, width, active,
        (buttons & SDL_BUTTON_LMASK) != 0, (buttons & SDL_BUTTON_RMASK) != 0,
        (buttons & SDL_BUTTON_MMASK) != 0);
    Controller* controller = kart->getController();
    controller->action(PA_STEER_LEFT, controls.steering < 0 ? (int)(-controls.steering * Input::MAX_VALUE) : 0);
    controller->action(PA_STEER_RIGHT, controls.steering > 0 ? (int)(controls.steering * Input::MAX_VALUE) : 0);
    controller->action(PA_ACCEL, controls.accelerate ? Input::MAX_VALUE : 0);
    controller->action(PA_BRAKE, controls.brake ? Input::MAX_VALUE : 0);
    controller->action(PA_FIRE, controls.fire ? Input::MAX_VALUE : 0);
    if (active && SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE)
        irr_driver->showPointer();
#endif
}
}
#endif
