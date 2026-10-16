// Quiet logging boundary; controller/rotation/state-machine are production sources.
#ifndef CREW_CONTROLLER_TEST_LOG_HPP
#define CREW_CONTROLLER_TEST_LOG_HPP
class Log {
public:
    template<class... Args> static void info(const char*,const char*,Args...) {}
    template<class... Args> static void warn(const char*,const char*,Args...) {}
};
#endif
