#ifdef ENABLE_RELAY_LOCAL_API
#include "modes/immersive_relay/remote/relay_world_controller.hpp"
#include "modes/immersive_relay/remote/relay_state_publisher.hpp"
#include "modes/immersive_relay/relay_session_controller.hpp"
#include "config/user_config.hpp"
#include "config/player_manager.hpp"
#include "input/input_manager.hpp"
#include "input/device_manager.hpp"
#include "input/keyboard_device.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/screen_keyboard.hpp"
#include "karts/kart_properties_manager.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "states_screens/state_manager.hpp"
#include "tracks/track.hpp"
#include "tracks/track_manager.hpp"
#include "utils/string_utils.hpp"
#include "modes/immersive_relay/render/immersive_render_pipeline.hpp"
#include <sstream>

void RelayWorldController::dismissPauseUI()
{
    if (GUIEngine::ModalDialog::isADialogActive())
        GUIEngine::ModalDialog::dismiss();
    if (GUIEngine::ScreenKeyboard::isActive())
        GUIEngine::ScreenKeyboard::dismiss();
}

bool RelayWorldController::validate(const RelaySessionConfig& config,
                                     std::string* error)
{
    Track* track = track_manager ? track_manager->getTrack(config.m_track_id) : NULL;
    if (!track || track->isArena() || track->isSoccer() || track->isInternal())
        *error = "invalid_track";
    else if (!kart_properties_manager ||
             !kart_properties_manager->getKart(config.m_kart_id))
        *error = "invalid_kart";
    else if (config.m_difficulty != "easy" && config.m_difficulty != "medium" &&
             config.m_difficulty != "hard" && config.m_difficulty != "best")
        *error = "invalid_difficulty";
    else if (config.m_ai_karts < 0 || config.m_ai_karts > 19)
        *error = "invalid_ai_karts";
    else if (config.m_output != "syphon" && config.m_output != "window" &&
             config.m_output != "none")
        *error = "invalid_output";
    else if (config.m_crew_mode != "enabled" && config.m_crew_mode != "disabled")
        *error = "invalid_crew_mode";
    else
        return true;
    return false;
}

bool RelayWorldController::load(const RelaySessionConfig& config,
                                 std::string* error)
{
    if (!validate(config, error))
        return false;
    dismissPauseUI();
    StateManager* state = StateManager::get();
    if (state->activePlayerCount() == 0)
    {
        PlayerManager::get()->enforceCurrentPlayer();
        KeyboardDevice* keyboard = input_manager->getDeviceManager()->getKeyboard(0);
        if (!keyboard)
        {
            *error = "local_input_unavailable";
            return false;
        }
        state->createActivePlayer(PlayerManager::getCurrentPlayer(), keyboard);
        input_manager->getDeviceManager()->setAssignMode(ASSIGN);
    }
    RaceManager* race = RaceManager::get();
    // Capture cameras belong to the current scene; release them before exitRace.
    ImmersiveRelay::ImmersiveRenderPipeline::get()->shutdown();
    if (World::getWorld())
        race->exitRace();
    // One shared kart accepts the configured keyboard/gamepad devices.
    input_manager->getDeviceManager()->setSinglePlayer(state->getActivePlayer(0));
    input_manager->getDeviceManager()->setAssignMode(ASSIGN);
    race->setMinorMode(RaceManager::MINOR_MODE_NORMAL_RACE);
    race->setNumPlayers(1, 1); // Relay participants share one physical kart.
    race->setPlayerKart(0, config.m_kart_id);
    race->setNumKarts(config.m_ai_karts + 1);
    race->setDifficulty(config.m_difficulty == "easy" ? RaceManager::DIFFICULTY_EASY :
        config.m_difficulty == "medium" ? RaceManager::DIFFICULTY_MEDIUM :
        config.m_difficulty == "hard" ? RaceManager::DIFFICULTY_HARD :
        RaceManager::DIFFICULTY_BEST);
    race->setReverseTrack(false);
    // The operator countdown replaces vanilla ready/set/go. Until it ends,
    // RelayRaceMode freezes physics and the race clock while rendering stays live.
    UserConfigParams::m_race_now = true;
    race->startSingleRace(config.m_track_id,
        RelaySessionController::getRecommendedLapCount(config.m_player_count), false);
    return World::getWorld() != NULL;
}

void RelayWorldController::reset()
{
    if (!World::getWorld())
        return;
    dismissPauseUI();
    UserConfigParams::m_race_now = true;
    RaceManager::get()->rerunRace();
}

std::string RelayWorldController::tracksJson()
{
    static const char* preferred[] = {"snowtuxpeak", "lighthouse", "olivermath"};
    std::ostringstream out;
    out << "{\"ok\":true,\"tracks\":[";
    bool first = true;
    for (const char* id : preferred)
    {
        Track* track = track_manager ? track_manager->getTrack(id) : NULL;
        if (!track || track->isInternal() || track->isArena() || track->isSoccer())
            continue;
        if (!first) out << ',';
        first = false;
        out << "{\"id\":\"" << id << "\",\"name\":\""
            << RelayStatePublisher::jsonEscape(StringUtils::wideToUtf8(track->getName()))
            << "\"}";
    }
    out << "]}";
    return out.str();
}
#endif
