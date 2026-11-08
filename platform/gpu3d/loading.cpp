/* The loading screen (scene3d/loading.cpp), presented on its own between
   the steps of a level's build and bakes: the 640x400 loading image placed
   like the settings panel, over black. */
#include <string>
#include "internal.h"

namespace gpu3d {
static SDL_GPUBuffer *quadBuffer; // The panel quad's six vertices.
static SDL_GPUTransferBuffer *transfer;
static std::vector<unsigned> image;

// PS1 loading screen (settings.ps1Loading): the LOADING plaque at (95, 109) of
// the PlayStation's 256x240, over the last screen it showed (the intermission,
// the title), or black after a level. The art is the player's: hud/loading.png
// in the save folder or beside the game.
std::vector<unsigned> loadingBackdrop;
bool loadingBackdropValid;
static GpuTextureRef plaque;
static bool plaqueTried;
static SDL_GPUBuffer *plaqueBuffer;
static SDL_GPUTransferBuffer *plaqueTransfer;

static void loadPlaque() {
    plaqueTried=true;
    std::string paths[2]={"hud/loading.png",""};
    if(const char *base=SDL_GetBasePath())paths[1]=std::string(base)+"hud/loading.png";
    for(const auto &path:paths) {
        if(path.empty())continue;
        SDL_Surface *file=SDL_LoadPNG(path.c_str());
        if(!file)continue;
        SDL_Surface *rgba=SDL_ConvertSurface(file,SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(file);
        if(!rgba)continue;
        plaque=gpuCreateTexture(GpuFormat::RGBA8,rgba->w,rgba->h,rgba->pixels,rgba->pitch);
        SDL_DestroySurface(rgba);
        if(plaque)return;
    }
}
// False with no plaque to show: the title and progress bar then.
static bool showPlaque(SDL_GPUCommandBuffer *command,SDL_GPUTexture *swapchain,Uint32 drawableWidth,Uint32 drawableHeight) {
    if(!plaqueTried)loadPlaque();
    if(!plaque)return false;
    const Uint32 backdropBytes=SCREENWIDTH*SCREENHEIGHT*4;
    std::vector<Vertex> quads;
    screenQuad(quads,-1,1,2,2,0,0,1,1);
    Uint32 full=(Uint32)quads.size();
    screenQuad(quads,95/128.0f-1,1-109/120.0f,64/128.0f,20/120.0f,0,0,1,1);
    Uint32 quadBytes=(Uint32)(quads.size()*sizeof(Vertex));
    if(!plaqueBuffer) {
        SDL_GPUBufferCreateInfo info={SDL_GPU_BUFFERUSAGE_VERTEX,quadBytes,0};
        plaqueBuffer=SDL_CreateGPUBuffer(device,&info);
    }
    if(!plaqueTransfer) {
        SDL_GPUTransferBufferCreateInfo info={SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,backdropBytes+quadBytes,0};
        plaqueTransfer=SDL_CreateGPUTransferBuffer(device,&info);
    }
    if(!plaqueBuffer||!plaqueTransfer)return false;
    bool backdrop=loadingBackdropValid&&loadingBackdrop.size()==(size_t)SCREENWIDTH*SCREENHEIGHT;
    byte *mapped=(byte*)SDL_MapGPUTransferBuffer(device,plaqueTransfer,true);
    if(backdrop)memcpy(mapped,loadingBackdrop.data(),backdropBytes);
    memcpy(mapped+backdropBytes,quads.data(),quadBytes);
    SDL_UnmapGPUTransferBuffer(device,plaqueTransfer);
    SDL_GPUCopyPass *copy=SDL_BeginGPUCopyPass(command);
    if(backdrop) {
        SDL_GPUTextureTransferInfo pixels={plaqueTransfer,0,SCREENWIDTH,SCREENHEIGHT};
        SDL_GPUTextureRegion target={};
        target.texture=hudTexture;target.w=SCREENWIDTH;target.h=SCREENHEIGHT;target.d=1;
        SDL_UploadToGPUTexture(copy,&pixels,&target,true);
    }
    SDL_GPUTransferBufferLocation vertices={plaqueTransfer,backdropBytes};
    SDL_GPUBufferRegion buffer={plaqueBuffer,0,quadBytes};
    SDL_UploadToGPUBuffer(copy,&vertices,&buffer,true);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUColorTargetInfo info={};
    info.texture=swapchain;info.load_op=SDL_GPU_LOADOP_CLEAR;info.store_op=SDL_GPU_STOREOP_STORE;info.clear_color={0,0,0,1};
    SDL_GPURenderPass *pass=SDL_BeginGPURenderPass(command,&info,1,nullptr);
    SDL_BindGPUGraphicsPipeline(pass,settingsPipeline);
    // The 4:3 picture in the window, where the HUD goes.
    float w=std::min((float)drawableWidth,drawableHeight*4.0f/3),h=w*0.75f;
    SDL_GPUViewport view={(drawableWidth-w)/2,(drawableHeight-h)/2,w,h,0,1};
    SDL_SetGPUViewport(pass,&view);
    Uniforms none={};
    SDL_PushGPUVertexUniformData(command,0,&none,sizeof(none));
    SDL_GPUBufferBinding binding={plaqueBuffer,0};
    SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    if(backdrop) {
        SDL_GPUTextureSamplerBinding picture={hudTexture,linearClamp};
        SDL_BindGPUFragmentSamplers(pass,0,&picture,1);
        SDL_DrawGPUPrimitives(pass,full,1,0,0);
    }
    SDL_GPUTextureSamplerBinding art={gpu(plaque),linearClamp};
    SDL_BindGPUFragmentSamplers(pass,0,&art,1);
    SDL_DrawGPUPrimitives(pass,(Uint32)quads.size()-full,1,full,0);
    SDL_EndGPURenderPass(pass);
    flushUploads();
    if(!SDL_SubmitGPUCommandBuffer(command))fprintf(stderr,"GPU loading screen: %s\n",SDL_GetError());
    return true;
}

void showLoading(const char *title,const char *stage,float fraction) {
    // Window events wait in the queue for the game; pumping them keeps the
    // window moving and the system from calling the game unresponsive.
    SDL_PumpEvents();
    SDL_GPUCommandBuffer *command=SDL_AcquireGPUCommandBuffer(device);
    if(!command)return;
    SDL_GPUTexture *swapchain=nullptr;Uint32 drawableWidth=0,drawableHeight=0;
    if((SDL_GetWindowFlags(gameWindow)&SDL_WINDOW_HIDDEN)||!SDL_WaitAndAcquireGPUSwapchainTexture(command,gameWindow,&swapchain,&drawableWidth,&drawableHeight)||!swapchain) {
        SDL_SubmitGPUCommandBuffer(command);return;
    }
    if(settings.ps1Loading&&showPlaque(command,swapchain,drawableWidth,drawableHeight))return;
    image.assign((size_t)DOOM_SETTINGS_WIDTH*DOOM_SETTINGS_HEIGHT,0);
    I_Render3DLoadingDraw(image.data(),title,stage,fraction);
    float origin[2],scale;
    std::vector<Vertex> quad=panelQuad(drawableWidth,drawableHeight,origin,scale);
    Uint32 imageBytes=(Uint32)(image.size()*4),quadBytes=(Uint32)(quad.size()*sizeof(Vertex));
    if(!quadBuffer) {
        SDL_GPUBufferCreateInfo info={SDL_GPU_BUFFERUSAGE_VERTEX,quadBytes,0};
        quadBuffer=SDL_CreateGPUBuffer(device,&info);
    }
    if(!transfer) {
        SDL_GPUTransferBufferCreateInfo info={SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,imageBytes+quadBytes,0};
        transfer=SDL_CreateGPUTransferBuffer(device,&info);
    }
    if(!quadBuffer||!transfer) {SDL_SubmitGPUCommandBuffer(command);return;}
    byte *mapped=(byte*)SDL_MapGPUTransferBuffer(device,transfer,true);
    memcpy(mapped,image.data(),imageBytes);
    memcpy(mapped+imageBytes,quad.data(),quadBytes);
    SDL_UnmapGPUTransferBuffer(device,transfer);
    SDL_GPUCopyPass *copy=SDL_BeginGPUCopyPass(command);
    SDL_GPUTextureTransferInfo pixels={transfer,0,DOOM_SETTINGS_WIDTH,DOOM_SETTINGS_HEIGHT};
    SDL_GPUTextureRegion target={};
    target.texture=settingsTexture;target.w=DOOM_SETTINGS_WIDTH;target.h=DOOM_SETTINGS_HEIGHT;target.d=1;
    SDL_UploadToGPUTexture(copy,&pixels,&target,true);
    SDL_GPUTransferBufferLocation vertices={transfer,imageBytes};
    SDL_GPUBufferRegion buffer={quadBuffer,0,quadBytes};
    SDL_UploadToGPUBuffer(copy,&vertices,&buffer,true);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUColorTargetInfo info={};
    info.texture=swapchain;info.load_op=SDL_GPU_LOADOP_CLEAR;info.store_op=SDL_GPU_STOREOP_STORE;info.clear_color={0,0,0,1};
    SDL_GPURenderPass *pass=SDL_BeginGPURenderPass(command,&info,1,nullptr);
    SDL_BindGPUGraphicsPipeline(pass,settingsPipeline);
    Uniforms none={};
    SDL_PushGPUVertexUniformData(command,0,&none,sizeof(none));
    SDL_GPUTextureSamplerBinding panel={settingsTexture,nearestClamp};
    SDL_BindGPUFragmentSamplers(pass,0,&panel,1);
    SDL_GPUBufferBinding binding={quadBuffer,0};
    SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_DrawGPUPrimitives(pass,(Uint32)quad.size(),1,0,0);
    SDL_EndGPURenderPass(pass);
    // Textures the bakes made so far go up now rather than piling up.
    flushUploads();
    if(!SDL_SubmitGPUCommandBuffer(command))fprintf(stderr,"GPU loading screen: %s\n",SDL_GetError());
}
void releaseLoading() {
    release(quadBuffer,SDL_ReleaseGPUBuffer);
    release(transfer,SDL_ReleaseGPUTransferBuffer);
    release(plaqueBuffer,SDL_ReleaseGPUBuffer);
    release(plaqueTransfer,SDL_ReleaseGPUTransferBuffer);
    plaque=nullptr;plaqueTried=false;
    image.clear();image.shrink_to_fit();
}
} // namespace gpu3d
