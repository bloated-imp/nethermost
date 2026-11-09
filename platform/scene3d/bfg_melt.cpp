/* BFG Melt: a monster the BFG kills goes up in the projectile's green, as if
   the energy melted it: its gib frames (or, with bfgMeltDeaths, its ordinary
   death frames) start glowing green and ease back to their own colors by the
   end of the animation. A drawing-only layer over the game's frames (R_BfgGib,
   world.frag). */
#include "internal.h"

namespace doom3d {
struct Melt { int start,duration; };
static std::unordered_map<const mobj_t*,Melt> melting;

// Tics from a death state to its resting frame (or removal).
static int gibTics(int state) {
    int total=0;
    for(int n=0;n<64&&state>0;++n) {
        const state_t &s=states[state];
        if(s.tics<0)break;
        total+=s.tics;state=s.nextstate;
    }
    return std::max(total,4);
}
void bfgGib(mobj_t *thing) {
    if(!thing||!thing->info)return;
    bool gibbed=thing->health<-thing->info->spawnhealth&&thing->info->xdeathstate;
    // An ordinary death is a monster's (no barrels).
    if(!gibbed&&!(settings.bfgMeltDeaths&&(thing->flags&MF_COUNTKILL)))return;
    melting[thing]={leveltime,gibTics((int)(thing->state-states))};
}
// How green it is now, 0-255: held for the first moments, then easing away.
unsigned bfgMelt(const mobj_t &thing,float fraction) {
    auto found=melting.find(&thing);
    if(found==melting.end())return 0;
    float through=(leveltime-found->second.start+fraction)/found->second.duration;
    if(through>=1||thing.health>0) {melting.erase(found);return 0;}
    float fade=std::clamp((through-0.12f)/0.88f,0.0f,1.0f);
    fade=fade*fade*(3-2*fade);
    return (unsigned)std::lround((1-fade)*255);
}
void bfgMeltShutdown() {melting.clear();}
}
