// Publisher boundary only. Production controller/report/serializer remain real.
#ifndef CREW_CONTROLLER_TEST_RENDER_PIPELINE_HPP
#define CREW_CONTROLLER_TEST_RENDER_PIPELINE_HPP
#include <string>
namespace ImmersiveRelay {
class ImmersiveRenderPipeline {
public:
    struct Stats {std::string toJson()const{return "{}";}};
    static ImmersiveRenderPipeline* get(){static ImmersiveRenderPipeline pipeline;return &pipeline;}
    Stats getStats()const{return Stats();}
};
}
#endif
