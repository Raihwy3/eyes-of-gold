#ifndef EOG_NATIVE_RSP_PROFILE_H
#define EOG_NATIVE_RSP_PROFILE_H
#include <cstdint>
#ifdef __3DS__
/* Including SDK integer typedefs conflicts with the original engine ABI. */
extern "C" uint64_t svcGetSystemTick(void);
#else
#include <chrono>
#endif
namespace native_rsp_profile {
inline constexpr uint32_t tick_frequency=16756991u*16u;
enum Stage {matrix,vertex,triangle,batch,packing,backend,count};
struct Scope;
inline Scope *active;
inline bool enabled;
inline uint64_t elapsed[count],exclusive[count],calls[count],vertices;
inline uint64_t now(){
#ifdef __3DS__
    return svcGetSystemTick();
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
inline double milliseconds(Stage s) {
#ifdef __3DS__
    return double(elapsed[s])*1000.0/tick_frequency /* checked against libctru in the PICA backend */;
#else
    return elapsed[s]/1.e6;
#endif
}
inline void reset(bool sample){enabled=sample;active=nullptr;for(unsigned i=0;i<count;i++)elapsed[i]=exclusive[i]=calls[i]=0;vertices=0;}
inline double exclusive_ms(Stage s) {
#ifdef __3DS__
 return double(exclusive[s])*1000.0/tick_frequency;
#else
 return exclusive[s]/1.e6;
#endif
}
struct Scope {
    Stage stage;uint64_t start,children=0;Scope *parent;bool sampled;
    explicit Scope(Stage s):stage(s),start(enabled?now():0),parent(active),sampled(enabled){if(sampled){calls[s]++;active=this;}}
    ~Scope(){if(sampled){uint64_t dt=now()-start;elapsed[stage]+=dt;exclusive[stage]+=dt>=children?dt-children:0;if(parent)parent->children+=dt;active=parent;}}
};
// Reuse backend timer readings instead of adding timestamp calls per draw.
inline void note_backend(uint64_t ticks) {
 if(!enabled)return;
 elapsed[backend]+=ticks;exclusive[backend]+=ticks;calls[backend]++;
 if(active)active->children+=ticks;
}

}
#endif
