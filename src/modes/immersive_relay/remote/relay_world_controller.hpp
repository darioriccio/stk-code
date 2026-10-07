#ifndef HEADER_RELAY_WORLD_CONTROLLER_HPP
#define HEADER_RELAY_WORLD_CONTROLLER_HPP
#ifdef ENABLE_RELAY_LOCAL_API
#include <string>
struct RelaySessionConfig;
namespace RelayWorldController
{
bool validate(const RelaySessionConfig& config, std::string* error);
bool load(const RelaySessionConfig& config, std::string* error);
void reset();
void dismissPauseUI();
std::string tracksJson();
}
#endif
#endif
