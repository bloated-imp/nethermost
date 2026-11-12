/* The relit weapon: its light probe of the surroundings, draws, and the
   crosshair. */
#include "internal.h"

namespace doom3d {
// Weapon frames to relight (weapon_lighting.h), keyed below the WAD lumps:
// the de-lit artwork with its normals in B/A, two transparent columns, then
// its gloss in R, how much of it is metal in G and its normals from the silhouette alone in B and A.
// keepPaint (the status bar's face): its colors stay as painted. It is too
// small for the weapon's de-lighting, which flattens it to mud; only its
// normals and gloss are derived.
// Which pixels of the rocket launcher's flash frames (MISF) are the launcher:
// id painted the front of the barrel into them, with a bright stripe and a glow
// in the sight slot, so drawn at full brightness the barrel seems lit from inside.
// In each column the barrel starts at the first run of grey from the top, and
// reaches over the stripe and the notch of the slot from the columns on each side.
// The plasma rifle's (PLSF, plasma) hold the whole gun: there it is everything
// from the first dark or tan run down, but for the blue of its vents.
// The pistol's (PISF) hold its top, with the muzzle's light painted on it: the
// dark metal under the flame, and the orange painted between its edges.
// The shotgun's (SHTF) hold no gun: only the notch left open over its barrel,
// the empty pixels reaching up into the flame from the middle of its bottom edge.
enum FlashArt {FLASH_BARREL,FLASH_PLASMA,FLASH_PISTOL,FLASH_NOTCH};
static FlashArt flashArt(int lump) {
    const char *name=lumpinfo[lump].name;
    return !strncmp(name,"PLSF",4)?FLASH_PLASMA:!strncmp(name,"PISF",4)?FLASH_PISTOL:!strncmp(name,"SHTF",4)?FLASH_NOTCH:FLASH_BARREL;
}
static std::vector<byte> flashBarrel(const Image &art,const byte *palette,FlashArt kind) {
    int w=art.width,h=art.height;
    bool plasma=kind==FLASH_PLASMA;
    if(kind==FLASH_NOTCH) {
        std::vector<byte> notch((size_t)w*h,0);
        auto open=[&](int x,int y) {return x>=0&&y>=0&&x<w&&y<h&&!art.pixels[2*((size_t)y*w+x)+1]&&!notch[(size_t)y*w+x];};
        // From the bottom row's empty run nearest the middle, up through empty
        // pixels with flame on both sides in their row.
        std::vector<std::pair<int,int>> todo;
        for(int d=0;d<w/2&&todo.empty();++d)for(int x:{w/2-d,w/2+d})if(open(x,h-1)&&todo.empty())todo.push_back({x,h-1});
        auto enclosed=[&](int x,int y) {
            bool left=false,right=false;
            for(int k=x-1;k>=0&&!left;--k)left=art.pixels[2*((size_t)y*w+k)+1]!=0;
            for(int k=x+1;k<w&&!right;++k)right=art.pixels[2*((size_t)y*w+k)+1]!=0;
            return left&&right;
        };
        while(!todo.empty()) {
            auto [x,y]=todo.back();todo.pop_back();
            if(!open(x,y)||!enclosed(x,y))continue;
            notch[(size_t)y*w+x]=1;
            todo.push_back({x-1,y});todo.push_back({x+1,y});todo.push_back({x,y-1});todo.push_back({x,y+1});
        }
        return notch;
    }
    if(kind==FLASH_PISTOL) {
        auto color=[&](int x,int y) {return &palette[art.pixels[2*((size_t)y*w+x)]*3];};
        auto opaque=[&](int x,int y) {return art.pixels[2*((size_t)y*w+x)+1]!=0;};
        // Metal: dark, or dull, not the flame's red, yellow and white.
        auto metal=[&](int x,int y) {
            if(!opaque(x,y))return false;
            const byte *c=color(x,y);int top=std::max({c[0],c[1],c[2]});
            return top<110&&!(c[0]>=100&&c[1]<0.55f*c[0]);
        };
        // The gun starts at the first row with a run of metal across its middle.
        int first=-1;
        for(int y=0;y<h&&first<0;++y) {
            int run=0;
            for(int x=w/4;x<w*3/4;++x)if(metal(x,y))++run;
            if(run>=2)first=y;
        }
        std::vector<byte> gun((size_t)w*h,0);
        if(first<0)return gun;
        for(int y=first;y<h;++y) {
            int left=-1,right=-1;
            for(int x=0;x<w;++x)if(metal(x,y)) {if(left<0)left=x;right=x;}
            if(left<0)continue;
            for(int x=left;x<=right;++x) {
                if(!opaque(x,y))continue;
                const byte *c=color(x,y);
                // Between its edges, all but the flame's white core is gun.
                if(std::min({c[0],c[1],c[2]})<150)gun[(size_t)y*w+x]=1;
            }
        }
        return gun;
    }
    if(plasma) {
        auto color=[&](int x,int y) {return &palette[art.pixels[2*((size_t)y*w+x)]*3];};
        auto opaque=[&](int x,int y) {return art.pixels[2*((size_t)y*w+x)+1]!=0;};
        auto bluish=[&](int x,int y) {const byte *c=color(x,y);return c[2]>=std::max(c[0],c[1])+16;};
        auto gunLike=[&](int x,int y) {const byte *c=color(x,y);return opaque(x,y)&&!bluish(x,y)&&std::max({c[0],c[1],c[2]})<=200;};
        std::vector<byte> gun((size_t)w*h,0);
        for(int x=0;x<w;++x) {
            int start=-1;
            for(int y=0;y<h&&start<0;++y) {
                int run=0;
                for(int k=y;k<std::min(h,y+4);++k)run+=gunLike(x,k);
                if(gunLike(x,y)&&run>=3)start=y;
            }
            if(start>=0)for(int y=start;y<h;++y)if(opaque(x,y)&&!bluish(x,y))gun[(size_t)y*w+x]=1;
        }
        return gun;
    }
    auto grey=[&](int x,int y) {
        size_t i=2*((size_t)y*w+x);
        if(!art.pixels[i+1])return false;
        const byte *color=&palette[art.pixels[i]*3];
        int top=std::max({color[0],color[1],color[2]}),low=std::min({color[0],color[1],color[2]});
        return top-low<=0.22f*top&&top<=235;
    };
    std::vector<int> start(w,-1);
    for(int x=0;x<w;++x)for(int y=0;y<h&&start[x]<0;++y) {
        int run=0;
        for(int k=y;k<std::min(h,y+4);++k)run+=grey(x,k);
        if(grey(x,y)&&run>=3)start[x]=y;
    }
    std::vector<int> reached=start;
    const int across=9;
    for(int x=0;x<w;++x) {
        int left=-1,right=-1;
        for(int k=std::max(0,x-across);k<x;++k)if(start[k]>=0&&(left<0||start[k]<left))left=start[k];
        for(int k=x+1;k<std::min(w,x+across+1);++k)if(start[k]>=0&&(right<0||start[k]<right))right=start[k];
        if(left>=0&&right>=0&&(start[x]<0||start[x]>std::max(left,right)+2))reached[x]=std::max(left,right);
    }
    std::vector<byte> barrel((size_t)w*h,0);
    for(int x=0;x<w;++x)if(reached[x]>=0)for(int y=reached[x];y<h;++y)barrel[(size_t)y*w+x]=1;
    return barrel;
}
// The Chrome chaingun (F4 Nethermost, with Weapons 2.0): the stock chaingun's
// frames keep their painted chrome and take the shape drawn for them
// (chaingun_shape_stock.h): round barrels, banded rings, a rounded housing.
static bool chromeChaingun() {return settings.chromeChaingun&&settings.weapons2&&settings.weaponLighting;}
// The drawn shape for a chaingun frame, or null (not the stock art; remembered).
static const unsigned char *chaingunShape(int lump) {
    static std::map<int,const unsigned char*> known;
    auto found=known.find(lump);if(found!=known.end())return found->second;
    const Image &art=lumpImage(lump,false);
    return known[lump]=doom_chaingun_shape(lumpinfo[lump].name,art.width,art.height,art.pixels.data());
}
// Weapons 2.0: the displayed player's shotgun is in its reload frames (world.frag
// shades them more strongly: their paint is kept, which leaves the light subtle).
bool shotgunPumpingNow() {
    if(!settings.weapons2||!settings.weaponLighting||!heldWeaponIs("SHTG"))return false;
    const pspdef_t &gun=players[displayplayer].psprites[0];
    const spriteframe_t &frame=sprites[gun.state->sprite].spriteframes[gun.state->frame&FF_FRAMEMASK];
    return lumpinfo[firstspritelump+frame.lump[0]].name[4]!='A';
}
// The displayed player's chaingun is up and drawn in chrome (world.frag).
bool chromeChaingunNow() {
    if(!chromeChaingun()||!heldWeaponIs("CHGG"))return false;
    const pspdef_t &gun=players[displayplayer].psprites[0];
    const spriteframe_t &frame=sprites[gun.state->sprite].spriteframes[gun.state->frame&FF_FRAMEMASK];
    return chaingunShape(firstspritelump+frame.lump[0])!=nullptr;
}
// angryEyes: an invincible player's rampage or grin face, its eyes painted as
// the god face's are (the game shows those two over the god face).
// barrelOnly: of the rocket launcher's flash frames, just the launcher (flashBarrel).
static Image &weaponImage(int lump,bool keepPaint=false,bool angryEyes=false,bool barrelOnly=false) {
    // Weapons 2.0 keeps the weapon's paint over a glowing floor: that strong a
    // light leaves only the artist's shading to read as detail (world.frag).
    // Anywhere else the paint's light is taken out, for the room's to replace.
    static bool overGlow=false;
    if(weaponProbe[5*4+3]>0.6)overGlow=true;else if(weaponProbe[5*4+3]<0.4)overGlow=false;
    bool face=keepPaint;
    keepPaint=keepPaint||(settings.weapons2&&overGlow);
    // The Chrome chaingun: its own frames with their drawn shape, and the ring
    // its flash frames carry, both with the chrome painted on them kept.
    const unsigned char *drawn=!face&&chromeChaingun()&&!strncmp(lumpinfo[lump].name,"CHGG",4)?chaingunShape(lump):nullptr;
    bool chrome=drawn||(!face&&barrelOnly&&chromeChaingun()&&!strncmp(lumpinfo[lump].name,"CHGF",4));
    if(chrome)keepPaint=true;
    // Weapons 2.0: the shotgun's reload frames are a hand close up, its fingers
    // drawn only by their painted shading; taken out, the hand goes to mush.
    // Its shape then comes from the outline alone: from the paint, the sleeve's
    // dither would turn every speckle its own way and glitter.
    bool pumping=!face&&settings.weapons2&&!strncmp(lumpinfo[lump].name,"SHTG",4)&&lumpinfo[lump].name[4]!='A';
    if(pumping)keepPaint=true;
    // The pistol's top in its flash frame keeps the muzzle's light painted on it.
    if(!face&&barrelOnly&&!strncmp(lumpinfo[lump].name,"PISF",4))keepPaint=true;
    int key=chrome?INT_MIN/2-(4<<24)-lump*2-(barrelOnly?1:0):barrelOnly?INT_MIN/2-(2<<24)-lump*2-(keepPaint?1:0):angryEyes?INT_MIN/2-(1<<24)-lump:INT_MIN/2-lump*2-(keepPaint?1:0); auto found=images.find(key); if(found!=images.end()) return found->second;
    const Image &source=lumpImage(lump,false);
    int w=source.width,h=source.height,stride=2*w+2;
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::vector<byte> art=source.pixels;
    if(barrelOnly) {
        auto barrel=flashBarrel(source,palette,flashArt(lump));
        for(size_t i=0;i<barrel.size();++i)if(!barrel[i])art[2*i+1]=0;
    }
    auto maps=doom_weapon_delight(w,h,art.data(),palette);
    if(keepPaint) maps.pixels=art;
    if(drawn) {maps.normals.assign(drawn,drawn+(size_t)w*h*2);maps.smooth=maps.normals;}
    else if(chrome||pumping) maps.normals=maps.smooth;
    // The rocket launcher's front in its flash frames: id painted the flame's
    // light on it as a thin stripe round its ring, which as gloss would take every
    // light as a dotted hard line. Its own muzzle light (gunBlastLight) lights it.
    if(barrelOnly&&!strncmp(lumpinfo[lump].name,"MISF",4))std::fill(maps.gloss.begin(),maps.gloss.end(),0);
    // Weapons 2.0, the plasma rifle: its beige casing is ridged in one- and
    // two-pixel bands, which the broad blur the normals are read through
    // (weapon_lighting.h) smooths away, so it relit flat. Its relief comes from
    // the paint through a fine blur instead, tilting the casing's rounded
    // shape, so each ridge catches the light; the black metal keeps its own.
    if(!face&&settings.weapons2&&(!strncmp(lumpinfo[lump].name,"PLSG",4)||!strncmp(lumpinfo[lump].name,"PLSF",4))) {
        const float gain=10;   // how far each ridge tilts
        auto opaque=[&](int x,int y){return x>=0&&y>=0&&x<w&&y<h&&art[2*((size_t)y*w+x)+1];};
        auto luma=[&](int x,int y){const byte *c=palette+art[2*((size_t)y*w+x)]*3;return (0.299f*c[0]+0.587f*c[1]+0.114f*c[2])/255;};
        std::vector<float> relief((size_t)w*h,0);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            if(!opaque(x,y))continue;
            float sum=0,weight=0;static const float tap[3]={1,2,1};
            for(int j=-1;j<=1;++j)for(int k=-1;k<=1;++k)if(opaque(x+k,y+j)){float s=tap[j+1]*tap[k+1];sum+=luma(x+k,y+j)*s;weight+=s;}
            relief[(size_t)y*w+x]=sum/weight;
        }
        auto height=[&](int x,int y,int fx,int fy){return opaque(x,y)?relief[(size_t)y*w+x]:relief[(size_t)fy*w+fx];};
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            size_t i=(size_t)y*w+x;
            if(!opaque(x,y)||maps.metal[i]>=128)continue;
            float sx=maps.smooth[2*i]/255.0f*2-1,sy=maps.smooth[2*i+1]/255.0f*2-1,sz=std::sqrt(std::max(0.0f,1-sx*sx-sy*sy));
            float gx=(height(x+1,y,x,y)-height(x-1,y,x,y))*gain,gy=(height(x,y+1,x,y)-height(x,y-1,x,y))*gain;
            float nx=sx-gx*sz,ny=sy-gy*sz,nz=sz,length=std::sqrt(nx*nx+ny*ny+nz*nz);
            maps.normals[2*i]=(byte)std::lround(std::clamp(nx/length*0.5f+0.5f,0.0f,1.0f)*255);
            maps.normals[2*i+1]=(byte)std::lround(std::clamp(ny/length*0.5f+0.5f,0.0f,1.0f)*255);
        }
    }
    // The sleeve under the reloading hand: dithered grey cloth, which the art's
    // grey marks as metal and gloss would make glitter. Grey in a grey
    // neighbourhood is cloth; the barrels' grey highlights sit in black metal.
    if(pumping) {
        auto luma=[&](int x,int y){size_t i=(size_t)y*w+x;const byte *c=palette+art[2*i]*3;return art[2*i+1]?0.299f*c[0]+0.587f*c[1]+0.114f*c[2]:0.0f;};
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            size_t i=(size_t)y*w+x;const byte *c=palette+art[2*i]*3;
            if(!art[2*i+1]||std::max({c[0],c[1],c[2]})-std::min({c[0],c[1],c[2]})>24)continue;
            float sum=0;int n=0;
            for(int j=-3;j<=3;++j)for(int k=-3;k<=3;++k)if(y+j>=0&&y+j<h&&x+k>=0&&x+k<w){sum+=luma(x+k,y+j);++n;}
            if(sum/n>35)maps.gloss[i]=0;
        }
    }   // the flash's ring: rounded by its outline, its paint no bumps
    // Row 16, the five pixels of each squinting eye, in every blood level; the
    // god face's own yellows, near white toward the outside as its eyes are.
    if(angryEyes&&w==24&&h>16) {
        static const byte ramp[5]={0xa3,0xe1,0xe0,0xa2,0xa3};
        for(int n=0;n<5;++n) {maps.pixels[2*((size_t)16*w+5+n)]=ramp[n];maps.pixels[2*((size_t)16*w+18-n)]=ramp[n];}
    }
    // The face's stripe has no metal to hold: G marks the god face's eyes,
    // which light themselves (255), and how much of their light falls on the
    // face around them (below 250, how far each pixel is from an eye: 249 at
    // the eye to 0 twenty pixels off; world.frag). Their yellow ramp runs up to
    // near white in the middle of each eye.
    bool god=face&&(angryEyes||lump==W_CheckNumForName((char*)"STFGOD0"));
    auto eyeColor=[&](int index) {
        float r=palette[index*3],g=palette[index*3+1],b=palette[index*3+2];
        return (r>=235&&g>=235)||(r>=120&&g>=0.55f*r&&b<=0.4f*g);
    };
    std::vector<byte> eyeLight;
    if(god) {
        eyeLight.assign((size_t)w*h,0);
        // The upper part of the face only: the teeth are painted in the same ramp.
        auto isEye=[&](int x,int y) {
            if(angryEyes)return y==16&&((x>=5&&x<=9)||(x>=14&&x<=18));
            size_t i=(size_t)y*w+x;return y*3<h*2&&maps.pixels[2*i+1]&&eyeColor(maps.pixels[2*i]);
        };
        const int reach=20;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            float nearest=(float)reach;
            for(int j=-reach;j<=reach;++j)for(int k=-reach;k<=reach;++k)
                if(y+j>=0&&y+j<h&&x+k>=0&&x+k<w&&isEye(x+k,y+j))nearest=std::min(nearest,std::sqrt((float)(j*j+k*k)));
            eyeLight[(size_t)y*w+x]=nearest==0?255:(byte)std::lround((1-nearest/reach)*249);
        }
    }
    // The face's stripe has no gloss to hold either: R marks the ears, which
    // light from behind the head shows through (world.frag). Looking ahead they
    // are the skin at the head's outline around the eye line; turned, the far
    // ear is a curl of skin set in the hair, and the near one is out of sight.
    std::vector<byte> ears;
    if(face) {
        ears.assign((size_t)w*h,0);
        bool left=!strncmp(lumpinfo[lump].name,"STFTL",5),right=!strncmp(lumpinfo[lump].name,"STFTR",5);
        auto skin=[&](int x,int y,int least) {
            size_t i=(size_t)y*w+x;
            float r=palette[maps.pixels[2*i]*3],g=palette[maps.pixels[2*i]*3+1],b=palette[maps.pixels[2*i]*3+2];
            return maps.pixels[2*i+1]&&r>=least&&g>=0.45f*r&&g<r&&b<g;
        };
        for(int row=14;row<23;++row) {          // on the screen: the art's own rows differ with its offset
            int y=row+source.top;
            if(y<0||y>=h)continue;
            int first=0,last=w-1;
            while(first<w&&!maps.pixels[2*((size_t)y*w+first)+1])++first;
            while(last>=0&&!maps.pixels[2*((size_t)y*w+last)+1])--last;
            if(first>last)continue;
            if(left||right) {
                if(row<16)continue;
                for(int k=2;k<7;++k) {
                    int x=left?last-k:first+k;
                    if(x>=0&&x<w&&skin(x,y,125))ears[(size_t)y*w+x]=200;
                }
            }
            else if(row<20)for(int k=0;k<2;++k) {
                // Thinnest at the outside and in the middle of its height.
                static const float along[6]={0.55f,1,1,1,0.85f,0.5f};
                byte amount=(byte)std::lround((k?150:255)*along[row-14]);
                if(first+k<w&&skin(first+k,y,95))ears[(size_t)y*w+first+k]=amount;
                if(last-k>=0&&skin(last-k,y,95))ears[(size_t)y*w+last-k]=amount;
            }
        }
    }
    std::vector<byte> packed((size_t)stride*h*4,0);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        size_t i=(size_t)y*w+x;byte *art=&packed[((size_t)y*stride+x)*4];
        art[0]=maps.pixels[2*i];art[1]=maps.pixels[2*i+1];art[2]=maps.normals[2*i];art[3]=maps.normals[2*i+1];
        packed[((size_t)y*stride+w+2+x)*4]=face?ears[i]:maps.gloss[i];
        packed[((size_t)y*stride+w+2+x)*4+1]=face?(god?eyeLight[i]:0):maps.metal[i];
        packed[((size_t)y*stride+w+2+x)*4+2]=maps.smooth[2*i];
        packed[((size_t)y*stride+w+2+x)*4+3]=maps.smooth[2*i+1];
    }
    Image image;image.pixels=std::move(maps.pixels);image.opaque=false;
    image.width=w;image.height=h;image.left=source.left;image.top=source.top;
    image.texture=gpuCreateTexture(GpuFormat::RGBA8,stride,h,packed.data(),stride*4);
    if(!image.texture) I_Error((char*)"Could not allocate a GPU texture");
    return images.emplace(key,std::move(image)).first->second;
}
// The rocket launcher's flash frame without the launcher: the flame, which
// stays at full brightness over the relit barrel (weaponImage's barrelOnly).
static Image &flashFlame(int lump) {
    int key=INT_MIN/2-(3<<24)-lump; auto found=images.find(key); if(found!=images.end()) return found->second;
    const Image &source=lumpImage(lump,false);
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::vector<byte> flame=source.pixels;
    auto barrel=flashBarrel(source,palette,flashArt(lump));
    for(size_t i=0;i<barrel.size();++i)if(barrel[i])flame[2*i+1]=0;
    return images.emplace(key,upload(source.width,source.height,flame.data(),source.left,source.top)).first->second;
}
// Weapons 2.0, Varied muzzle flashes (F4 Nethermost): each shot's flame takes
// one of a few shapes made from the original and a size of its own, so no two
// shots look quite alike. The pistol's so far. Shape 0 is the original; the
// others flicker as a flame does, its body left where it is: half are its mirror
// image, the outermost pixels of its outline trimmed in some places and grown
// out in others, and its tongues reach a little further one way than another.
// Only the flame's own pixels and colors, never over the gun, and the
// renderer's own dice pick them, not the game's.
static const int FLAME_SHAPES=8;
static bool variedFlame(int lump) {
    const char *name=lumpinfo[lump].name;
    return settings.variedFlashes&&settings.weapons2&&settings.weaponLighting&&(!strncmp(name,"PISF",4)||!strncmp(name,"SHTF",4));
}
struct FlameShot {int shape=0;float wide=1,tall=1,light=1;};
static FlameShot flameShot;   // the shot now showing
// How much the flames vary, in quarter steps (the shapes are made per step).
static int flameVariety() {return (int)std::lround(std::clamp(settings.flashVariety,0.0f,2.0f)*4);}
// A new shot: the flash's state entered afresh (or its tics wound back up), not
// the next frame of the same flash (the shotgun's two).
static void nextFlameShot(const pspdef_t &flash) {
    static const state_t *last=nullptr;static int lastTics=0;
    bool following=last&&flash.state==&states[last->nextstate];
    bool fresh=flash.state&&!following&&(flash.state!=last||flash.tics>lastTics);
    last=flash.state;lastTics=flash.tics;
    if(!fresh)return;
    static std::mt19937 dice{std::random_device{}()};
    std::uniform_real_distribution<float> unit(0,1);
    float variety=flameVariety()/4.0f;
    int shape=(int)(unit(dice)*FLAME_SHAPES)%FLAME_SHAPES;
    if(shape==flameShot.shape)shape=(shape+1+(int)(unit(dice)*(FLAME_SHAPES-1)))%FLAME_SHAPES;   // never the same twice running
    flameShot.shape=variety>0?shape:0;
    flameShot.wide=1+variety*(unit(dice)*0.08f-0.04f);
    flameShot.tall=1+variety*(unit(dice)*0.16f-0.08f);
    // A bigger flame lights the gun a little more.
    flameShot.light=std::clamp(1+1.2f*(flameShot.wide*flameShot.tall-1),0.8f,1.2f);
}
// How much the shot now showing lights the gun, against the original flame's (1).
float variedFlashLight() {
    const pspdef_t &flash=players[displayplayer].psprites[1];
    if(!flash.state)return 1;
    const spriteframe_t &frame=sprites[flash.state->sprite].spriteframes[flash.state->frame&FF_FRAMEMASK];
    return variedFlame(firstspritelump+frame.lump[0])?flameShot.light:1.0f;
}
// The muzzle in a flash frame: the middle of the gun's top row in it, or the
// middle of the flame's bottom.
static void flameAnchor(const Image &art,const std::vector<byte> &gun,float &ax,float &ay) {
    int w=art.width,h=art.height,top=-1;
    for(int y=0;y<h&&top<0;++y)for(int x=0;x<w;++x)if(gun[(size_t)y*w+x]){top=y;break;}
    if(top>=0) {
        float sum=0;int n=0;
        for(int x=0;x<w;++x)if(gun[(size_t)top*w+x]){sum+=x+0.5f;++n;}
        ax=sum/n;ay=(float)top;return;
    }
    ax=w/2.0f;ay=(float)h;
}
// The flame of a flash frame in one of its shapes, with a margin round it for
// what grows out; anchorX/Y: the muzzle in it.
static const int FLAME_MARGIN=6;
static Image &flameShape(int lump,int shape,float &anchorX,float &anchorY) {
    const Image &source=lumpImage(lump,false);
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    static std::map<int,std::pair<float,float>> anchors;
    auto gun=flashBarrel(source,palette,flashArt(lump));
    auto at=anchors.find(lump);
    if(at==anchors.end()) {float ax,ay;flameAnchor(source,gun,ax,ay);at=anchors.emplace(lump,std::make_pair(ax,ay)).first;}
    anchorX=at->second.first+FLAME_MARGIN;anchorY=at->second.second+FLAME_MARGIN;
    int variety=flameVariety();
    int key=INT_MIN/2-(5<<24)-lump*128-variety*8-shape; auto found=images.find(key); if(found!=images.end()) return found->second;
    int w=source.width,h=source.height,W=w+2*FLAME_MARGIN,H=h+2*FLAME_MARGIN;
    float ax=at->second.first,ay=at->second.second,amount=variety/4.0f;
    auto flame=[&](int x,int y) {return x>=0&&y>=0&&x<w&&y<h&&source.pixels[2*((size_t)y*w+x)+1]&&!gun[(size_t)y*w+x];};
    auto gunAt=[&](int ox,int oy) {int x=ox-FLAME_MARGIN,y=oy-FLAME_MARGIN;return x>=0&&y>=0&&x<w&&y<h&&gun[(size_t)y*w+x];};
    float reach=1;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(flame(x,y))reach=std::max(reach,std::hypot(x+0.5f-ax,y+0.5f-ay));
    // Each shape's flicker, the same every time it is made.
    std::mt19937 dice(0x9e3779b9u*(unsigned)(shape+1)+(unsigned)lump);
    std::uniform_real_distribution<float> unit(-1,1);
    bool mirror=shape%2==1;
    float lobes=2+(int)(1.5f*(unit(dice)+1)),lobeAmount=amount*(0.025f+0.03f*std::abs(unit(dice))),lobePhase=3.14159f*unit(dice);
    float edge=1.6f*amount,k1=3+(int)(2*(unit(dice)+1)),p1=3.14159f*unit(dice),k2=7+(int)(3*(unit(dice)+1)),p2=3.14159f*unit(dice);
    std::vector<byte> out((size_t)W*H*2,0);
    // The body: the original, mirrored about the muzzle for half of them, its
    // tongues reaching a little further one way than another.
    for(int oy=0;oy<H;++oy)for(int ox=0;ox<W;++ox) {
        if(gunAt(ox,oy))continue;   // the gun stays clear
        float px=ox-FLAME_MARGIN+0.5f,py=oy-FLAME_MARGIN+0.5f;
        float rx=px-ax,ry=py-ay,r=std::hypot(rx,ry),t=std::min(1.0f,r/reach);
        float sx=px,sy=py;
        if(shape>0&&r>0.5f) {
            float grow=1+lobeAmount*t*std::sin(lobes*std::atan2(ry,rx)+lobePhase);
            sx=ax+rx/grow;sy=ay+ry/grow;
            if(mirror)sx=2*ax-sx;
        }
        int ix=(int)std::floor(sx),iy=(int)std::floor(sy);
        // Mirrored, the flame hugging the gun may not meet its other side: there the original's own pixel.
        if(!flame(ix,iy)&&mirror&&flame(ox-FLAME_MARGIN,oy-FLAME_MARGIN)&&t<0.45f) {ix=ox-FLAME_MARGIN;iy=oy-FLAME_MARGIN;}
        if(!flame(ix,iy))continue;
        size_t from=2*((size_t)iy*w+ix),to=2*((size_t)oy*W+ox);
        out[to]=source.pixels[from];out[to+1]=source.pixels[from+1];
    }
    // The outline flickers: away from the muzzle its outermost pixels go in some
    // directions and grow by a pixel or two (the edge's own colors) in others.
    if(shape>0&&edge>0) {
        std::vector<byte> before=out;
        for(int oy=0;oy<H;++oy)for(int ox=0;ox<W;++ox) {
            if(gunAt(ox,oy))continue;
            float rx=ox+0.5f-(ax+FLAME_MARGIN),ry=oy+0.5f-(ay+FLAME_MARGIN),t=std::min(1.0f,std::hypot(rx,ry)/reach);
            if(t<0.4f)continue;
            float a=std::atan2(ry,rx),n=(0.6f*std::sin(k1*a+p1)+0.4f*std::sin(k2*a+p2))*edge*(t-0.4f)/0.6f;
            bool here=before[2*((size_t)oy*W+ox)+1]!=0;
            // How far to the outline: the nearest pixel of the other kind, within 2.
            int nearest=3;size_t nearestAt=0;
            for(int j=-2;j<=2;++j)for(int k=-2;k<=2;++k) {
                int x=ox+k,y=oy+j;int d=std::max(std::abs(j),std::abs(k));
                bool there=x>=0&&y>=0&&x<W&&y<H&&before[2*((size_t)y*W+x)+1];
                if(there!=here&&d<nearest) {nearest=d;nearestAt=2*((size_t)y*W+x);}
            }
            size_t to=2*((size_t)oy*W+ox);
            if(here&&n<0&&nearest<=-n)out[to+1]=0;
            else if(!here&&n>0&&nearest<=n) {out[to]=before[nearestAt];out[to+1]=before[nearestAt+1];}
        }
    }
    return images.emplace(key,upload(W,H,out.data(),source.left+FLAME_MARGIN,source.top+FLAME_MARGIN)).first->second;
}
// The BFG's green on the gun, 0 to 1. The game's muzzle flash only comes when the
// ball leaves, but its flames sit on the gun from the start of the charge: they
// grow over the small flame frame, peak on the big one as the ball leaves, and
// the shot's own flash carries the fade.
static float bfgBlast() {
    if(!heldWeaponIs("BFGG"))return 0;
    const pspdef_t &flame=players[displayplayer].psprites[1];
    float amount=0;
    if(flame.state&&flame.state->tics>0) {
        const spriteframe_t &frame=sprites[flame.state->sprite].spriteframes[flame.state->frame&FF_FRAMEMASK];
        const char *name=lumpinfo[firstspritelump+frame.lump[0]].name;
        float through=std::clamp(1-(flame.tics-renderFraction)/(float)flame.state->tics,0.0f,1.0f);
        if(!strncmp(name,"BFGFA",5))amount=0.45f+0.3f*through;
        else if(!strncmp(name,"BFGFB",5))amount=1-0.4f*through;
    }
    float radius=0,strength=0;std::array<float,3> color={};
    if(ownMuzzleFlash(radius,strength,color))amount=std::max(amount,strength/2.6f);
    return std::min(amount,1.0f);
}
static const float bfgGreen[3]={0.4f,1.0f,0.35f};
// The plasma rifle's blue on the gun, 0 to 1: the arcs while the flash frames
// show, a shade less on the smaller one, and each shot's own flash.
static float plasmaBlast() {
    const pspdef_t &arcs=players[displayplayer].psprites[1];
    float amount=0;
    if(arcs.state) {
        const spriteframe_t &frame=sprites[arcs.state->sprite].spriteframes[arcs.state->frame&FF_FRAMEMASK];
        const char *name=lumpinfo[firstspritelump+frame.lump[0]].name;
        if(!strncmp(name,"PLSFA",5))amount=0.75f;
        else if(!strncmp(name,"PLSFB",5))amount=1;
    }
    float radius=0,strength=0;std::array<float,3> color={};
    if(ownMuzzleFlash(radius,strength,color))amount=std::max(amount,strength/1.6f);
    return std::min(amount,1.0f);
}
static const float plasmaBlue[3]={0.62f,0.72f,1.0f};   // the arcs' pale blue; a purer one only darkens the tan paint
// The pistol's muzzle flash on the gun and hand, 0 to 1: full as its flash frame
// shows, fading over it; and the screen row of the gun's top in that frame.
static float flashMuzzleRow();
static float pistolBlast() {
    const pspdef_t &flash=players[displayplayer].psprites[1];
    if(!flash.state||flash.state->tics<=0)return 0;
    const spriteframe_t &frame=sprites[flash.state->sprite].spriteframes[flash.state->frame&FF_FRAMEMASK];
    if(strncmp(lumpinfo[firstspritelump+frame.lump[0]].name,"PISF",4))return 0;
    float through=std::clamp(1-(flash.tics-renderFraction)/(float)flash.state->tics,0.0f,1.0f);
    return (1-0.7f*through)*(variedFlame(firstspritelump+frame.lump[0])?flameShot.light:1.0f);
}
static const float pistolOrange[3]={1.0f,0.62f,0.32f};
// The rocket launcher's flame on its front, 0 to 1, as its four flash frames
// grow and die away.
static float launcherBlast() {
    const pspdef_t &flash=players[displayplayer].psprites[1];
    if(!flash.state||flash.state->tics<=0)return 0;
    const spriteframe_t &frame=sprites[flash.state->sprite].spriteframes[flash.state->frame&FF_FRAMEMASK];
    const char *name=lumpinfo[firstspritelump+frame.lump[0]].name;
    if(strncmp(name,"MISF",4))return 0;
    float through=std::clamp(1-(flash.tics-renderFraction)/(float)flash.state->tics,0.0f,1.0f);
    static const float start[4]={0.5f,0.85f,1.0f,0.7f},end[4]={0.85f,1.0f,0.7f,0.25f};
    int at=std::clamp(name[4]-'A',0,3);
    return start[at]+(end[at]-start[at])*through;
}
static const float launcherOrange[3]={1.0f,0.55f,0.25f};
// The glowing vent at the back of the plasma rifle, which one of its two flash
// frames (PLSFB) paints lit in the middle of the casing's lower edge: the blue
// the flash split leaves glowing (flashBarrel) low in the frame's middle. Its
// middle in the frame, its top row and how many pixels glow, per flash frame; none 0.
struct Vent {float x,y,top;int glowing;};
static Vent ventOf(int lump) {
    static std::map<int,Vent> known;
    auto found=known.find(lump);if(found!=known.end())return found->second;
    const Image &art=lumpImage(lump,false);
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    auto gun=flashBarrel(art,palette,FLASH_PLASMA);
    Vent vent={0,0,1e9f,0};
    // The middle third, lower part: the arcs reach down the frame's sides.
    for(int y=art.height*3/5;y<art.height;++y)for(int x=art.width/3;x<art.width*2/3;++x) {
        size_t i=(size_t)y*art.width+x;
        if(!art.pixels[2*i+1]||gun[i])continue;
        const byte *c=palette+art.pixels[2*i]*3;
        if(std::max({c[0],c[1],c[2]})<120)continue;
        vent.x+=x+0.5f;vent.y+=y+0.5f;vent.top=std::min(vent.top,(float)y);++vent.glowing;
    }
    if(vent.glowing<3)vent={0,0,0,0};else {vent.x/=vent.glowing;vent.y/=vent.glowing;}
    return known[lump]=vent;
}
// The held gun's own light on itself (weapon.glsl), for the guns with one: its
// color now, and its reach, fade, the screen row of its muzzle (of 200) and how
// much of it reaches even what faces away.
bool gunBlastLight(float color[3],float shape[4]) {
    float amount=0,strength=1,power=2.4f;const float *hue=nullptr;
    if(heldWeaponIs("BFGG")) {
        amount=bfgBlast();hue=bfgGreen;strength=settings.bfgLightStrength;
        shape[0]=settings.bfgLightSize;shape[1]=settings.bfgLightFalloff;shape[2]=140;shape[3]=0.15f;     // the flames hide the gun's top rows
    } else if(heldWeaponIs("PISG")) {
        amount=pistolBlast();hue=pistolOrange;strength=settings.pistolLightStrength;power=2.2f;
        shape[0]=1.2f;shape[1]=1.6f;shape[2]=flashMuzzleRow();shape[3]=0.3f;
    } else if(heldWeaponIs("MISG")) {
        amount=launcherBlast();hue=launcherOrange;strength=settings.launcherLightStrength;power=1.8f;
        shape[0]=1.1f;shape[1]=1.8f;shape[2]=flashMuzzleRow();shape[3]=0.2f;
    } else if(heldWeaponIs("PLSG")) {
        amount=plasmaBlast();hue=plasmaBlue;strength=settings.plasmaLightStrength;power=3.5f;     // the part next to its muzzle is black
        shape[0]=settings.plasmaLightSize;shape[1]=settings.plasmaLightFalloff;shape[2]=112;shape[3]=0.1f;
    }
    if(amount<=0)return false;
    for(int c=0;c<3;++c)color[c]=hue[c]*amount*power*strength;
    return true;
}
// What the relit weapon sees around it, in the view's frame: ahead, behind,
// right, left, up and down (rgb; the down face's w: how much of the floor below glows). Five rays a direction walk the
// bake map from the eye; each hit shows its surface's average color under
// its sector and baked light, plus its glow, as world.frag draws it. Eased
// over about a tenth of a second so single rays changing what they hit don't
// flicker. The shader lights the weapon with it as an ambient cube, and its
// glossy and grazing edges mirror it, so a bright wall ahead creeps over the
// weapon's outline even when no light reaches the player. The w of the first
// three vectors holds the player's own muzzle flash coming back off what the
// rays hit (rgb): the flash lights those surfaces, near ones the most, and
// they light the weapon, whose visible side faces away from the muzzle.
// The w of the fourth and fifth: which way around the player a glowing liquid
// lies, on the map (x, y; 0 when it is all around or nowhere).
float weaponProbe[24]={};
// The displayed player is holding the weapon drawn with this sprite (its four
// letters: the frames tell, in any game).
bool heldWeaponIs(const char *sprite) {
    const player_t &me=players[displayplayer];
    if(!me.mo||!me.psprites[0].state)return false;
    const spriteframe_t &frame=sprites[me.psprites[0].state->sprite].spriteframes[me.psprites[0].state->frame&FF_FRAMEMASK];
    return !strncmp(lumpinfo[firstspritelump+frame.lump[0]].name,sprite,4);
}
float weaponGlowColor[3]={};
static float weaponGlowLean[2]={};
static std::array<float,18> weaponProbeTarget={};
static uint64_t weaponProbeTime=0;static unsigned weaponProbeSerial=~0u;
static float weaponProbeFloorGlow=0;
static void updateWeaponProbe(const Uniforms &camera,sector_t *sector) {
    uint64_t now=SDL_GetTicksNS();
    bool fresh=weaponProbeSerial!=r_levelserial;
    weaponProbeSerial=r_levelserial;
    if(bakeMap.sectors.size()!=(size_t)numsectors)return;
    const float *axes[3]={camera.forward,camera.right,camera.up};
    float eye[3]={camera.eye[0],camera.eye[1],camera.eye[2]};
    const BakeSector &own=bakeMap.sectors[sector-sectors];
    eye[2]=std::clamp(eye[2],own.floor+1,std::max(own.floor+1,own.ceiling-1));
    bool glow=settings.emissive;
    std::array<float,3> albedoSeen={},bounce={},flashColor={};
    float glowSeen=0,floorGlow=0,glowAround=0;
    float flashRadius=0,flashStrength=0;
    bool flash=settings.weapons2&&settings.flashBounce>0&&ownMuzzleFlash(flashRadius,flashStrength,flashColor);
    auto lit=[&](const std::array<float,3> &albedo,const Image *image,int at,const uint8_t *baked,float out[3]) {
        albedoSeen=albedo;
        float level=lighting(&sectors[at]),shown[3]={};
        if(baked) {for(int c=0;c<3;++c)shown[c]=baked[c]/255.0f*2;fillHeadroom(shown,level);}
        float cover=glow&&image&&image->emissionWeight>0?image->emissionCoverage:0;
        glowSeen=cover>0?1.0f:0.0f;   // however little of it glows (radiance keeps this for glowing liquids)
        for(int c=0;c<3;++c)out[c]=albedo[c]*(level+shown[c])*(1-cover)+(cover>0?image->emissionColor[c]*cover*0.9f:0);
    };
    auto radiance=[&](const BakeHit &hit,float out[3]) {
        if(hit.kind==BakeHit::thing) {lit({0.4f,0.4f,0.4f},nullptr,hit.sector,nullptr,out);return;}
        if(hit.kind==BakeHit::wall) {
            const line_t &line=lines[hit.line];
            int side=line.frontsector==&sectors[hit.sector]?0:1;
            if(line.sidenum[side]<0) side^=1;
            const side_t &facing=sides[line.sidenum[side]];
            short texture=hit.part==1?facing.toptexture:hit.part==2?facing.bottomtexture:facing.midtexture;
            Image *image=texture>0?&wallImage(texturetranslation[texture]):nullptr;
            const uint8_t *baked=nullptr;
            const BakeStrip &strip=bakeStrips.size()>(size_t)hit.line*2+side?bakeStrips[(size_t)hit.line*2+side]:BakeStrip{};
            if(!wallBakeCells.empty()&&strip.x>=0) {
                float along=(hit.x-strip.ax)*strip.dx+(hit.y-strip.ay)*strip.dy;
                int column=std::clamp((int)std::floor(along*strip.scale),0,strip.width-1);
                int row=std::clamp((int)std::floor((hit.z-strip.low)*strip.scale),0,strip.height-1);
                baked=&wallBakeCells[4*((size_t)(strip.y+row)*atlasStride+strip.x+column)];
            }
            lit(image?averageColor(*image):std::array<float,3>{0.5f,0.5f,0.5f},image,hit.sector,baked,out);
            glowSeen=0;
            return;
        }
        bool ceiling=hit.kind==BakeHit::ceiling;
        int pic=ceiling?sectors[hit.sector].ceilingpic:sectors[hit.sector].floorpic;
        Image &image=lumpImage(firstflat+flattranslation[pic],true);
        const uint8_t *baked=nullptr;
        if(!flatLightCells.empty()) {
            int cx=std::clamp((int)std::floor((hit.x-mapOrigin[0])/mapCell),0,mapWidth-1);
            int cy=std::clamp((int)std::floor((hit.y-mapOrigin[1])/mapCell),0,mapHeight-1);
            baked=flatCell(ceiling?ceilingLight:floorLight,(size_t)cy*mapWidth+cx);
        }
        lit(averageColor(image),&image,hit.sector,baked,out);
        // Only a glowing liquid: a lit floor panel is an ordinary lamp.
        if(ceiling||liquidLook(pic)==LIQUID_LOOK_NONE)glowSeen=0;
    };
    std::array<float,18> target={};
    for(int axis=0;axis<6;++axis) {
        float sign=axis&1?-1.0f:1.0f;const float *main=axes[axis/2],*a=axes[(axis/2+1)%3],*b=axes[(axis/2+2)%3];
        // The axis and four rays tilted 35 degrees around it.
        static const float tilt[5][2]={{0,0},{0.7f,0},{-0.7f,0},{0,0.7f},{0,-0.7f}};
        // Weapons 2.0: only the down face sees a glowing floor. A ray tilted down
        // out of a side face would bring it into what the top and the sides of
        // the gun mirror. (Looking straight down they all are: then all count.)
        float all[3]={},kept[3]={};int keptRays=0;
        for(const auto &t:tilt) {
            float direction[3],length=0,out[3]={};
            glowSeen=0;
            for(int c=0;c<3;++c){direction[c]=main[c]*sign+a[c]*t[0]+b[c]*t[1];length+=direction[c]*direction[c];}
            length=std::sqrt(length);for(float &c:direction)c/=length;
            BakeHit hit;
            if(traceRay(bakeMap,(int)(sector-sectors),eye[0],eye[1],eye[2],direction,2048,SkyCeiling::escape,&hit)||hit.kind==BakeHit::none) {
                // Open sky above, or nothing within reach: the sky's light, or the room's.
                float level=lighting(sector);
                for(int c=0;c<3;++c)out[c]=direction[2]>0.3f&&skyLevel>0?skyTint[c]*skyLevel:0.5f*level;
            } else {
                glowSeen=0;
                radiance(hit,out);
                if(axis==5)floorGlow+=glowSeen/5;
                if(hit.kind!=BakeHit::wall&&hit.kind!=BakeHit::ceiling&&hit.kind!=BakeHit::thing)glowAround+=glowSeen;
                if(flash) {
                    // Out to the surface and back again.
                    float dx=hit.x-eye[0],dy=hit.y-eye[1],dz=hit.z-eye[2];
                    float reach=std::max(0.0f,1-std::sqrt(dx*dx+dy*dy+dz*dz)/flashRadius);
                    for(int c=0;c<3;++c)bounce[c]+=albedoSeen[c]*flashColor[c]*flashStrength*reach*reach/30;
                }
            }
            bool floorward=settings.weapons2&&axis!=5&&direction[2]<-0.25f&&glowSeen>0;
            for(int c=0;c<3;++c) {all[c]+=out[c];if(!floorward)kept[c]+=out[c];}
            if(!floorward)++keptRays;
        }
        for(int c=0;c<3;++c)target[axis*3+c]=keptRays?kept[c]/keptRays:all[c]/5;
    }
    float seconds=fresh||!weaponProbeTime?1.0f:std::min(0.1f,(now-weaponProbeTime)/1e9f);
    weaponProbeTime=now;float blend=1-std::exp(-seconds/0.1f);
    for(int i=0;i<18;++i) {
        weaponProbeTarget[i]+=(target[i]-weaponProbeTarget[i])*blend;
        weaponProbe[(i/3)*4+i%3]=weaponProbeTarget[i];
    }
    // A walkway through a glowing pool counts as well as the pool: six of the
    // thirty rays on a glowing floor, anywhere around.
    floorGlow=std::max(floorGlow,std::min(1.0f,glowAround/6));
    // Weapons 2.0: which way that liquid lies, and its color. A ring of rays
    // down onto the floor all around, fixed on the map so turning does not
    // make them see anything new. Walls stop them: a pool behind one is not lit.
    float lean[2]={},seen=0,seenColor[3]={};
    if(settings.weapons2&&glow&&(floorGlow>0||weaponProbeFloorGlow>0.001f)) {
        static const float pitches[2][3]={{0.906f,0.423f,0.6f},{0.574f,0.819f,1.0f}};   // 25 and 55 degrees down: cosine, sine, weight (the nearer floor counts more)
        for(int a=0;a<12;++a)for(const auto &down:pitches) {
            float angle=a*doomPi/6,direction[3]={std::cos(angle)*down[0],std::sin(angle)*down[0],-down[1]};
            BakeHit hit;
            if(traceRay(bakeMap,(int)(sector-sectors),eye[0],eye[1],eye[2],direction,2048,SkyCeiling::escape,&hit)||hit.kind==BakeHit::none)continue;
            if(hit.kind==BakeHit::wall||hit.kind==BakeHit::ceiling||hit.kind==BakeHit::thing)continue;
            int pic=sectors[hit.sector].floorpic;
            if(liquidLook(pic)==LIQUID_LOOK_NONE)continue;
            const Image &image=lumpImage(firstflat+flattranslation[pic],true);
            if(image.emissionWeight<=0||image.emissionCoverage<=0)continue;
            lean[0]+=std::cos(angle)*down[2];lean[1]+=std::sin(angle)*down[2];seen+=down[2];
            for(int c=0;c<3;++c)seenColor[c]+=image.emissionColor[c]*down[2];
        }
    }
    for(int c=0;c<2;++c)weaponGlowLean[c]+=((seen>0?lean[c]/seen:0.0f)-weaponGlowLean[c])*blend;
    weaponProbe[3*4+3]=weaponGlowLean[0];weaponProbe[4*4+3]=weaponGlowLean[1];
    // The first sight of a liquid takes its color at once, for the glow fading in.
    if(seen>0)for(int c=0;c<3;++c)weaponGlowColor[c]+=(seenColor[c]/seen-weaponGlowColor[c])*(weaponProbeFloorGlow<0.02f?1.0f:blend);
    weaponProbeFloorGlow+=(floorGlow-weaponProbeFloorGlow)*blend;
    weaponProbe[5*4+3]=weaponProbeFloorGlow;
    // Not eased: it lasts as long as the flash, a few tics. The gain keeps
    // the dark artwork in step with the bright walls it answers.
    const float bounceGain=3;
    for(int c=0;c<3;++c)weaponProbe[c*4+3]=bounce[c]*bounceGain*settings.flashBounce;
    // The BFG's and plasma rifle's flash comes back off the walls in their color and
    // faint: it reaches the whole gun evenly, and at full strength would flatten
    // the shape their own light gives (gunBlastLight).
    const float *bounceHue=!settings.weapons2?nullptr:heldWeaponIs("BFGG")?bfgGreen:heldWeaponIs("PLSG")?plasmaBlue:nullptr;
    if(bounceHue) {
        float back=(weaponProbe[3]+weaponProbe[7]+weaponProbe[11])/3;
        for(int c=0;c<3;++c)weaponProbe[c*4+3]=back*bounceHue[c]*0.3f;
    }
    // Weapons 2.0: a rocket leaving lights the launcher as a whole, a little, to
    // go with the flare on the face; the flame's in front, where the gun can't face
    // it. The shotgun's blast does the same for the gloves holding it.
    float launchRadius=0,launchStrength=0;std::array<float,3> launchColor={};
    if(settings.weapons2&&(heldWeaponIs("MISG")||heldWeaponIs("SHTG"))&&ownMuzzleFlash(launchRadius,launchStrength,launchColor))
        for(int c=0;c<3;++c)weaponProbe[c*4+3]+=launchColor[c]*launchStrength*0.2f;
}
// The weapon's place on the screen at the last tic (beforeTic), so its bob,
// raise and lower blend between tics like the world. The flash follows the
// weapon, so every layer moves by the weapon's blend.
static fixed_t oldWeaponX,oldWeaponY;
static bool oldWeaponSaved;
void saveWeaponPlace() {
    const pspdef_t &weapon=players[displayplayer].psprites[0];
    oldWeaponX=weapon.sx;oldWeaponY=weapon.sy;oldWeaponSaved=weapon.state!=nullptr;
}
// The light around the eye on a relit screen quad (the weapon, the face):
// a restrained, capped share of it, so nearby lamps tint the artwork
// without washing it out. Relit, the lights that reach it too: the weapon
// where the shader places it, about ten units ahead of the eye, the face
// (MODE_FACE) at the eye; the flashlight only tints them. With bake-only lights the static light's
// direction comes packed as for sprites.
static void lightScreenQuad(std::vector<Vertex> &quad,const Uniforms &camera,player_t *player,float light,bool relit,bool face) {
    StaticSample statics;
    auto baked=bakedLightAt(camera.eye[0],camera.eye[1],camera.eye[2],player->mo->subsector->sector,&statics);
    for(float &c:baked)c=std::min(0.5f,c*0.4f);
    unsigned tint=packedLampTint(statics,light);
    for(auto &vertex:quad) {vertex.red=baked[0];vertex.green=baked[1];vertex.blue=baked[2];vertex.lampTint=tint;}
    if(!relit)return;
    float low[3],high[3];unsigned mask[2]={},packedStatics=0;
    // Where the shader places it: the weapon ahead and below, the face (MODE_FACE) at the eye.
    for(int c=0;c<3;++c) {
        float at=face?camera.eye[c]+camera.forward[c]*2:camera.eye[c]+camera.forward[c]*10-camera.up[c]*5;
        low[c]=at-12;high[c]=at+12;
    }
    // The player's own rocket, flying off dead ahead, would flare over the whole
    // face: the face takes its launch from the muzzle flash alone.
    auto ownRocket=[&](unsigned n) {
        const mobj_t *source=lightSources[n];
        return face&&source&&lightKinds[n]==I_RENDER3D_LIGHT_PROJECTILE&&source->target==player->mo&&Game_ThingRole(source->type)==GAME_THING_ROCKET;
    };
    for(unsigned n=0;n<flashes.count;++n) {
        const auto &flash=flashes.lights[n];
        if(!fogOnly[n]&&flash.direction[3]<=0&&!ownRocket(n)&&doom_flash_reaches_bounds(&flash,low,high))mask[n/32]|=1u<<(n%32);
    }
    float towards=std::sqrt(statics.towards[0]*statics.towards[0]+statics.towards[1]*statics.towards[1]+statics.towards[2]*statics.towards[2]);
    if(bakeOnlyActive&&statics.amount>0&&towards>1e-6f) {
        for(int c=0;c<3;++c)packedStatics|=(unsigned)std::lround((statics.towards[c]/towards*0.5f+0.5f)*255)<<(8*c);
        packedStatics|=(unsigned)std::lround(std::clamp(towards/statics.amount,0.0f,1.0f)*255)<<24;
    }
    for(auto &vertex:quad) {std::copy(mask,mask+2,vertex.lightMask);vertex.statics=packedStatics;}
}
// The WAD lump a cached patch came from (the status bar keeps patches, not
// lumps); -1 for one made in code.
int lumpOfPatch(const void *patch) {
    static std::map<const void*,int> known;
    auto found=known.find(patch);if(found!=known.end())return found->second;
    int lump=-1;
    for(int i=0;i<numlumps&&lump<0;++i)if(lumpcache[i]==patch)lump=i;
    return known[patch]=lump;
}
// The screen row (of 200) where the gun's top shows in its flash frame (the
// pistol's, the rocket launcher's): the gun's first rows there are its muzzle.
static float flashMuzzleRow() {
    const player_t &me=players[displayplayer];
    const pspdef_t &flash=me.psprites[1];
    if(!flash.state)return 120;
    const spriteframe_t &frame=sprites[flash.state->sprite].spriteframes[flash.state->frame&FF_FRAMEMASK];
    int lump=firstspritelump+frame.lump[0];
    const Image &art=lumpImage(lump,false);
    static std::map<int,int> tops;
    auto found=tops.find(lump);
    if(found==tops.end()) {
        const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
        auto gun=flashBarrel(art,palette,flashArt(lump));
        int top=art.height;
        for(int y=0;y<art.height&&top==art.height;++y)for(int x=0;x<art.width;++x)if(gun[(size_t)y*art.width+x]){top=y;break;}
        found=tops.emplace(lump,top).first;
    }
    float shiftY=0;
    if(ticSaved&&oldWeaponSaved&&me.psprites[0].state) {
        float dx=units(oldWeaponX)-units(me.psprites[0].sx),dy=units(oldWeaponY)-units(me.psprites[0].sy);
        if(std::abs(dx)<64&&std::abs(dy)<64)shiftY=dy*(1-renderFraction);
    }
    return viewwindowy+viewheight/2.0f-100+units(flash.sy)+shiftY-art.top+found->second;
}
// Weapons 2.0: that vent's light on the casing around it (weapon.glsl): where
// it is on the 320 by 200 screen, how far its light reaches (screen pixels) and
// how strong it is now; false when no vent glows.
bool plasmaVentLight(float vent[4]) {
    vent[0]=vent[1]=vent[2]=vent[3]=0;
    if(!settings.weapons2||!settings.weaponLighting||!heldWeaponIs("PLSG"))return false;
    const player_t &me=players[displayplayer];
    const pspdef_t &arcs=me.psprites[1];
    if(!arcs.state)return false;
    const spriteframe_t &frame=sprites[arcs.state->sprite].spriteframes[arcs.state->frame&FF_FRAMEMASK];
    int lump=firstspritelump+frame.lump[0];
    if(strncmp(lumpinfo[lump].name,"PLSF",4))return false;
    Vent at=ventOf(lump);
    if(!at.glowing)return false;
    const Image &art=lumpImage(lump,false);
    float shiftX=0,shiftY=0;   // as weaponDraws places it between tics
    if(ticSaved&&oldWeaponSaved&&me.psprites[0].state) {
        float dx=units(oldWeaponX)-units(me.psprites[0].sx),dy=units(oldWeaponY)-units(me.psprites[0].sy);
        if(std::abs(dx)<64&&std::abs(dy)<64) {shiftX=dx*(1-renderFraction);shiftY=dy*(1-renderFraction);}
    }
    vent[0]=units(arcs.sx)+shiftX-art.left+at.x;
    vent[1]=viewwindowy+viewheight/2.0f-100+units(arcs.sy)+shiftY-art.top+at.y;
    vent[2]=vent[1]-at.y+at.top;   // its top row: the casing above it is out of its light
    vent[3]=2.2f*settings.plasmaLightStrength*settings.plasmaVentGlow*std::min(1.0f,at.glowing/12.0f);
    return vent[3]>0;
}
std::vector<SpriteDraw> weaponDraws(const Uniforms &camera) {
    std::vector<SpriteDraw> draws;
    player_t *player=&players[displayplayer];
    float shiftX=0,shiftY=0;
    if(ticSaved&&oldWeaponSaved&&player->psprites[0].state) {
        float dx=units(oldWeaponX)-units(player->psprites[0].sx),dy=units(oldWeaponY)-units(player->psprites[0].sy);
        // A jump (a new weapon's own place) is taken whole.
        if(std::abs(dx)<64&&std::abs(dy)<64) {shiftX=dx*(1-renderFraction);shiftY=dy*(1-renderFraction);}
    }
    if(settings.weaponLighting&&camera.effects[2]==0&&player->mo)updateWeaponProbe(camera,player->mo->subsector->sector);
    for(int i=0;i<NUMPSPRITES;++i) {
        auto psp=player->psprites[i];if(!psp.state)continue;
        auto sf=sprites[psp.state->sprite].spriteframes[psp.state->frame&FF_FRAMEMASK];
        bool bright=psp.state->frame&FF_FULLBRIGHT;
        // MODE_RELIT on the screen: the weapon relit per pixel from its own
        // normals, its painted light taken out.
        bool relit=settings.weaponLighting&&!bright;
        int lump=firstspritelump+sf.lump[0];
        // Weapons 2.0: the rocket launcher's flash frames bring the front of the
        // launcher with them, the plasma rifle's the whole gun, the chaingun's (in
        // chrome) the ring at its barrels' end; that part is lit as the rest of the gun is.
        bool flameOnly=bright&&settings.weaponLighting&&settings.weapons2&&(!strncmp(lumpinfo[lump].name,"MISF",4)||!strncmp(lumpinfo[lump].name,"PLSF",4)||
            !strncmp(lumpinfo[lump].name,"PISF",4)||
            (chromeChaingun()&&!strncmp(lumpinfo[lump].name,"CHGF",4)));
        bool varied=bright&&variedFlame(lump);
        if(varied)nextFlameShot(psp);
        float anchorX=0,anchorY=0;
        Image &image=varied?flameShape(lump,flameShot.shape,anchorX,anchorY):flameOnly?flashFlame(lump):relit?weaponImage(lump):lumpImage(lump,false);
        float x=units(psp.sx)+shiftX-image.left,y=viewwindowy+viewheight/2.0f-100+units(psp.sy)+shiftY-image.top;
        if(flameOnly) {
            Image &barrel=weaponImage(lump,false,false,true);
            sector_t *sector=player->mo->subsector->sector;
            float lit=sunLightAt(camera.eye[0],camera.eye[1],sector,flowLightAt(camera.eye[0],camera.eye[1],sector,
                    seamLightAt(camera.eye[0],camera.eye[1],sector,lighting(sector))));
            std::vector<Vertex> quad;
            float bx=varied?x+FLAME_MARGIN:x,by=varied?y+FLAME_MARGIN:y;   // the varied flame's image has a margin round the frame
            screenQuad(quad,bx/160-1,1-by/100,barrel.width/160.0f,barrel.height/100.0f,0,0,(float)barrel.width,(float)barrel.height,lit,
                MODE_SCREEN|MODE_SPRITE|MODE_RELIT);
            lightScreenQuad(quad,camera,player,lit,true,false);
            draws.push_back({&barrel,std::move(quad)});
        }
        std::vector<Vertex> weapon;
        float u0=sf.flip[0]?image.width:0,u1=sf.flip[0]?0:image.width;
        float light=bright?1.0f:sunLightAt(camera.eye[0],camera.eye[1],player->mo->subsector->sector,
                flowLightAt(camera.eye[0],camera.eye[1],player->mo->subsector->sector,
                seamLightAt(camera.eye[0],camera.eye[1],player->mo->subsector->sector,lighting(player->mo->subsector->sector))));
        unsigned mode=MODE_SCREEN|MODE_SPRITE|(relit?MODE_RELIT|(sf.flip[0]?MODE_FLIPPED:0u):0u);
        // The Chrome chaingun over the status bar: the gun goes on under the bar,
        // and a ring cut off at its top edge shows only its bright top row, which
        // the chrome makes a lone line. Its last row above the bar repeats the
        // one above it, so the barrels run on into the bar.
        float bottom=(float)(viewwindowy+viewheight),cut=bottom-1-y;
        if(relit&&viewwindowy+viewheight<SCREENHEIGHT&&chromeChaingun()&&!strncmp(lumpinfo[lump].name,"CHGG",4)&&cut>=1&&cut+1<image.height) {
            screenQuad(weapon,x/160-1,1-y/100,image.width/160.0f,cut/100.0f,u0,0,u1,cut,light,mode);
            screenQuad(weapon,x/160-1,1-(bottom-1)/100,image.width/160.0f,1/100.0f,u0,cut-1,u1,cut,light,mode);
        }
        else if(varied) {
            // This shot's size, about the muzzle.
            float left=x+anchorX*(1-flameShot.wide),top=y+anchorY*(1-flameShot.tall);
            screenQuad(weapon,left/160-1,1-top/100,image.width*flameShot.wide/160.0f,image.height*flameShot.tall/100.0f,u0,0,u1,image.height,light,mode);
        }
        else screenQuad(weapon,x/160-1,1-y/100,image.width/160.0f,image.height/100.0f,u0,0,u1,image.height,light,mode);
        if(bright)for(auto &vertex:weapon)vertex.lampTint=fullbrightTint;   // the gun firing: the Light level leaves it alone
        if(!bright)lightScreenQuad(weapon,camera,player,light,relit,false);
        draws.push_back({&image,std::move(weapon)});
    }
    // The status bar's face, relit as the weapon is and drawn over the bar
    // (overHud). MODE_FACE turns its normals round in the shader to face
    // ahead, where it looks: the room ahead lights it, and a lamp or a
    // muzzle flash to one side lights that side of it.
    int faceX=0,faceY=0;
    const void *face=settings.faceLighting&&settings.weaponLighting&&player->mo?ST_Face(&faceX,&faceY):nullptr;
    int faceLump=face?lumpOfPatch(face):-1;
    if(faceLump>=0) {
        // The rampage and grin faces outrank the god face (st_stuff.c); invincible,
        // they show its eyes too.
        static const std::vector<int> angryFaces=[] {
            std::vector<int> found;char name[9];
            for(int n=0;n<5;++n) {
                snprintf(name,sizeof name,"STFKILL%d",n);found.push_back(W_CheckNumForName(name));
                snprintf(name,sizeof name,"STFEVL%d",n);found.push_back(W_CheckNumForName(name));
            }
            return found;
        }();
        bool invincible=(player->cheats&CF_GODMODE)||player->powers[pw_invulnerability];
        bool angryEyes=settings.godEyes&&invincible&&std::find(angryFaces.begin(),angryFaces.end(),faceLump)!=angryFaces.end();
        Image &image=weaponImage(faceLump,true,angryEyes);
        float x=(float)(faceX-image.left),y=(float)(faceY-image.top);
        sector_t *sector=player->mo->subsector->sector;
        float light=sunLightAt(camera.eye[0],camera.eye[1],sector,flowLightAt(camera.eye[0],camera.eye[1],sector,
                seamLightAt(camera.eye[0],camera.eye[1],sector,lighting(sector))));
        std::vector<Vertex> quad;
        screenQuad(quad,x/160-1,1-y/100,image.width/160.0f,image.height/100.0f,0,0,(float)image.width,(float)image.height,light,
            MODE_SCREEN|MODE_SPRITE|MODE_RELIT|MODE_FACE);
        lightScreenQuad(quad,camera,player,light,true,true);
        draws.push_back({&image,std::move(quad),true});
    }
    return draws;
}
std::vector<Vertex> crosshairVertices(float worldW,float worldH) {
    std::vector<Vertex> cross;
    if(!worldPending||!settings.crosshair||menuactive)return cross;
    float px=2/worldW,py=2/worldH;
    screenQuad(cross,-7*px,py,5*px,2*py,0,0,1,1);screenQuad(cross,2*px,py,5*px,2*py,0,0,1,1);
    screenQuad(cross,-px,7*py,2*px,5*py,0,0,1,1);screenQuad(cross,-px,-2*py,2*px,5*py,0,0,1,1);
    return cross;
}
} // namespace doom3d
