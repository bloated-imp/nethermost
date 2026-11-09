/* The PlayStation title, and the background it puts behind the PC's menus, as
   one 256x240 picture the frame draws in the 4:3 box (frame.cpp), with the
   positions PlayStation DOOM used (the PsyDoom reconstruction's ti_main). The art is the player's, never
   the repository's: PNGs in hud/ in the save folder or beside the game. Without
   all of them the game keeps its WAD-made title (f_firetitle.c). */
#include <string>
#include "internal.h"
#include "../ps1_screen.h"

namespace doom3d {
static constexpr int W=256,H=240;
static constexpr int fireTop=116,fireTiles=4;
struct Art {int w=0,h=0;std::vector<unsigned> pixels;explicit operator bool() const {return !pixels.empty();}};
static Art title,menuBack,logo,titleMark,menuMark;
static bool tried,complete;
static GpuTextureRef picture;
static std::vector<unsigned> frame;

static Art load(const char *name) {
    Art art;
    std::string local=std::string("hud/")+name,beside;
    if(const char *base=SDL_GetBasePath())beside=std::string(base)+"hud/"+name;
    for(const std::string *path:{&local,&beside}) {
        if(path->empty())continue;
        SDL_Surface *file=SDL_LoadPNG(path->c_str());
        if(!file)continue;
        SDL_Surface *rgba=SDL_ConvertSurface(file,SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(file);
        if(!rgba)continue;
        art.w=rgba->w;art.h=rgba->h;art.pixels.resize((size_t)art.w*art.h);
        for(int y=0;y<art.h;++y)memcpy(&art.pixels[(size_t)y*art.w],(byte*)rgba->pixels+(size_t)y*rgba->pitch,(size_t)art.w*4);
        SDL_DestroySurface(rgba);
        break;
    }
    return art;
}
bool ps1ArtPresent() {
    if(!tried) {
        tried=true;
        title=load("ps1_title.png");menuBack=load("ps1_menu.png");logo=load("ps1_logo.png");
        titleMark=load("ps1_title_mark.png");menuMark=load("ps1_menu_mark.png");   // the game's name over the logos; optional
        complete=title&&menuBack&&logo;
    }
    return complete;
}
static void blit(const Art &art,int x,int y) {
    for(int j=std::max(0,-y);j<art.h&&y+j<H;++j)for(int i=std::max(0,-x);i<art.w&&x+i<W;++i) {
        unsigned texel=art.pixels[(size_t)j*art.w+i];
        if(texel>>31)frame[(size_t)(y+j)*W+x+i]=texel;
    }
}
static void drawTitle() {
    blit(title,0,ps1screen.logoY);
    blit(titleMark,42,94+ps1screen.logoY);
    if(!ps1screen.heat)return;
    for(int t=0;t<fireTiles;++t)for(int y=0;y<PS1_FIRE_H&&fireTop+y<H;++y)for(int x=0;x<PS1_FIRE_W;++x) {
        int heat=ps1screen.heat[(size_t)y*PS1_FIRE_W+x];
        if(!heat)continue;
        const byte *color=psxFireColor(heat);
        frame[(size_t)(fireTop+y)*W+t*PS1_FIRE_W+x]=0xFF000000u|(unsigned)color[2]<<16|(unsigned)color[1]<<8|color[0];
    }
}
const GpuTextureRef *ps1ScreenPicture() {
    if(!ps1screen.mode||!ps1ArtPresent())return nullptr;
    frame.assign((size_t)W*H,0xFF000000u);
    if(ps1screen.mode==1)drawTitle();
    else {
        blit(menuBack,0,0);
        if(ps1screen.menuLogo) {
            blit(logo,75,2);   // where the PC's menu has its logo (94,2 of 320x200)
            blit(menuMark,85,39);
        }
    }
    if(!picture)picture=gpuCreateTexture(GpuFormat::RGBA8,W,H,frame.data(),W*4);
    else gpuUpdateTexture(picture,GpuFormat::RGBA8,{{0,0,W,H}},frame.data(),W*4);
    return picture?&picture:nullptr;
}
void ps1ScreenShutdown() {picture=nullptr;frame.clear();tried=false;complete=false;}
} // namespace doom3d
