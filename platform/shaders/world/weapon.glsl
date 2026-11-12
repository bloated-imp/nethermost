// The relit weapon (weaponDraws, weapon_lighting.h): its surface, and the
// light from its surroundings and the lights around the player on it.
// Part of world.frag, which declares the resources these read.
// The relit weapon looking toward a light (ahead: 1 dead ahead): the light
// creeps in from the silhouette, more on the side nearer it, with a faint
// veil over the rest; glossy edges mirror it at grazing angles (mirror).
float weaponCreep(vec3 n,vec3 view,vec3 toLight,out float mirror) {
    float ahead=saturate(dot(toLight,-view));
    float edge=1.0-saturate(dot(n,view));
    float around=saturate((dot(n,toLight)+1.0)*0.5);
    mirror=pow(saturate(dot(reflect(-view,n),toLight)),12.0)*edge*ahead;
    mirror=mirror>0.4?1.4:mirror>0.15?0.5:0.0;
    return ahead*ahead*around*(0.1+0.9*edge*edge);
}
// F4 Light level (c.levels.x) on what the weapon sees around it: the room
// it reflects is as much darker as the room itself. Weapons 2.0 off, the
// weapon is DERP's, which no Light level reaches (the face still follows it).
float roomLevel() {return c.levels.x>0.0&&(c.levels.w>0.0||(vMode&MODE_FACE)!=0u)?c.levels.x:1.0;}
// c.levels.w: F4 Nethermost, Weapons 2.0.
// Weapons 2.0: how far the weapon takes the look it has over a glowing floor,
// 0 anywhere else (weaponProbe[5].w: how much of the floor around glows).
// Everywhere else its shape and shine come from the painted detail as well,
// which is what makes a light seem to move over it.
float overGlow() {return c.levels.w>0.0&&(vMode&MODE_FACE)==0u?smoothstep(0.0,0.6,weaponProbe[5].w):0.0;}
// A colour in the colormaps' 1/16 steps, stepped by its brightest channel so
// it keeps its hue. Stepped channel by channel, a slightly warm light has
// red round up while green and blue round down: pink on grey metal.
vec3 hueSteps(vec3 color) {
    if(c.levels.w<=0.0) return round(color*16.0)/16.0;
    float top=max(color.r,max(color.g,color.b));
    return top>0.0?color*(round(top*16.0)/16.0)/top:vec3(0);
}
// Weapons 2.0: over a glowing floor what is behind the gun is the player
// holding it, who hides the glow behind him: a dim, colorless stand-in.
vec3 behindGun() {
    return mix(weaponProbe[1].rgb,vec3(dot(weaponProbe[1].rgb,vec3(0.299,0.587,0.114)))*0.6,overGlow());
}
// The player's own muzzle flash on the weapon, kept apart for the look over
// a glowing floor (world.frag), where the room's light is replaced.
vec3 ownWeaponFlash=vec3(0);
// The plasma rifle's vent (below): its light on the paint and its glow over it,
// kept apart from the rest so world.frag adds them after it greys and caps the
// gun's light for its metal, which would turn the blue on olive paint green.
vec3 ventLit=vec3(0),ventGlow=vec3(0);
// The Chrome chaingun (world.frag): all the player's own muzzle flash gives the
// gun, light and highlights, kept apart so the chrome's softened shading
// doesn't halve it; and sharp glints of the lights round it along its tubes.
vec3 ownFlashLit=vec3(0),ownFlashSpec=vec3(0),chromeGlint=vec3(0);
// The light behind the head, for the ears to let through (world.frag).
vec3 earBacklight=vec3(0);
// How much of a light in that direction reaches the ear at across (the face's
// picture, -1 left to 1 right; its right is the player's left): none from ahead,
// both ears from straight behind, the nearer one from behind to a side.
float behindEar(vec3 toLight,float across) {
    return smoothstep(0.1,0.8,-dot(toLight,c.forward.xyz))*saturate(0.7-sign(across)*dot(toLight,c.right.xyz));
}
// The weapon's surroundings toward a direction in the view's frame (x
// right, y up, z ahead), blended as an ambient cube.
vec3 weaponSurroundings(vec3 d) {
    vec3 w=d*d;
    return roomLevel()*(w.x*(d.x>0.0?weaponProbe[2]:weaponProbe[3]).rgb+w.y*(d.y>0.0?weaponProbe[4]:weaponProbe[5]).rgb+
           w.z*(d.z>0.0?weaponProbe[0].rgb:behindGun()));
}
// MODE_SCREEN|MODE_RELIT: the relit weapon (weaponDraws, weapon_lighting.h). Its
// painted light was taken out at load; its normals face the viewer, and
// its gloss sits two columns past the artwork's right edge. It stands
// about ten units ahead of the eye, where the screen pixel points. The
// sector's light falls from above: undersides and the rounded edges
// that turn away from the eye darken, in the colormaps' 1/16 steps.
// Returns the weapon's normal, the point it stands at, the direction to the
// eye and its gloss, and shades light by its normal.
// across: where along the sprite's width, -1 its left on the screen to 1 its right.
struct Weapon { vec3 normal,at,view; float gloss,metal,fromBelow,across; };
Weapon weaponSurface(inout float light) {
    vec3 weaponNormal,weaponAt,weaponView;vec4 weaponMaterial;
    ivec2 size=textureSize(image,0),p=clamp(ivec2(floor(vUV)),ivec2(0),size-1);
    int width=(size.x-2)/2;
    weaponMaterial=texelFetch(image,ivec2(min(p.x,width-1)+width+2,p.y),0);
    // Over a glowing floor (overGlow) the shape comes from the silhouette alone,
    // for the rim light: read from the painted shading as well, that bright
    // a light turns the weapon's detail to grit.
    vec2 packed=mix(texelFetch(image,p,0).ba,weaponMaterial.ba,overGlow())*2.0-1.0;
    if((vMode&MODE_FLIPPED)!=0u) packed.x=-packed.x;
    weaponNormal=normalize(c.right.xyz*packed.x-c.up.xyz*packed.y-c.forward.xyz*sqrt(saturate(1.0-dot(packed,packed))));
    weaponAt=c.eye.xyz+10.0*(c.forward.xyz+c.right.xyz*vWorld.x/c.projection.x+c.up.xyz*vWorld.y/c.projection.y);
    weaponView=normalize(c.eye.xyz-weaponAt);
    // MODE_FACE: the status bar's face looks out at the view, so its normals
    // turn round to face ahead, mirrored (the picture's right is its left
    // cheek); it sits at the eye and is seen from ahead, so lights in front
    // fall on it and one to a side lights that side.
    if((vMode&MODE_FACE)!=0u) {
        weaponNormal=normalize(-c.right.xyz*packed.x-c.up.xyz*packed.y+c.forward.xyz*sqrt(saturate(1.0-dot(packed,packed))));
        weaponAt=c.eye.xyz+2.0*c.forward.xyz;
        weaponView=c.forward.xyz;
    }
    float edge=1.0-saturate(dot(weaponNormal,weaponView));
    float lit=light;
    light=round(light*(0.95+0.25*weaponNormal.z)*(1.0-0.4*edge*edge)*16.0)/16.0;
    // The face keeps at least the darkest step: in the darkest rooms the
    // texels turned furthest down and away (the jaw's) would otherwise round
    // to nothing, pure black beside faintly lit neighbours.
    if((vMode&MODE_FACE)!=0u&&lit>0.0) light=max(light,1.0/16.0);
    // Weapons 2.0: standing over a glowing floor, the room's colored light comes
    // up from below. It cannot reach what faces up, and most of what we see
    // of a held gun is its top and back (weaponProbe[5].w: how much of the
    // floor below glows).
    float fromBelow=c.levels.w>0.0&&(vMode&MODE_FACE)==0u?weaponProbe[5].w*smoothstep(-0.6,0.2,dot(weaponNormal,c.up.xyz)):0.0;
    // Over a glowing floor metal shines evenly, not in the speckles of where
    // the art had highlights.
    float gloss=c.levels.z>=0.0?mix(weaponMaterial.x,mix(weaponMaterial.x,0.55*weaponMaterial.y,0.8),overGlow()):weaponMaterial.x;
    float across=(float(min(p.x,width-1))+0.5)/float(width)*2.0-1.0;
    if((vMode&MODE_FLIPPED)!=0u) across=-across;
    return Weapon(weaponNormal,weaponAt,weaponView,gloss,weaponMaterial.y,fromBelow,across);
}
// Lights around the player on the relit weapon (w from weaponSurface),
// added to pulse and specular; light is its shaded sector light.
void weaponLight(Weapon w,float light,inout vec3 pulse,inout vec3 specular) {
    // Lights around the player tint and shape the weapon with a
    // capped share, so they never wash out the artwork; the side
    // facing a light takes most of it, and a light ahead creeps
    // over it from the silhouette (weaponCreep). Highlights
    // come in two hard steps on the gloss map, sharper where it is
    // glossier: polish over dark metal, not an even sheen. The light
    // from above gives one too, sliding over the metal as the view
    // pitches and the weapon bobs.
    vec3 n=w.normal,view=w.view,weaponAt=w.at;float weaponGloss=w.gloss;
    // The face is skin, not metal: no hard highlights or mirrored room,
    // only a faint broad sheen from each light (face) and a little of the
    // room at its grazing edges.
    bool face=(vMode&MODE_FACE)!=0u;
    // F4 Nethermost, Separate skin and metal (c.levels.z, the metal's shine;
    // below 0 off): what is not metal, a hand or a wooden stock, is matte as
    // the face is, and the metal takes more of the room and sharper highlights.
    float skin=face?1.0:c.levels.z<0.0?0.0:1.0-w.metal;
    float shine=face||c.levels.z<0.0?0.0:w.metal*c.levels.z,polish=1.0+0.6*shine;
    weaponGloss*=1.0-skin;
    // A bright rim where the outline turns to a light reads as wet on skin.
    float rimGain=mix(1.6,0.6,skin);
    // The room's tint arrives as one color for the whole gun: the part of it
    // from below weakens, keeping its color (world.frag does the same to the lamp tint).
    if(w.fromBelow>0.0) pulse*=1.0-0.5*w.fromBelow;
    float shininess=10.0+30.0*weaponGloss;
    // The sprite's rounded normals describe its camera-facing skin,
    // not the top of the barrel. Approximate that upper plane for
    // broad metal highlights, fading it out on downward-facing parts.
    // Over a glowing floor only the gun's real upper edge. With normals from
    // the silhouette the broad middle faces the eye, and would take half of
    // this as one flat pale patch.
    float upper=mix(smoothstep(-0.25,0.25,dot(n,c.up.xyz)),smoothstep(0.05,0.5,dot(n,c.up.xyz)),overGlow());
    vec3 upperNormal=normalize(c.up.xyz+0.35*n);
    float upperShininess=8.0+16.0*weaponGloss;
    // That upper plane also takes light: a lamp ahead and above falls
    // on the top of the barrel, which the eye-facing normals turn
    // away from. A lamp ahead at eye level still misses it (creep and
    // the grazing mirror cover that); the stronger of the two wins,
    // so a light both see is not counted twice. Only light from ahead
    // counts: overhead the sector's light already shades the weapon,
    // and the open sky outside would flood the barrel.
    const float upperDiffuse=0.5;
    float top=pow(saturate(dot(n,normalize(vec3(0,0,1)+view))),shininess);
    specular+=vec3(light*weaponGloss*(top>0.5?0.35:top>0.2?0.15:0.0));
    if(c.bake.z>0.0&&vStatic!=0u) {
        vec4 statics=unpackUnorm4x8(vStatic);
        vec3 toLight=normalize(statics.xyz*2.0-1.0);
        float facing=saturate(dot(n,toLight));
        float rim=pow(1.0-saturate(dot(n,view)),2.0)*facing*saturate(0.35-dot(toLight,view));
        float highlight=pow(saturate(dot(n,normalize(toLight+view))),shininess),mirror;
        float creep=weaponCreep(n,view,toLight,mirror);
        specular+=pulse*statics.w*weaponGloss*polish*((highlight>0.5?1.6:highlight>0.2?0.6:0.0)+mirror);
        vec3 halfDirection=(toLight+view)/max(length(toLight+view),0.001);
        specular+=pulse*statics.w*weaponGloss*upper*pow(saturate(dot(upperNormal,halfDirection)),upperShininess)*0.8;
        specular+=pulse*statics.w*0.12*skin*pow(saturate(dot(n,halfDirection)),4.0);
        if(c.weaponSheen.w<0.0) chromeGlint+=pulse*statics.w*pow(saturate(dot(n,halfDirection)),48.0);
        float lit=max(facing,upper*upperDiffuse*saturate(dot(upperNormal,toLight))*smoothstep(0.1,0.6,dot(toLight,c.forward.xyz)));
        if(face) earBacklight+=pulse*statics.w*behindEar(toLight,w.across);
        pulse*=mix(1.0,0.25+1.0*lit+rimGain*rim+1.2*creep,statics.w);
    }
    // The room around it, even with no light near: a fill from each
    // side; glossy faces and every grazing edge mirror what lies
    // that way (mostly dark, bright where a lit wall is), in 1/16
    // steps; and a bright view ahead creeps over the outline.
    // surroundings: the strength of everything taken from the probe.
    const float surroundings=0.7;
    mat3 frame=transpose(mat3(c.right.xyz,c.up.xyz,c.forward.xyz));
    vec3 environment=weaponSurroundings(frame*n)*0.3;
    float edge=1.0-saturate(dot(n,view)),base=0.02+0.12*weaponGloss+shine*(0.18+0.22*weaponGloss);
    vec3 mirrored=weaponSurroundings(frame*reflect(-view,n))*(base+(1.0-base)*pow(edge,4.0));
    specular+=hueSteps(mirrored*0.8*surroundings*mix(1.0,0.3,skin));
    // Like a glossy floor, the upper metal reflects a bright panel
    // ahead. The ambient cube represents a broad source, rather than
    // a sharp point highlight. Keep the reflection in palette steps.
    // Over a glowing floor the top of the gun mirrors what is above it, not
    // the glow ahead.
    vec3 upperReflection=weaponSurroundings(normalize(mix(frame*reflect(-view,upperNormal),normalize(vec3(0.0,1.0,0.3)+0.25*(frame*n)),overGlow())));
    specular+=hueSteps(upperReflection*weaponGloss*polish*upper*0.8*surroundings);
    float aheadMirror;
    environment+=roomLevel()*weaponProbe[0].rgb*(0.12+0.6*weaponCreep(n,view,c.forward.xyz,aheadMirror));
    specular+=roomLevel()*weaponProbe[0].rgb*weaponGloss*aheadMirror*0.4*surroundings;
    uvec2 mask=vLightMask;
    while(any(notEqual(mask,uvec2(0)))) {
        uint word=mask.x!=0u?0u:1u;
        uint i=word*32u+uint(findLSB(mask[word]));
        mask[word]&=mask[word]-1u;
        float amount=flashAt(weaponAt,i);
        if(amount<=0.0) continue;
        amount=min(amount,1.5)*0.5;
        vec3 toLight=normalize(lights[i].position.xyz-weaponAt);
        bool ownFlash=false;vec3 pulseBefore=pulse,specularBefore=specular;
        // The face's own hitscan muzzle flash shines from the player's
        // middle, just under and behind the face, so it would graze only
        // the edges; it comes from the gun out in front, a little low.
        if((vMode&MODE_FACE)!=0u&&distance(lights[i].position.xyz,c.eye.xyz)<24.0)
            {toLight=normalize(c.forward.xyz-0.3*c.up.xyz);amount*=c.levels.y;}   // F4 Nethermost, Muzzle flash on face
        // On the weapon, the player's own flash shines from out past the
        // muzzle, ahead of the gun, not from inside it at the player's
        // middle: it rims and creeps over the gun from in front rather than
        // flooding the whole of it.
        else if(c.levels.w>0.0&&(vMode&MODE_FACE)==0u&&distance(lights[i].position.xyz,c.eye.xyz)<24.0)
            {toLight=normalize(c.eye.xyz+48.0*c.forward.xyz-6.0*c.up.xyz-weaponAt);ownWeaponFlash+=lights[i].color.rgb*amount*0.7;ownFlash=true;}
        if(face) earBacklight+=lights[i].color.rgb*amount*behindEar(toLight,w.across);
        float facing=saturate(dot(n,toLight));
        float rim=pow(1.0-saturate(dot(n,view)),2.0)*facing*saturate(0.35-dot(toLight,view));
        // A static light's light already arrives in the tint, and a
        // bright sector leaves it little headroom; its copy only
        // moves that light toward the side facing it, keeping about
        // the same total, so it shapes the weapon even in bright rooms.
        float bakedShare=c.bake.x>0.0?lights[i].baked:0.0;
        float lit=max(facing,upper*upperDiffuse*saturate(dot(upperNormal,toLight))*smoothstep(0.1,0.6,dot(toLight,c.forward.xyz)));
        pulse+=lights[i].color.rgb*amount*(0.15+1.1*lit+rimGain*rim-0.6*bakedShare);
        float highlight=pow(saturate(dot(n,normalize(toLight+view))),shininess),mirror;
        // Looking toward the light, it also spills over the weapon.
        pulse+=lights[i].color.rgb*amount*1.2*weaponCreep(n,view,toLight,mirror);
        // A matte own light (c.weaponSheen.w 0.5, the pistol): its own flash, out
        // ahead of it and pointing away, puts no highlights on it either.
        float glints=ownFlash&&c.weaponSheen.w>0.0?saturate(c.weaponSheen.w*2.0-1.0):1.0;
        specular+=glints*lights[i].color.rgb*amount*weaponGloss*polish*((highlight>0.5?1.6:highlight>0.2?0.6:0.0)+mirror);
        vec3 halfDirection=(toLight+view)/max(length(toLight+view),0.001);
        specular+=glints*lights[i].color.rgb*amount*weaponGloss*upper*pow(saturate(dot(upperNormal,halfDirection)),upperShininess)*0.8;
        specular+=lights[i].color.rgb*amount*0.12*skin*pow(saturate(dot(n,halfDirection)),4.0);
        float glint=c.weaponSheen.w<0.0?pow(saturate(dot(n,halfDirection)),48.0):0.0;
        if(ownFlash) {ownFlashLit+=pulse-pulseBefore;ownFlashSpec+=specular-specularBefore+lights[i].color.rgb*amount*glint;}
        else chromeGlint+=lights[i].color.rgb*amount*glint;
    }
    // Weapons 2.0, the BFG charging and firing or the plasma rifle firing
    // (c.weaponSheen, w 1): the blob or arcs hang ahead of the gun and above it, a light as any other, so that the
    // art's own shape shows in it: the tops and edges take it, what faces the
    // player stays darker, the metal's gloss gives hard highlights. Strongest at
    // the muzzle (c.bfgLight.z, its row of the 200 on the screen, whichever frame
    // of the gun is showing), and fading with distance from it as the god face's
    // eyes' light does: c.bfgLight.x how far (1 the 60 rows of gun that show),
    // y how sharply. In the same 1/16 steps.
    if(c.weaponSheen.w>0.0&&!face) {
        float fromMuzzle=length(vec2(vWorld.x*160.0*0.35,max((1.0-vWorld.y)*100.0-c.bfgLight.z,0.0)))/60.0;
        float near=saturate(1.0-fromMuzzle/max(c.bfgLight.x,0.01));
        vec3 tint=c.weaponSheen.rgb*round(pow(near,c.bfgLight.y)*16.0)/16.0;
        vec3 toLight=normalize(c.eye.xyz+36.0*c.forward.xyz+14.0*c.up.xyz-weaponAt);
        float facing=saturate(dot(n,toLight));
        float rim=pow(1.0-saturate(dot(n,view)),2.0)*facing*saturate(0.35-dot(toLight,view));
        float lit=max(facing,upper*upperDiffuse*saturate(dot(upperNormal,toLight))*smoothstep(0.1,0.6,dot(toLight,c.forward.xyz)));
        float mirror,creep=weaponCreep(n,view,toLight,mirror);
        pulse+=tint*(c.bfgLight.w+1.1*lit+rimGain*rim+1.2*creep);
        float highlight=pow(saturate(dot(n,normalize(toLight+view))),shininess);
        float shines=saturate(c.weaponSheen.w*2.0-1.0);   // 0.5: matte, no highlights (the pistol)
        specular+=shines*tint*weaponGloss*polish*((highlight>0.5?1.6:highlight>0.2?0.6:0.0)+mirror);
        vec3 halfDirection=(toLight+view)/max(length(toLight+view),0.001);
        specular+=shines*tint*weaponGloss*upper*pow(saturate(dot(upperNormal,halfDirection)),upperShininess)*0.8;
    }
    // Weapons 2.0, the plasma rifle firing (c.plasmaVent): one of its flash
    // frames lights the vent at the back of its casing, and that glow lights the
    // gun round it and below it, most what turns toward it, fading over 34
    // screen pixels from it (x, y: where it is on the 320 by 200 screen; z: its
    // top row; w: how bright, 0 none). It opens below the beige casing, which
    // stands above it out of its light. In 1/16 steps.
    if(c.plasmaVent.w>0.0&&!face) {
        vec2 at=vec2((vWorld.x+1.0)*160.0,(1.0-vWorld.y)*100.0),d=c.plasmaVent.xy-at;
        d.y*=1.2;   // the screen's pixels are taller than wide
        float near=saturate(1.0-length(d)/34.0)*smoothstep(c.plasmaVent.z-1.5,c.plasmaVent.z+0.5,at.y);near*=near;
        vec3 toVent=normalize(c.right.xyz*d.x-c.up.xyz*d.y-c.forward.xyz*6.0);
        float facing=saturate(dot(n,toVent));
        // The plasma's own blue. The paint round the vent is olive and khaki,
        // with little blue to give back: light alone turns teal or green on it,
        // so most of the vent's shows as a glow over the paint.
        const vec3 hue=vec3(0.2,0.32,1.0);
        vec3 tint=hue*c.plasmaVent.w*round(near*16.0)/16.0;
        ventLit+=tint*(0.15+0.9*facing);
        float highlight=pow(saturate(dot(n,normalize(toVent+view))),shininess);
        ventGlow+=hueSteps(tint*(0.22+0.25*facing+weaponGloss*(highlight>0.5?0.5:highlight>0.2?0.2:0.0)));
    }
    // Static-light redistribution may subtract its own baked tint,
    // but must not consume the independent light from the room.
    // Metal shows the room in its reflection more than in a matte fill.
    pulse=max(pulse,vec3(0))+environment*surroundings*(1.0-0.25*min(shine,3.0));
    // A room brighter behind the player than ahead backlights the head too; an
    // evenly lit one doesn't.
    if(face) {
        const vec3 luma=vec3(0.299,0.587,0.114);
        earBacklight+=roomLevel()*weaponProbe[1].rgb*0.7*smoothstep(0.05,0.4,dot(weaponProbe[1].rgb-weaponProbe[0].rgb,luma));
    }
    // The player's own muzzle flash back off the walls around (weaponProbe's
    // w): soft over the side we see, a little more at its turning edges.
    if(!face) {
        vec3 bounced=vec3(weaponProbe[0].w,weaponProbe[1].w,weaponProbe[2].w)*(0.6+0.4*edge);
        pulse+=bounced;ownWeaponFlash+=bounced;ownFlashLit+=bounced;
    }
}
