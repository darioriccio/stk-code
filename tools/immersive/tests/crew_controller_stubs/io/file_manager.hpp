// No venue assets are needed to verify the native handoff admission state machine.
#ifndef CREW_CONTROLLER_TEST_FILE_MANAGER_HPP
#define CREW_CONTROLLER_TEST_FILE_MANAGER_HPP
#include <string>
class FileManager {
public:
    std::string getAsset(const std::string&)const{return "crew-controller-no-assets";}
};
extern FileManager* file_manager;
#endif
