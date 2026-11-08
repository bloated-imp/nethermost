/* The PlayStation's fire sky (settings.fireSky, F4 Nethermost): on DOOM's
   Hell maps, those whose sky is SKY3 (the third episode, DOOM II's last
   third), the sky is a wall of fire, as PlayStation DOOM's P_FireSky made
   it (p_spec.c in the PSXDOOM-RE reconstruction). A field of heat, 0 to
   36, its bottom row burning white-hot. Every update each texel's heat
   moves up a row, 0 or 1 cooler at random, landing a texel to the right,
   straight above, or one or two to the left, so the flames lean a little
   left as they climb and die out into black. Columns are walked left to
   right and rows top to bottom in place, as there, so heat pushed right
   can be carried again. The PlayStation stepped it on each of its game
   tics, 15 a second; here every second tic, 17.5. Its own random numbers:
   the game's, and demos, are untouched.

   Heat is coloured by the PlayStation fire's own 37 colours (black, deep
   red, orange, yellow, white), exactly: the heat is the texel's index, read
   through a palette of those colours (skyPalette, bound for the sky and the
   window glass's reflections of it). It stands in for
   the sky's texture (skyImage in space.cpp), and sky.frag draws it crisp,
   without the painted sky's softening and haze (c.phobos.w). The
   PlayStation drew its sky texel for pixel; this one is taller than the
   painted sky, so at 240p a texel is about a pixel there too. Its base sits
   a little under the horizon (sky.frag's v of 0.70), mostly behind the far
   walls, as there. The CPU's sky color (the haze, the sky light) still
   comes from SKY3, whose reds suit it. */
#include "internal.h"
#include "../wad_probe.h"
extern "C" int R_CheckTextureNumForName(char *name);

namespace doom3d {
static constexpr int levels=37,width=256,height=224;
// The bottom (white-hot) row, as a share of the height: 0.70 is the horizon.
static constexpr float floorAt=0.82f;
// Updates a frame may catch up on (after a pause or a load).
static constexpr int catchUp=4;
// The PlayStation fire's colours, heat 0 to 36.
static const byte psxFire[levels][3]={
    {0x07,0x07,0x07},{0x1F,0x07,0x07},{0x2F,0x0F,0x07},{0x47,0x0F,0x07},{0x57,0x17,0x07},{0x67,0x1F,0x07},
    {0x77,0x1F,0x07},{0x8F,0x27,0x07},{0x9F,0x2F,0x07},{0xAF,0x3F,0x07},{0xBF,0x47,0x07},{0xC7,0x47,0x07},
    {0xDF,0x4F,0x07},{0xDF,0x57,0x07},{0xDF,0x57,0x07},{0xD7,0x5F,0x07},{0xD7,0x5F,0x07},{0xD7,0x67,0x0F},
    {0xCF,0x6F,0x0F},{0xCF,0x77,0x0F},{0xCF,0x7F,0x0F},{0xCF,0x87,0x17},{0xC7,0x87,0x17},{0xC7,0x8F,0x17},
    {0xC7,0x97,0x1F},{0xBF,0x9F,0x1F},{0xBF,0x9F,0x1F},{0xBF,0xA7,0x27},{0xBF,0xA7,0x27},{0xBF,0xAF,0x2F},
    {0xB7,0xAF,0x2F},{0xB7,0xB7,0x2F},{0xB7,0xB7,0x37},{0xCF,0xCF,0x6F},{0xDF,0xDF,0x9F},{0xEF,0xEF,0xC7},
    {0xFF,0xFF,0xFF}};

static GpuTextureRef fireTexture,firePalette;
static std::vector<byte> heat,pixels;
static int lastStep=-1;
static unsigned seed=0x9e3779b9u;
static unsigned fireRand() {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}

// Whether the sky burns now: DOOM (not Heretic, whose SKY3 is its own),
// a level, the setting on, and the Hell sky.
static bool burning() {
    if(!settings.fireSky||gamestate!=GS_LEVEL||Game_WadKind()!=DOOM_WAD_DOOM||Game_Phobos())return false;
    static int hellSky=-2;static unsigned hellSerial=~0u;
    if(hellSerial!=r_levelserial){hellSky=R_CheckTextureNumForName((char*)"SKY3");hellSerial=r_levelserial;}
    return hellSky>=0&&skytexture==hellSky;
}
static void start() {
    byte palette[256*4]={};
    for(int level=0;level<levels;++level) {
        for(int c=0;c<3;++c)palette[level*4+c]=psxFire[level][c];
        palette[level*4+3]=255;
    }
    firePalette=gpuCreateTexture(GpuFormat::RGBA8,256,1,palette,256*4);
    heat.assign((size_t)width*height,0);
    pixels.assign((size_t)width*height*2,0);
    for(int y=(int)(height*floorAt);y<height;++y)std::fill(heat.begin()+(size_t)y*width,heat.begin()+(size_t)(y+1)*width,(byte)(levels-1));
    fireTexture=gpuCreateTexture(GpuFormat::RG8,width,height,pixels.data(),width*2);
    lastStep=-1;
}
// One update (P_FireSky): column by column, top to bottom, each texel's heat
// to the row above.
static void burn() {
    int floorRow=(int)(height*floorAt);
    for(int x=0;x<width;++x)for(int y=1;y<=floorRow;++y) {
        byte h=heat[(size_t)y*width+x];
        if(!h){heat[(size_t)(y-1)*width+x]=0;continue;}
        unsigned r=fireRand();
        int to=(x+1-(int)(r&3)+width)&(width-1);
        heat[(size_t)(y-1)*width+to]=(byte)(h-((r>>2)&1));
    }
}
void fireSkyTick() {
    if(!burning())return;
    if(!fireTexture)start();
    if(!fireTexture||!firePalette)return;
    int step=gametic/2;
    int steps=lastStep<0?height:std::min(step-lastStep,catchUp);   // a new sky starts already ablaze
    if(steps<=0)return;
    lastStep=step;
    for(int i=0;i<steps;++i)burn();
    for(size_t i=0;i<heat.size();++i){pixels[2*i]=heat[i];pixels[2*i+1]=255;}
    gpuUpdateTexture(fireTexture,GpuFormat::RG8,{{0,0,width,height}},pixels.data(),width*2);
}
const GpuTextureRef *fireSkyImage() {return fireTexture&&firePalette&&burning()?&fireTexture:nullptr;}
const GpuTextureRef *skyPalette() {return fireSkyImage()?&firePalette:nullptr;}
void fireSkyShutdown() {fireTexture=nullptr;firePalette=nullptr;heat.clear();pixels.clear();lastStep=-1;}
} // namespace doom3d
