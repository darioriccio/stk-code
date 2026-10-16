// Diagnostic ghosts never enter the SC1 target list, live oracle or mission core.
// Three no-write occlusion queries execute this SAME shader at output resolution.
uniform int scintilla_measure_mode; // 0 normal,1 ideal footprint,2 HUD/clip/range,3 scene depth
uniform int scintilla_measure_index;
uniform int scintilla_measure_count;
uniform int scintilla_measure_show;
uniform int scintilla_measure_layout_revision; // explicit v2 SC4 / v3 SC5
uniform int scintilla_measure_actual_indices[4]; // native 0..31 mapping or -1, never entity IDs
uniform vec4 scintilla_measure_spheres[4]; // camera XYZ,radius
uniform vec4 scintilla_measure_styles[4]; // kind,phase,range admitted,rectangle admitted
// SC1 markers are visible target primitives, separate from damage and pickup logic.
// All positions/radii, depth tests and ray conventions match the historical resolver.
uniform int scintilla_enabled;
uniform int scintilla_combat_enabled;
uniform vec4 scintilla_combat_targets[32]; // phase,count,required,acquisition
uniform vec4 scintilla_combat_actors[32]; // opener,contributors,eligible,reserved
uniform vec4 scintilla_mission_targets[32]; // kind (0 legacy/1 capsule/2 tether),phase,progress,eligible
uniform float scintilla_mission_owners[32]; // source slot 0..4 or -1; never participant/entity IDs
uniform vec4 scintilla_mission_tools[5]; // Status,owner source slot,outcome,progress; never entity IDs
uniform vec4 scintilla_mission_cable_start; // immutable frame camera-space XYZ,radius
uniform vec4 scintilla_mission_cable_end; // immutable frame camera-space XYZ,enabled
uniform int scintilla_mission_hud_enabled; // required layout, including terminal results
uniform vec4 scintilla_mission_slots[3]; // state,seconds remaining (-1 awaiting),beacon lit,ever acquired
uniform vec4 scintilla_mission_beacons[3]; // decorative frame camera-space XYZ,radius
uniform vec4 scintilla_mission_beacon_states; // persistent lit flags xyz,reserved
uniform vec4 scintilla_mission_nucleus[3]; // decorative frame camera-space XYZ,radius
uniform vec4 scintilla_mission_result; // outcome code,official rank (0 absent),secured count,atlas ready
uniform sampler2D scintilla_mission_result_text; // five transparent Italian rows, native top-row UV
uniform int scintilla_target_count;
uniform vec4 scintilla_targets[32];
uniform vec4 scintilla_reticles[5]; // top-left u,v,valid,held mode
uniform vec4 scintilla_latency_flash[5]; // top-left u,v,active,radius in pixels (diagnostic)
uniform vec4 scintilla_tools[5]; // ToolStatus, opener source slot, reserved, reserved
uniform vec4 scintilla_feedback[5]; // frozen u,v,FeedbackKind,remaining fade
uniform vec2 scintilla_near_far;
uniform uint scintilla_stamp[8]; // THB1, stream128, frame64, CRC32; little endian

// SC3 layout data is generated once by the producer and retained in each frame.
uniform int scintilla_defense_enabled;
uniform int scintilla_layout_enabled; // historical geometry may precede gameplay preparation
uniform vec4 scintilla_rear_rect; // output top-left UV rectangle
uniform vec4 scintilla_defense_nodes[5]; // u,v,radiusPixels,sector (boost=4)
uniform vec4 scintilla_defense_state; // phase,sector,progress,terminal outcome
uniform float scintilla_boost_progress;
uniform float scintilla_boost_cost; // same profile value used for admission/debit
uniform vec4 scintilla_defense_resources; // energy,reserve,heat,boost Status
uniform vec4 scintilla_defense_source; // camera-space center,radius
uniform vec4 scintilla_defense_pulse;
uniform vec4 scintilla_defense_tools[5]; // status,owner slot,intent context,sector
uniform vec4 scintilla_defense_badges[5]; // persistent u,v,transient u,v
uniform vec4 scintilla_defense_bars[2]; // top-left UV rect
uniform int scintilla_crew_enabled;
uniform int scintilla_crew_text_ready;
uniform vec4 scintilla_crew_badges[5]; // atlas name,competence,handoff/pointer state,reserved
uniform vec4 scintilla_crew_tasks[5]; // real task and target atlas cells
uniform vec4 scintilla_crew_target_masks[8]; // packed 32 participant-colour hints on real instances
uniform vec4 scintilla_crew_tools[5]; // progress,consenting,reason,reserved
uniform vec4 scintilla_crew_party_state; // state,reason,deadline seconds,consents
uniform vec4 scintilla_crew_party_flags; // quorum,cost,used,profile enabled
vec2 scintillaMissionHUDPoint(vec2 topUV,vec2 dimensions)
{
    return (2.0*topUV*dimensions-dimensions)/min(dimensions.x,dimensions.y);
}
bool scintillaMissionHUDReserved(vec2 topUV,vec2 dimensions)
{
    if(scintilla_mission_hud_enabled==0&&scintilla_mission_result.x<.5)return false;
    vec2 p=scintillaMissionHUDPoint(topUV,dimensions);
    if(scintilla_mission_result.x>.5&&p.x>=-.76&&p.x<=.76&&p.y>=-.08&&p.y<=.40)return true;
    if(scintilla_mission_hud_enabled==0)return false;
    for(int slot=0;slot<3;slot++){
        float x=(float(slot)-1.0)*.18;
        if(length(p-vec2(x,-.16))<=.06||length(p-vec2(x,-.27))<=.025)return true;
    }
    return false;
}
int scintillaSurface(vec2 uv,vec2 dimensions)
{
    if(scintillaMissionHUDReserved(uv,dimensions))return 5;
    if(scintilla_crew_enabled!=0&&length(scintillaMissionHUDPoint(uv,dimensions)-vec2(.22,.85))<=.06)return 6;
    if(scintilla_layout_enabled==0)return 1;
    for(int i=0;i<5;i++)if(length((uv-scintilla_defense_nodes[i].xy)*dimensions)<=scintilla_defense_nodes[i].z)return i<4?2:3;
    if(all(greaterThanEqual(uv,scintilla_rear_rect.xy))&&all(lessThan(uv,scintilla_rear_rect.zw)))return 4;
    for(int i=0;i<2;i++)if(all(greaterThanEqual(uv,scintilla_defense_bars[i].xy))&&all(lessThanEqual(uv,scintilla_defense_bars[i].zw)))return 5;
    float halfBox=.0275*min(dimensions.x,dimensions.y);
    for(int i=0;i<5;i++)for(int row=0;row<2;row++){
        vec2 p=row==0?scintilla_defense_badges[i].xy:scintilla_defense_badges[i].zw;
        if(all(lessThanEqual(abs((uv-p)*dimensions),vec2(halfBox))))return 5;
    }
    return 1;
}
// Same float policy as DepthVisibility::visible in the historical resolver.
bool scintillaDepthVisible(float scene,float z,float n,float f)
{
    if(any(isnan(vec4(scene,z,n,f))) || any(isinf(vec4(scene,z,n,f))))return false;
    if(!(scene>=0.0 && scene<=1.0 && n>0.0 && z>n && z<f))return false;
    // Stable 1-depth, avoiding cancellation at distant normalized depths.
    const float depthStep=1.0/16777215.0;
    const float arithmeticGuard=8.0/8388608.0;
    // GLSL 4.10 const initializers must be constant expressions; these three
    // values depend on runtime parameters, unlike the two bounds above.
    float markerGap=(n/(f-n))*((f-z)/z);
    // D24 conversion/quantization and float arithmetic uncertainty only shrink
    // visibility. Equality/overlap is occluded, including near a clear far plane.
    float sceneGapUpper=(1.0-scene)+2.0*depthStep;
    float markerGapLower=markerGap*(1.0-arithmeticGuard);
    return markerGapLower>sceneGapUpper;
}
float scintillaDepth(sampler2D image, vec2 uv)
{
    ivec2 size=textureSize(image,0);
    return texelFetch(image,clamp(ivec2(uv*vec2(size)),ivec2(0),size-1),0).r;
}
float scintillaSceneDepth(vec3 d, out float ma)
{
    vec3 a=abs(d);vec2 q;
    if(a.x>=a.y && a.x>=a.z) {
        ma=a.x;q=vec2(d.x>=0.0?-d.z:d.z,d.y)/(ma*face_tan_half_fov)*0.5+0.5;
        return d.x>=0.0?scintillaDepth(depth_face_pos_x,q):scintillaDepth(depth_face_neg_x,q);
    }
    if(a.y>=a.z) {
        ma=a.y;q=vec2(d.x,d.y>=0.0?-d.z:d.z)/(ma*face_tan_half_fov)*0.5+0.5;
        return d.y>=0.0?scintillaDepth(depth_face_pos_y,q):scintillaDepth(depth_face_neg_y,q);
    }
    ma=a.z;q=vec2(d.z>=0.0?d.x:-d.x,d.y)/(ma*face_tan_half_fov)*0.5+0.5;
    return d.z>=0.0?scintillaDepth(depth_face_pos_z,q):scintillaDepth(depth_face_neg_z,q);
}
vec3 scintillaIdentityColor(int slot)
{
    if(slot==0)return vec3(0.0,1.0,1.0);
    if(slot==1)return vec3(1.0,0.65,0.0);
    if(slot==2)return vec3(1.0,0.3,0.9);
    if(slot==3)return vec3(0.4,1.0,0.2);
    return vec3(0.7,0.65,1.0);
}
float scintillaSegmentDistance(vec2 p,vec2 a,vec2 b)
{
    vec2 d=b-a;
    return length(p-a-d*clamp(dot(p-a,d)/max(dot(d,d),0.0001),0.0,1.0));
}
float scintillaCableSphereContact(vec3 direction,vec3 center,float radius)
{
    float along=dot(center,direction);
    vec3 perpendicular=center-direction*along;
    float remaining=radius*radius-dot(perpendicular,perpendicular);
    if(remaining<0.0)return 1e20;
    float contact=along-sqrt(remaining);
    return contact>0.0?contact:1e20;
}
// A decorative, non-pickable finite capsule. Compute closest approach instead
// of subtracting squared distances at ~100m for a 25mm cable. The same front
// contact is tested against native scene depth before colouring the pixel.
float scintillaCableContact(vec3 direction,vec3 start,vec3 end,float radius)
{
    if(any(isnan(vec4(start,radius)))||any(isinf(vec4(start,radius)))||
       any(isnan(end))||any(isinf(end))||radius<=0.0||radius>1.0)return 1e20;
    vec3 segment=end-start;float span=length(segment);
    if(span<.00001){
        if(dot(start,start)<=radius*radius)return 1e20;
        return scintillaCableSphereContact(direction,start,radius);
    }
    vec3 axis=segment/span;
    vec3 nearestToEye=start+axis*clamp(-dot(start,axis),0.0,span);
    if(dot(nearestToEye,nearestToEye)<=radius*radius)return 1e20;
    float contact=min(scintillaCableSphereContact(direction,start,radius),
                      scintillaCableSphereContact(direction,end,radius));
    float axialRay=dot(direction,axis),axialOrigin=-dot(start,axis);
    vec3 rayPerpendicular=direction-axis*axialRay;
    vec3 originPerpendicular=-start-axis*axialOrigin;
    float a=dot(rayPerpendicular,rayPerpendicular);
    if(a>1e-10){
        float middle=-dot(rayPerpendicular,originPerpendicular)/a;
        vec3 closest=originPerpendicular+rayPerpendicular*middle;
        float remaining=radius*radius-dot(closest,closest);
        if(remaining>=0.0){
            float side=middle-sqrt(remaining/a);
            float along=axialOrigin+side*axialRay;
            if(side>0.0&&along>=0.0&&along<=span)contact=min(contact,side);
        }
    }
    return contact;
}
// Styling only: the complete sphere disc stays visible in every phase. The
// caller applies the same selected front surface and strict depth test used by
// historical picking; neither a crack nor the open core creates a hit aperture.
vec3 scintillaMissionTint(vec2 icon,float offset,vec4 state,int owner)
{
    int phase=int(floor(state.y+0.5));
    vec3 tint;
    if(state.x>1.5){
        // A clock identifies the tether even while first presentation is
        // pending. Inward chevrons appear only in the recoverable phase.
        tint=vec3(.17,.19,.34);
        float clock=abs(offset-.39);
        clock=min(clock,scintillaSegmentDistance(icon,vec2(0),vec2(0,.23)));
        clock=min(clock,scintillaSegmentDistance(icon,vec2(0),vec2(.19,-.10)));
        tint=mix(tint,vec3(.88,.86,1.0),1.0-smoothstep(.022,.042,clock));
        if(phase==5){
            float arrows=min(scintillaSegmentDistance(icon,vec2(-.70,.18),vec2(-.50,0)),
                             scintillaSegmentDistance(icon,vec2(-.50,0),vec2(-.70,-.18)));
            arrows=min(arrows,min(scintillaSegmentDistance(icon,vec2(.70,.18),vec2(.50,0)),
                                  scintillaSegmentDistance(icon,vec2(.50,0),vec2(.70,-.18))));
            tint=mix(tint,vec3(.55,1.0,.84),1.0-smoothstep(.025,.045,arrows));
        }else{
            float pause=min(scintillaSegmentDistance(icon,vec2(-.11,-.18),vec2(-.11,.18)),
                            scintillaSegmentDistance(icon,vec2(.11,-.18),vec2(.11,.18)));
            tint=mix(tint,vec3(1.0,.85,.3),1.0-smoothstep(.025,.045,pause));
        }
        if(offset>.79&&offset<.91){
            float turn=mod(atan(icon.y,icon.x)+6.28318530718,6.28318530718)/6.28318530718;
            vec3 progressTint=owner>=0&&owner<5?scintillaIdentityColor(owner):vec3(.55,1.0,.84);
            tint=turn<clamp(state.z,0.0,1.0)?progressTint:vec3(.24,.26,.42);
        }
        if(offset>.94)tint=owner>=0&&owner<5?scintillaIdentityColor(owner):vec3(.72,.68,1.0);
    }else if(phase==0){
        tint=mix(vec3(0.35,0.17,0.055),vec3(1.0,0.72,0.18),smoothstep(0.78,0.96,offset));
        float crack=scintillaSegmentDistance(icon,vec2(-.05,.70),vec2(.13,.20));
        crack=min(crack,scintillaSegmentDistance(icon,vec2(.13,.20),vec2(-.18,.04)));
        crack=min(crack,scintillaSegmentDistance(icon,vec2(-.18,.04),vec2(.15,-.30)));
        crack=min(crack,scintillaSegmentDistance(icon,vec2(.15,-.30),vec2(.03,-.65)));
        tint=mix(tint,vec3(.025),1.0-smoothstep(.055,.075,crack));
        tint=mix(tint,vec3(1.0,.95,.70),1.0-smoothstep(.022,.038,crack));
    }else{
        // The recoverable core and inward chevrons differ from the intact
        // shell by shape as well as colour. The dark backing remains opaque.
        tint=vec3(.10,.27,.30);
        tint=mix(tint,vec3(.12,.95,.78),1.0-smoothstep(.32,.39,offset));
        float core=abs(offset-.43);
        float chevrons=min(scintillaSegmentDistance(icon,vec2(-.72,.20),vec2(-.52,0)),
                           scintillaSegmentDistance(icon,vec2(-.52,0),vec2(-.72,-.20)));
        chevrons=min(chevrons,min(scintillaSegmentDistance(icon,vec2(.72,.20),vec2(.52,0)),
                                 scintillaSegmentDistance(icon,vec2(.52,0),vec2(.72,-.20))));
        tint=mix(tint,vec3(.70,1.0,.94),1.0-smoothstep(.025,.045,min(core,chevrons)));
        if(offset>.79&&offset<.91){
            float turn=mod(atan(icon.y,icon.x)+6.28318530718,6.28318530718)/6.28318530718;
            vec3 progressTint=owner>=0&&owner<5?scintillaIdentityColor(owner):vec3(1.0,.94,.30);
            tint=turn<clamp(state.z,0.0,1.0)?progressTint:vec3(.16,.40,.43);
        }
        if(offset>.94)tint=owner>=0&&owner<5?scintillaIdentityColor(owner):vec3(.65,.95,1.0);
    }
    if(state.w<.5){
        // Diagonal hatching communicates an unavailable target without
        // removing any part of its visible or selectable silhouette.
        float hatch=step(.5,fract((icon.x+icon.y)*5.0));
        float gray=dot(tint,vec3(.2126,.7152,.0722));
        tint=mix(vec3(.12+.55*gray),vec3(.20+.75*gray),hatch);
    }
    return tint;
}
vec3 scintillaMissionDecorationTint(vec2 icon,float offset,bool lit,bool nucleus)
{
    vec3 tint=nucleus?vec3(.06,.28,.24):lit?vec3(.18,.23,.08):vec3(.10,.12,.16);
    float glyph=1e4;
    if(nucleus){
        glyph=min(abs(offset-.34),abs(offset-.69));
        if(offset<.25)tint=vec3(.35,1.0,.78);
    }else if(lit){
        for(int point=0;point<5;point++){
            float a=float(point)*1.25663706144-1.57079632679;
            vec2 outer=.62*vec2(cos(a),sin(a));
            vec2 inner=.26*vec2(cos(a+.62831853072),sin(a+.62831853072));
            vec2 next=.62*vec2(cos(a+1.25663706144),sin(a+1.25663706144));
            glyph=min(glyph,min(scintillaSegmentDistance(icon,outer,inner),scintillaSegmentDistance(icon,inner,next)));
        }
    }else{
        // An unlit lamp outline differs from a lit five-point star by shape.
        glyph=abs(length(icon-vec2(0,.06))-.34);
        glyph=min(glyph,scintillaSegmentDistance(icon,vec2(-.18,-.40),vec2(.18,-.40)));
    }
    vec3 ink=nucleus?vec3(.52,1.0,.84):lit?vec3(1.0,.93,.35):vec3(.50,.58,.68);
    tint=mix(tint,ink,1.0-smoothstep(.025,.045,glyph));
    if(offset>.91)tint=ink;
    return tint;
}
vec3 scintillaFeedback(vec3 color,vec2 uvGL,vec2 dimensions)
{
    vec3 palette[5]=vec3[5](vec3(0.0,1.0,1.0),vec3(1.0,0.65,0.0),vec3(1.0,0.3,0.9),vec3(0.4,1.0,0.2),vec3(0.7,0.65,1.0));
    for(int i=0;i<5;i++) {
        vec4 f=scintilla_feedback[i];
        int kind=int(floor(f.z+0.5));
        if(kind<1 || kind>7 || f.w<=0.0)continue;
        vec2 p=(uvGL-vec2(f.x,1.0-f.y))*dimensions;
        if(scintilla_defense_enabled!=0)p/=max(0.1,.0275*min(dimensions.x,dimensions.y)/32.0);
        // Glyphs, identity dots and their outlines fit in this pixel box.
        if(max(abs(p.x),abs(p.y))>32.0)continue;
        float distance=1e4;
        vec3 tint=palette[i];
        if(kind==1) {
            // Dashed ring: an actual accepted shot is waiting on historical data.
            if(fract(atan(p.y,p.x)*1.27323954+0.5)>0.35)distance=abs(length(p)-18.0);
        } else if(kind==2) {
            // Ring and check: confirmed hit, distinguishable without colour.
            distance=min(abs(length(p)-18.0),min(scintillaSegmentDistance(p,vec2(-7.0,0.0),vec2(-2.0,-5.0)),
                scintillaSegmentDistance(p,vec2(-2.0,-5.0),vec2(8.0,6.0))));
            tint=vec3(0.25,1.0,0.35);
        } else if(kind==3) {
            // Diagonal cross: valid shot resolved as a miss.
            distance=min(scintillaSegmentDistance(p,vec2(-8.0,-8.0),vec2(8.0,8.0)),
                scintillaSegmentDistance(p,vec2(-8.0,8.0),vec2(8.0,-8.0)));
            tint=vec3(1.0,0.9,0.25);
        } else if(kind==5) {
            // Contribution: one filled square, distinct from a completed effect.
            distance=abs(max(abs(p.x),abs(p.y))-8.0);tint=vec3(1.0,0.85,0.15);
        } else if(kind==6) {
            // Applied stagger: double ring, only after the physics factory succeeds.
            distance=min(abs(length(p)-13.0),abs(length(p)-19.0));tint=vec3(0.3,1.0,0.35);
        } else if(kind==7) {
            // Shield rebound: outward chevrons, never a geometric-hit check.
            distance=min(abs(abs(p.x)-abs(p.y)-8.0)*0.70710678,abs(p.y)-10.0);
            distance=abs(distance);tint=vec3(1.0,0.5,0.1);
        } else {
            // Diamond and exclamation: denied/unverifiable, never a presumed miss.
            distance=min(abs(abs(p.x)+abs(p.y)-20.0)*0.70710678,
                min(scintillaSegmentDistance(p,vec2(0.0,5.0),vec2(0.0,-2.0)),length(p-vec2(0.0,-7.0))));
            tint=vec3(1.0,0.35,0.1);
        }
        float alpha=clamp(f.w,0.0,1.0);
        float outline=(1.0-smoothstep(2.2,3.2,distance))*alpha;
        float glyph=(1.0-smoothstep(0.8,1.8,distance))*alpha;
        color=mix(color,vec3(0.015),outline);
        color=mix(color,tint,glyph);
        // Source identity remains the same colour and dot count for every result.
        for(int j=0;j<5;j++)if(j<=i) {
            float dotDistance=length(p-vec2((float(j)-float(i)*0.5)*5.0,-26.0));
            color=mix(color,vec3(0.015),(1.0-smoothstep(2.2,3.2,dotDistance))*alpha);
            color=mix(color,palette[i],(1.0-smoothstep(1.0,2.0,dotDistance))*alpha);
        }
    }
    return color;
}
// Persistent control instructions live outside world geometry. These badges
// never imply a hit volume, and cannot expire like primary-shot feedback.
vec3 scintillaToolFeedback(vec3 color,vec2 uvGL,vec2 dimensions)
{
    if(scintilla_combat_enabled==0||scintilla_mission_result.x>.5)return color;
    // Keep five badges separate at small output sizes as well. Their anchors
    // and complete pixel boxes lie inside the fisheye disc and full sphere.
    float scale=min(1.0,dimensions.x/1024.0);
    if(scintilla_defense_enabled!=0)scale=.0275*min(dimensions.x,dimensions.y)/32.0;
    for(int i=0;i<5;i++){
        int status=int(scintilla_tools[i].x+0.5);
        vec4 mission=scintilla_mission_tools[i];
        bool missionBadge=(mission.x>.5||mission.z>.5)&&
            (scintilla_defense_enabled==0||scintilla_defense_tools[i].z==1.0);
        if(missionBadge)status=int(floor((mission.x>.5?mission.x:mission.z)+.5));
        if(scintilla_defense_enabled!=0&&scintilla_defense_tools[i].z!=1.0){
            int ds=int(scintilla_defense_tools[i].x+0.5);status=ds==1||ds==4||ds==5||ds==6||ds==9||ds==10||ds==11?1:ds==2?2:0;
        }
        if((!missionBadge&&status!=1&&status!=2)||(missionBadge&&(status<1||status>9)))continue;
        vec2 anchor=vec2(0.34+0.08*float(i),1.0-0.62);
        if(scintilla_defense_enabled!=0)anchor=vec2(scintilla_defense_badges[i].x,1.0-scintilla_defense_badges[i].y);
        vec2 p=(uvGL-anchor)*dimensions/scale;
        if(max(abs(p.x),abs(p.y))>32.0)continue;
        float panel=max(abs(p.x)-29.0,abs(p.y)-30.0);
        color=mix(color,vec3(0.015),0.9*(1.0-smoothstep(0.0,1.0,panel)));
        float d=1e4;
        vec3 tint;
        if(status==1){
            // Two tool buttons in reading order: lift off, then press again.
            float buttons=min(abs(max(abs(p.x+12.0)-6.0,abs(p.y+3.0)-4.0)),
                              abs(max(abs(p.x-12.0)-6.0,abs(p.y+3.0)-4.0)));
            float lift=scintillaSegmentDistance(p,vec2(-12,3),vec2(-12,17));
            lift=min(lift,min(scintillaSegmentDistance(p,vec2(-16,13),vec2(-12,17)),
                              scintillaSegmentDistance(p,vec2(-8,13),vec2(-12,17))));
            float press=scintillaSegmentDistance(p,vec2(12,17),vec2(12,3));
            press=min(press,min(scintillaSegmentDistance(p,vec2(8,7),vec2(12,3)),
                                scintillaSegmentDistance(p,vec2(16,7),vec2(12,3))));
            float next=min(scintillaSegmentDistance(p,vec2(-4,-3),vec2(4,-3)),
                min(scintillaSegmentDistance(p,vec2(1,0),vec2(4,-3)),scintillaSegmentDistance(p,vec2(1,-6),vec2(4,-3))));
            d=min(min(buttons,lift),min(press,next));tint=vec3(1.0,0.9,0.25);
        }else if(status==2){
            // Padlock + pause bars: another participant owns this target.
            float body=abs(max(abs(p.x)-10.0,abs(p.y+3.0)-8.0));
            float shackle=p.y>=6.0?abs(length(p-vec2(0,6))-7.0):1e4;
            shackle=min(shackle,min(scintillaSegmentDistance(p,vec2(-7,0),vec2(-7,6)),
                                    scintillaSegmentDistance(p,vec2(7,0),vec2(7,6))));
            float pause=min(scintillaSegmentDistance(p,vec2(-3,-7),vec2(-3,1)),
                            scintillaSegmentDistance(p,vec2(3,-7),vec2(3,1)));
            d=min(body,min(shackle,pause));tint=vec3(0.85,0.85,1.0);
            int owner=int(floor(scintilla_tools[i].y+0.5));
            if(missionBadge)owner=int(floor(mission.y+.5));
            if(scintilla_defense_enabled!=0&&scintilla_defense_tools[i].z!=1.0)owner=int(scintilla_defense_tools[i].y);
            for(int j=0;j<5;j++)if(owner>=0&&owner<5&&j<=owner){
                float dotDistance=length(p-vec2((float(j)-float(owner)*0.5)*5.0,23.0));
                color=mix(color,scintillaIdentityColor(owner),1.0-smoothstep(1.0,2.0,dotDistance));
            }
        }else if(status==3){
            // Three filled cargo boxes and a stop bar: all slots are occupied.
            for(int slot=0;slot<3;slot++){
                vec2 q=p-vec2((float(slot)-1.0)*16.0,4.0);
                d=min(d,abs(max(abs(q.x)-5.0,abs(q.y)-7.0)));
                d=min(d,scintillaSegmentDistance(q,vec2(-2,-3),vec2(2,3)));
            }
            d=min(d,scintillaSegmentDistance(p,vec2(-21,-10),vec2(21,-10)));
            tint=vec3(1.0,.72,.25);
        }else if(status==4){
            // Separated shell halves with a central core: primary opened it.
            d=min(scintillaSegmentDistance(p,vec2(-12,-10),vec2(-18,0)),
                  scintillaSegmentDistance(p,vec2(-18,0),vec2(-12,10)));
            d=min(d,min(scintillaSegmentDistance(p,vec2(12,-10),vec2(18,0)),
                        scintillaSegmentDistance(p,vec2(18,0),vec2(12,10))));
            d=min(d,abs(length(p)-5.0));tint=vec3(.5,1.0,.8);
        }else if(status==5){
            // Tool button plus inward arrows: hold secondary on the open core.
            d=abs(max(abs(p.x)-7.0,abs(p.y)-6.0));
            d=min(d,min(scintillaSegmentDistance(p,vec2(-20,5),vec2(-12,0)),
                        scintillaSegmentDistance(p,vec2(-12,0),vec2(-20,-5))));
            d=min(d,min(scintillaSegmentDistance(p,vec2(20,5),vec2(12,0)),
                        scintillaSegmentDistance(p,vec2(12,0),vec2(20,-5))));
            tint=vec3(.5,1.0,.8);
        }else if(status==6||status==7){
            // A check inside cargo; a second return arc distinguishes recovery.
            d=abs(max(abs(p.x)-12.0,abs(p.y)-10.0));
            d=min(d,min(scintillaSegmentDistance(p,vec2(-7,0),vec2(-2,-5)),
                        scintillaSegmentDistance(p,vec2(-2,-5),vec2(8,6))));
            if(status==7)d=min(d,abs(length(p)-18.0));
            tint=vec3(.25,1.0,.45);
        }else{
            // Lost cargo has an empty box and cross; rejected proof uses a
            // diamond/exclamation, preserving the distinction from a miss.
            if(status==8){
                d=abs(max(abs(p.x)-12.0,abs(p.y)-10.0));
                d=min(d,min(scintillaSegmentDistance(p,vec2(-6,-6),vec2(6,6)),
                            scintillaSegmentDistance(p,vec2(-6,6),vec2(6,-6))));
            }else{
                d=abs(abs(p.x)+abs(p.y)-18.0)*.70710678;
                d=min(d,min(scintillaSegmentDistance(p,vec2(0,7),vec2(0,-2)),length(p-vec2(0,-7))));
            }
            tint=vec3(1.0,.4,.15);
        }
        color=mix(color,vec3(0.0),1.0-smoothstep(2.2,3.2,d));
        color=mix(color,tint,1.0-smoothstep(0.8,1.8,d));
        if(missionBadge){
            // Progress is informational. It never replaces the release/busy
            // instruction or feeds a transaction back into gameplay.
            if(mission.w>0.0&&p.y>20.0&&p.y<22.0&&abs(p.x)<23.0)
                color=p.x+23.0<46.0*clamp(mission.w,0.0,1.0)?vec3(.3,1.0,.8):vec3(.18);
            int outcome=int(floor(mission.z+.5));
            if(status==1){
                // Completion/full/loss can all require release. Keep the lift
                // and press arrows, plus a separate bounded outcome pictogram.
                float result=1e4;vec3 resultTint=vec3(.25,1.0,.45);
                if(outcome==3){
                    for(int slot=0;slot<3;slot++){
                        vec2 q=p-vec2((float(slot)-1.0)*8.0,26.0);
                        result=min(result,max(abs(q.x)-2.5,abs(q.y)-2.0));
                    }
                    resultTint=vec3(1.0,.72,.25);
                }else if(outcome==8){
                    vec2 q=p-vec2(0,23);
                    result=abs(max(abs(q.x)-5.0,abs(q.y)-3.5));
                    result=min(result,min(scintillaSegmentDistance(q,vec2(-2,-2),vec2(2,2)),
                                          scintillaSegmentDistance(q,vec2(-2,2),vec2(2,-2))));
                    resultTint=vec3(1.0,.4,.15);
                }else if(outcome==4){
                    vec2 q=p-vec2(21,22);
                    result=min(abs(abs(q.x)-4.0)+abs(q.y)*.5,abs(q.x)+abs(q.y)-1.0);
                }else if(outcome==6||outcome==7){
                    vec2 q=p-vec2(21,22);
                    result=min(scintillaSegmentDistance(q,vec2(-4,0),vec2(-1,-3)),
                               scintillaSegmentDistance(q,vec2(-1,-3),vec2(4,3)));
                    if(outcome==7)result=min(result,abs(length(q)-6.0));
                }
                color=mix(color,vec3(.015),1.0-smoothstep(2.2,3.2,result));
                color=mix(color,resultTint,1.0-smoothstep(.8,1.8,result));
            }
        }
        // Recipient identity, distinct from the opener identity above the lock.
        for(int j=0;j<5;j++)if(j<=i){
            float dotDistance=length(p-vec2((float(j)-float(i)*0.5)*5.0,-25.0));
            color=mix(color,scintillaIdentityColor(i),1.0-smoothstep(1.0,2.0,dotDistance));
        }
    }
    return color;
}
// Small numeric pictogram, kept inside the existing boost disc. Segment bits:
// top, upper-right, lower-right, bottom, lower-left, upper-left, middle.
float scintillaCostDigit(vec2 p,int digit)
{
    int masks[10]=int[10](63,6,91,79,102,109,125,7,127,111);
    int bits=masks[clamp(digit,0,9)];float d=1000.0;
    if((bits&1)!=0)d=min(d,scintillaSegmentDistance(p,vec2(-.4,-.8),vec2(.4,-.8)));
    if((bits&2)!=0)d=min(d,scintillaSegmentDistance(p,vec2(.4,-.8),vec2(.4,0)));
    if((bits&4)!=0)d=min(d,scintillaSegmentDistance(p,vec2(.4,0),vec2(.4,.8)));
    if((bits&8)!=0)d=min(d,scintillaSegmentDistance(p,vec2(-.4,.8),vec2(.4,.8)));
    if((bits&16)!=0)d=min(d,scintillaSegmentDistance(p,vec2(-.4,0),vec2(-.4,.8)));
    if((bits&32)!=0)d=min(d,scintillaSegmentDistance(p,vec2(-.4,-.8),vec2(-.4,0)));
    if((bits&64)!=0)d=min(d,scintillaSegmentDistance(p,vec2(-.4,0),vec2(.4,0)));
    return 1.0-smoothstep(.06,.12,d);
}
float scintillaBoostCostGlyph(vec2 q)
{
    int cost=int(clamp(floor(scintilla_boost_cost+.5),0.0,99.0));
    float glyph=1.0-smoothstep(.015,.03,scintillaSegmentDistance(q,vec2(-.44,.25),vec2(-.24,.25)));
    glyph=max(glyph,scintillaCostDigit((q-vec2(0,.25))/vec2(.26,.21),cost/10));
    return max(glyph,scintillaCostDigit((q-vec2(.32,.25))/vec2(.26,.21),cost%10));
}
float scintillaCrewText(vec2 p,vec2 center,vec2 halfSize,float cell)
{
    if(scintilla_crew_text_ready==0||cell<0.0||cell>=134.0)return 0.0;
    vec2 local=(p-center)/(2.0*halfSize)+.5;
    if(any(lessThan(local,vec2(0)))||any(greaterThan(local,vec2(1))))return 0.0;
    float index=floor(cell+.5);
    vec2 start=vec2(mod(index,8.0)*256.0,floor(index/8.0)*64.0),size=vec2(256,64);
    if(index>=128.0){
        index-=128.0;start=vec2(mod(index,2.0)*1024.0,1840.0+floor(index/2.0)*64.0);size.x=1024.0;
    }
    // Half-texel inner bounds prevent another cell bleeding under filtering.
    vec2 at=(start+mix(vec2(.5),size-vec2(.5),local))/vec2(2048);
    return texture(scintilla_mission_result_text,at).a;
}
bool scintillaCrewResultReady()
{
    return scintilla_crew_enabled!=0&&scintilla_crew_text_ready!=0&&
        scintilla_mission_result.x>.5&&scintilla_crew_party_flags.w>1.5;
}
float scintillaCrewResultNumber(vec2 p,vec2 center,float count,float overflow)
{
    int value=int(clamp(floor(count+.5),0.0,999.0));float glyph=0.0;
    if(value>=100)glyph=scintillaCostDigit((p-center-vec2(-.038,0))/vec2(.020,.023),value/100);
    if(value>=10)glyph=max(glyph,scintillaCostDigit((p-center)/vec2(.020,.023),(value/10)%10));
    glyph=max(glyph,scintillaCostDigit((p-center-vec2(.038,0))/vec2(.020,.023),value%10));
    if(overflow>.5){
        vec2 plus=p-center-vec2(.073,0);
        float d=min(scintillaSegmentDistance(plus,vec2(-.008,0),vec2(.008,0)),
                    scintillaSegmentDistance(plus,vec2(0,-.008),vec2(0,.008)));
        glyph=max(glyph,1.0-smoothstep(.001,.003,d));
    }
    return glyph;
}
vec3 scintillaCrewResultHUD(vec3 color,vec2 p)
{
    // The original terminal rectangle is the entire clipping boundary.
    if(!scintillaCrewResultReady()||p.x<-.76||p.x>.76||p.y<-.08||p.y>.40)return color;
    float text=scintillaCrewText(p,vec2(-.43,.18),vec2(.24,.06),scintilla_crew_party_state.x>1.5?111.0:110.0);
    text=max(text,scintillaCrewText(p,vec2(.40,.18),vec2(.23,.0575),112.0));
    text=max(text,scintillaCrewText(p,vec2(-.52,.235),vec2(.12,.03),113.0));
    text=max(text,scintillaCrewResultNumber(p,vec2(-.31,.235),scintilla_crew_party_state.y,scintilla_crew_party_flags.y));
    text=max(text,scintillaCrewText(p,vec2(-.03,.235),vec2(.12,.03),114.0));
    text=max(text,scintillaCrewResultNumber(p,vec2(.18,.235),scintilla_crew_party_state.z,scintilla_crew_party_flags.z));
    text=max(text,scintillaCrewText(p,vec2(.52,.235),vec2(.16,.04),scintilla_crew_party_flags.x>.5?115.0:116.0));
    color=mix(color,vec3(.92,.95,1.0),text);
    for(int i=0;i<6;++i){
        if(float(i)>=scintilla_crew_party_state.w)break;
        vec4 person=scintilla_crew_target_masks[i];
        float x=mod(float(i),2.0)<.5?-.38:.38,y=.282+.046*floor(float(i)/2.0);
        float label=scintillaCrewText(p,vec2(x-.08,y),vec2(.235,.0146875),128.0+person.x);
        label=max(label,scintillaCrewResultNumber(p,vec2(x+.20,y),person.y,person.z));
        // A zero stays neutral; the count is a sum of confirmed effect credits.
        color=mix(color,person.y>0.0?vec3(.70,1.0,.82):vec3(.78,.83,.88),label);
    }
    return color;
}
vec3 scintillaCrewHUD(vec3 color,vec2 topUV,vec2 dimensions)
{
    if(scintilla_crew_enabled==0)return color;
    vec2 p=scintillaMissionHUDPoint(topUV,dimensions);
    if(scintilla_mission_result.x>.5)return scintillaCrewResultHUD(color,p);
    for(int i=0;i<5;i++)for(int row=0;row<2;row++){
        vec2 center=vec2(-.32+.16*float(i),row==0?.08:.25),q=p-center;
        if(any(greaterThan(abs(q),vec2(.055)))||scintilla_crew_badges[i].x<0.0)continue;
        vec3 tint=scintillaIdentityColor(i);color=mix(color,vec3(.025,.04,.055),.94);
        if(max(abs(q.x),abs(q.y))>.050)color=mix(color,tint,.8);
        float textAlpha;
        if(row==0){
            textAlpha=scintillaCrewText(p,center+vec2(0,-.032),vec2(.049,.012),scintilla_crew_badges[i].x);
            textAlpha=max(textAlpha,scintillaCrewText(p,center,vec2(.049,.012),scintilla_crew_badges[i].y));
            if(scintilla_crew_badges[i].w>=0.0){
                textAlpha=max(textAlpha,scintillaCrewText(p,center+vec2(-.025,.032),vec2(.024,.012),scintilla_crew_badges[i].z));
                textAlpha=max(textAlpha,scintillaCrewText(p,center+vec2(.025,.032),vec2(.024,.012),scintilla_crew_badges[i].w));
            }else textAlpha=max(textAlpha,scintillaCrewText(p,center+vec2(0,.032),vec2(.049,.012),scintilla_crew_badges[i].z));
        }else{
            textAlpha=scintillaCrewText(p,center+vec2(0,-.021),vec2(.049,.012),scintilla_crew_tasks[i].x);
            textAlpha=max(textAlpha,scintillaCrewText(p,center+vec2(0,.021),vec2(.049,.012),scintilla_crew_tasks[i].y));
        }
        color=mix(color,vec3(.98),textAlpha);
        if(row==1&&int(scintilla_crew_tasks[i].x+.5)==77&&q.y>=.041&&q.y<=.047&&abs(q.x)<=.047){
            float progress=clamp(scintilla_crew_tools[i].x,0.0,1.0);
            color=q.x<=-.047+.094*progress?tint:vec3(.12,.14,.17);
            if(scintilla_crew_tools[i].y>.5)color=tint;
        }
    }
    // Disabled Party keeps the v3 exclusion stable and paints no invitation.
    vec2 q=(p-vec2(.22,.85))/.06;float radius=length(q);
    if(scintilla_crew_party_flags.w>.5&&radius<=1.0){
        int reason=int(scintilla_crew_party_state.y+.5);
        vec3 tint=scintilla_crew_party_state.x>4.5?vec3(.28):reason==0?vec3(.42,.14,.65):vec3(.18,.13,.24);
        if(radius>.87)tint=reason==7?vec3(.95,.25,.2):vec3(.77,.55,1.0);
        // Cost 15 is visible before pressing; quorum and expiry are separate.
        float glyph=scintillaCostDigit((q-vec2(-.16,.37))/vec2(.24,.22),1);
        glyph=max(glyph,scintillaCostDigit((q-vec2(.16,.37))/vec2(.24,.22),5));
        int votes=int(clamp(scintilla_crew_party_state.w,0.0,2.0)),needed=int(clamp(scintilla_crew_party_flags.x,1.0,2.0));
        glyph=max(glyph,scintillaCostDigit((q-vec2(-.28,-.16))/vec2(.23,.23),votes));
        glyph=max(glyph,scintillaCostDigit((q-vec2(.28,-.16))/vec2(.23,.23),needed));
        glyph=max(glyph,1.0-smoothstep(.025,.055,scintillaSegmentDistance(q,vec2(-.08,.02),vec2(.08,-.33))));
        if(scintilla_crew_party_state.x>2.5&&scintilla_crew_party_state.x<4.5){
            float remaining=clamp(scintilla_crew_party_state.z/3.0,0.0,1.0);
            float turn=mod(atan(q.y,q.x)+6.28318530718,6.28318530718)/6.28318530718;
            if(radius>.72&&radius<.85)tint=turn<remaining?vec3(.95,.75,.2):vec3(.1);
        }
        if(scintilla_crew_party_flags.z>.5){
            glyph=max(glyph,1.0-smoothstep(.025,.055,scintillaSegmentDistance(q,vec2(-.5,-.6),vec2(.5,.6))));
        }
        glyph=max(glyph,scintillaCrewText(p,vec2(.22,.811),vec2(.034,.0085),77.0));
        color=mix(tint,vec3(1),glyph);
    }
    return color;
}
vec3 scintillaMissionSlotTint(vec2 q,vec4 state)
{
    int cargo=int(floor(state.x+.5));
    vec3 ink=cargo==1?vec3(.36,1.0,.70):cargo==2?vec3(1.0,.82,.32):vec3(.52,.60,.68);
    vec3 tint=cargo==1?vec3(.035,.16,.12):cargo==2?vec3(.16,.11,.035):vec3(.035,.045,.055);
    float glyph=1.0-smoothstep(.035,.065,abs(length(q)-.84));
    if(cargo==0){
        float dash=scintillaSegmentDistance(q,vec2(-.22,0),vec2(.22,0));
        glyph=max(glyph,1.0-smoothstep(.025,.055,dash));
    }else if(cargo==1){
        float check=min(scintillaSegmentDistance(q,vec2(-.40,-.05),vec2(-.12,.23)),
                        scintillaSegmentDistance(q,vec2(-.12,.23),vec2(.40,-.30)));
        glyph=max(glyph,1.0-smoothstep(.035,.065,check));
    }else if(cargo==2){
        vec2 clock=q-vec2(-.30,-.23);
        float d=abs(length(clock)-.20);
        d=min(d,min(scintillaSegmentDistance(clock,vec2(0),vec2(0,-.12)),
                    scintillaSegmentDistance(clock,vec2(0),vec2(.10,.04))));
        glyph=max(glyph,1.0-smoothstep(.018,.040,d));
        if(state.y>=0.0){
            int seconds=int(clamp(ceil(state.y),0.0,9.0));
            glyph=max(glyph,scintillaCostDigit((q-vec2(.24,.18))/vec2(.30,.33),seconds));
        }else{
            vec2 pause=q-vec2(.24,.18);
            float d=min(scintillaSegmentDistance(pause,vec2(-.09,-.18),vec2(-.09,.18)),
                        scintillaSegmentDistance(pause,vec2(.09,-.18),vec2(.09,.18)));
            glyph=max(glyph,1.0-smoothstep(.025,.045,d));
        }
    }else{
        // Invalid display data cannot be mistaken for secured cargo.
        float cross=min(scintillaSegmentDistance(q,vec2(-.25,-.25),vec2(.25,.25)),
                        scintillaSegmentDistance(q,vec2(-.25,.25),vec2(.25,-.25)));
        glyph=max(glyph,1.0-smoothstep(.025,.055,cross));ink=vec3(1.0,.4,.2);
    }
    return mix(tint,ink,glyph);
}
vec3 scintillaMissionResultHUD(vec3 color,vec2 p)
{
    int outcome=int(floor(scintilla_mission_result.x+.5));
    if(outcome<1||outcome>5||p.x<-.76||p.x>.76||p.y<-.08||p.y>.40)return color;
    color=mix(color,vec3(.015,.025,.035),.96);
    // The 2048x160 atlas row keeps its 12.8 aspect ratio in this 1.44x.1125
    // isotropic rectangle. Its own labels identify position and cargo below.
    vec2 textUV=(p-vec2(-.72,.03-.05625))/vec2(1.44,.1125);
    if(scintilla_mission_result.w>.5&&all(greaterThanEqual(textUV,vec2(0)))&&all(lessThanEqual(textUV,vec2(1)))){
        float row=float(outcome-1)+textUV.y;
        vec2 at=vec2(textUV.x,scintillaCrewResultReady()?(1024.0+row*160.0)/2048.0:row/5.0);
        vec4 text=texture(scintilla_mission_result_text,at);
        color=mix(color,vec3(.96,.98,1.0),text.a);
    }
    int rank=int(clamp(floor(scintilla_mission_result.y+.5),0.0,99.0));
    int secured=int(clamp(floor(scintilla_mission_result.z+.5),0.0,3.0));
    bool crewResult=scintillaCrewResultReady();float metricY=crewResult?.115:.24;
    vec2 metricSize=crewResult?vec2(.028,.023):vec2(.055,.065);
    float glyph=0.0;
    if(rank==0){
        float dash=scintillaSegmentDistance(p,vec2(-.34,metricY),vec2(-.26,metricY));
        glyph=1.0-smoothstep(.003,.006,dash);
    }else if(rank<10){
        glyph=scintillaCostDigit((p-vec2(-.30,metricY))/metricSize,rank);
    }else{
        glyph=max(scintillaCostDigit((p-vec2(-.335,metricY))/metricSize,rank/10),
                  scintillaCostDigit((p-vec2(-.265,metricY))/metricSize,rank%10));
    }
    glyph=max(glyph,scintillaCostDigit((p-vec2(.22,metricY))/metricSize,secured));
    glyph=max(glyph,scintillaCostDigit((p-vec2(.38,metricY))/metricSize,3));
    float slash=scintillaSegmentDistance(p,vec2(.315,metricY-(crewResult?.018:.055)),vec2(.285,metricY+(crewResult?.018:.055)));
    glyph=max(glyph,1.0-smoothstep(.003,.006,slash));
    return mix(color,vec3(.96,.98,1.0),glyph);
}
vec3 scintillaMissionHUD(vec3 color,vec2 uvGL,vec2 dimensions)
{
    if(scintilla_mission_hud_enabled==0&&scintilla_mission_result.x<.5)return color;
    vec2 p=scintillaMissionHUDPoint(vec2(uvGL.x,1.0-uvGL.y),dimensions);
    float size=min(dimensions.x,dimensions.y);
    for(int slot=0;slot<3;slot++)for(int row=0;row<2;row++){
        if(scintilla_mission_hud_enabled==0)continue;
        float radius=row==0?.06:.025;
        vec2 center=vec2((float(slot)-1.0)*.18,row==0?-.16:-.27);
        vec2 q=(p-center)/radius;float distance=length(q);
        // This is also the complete ReservedHUD shape. All strokes, glyphs
        // and antialiasing remain inside it; no outside shadow or glow.
        if(distance>1.0)continue;
        vec3 tint=row==0?scintillaMissionSlotTint(q,scintilla_mission_slots[slot]):
            scintillaMissionDecorationTint(vec2(q.x,-q.y),distance,scintilla_mission_slots[slot].z>.5,false);
        float aa=min(.25,4.0/(size*radius));
        color=mix(color,tint,1.0-smoothstep(1.0-aa,1.0,distance));
    }
    return scintillaMissionResultHUD(color,p);
}
// Exact revision-2 masks, independent of what ordinary non-SC4 HUD was
// actually shown. Trace records actualLayoutRevision and evaluatedLayoutRevision.
bool scintillaMeasureWorld(vec2 uv,vec2 dimensions)
{
    vec2 p=scintillaMissionHUDPoint(uv,dimensions);
    for(int i=0;i<3;i++){
        float x=(float(i)-1.0)*.18;
        if(length(p-vec2(x,-.16))<=.06||length(p-vec2(x,-.27))<=.025)return false;
    }
    float scale=min(dimensions.x,dimensions.y);
    vec4 rear=round(vec4(dimensions.x*.5-.18*scale,dimensions.y*.5-.35*scale,
                        dimensions.x*.5+.18*scale,dimensions.y*.5-.17*scale));
    vec2 pixel=uv*dimensions;
    if(all(greaterThanEqual(pixel,rear.xy))&&all(lessThan(pixel,rear.zw)))return false;
    for(int i=0;i<4;i++)if(length(p-vec2(-.6+.4*float(i),.62))<=.07)return false;
    if(length(p-vec2(0,.85))<=.06)return false;
    if(scintilla_measure_layout_revision==3&&length(p-vec2(.22,.85))<=.06)return false;
    if(abs(p.x)<=.42&&((p.y>=.42&&p.y<=.46)||(p.y>=.48&&p.y<=.52)))return false;
    for(int i=0;i<5;i++)for(int row=0;row<2;row++)
        if(all(lessThanEqual(abs(p-vec2(-.32+.16*float(i),row==0?.08:.25)),vec2(.055))))return false;
    return true;
}
float scintillaMeasureContact(vec3 direction,vec4 sphere)
{
    if(sphere.w<=0.0||dot(sphere.xyz,sphere.xyz)<=sphere.w*sphere.w)return 1e20;
    return scintillaCableSphereContact(direction,sphere.xyz,sphere.w);
}
bool scintillaMeasurementMask(vec3 direction,vec2 uvGL,vec2 dimensions,int index,int mode)
{
    if(index<0||index>=scintilla_measure_count||index>=4||use_multiview==0)return false;
    vec4 sphere=scintilla_measure_spheres[index];float contact=scintillaMeasureContact(direction,sphere);
    if(contact>=1e19)return false;
    if(mode==1)return true;
    if(scintilla_measure_styles[index].z<.5||scintilla_measure_styles[index].w<.5||
       !scintillaMeasureWorld(vec2(uvGL.x,1.0-uvGL.y),dimensions))return false;
    // Include simultaneous ghost spheres when selecting the actual front surface.
    for(int i=0;i<4;i++)if(i<scintilla_measure_count&&i!=index){
        if(scintilla_measure_styles[i].x>1.5&&scintilla_measure_styles[index].x>1.5)continue;
        if(scintilla_measure_show==0){
            // Runtime tether probes see only genuinely published occluders below.
            if(scintilla_measure_styles[index].x>1.5)continue;
            if(scintilla_measure_styles[i].x>1.5&&scintilla_measure_actual_indices[i]<0)continue;
        }
        if(scintillaMeasureContact(direction,scintilla_measure_spheres[i])<=contact)return false;
    }
    // The ordinary marker/picker selector also includes floating rival/pickup
    // primitives, which need not coincide with the model's scene-depth surface.
    for(int i=0;i<32;i++)if(i<scintilla_target_count){
        if(i==scintilla_measure_actual_indices[index])continue;
        if(scintilla_measure_styles[index].x>1.5&&scintilla_mission_targets[i].x>1.5)continue;
        if(scintillaMeasureContact(direction,scintilla_targets[i])<=contact)return false;
    }
    float ma;float scene=scintillaSceneDepth(direction,ma);float z=contact*ma;
    if(z<=scintilla_near_far.x||z>=scintilla_near_far.y)return false;
    return mode==2||scintillaDepthVisible(scene,z,scintilla_near_far.x,scintilla_near_far.y);
}
vec3 scintillaDiagnosticOverlay(vec3 color,vec3 direction,vec2 uvGL,vec2 dimensions)
{
    if(scintilla_measure_show==0)return color;
    for(int i=0;i<4;i++)if(i<scintilla_measure_count&&scintillaMeasurementMask(direction,uvGL,dimensions,i,3)){
        vec4 sphere=scintilla_measure_spheres[i];vec3 axis=normalize(sphere.xyz);
        vec3 right=normalize(cross(abs(axis.y)>.95?vec3(1,0,0):vec3(0,1,0),axis));
        vec3 up=cross(axis,right);float contact=scintillaMeasureContact(direction,sphere);
        vec3 offset=(direction*contact-sphere.xyz)/sphere.w;
        vec2 icon=vec2(dot(offset,right),dot(offset,up));
        vec4 style=vec4(scintilla_measure_styles[i].xy,0,1);
        color=scintillaMissionTint(icon,length(icon),style,-1);
    }
    return color;
}
vec3 scintillaOverlay(vec3 color,vec3 direction,vec2 uvGL,vec2 dimensions)
{
    color=scintillaDiagnosticOverlay(color,direction,uvGL,dimensions);
    if(scintilla_enabled==0){
        if(scintilla_crew_enabled!=0)return scintillaCrewHUD(scintillaMissionHUD(color,uvGL,dimensions),vec2(uvGL.x,1.0-uvGL.y),dimensions);
        return scintillaMissionHUD(color,uvGL,dimensions);
    }
    vec2 topUV=vec2(uvGL.x,1.0-uvGL.y);
    int surface=scintillaSurface(topUV,dimensions);
    if(surface==4){
        vec2 st=(topUV-scintilla_rear_rect.xy)/(scintilla_rear_rect.zw-scintilla_rear_rect.xy);
        float aspect=((scintilla_rear_rect.z-scintilla_rear_rect.x)*dimensions.x)/((scintilla_rear_rect.w-scintilla_rear_rect.y)*dimensions.y);
        direction=normalize(vec3(-(2.0*st.x-1.0)*1.19175359259421,(1.0-2.0*st.y)*1.19175359259421/aspect,-1.0));
        // Face RTTs use the same camera basis/FOV as the main view. A raw
        // cubemap lookup has different face UVs and would invert the rear.
        color=use_multiview!=0?sampleMultiview(direction).rgb:sampleEnvironment(direction).rgb;
    }
    if(surface==1&&use_multiview!=0&&scintilla_mission_hud_enabled!=0){
        float nearestDecoration=1e20;vec4 decoration=vec4(0);int decorationKind=0,decorationSlot=0;
        for(int slot=0;slot<3;slot++)for(int kind=0;kind<2;kind++){
            vec4 marker=kind==0?scintilla_mission_beacons[slot]:scintilla_mission_nucleus[slot];
            if(marker.w<=0.0)continue;
            float contact=scintillaCableSphereContact(direction,marker.xyz,marker.w);
            if(contact<nearestDecoration){nearestDecoration=contact;decoration=marker;decorationKind=kind;decorationSlot=slot;}
        }
        if(nearestDecoration<1e19){
            float ma;float scene=scintillaSceneDepth(direction,ma);
            if(scintillaDepthVisible(scene,nearestDecoration*ma,scintilla_near_far.x,scintilla_near_far.y)){
                vec3 axis=normalize(decoration.xyz);
                vec3 reference=abs(axis.y)>.98?vec3(1,0,0):vec3(0,1,0);
                vec3 right=normalize(cross(reference,axis));vec3 up=cross(axis,right);
                vec2 icon=vec2(dot(direction,right),dot(direction,up))*length(decoration.xyz)/decoration.w;
                float offset=length(cross(direction,decoration.xyz))/decoration.w;
                vec3 tint=scintillaMissionDecorationTint(icon,offset,scintilla_mission_beacon_states[decorationSlot]>.5,decorationKind==1);
                color=mix(color,tint,.86);
            }
        }
    }
    if(surface==1&&use_multiview!=0&&scintilla_mission_cable_end.w>.5){
        float contact=scintillaCableContact(direction,scintilla_mission_cable_start.xyz,
                                            scintilla_mission_cable_end.xyz,scintilla_mission_cable_start.w);
        float ma;float scene=scintillaSceneDepth(direction,ma);
        if(scintillaDepthVisible(scene,contact*ma,scintilla_near_far.x,scintilla_near_far.y))
            color=mix(color,vec3(.72,.80,.95),.85);
    }
    // Same picking policy as the historical consumer: nearest positive front
    // surface first, then strict clipping in the selected capture face's depth.
    // A clipped nearest marker never reveals another marker behind it.
    float nearest=1e20;vec4 selected=vec4(0.0);int selectedIndex=-1;
    for(int i=0;i<32;i++) {
        if(i>=scintilla_target_count)break;
        // RearViewReadonly does not display an interactive mission target.
        // Exclude it before nearest selection so it cannot hide a rear kart.
        if(surface!=1&&scintilla_mission_targets[i].x>.5)continue;
        vec4 marker=scintilla_targets[i];
        float contact=scintillaMeasureContact(direction,marker);
        if(contact>0.0 && contact<nearest) {nearest=contact;selected=marker;selectedIndex=i;}
    }
    if((surface==1||surface==4)&&nearest<1e19 && use_multiview!=0) {
        float ma;float scene=scintillaSceneDepth(direction,ma);
        float z=nearest*ma,n=scintilla_near_far.x,f=scintilla_near_far.y;
        if(scintillaDepthVisible(scene,z,n,f)) {
            // Disc and rim both visible; there is no invisible expanded kart hitbox.
            float offset=length(cross(direction,selected.xyz))/selected.w;
            vec3 tint=mix(vec3(0.12,0.45,0.55),vec3(0.1,0.95,1.0),smoothstep(0.65,0.95,offset));
            vec4 missionState=scintilla_mission_targets[selectedIndex];
            bool missionMarker=missionState.x>.5;
            if(missionMarker){
                vec3 axis=normalize(selected.xyz);
                vec3 reference=abs(axis.y)>.98?vec3(1,0,0):vec3(0,1,0);
                vec3 right=normalize(cross(reference,axis));vec3 up=cross(axis,right);
                vec2 icon=vec2(dot(direction,right),dot(direction,up))*length(selected.xyz)/selected.w;
                int owner=int(floor(scintilla_mission_owners[selectedIndex]+.5));
                tint=scintillaMissionTint(icon,offset,missionState,owner);
            }else if(scintilla_combat_enabled!=0 && selectedIndex>=0) {
                vec4 state=scintilla_combat_targets[selectedIndex];
                vec4 actors=scintilla_combat_actors[selectedIndex];
                if(state.z>0.5) {
                    vec3 axis=normalize(selected.xyz);
                    vec3 reference=abs(axis.y)>0.98?vec3(1,0,0):vec3(0,1,0);
                    vec3 right=normalize(cross(reference,axis));vec3 up=cross(axis,right);
                    float angle=mod(atan(dot(direction,up),dot(direction,right))+6.28318530718,6.28318530718);
                    float fraction=angle/6.28318530718;
                    int phase=int(state.x+0.5);
                    if(actors.z<0.5)tint=vec3(0.55,0.55,0.55);
                    if(phase==1) {
                        if(offset>0.65)tint=fraction<state.w?scintillaIdentityColor(int(actors.x+0.5)):vec3(0.22,0.32,0.38);
                    } else if(phase==2) {
                        tint=vec3(0.2,0.85,0.45);
                        float segment=floor(fraction*state.z);
                        if(offset>0.65) {
                            int who=-1,number=0,mask=int(actors.y+0.5);
                            for(int person=0;person<5;person++)if((mask&(1<<person))!=0){
                                if(number==int(segment))who=person;number++;
                            }
                            tint=who>=0?scintillaIdentityColor(who):vec3(0.1,0.4,0.15);
                            if(fract(fraction*state.z)<0.06)tint=vec3(0.03);
                        }
                        if(offset>0.93&&actors.x>=0.0)tint=scintillaIdentityColor(int(actors.x+0.5));
                    } else if(phase==3||phase==4) {
                        tint=vec3(1.0,0.95,0.3);
                    } else if(phase==5) {
                        // Static diagonal stripes distinguish recovery even without colour.
                        tint=mod(floor((uvGL.x+uvGL.y)*dimensions.y/5.0),2.0)<1.0?vec3(0.65):vec3(0.2);
                    } else if(state.z>4.5) {
                        float segment=floor(fraction*5.0);
                        if(offset>0.65){
                            tint=segment<state.y?vec3(1.0,0.85,0.2):vec3(0.2,0.45,0.65);
                            if(fract(fraction*5.0)<0.06)tint=vec3(0.03);
                        }
                    }
                    // The breach intent is displayed before pressing tool; rays
                    // into this same visible disc remain the only hit volume.
                    if(actors.z<0.5&&phase<3)tint=mix(tint,vec3(0.4),0.85);
                    if(phase==0&&state.z<4.5&&offset<0.35) {
                        vec2 icon=vec2(dot(direction,right),dot(direction,up))*length(selected.xyz)/selected.w;
                        bool openRing=abs(length(icon)-0.24)<0.04&&icon.x<0.13;
                        bool arrow=abs(icon.y)<0.035&&icon.x>-0.08&&icon.x<0.28;
                        bool tip=abs(abs(icon.y)-(0.28-icon.x))<0.035&&icon.x>0.16&&icon.x<0.28;
                        if(openRing||arrow||tip)tint=actors.z>0.5?vec3(1.0,0.9,0.25):vec3(0.4);
                    }
                }
            }
            if(scintilla_crew_enabled!=0&&offset>.48&&offset<.60){
                int mask=int(scintilla_crew_target_masks[selectedIndex/4][selectedIndex%4]+.5),count=0;
                for(int person=0;person<5;person++)if((mask&(1<<person))!=0)count++;
                if(count>0){
                    vec3 axis=normalize(selected.xyz),reference=abs(axis.y)>.98?vec3(1,0,0):vec3(0,1,0);
                    vec3 right=normalize(cross(reference,axis)),up=cross(axis,right);
                    float turn=mod(atan(dot(direction,up),dot(direction,right))+6.28318530718,6.28318530718)/6.28318530718;
                    int selectedHint=int(floor(turn*float(count))),ordinal=0;
                    for(int person=0;person<5;person++)if((mask&(1<<person))!=0){if(ordinal==selectedHint)tint=scintillaIdentityColor(person);ordinal++;}
                }
            }
            color=mix(color,tint,missionMarker?0.88:scintilla_combat_enabled!=0?0.78:0.65);
        }
    }
    if(scintilla_defense_enabled!=0&&(surface==1||surface==4)){
        for(int object=0;object<2;object++){
            vec4 marker=object==0?scintilla_defense_source:scintilla_defense_pulse;
            if(marker.w<=0.0)continue;
            float along=dot(marker.xyz,direction),disc=along*along-dot(marker.xyz,marker.xyz)+marker.w*marker.w;
            if(disc<0.0)continue;float contact=along-sqrt(disc),ma;
            float scene=scintillaSceneDepth(direction,ma);
            if(contact>0.0&&scintillaDepthVisible(scene,contact*ma,scintilla_near_far.x,scintilla_near_far.y)){
                float off=length(cross(direction,marker.xyz))/marker.w;
                float ring=object==1?1.0:step(0.6,off);
                color=mix(color,object==1?vec3(1,0.8,0.1):vec3(1,0.2,0.05),ring);
            }
        }
    }
    if(scintilla_defense_enabled!=0){
        for(int i=0;i<2;i++){
            vec4 rect=scintilla_defense_bars[i];
            if(all(greaterThanEqual(topUV,rect.xy))&&all(lessThanEqual(topUV,rect.zw))){
                float x=(topUV.x-rect.x)/(rect.z-rect.x);
                float amount=i==0?scintilla_defense_resources.x:scintilla_defense_resources.z;
                color=vec3(0.04);if(x<amount/100.0)color=i==0?vec3(0.15,0.9,0.6):vec3(1,0.5,0.1);
                if(i==0&&abs(x-scintilla_defense_resources.y/100.0)<0.008)color=vec3(1);
                if(i==0&&x<scintilla_defense_resources.y/100.0&&mod(floor(topUV.x*dimensions.x/5.0),2.0)<1.0)color*=.55;
                // The upper half previews the next boost debit. Dashes remain
                // distinct from the reserved area's hatching without colour.
                float afterBoost=max(0.0,scintilla_defense_resources.x-scintilla_boost_cost)/100.0;
                if(i==0&&topUV.y<(rect.y+rect.w)*.5&&x>=afterBoost&&x<amount/100.0&&mod(floor(topUV.x*dimensions.x/4.0),2.0)<1.0)
                    color=scintilla_defense_resources.x-scintilla_boost_cost>=scintilla_defense_resources.y?vec3(1,.9,.25):vec3(1,.4,.15);
            }
        }
        for(int i=0;i<5;i++){
            vec4 n=scintilla_defense_nodes[i];vec2 p=(topUV-n.xy)*dimensions;float r=length(p)/n.z;
            if(r>1.0)continue;
            bool threatened=i<4&&int(n.w)==int(scintilla_defense_state.y)&&scintilla_defense_state.x>=1.0&&scintilla_defense_state.x<=5.0;
            color=vec3(0.025);vec3 tint=threatened?vec3(1,.3,.08):i==4?vec3(.8,.8,.15):vec3(.3,.9,1);
            if(i==4&&scintilla_defense_resources.w!=8.0)tint=vec3(.5);
            // Availability is visible before pressing: reserve stripe, cooldown
            // clock, threat cross or ineligible diamond, independently of colour.
            if(i==4&&r<.82){
                int bs=int(scintilla_defense_resources.w);
                vec2 q=p/n.z;
                if(bs==4&&mod(floor((q.x+q.y)*8.0),2.0)<1.0)color=vec3(.25);
                if(bs==9&&(abs(r-.58)<.05||scintillaSegmentDistance(q,vec2(0),vec2(0,-.35))<.04))color=vec3(.8);
                if(bs==10&&abs(abs(q.x)-abs(q.y))<.06)color=vec3(1,.3,.08);
                if(bs==11&&abs(abs(q.x)+abs(q.y)-.65)<.06)color=vec3(.8);
            }
            for(int hand=0;hand<5;hand++)if(i<4&&int(scintilla_defense_tools[hand].w)==int(n.w)){
                if(scintilla_defense_tools[hand].x==3.0)tint=scintillaIdentityColor(hand);
                if(scintilla_defense_tools[hand].x==7.0&&abs(abs(p.x)-abs(p.y))<n.z*.1)color=vec3(1,.7,.1);
            }
            if(i==4&&r>.65&&r<.78&&atan(p.y,p.x)/6.28318530718+.5<scintilla_boost_progress)color=tint;
            if(i<4&&int(n.w)==int(scintilla_defense_state.y)&&scintilla_defense_state.x==6.0&&scintilla_defense_state.w>0.0){
                int outcome=int(scintilla_defense_state.w);
                if(outcome==1&&abs(r-.65)<.08)color=vec3(.1,1,.35);
                if(outcome==2&&abs(abs(p.x)-abs(p.y))<n.z*.12)color=vec3(1,.1,.1);
                if(outcome>=3&&abs(p.y)<n.z*.08&&abs(p.x)<n.z*.6)color=vec3(.9);
            }
            if(r>.84)color=tint;
            if(threatened&&r>.67&&r<.78&&atan(p.y,p.x)/6.28318530718+.5<scintilla_defense_state.z)color=tint;
            vec2 q=p/n.z;
            if(i<4){
                // Directional arrow and 1..4 dots distinguish every sector without colour.
                float angle=n.w*1.57079632679;vec2 a=vec2(sin(angle),-cos(angle));
                float shaft=scintillaSegmentDistance(q,-a*.35,a*.35);
                float tip=min(scintillaSegmentDistance(q,a*.35,a*.05+vec2(-a.y,a.x)*.22),scintillaSegmentDistance(q,a*.35,a*.05-vec2(-a.y,a.x)*.22));
                if(min(shaft,tip)<.05)color=tint;
                for(int d=0;d<4;d++)if(d<=int(n.w)&&length(q-vec2((float(d)-n.w*.5)*.17,.53))<.055)color=tint;
            }else{
                // Lightning above the cost, with both entirely inside the disc.
                vec2 lightning=(q-vec2(0,-.30))/.5;
                if(min(scintillaSegmentDistance(lightning,vec2(.15,-.5),vec2(-.2,.05)),scintillaSegmentDistance(lightning,vec2(-.2,.05),vec2(.2,-.05)))<.07||scintillaSegmentDistance(lightning,vec2(.2,-.05),vec2(-.15,.5))<.07)color=tint;
                float cost=scintillaBoostCostGlyph(q);
                color=mix(color,vec3(1.0),cost);
            }
            if(threatened&&scintilla_defense_state.x==5.0&&abs(abs(q.x)-abs(q.y))<.08)color=vec3(1,.1,.1);
        }
    }
    // Crew nodes/badges are surfaces under the pointers. Opaque Party pixels
    // must not erase aiming, source identity or hold feedback drawn afterward.
    if(scintilla_crew_enabled!=0){
        // Legacy status panels share the crew badge anchors. SC5 translates
        // those statuses into its own text; keep their old panels underneath.
        if(scintilla_mission_result.x<.5)color=scintillaFeedback(color,uvGL,dimensions);
        color=scintillaToolFeedback(color,uvGL,dimensions);
        color=scintillaCrewHUD(scintillaMissionHUD(color,uvGL,dimensions),vec2(uvGL.x,1.0-uvGL.y),dimensions);
        if(scintilla_mission_result.x>.5)return color;
    }
    vec3 palette[5]=vec3[5](vec3(0.0,1.0,1.0),vec3(1.0,0.65,0.0),vec3(1.0,0.3,0.9),vec3(0.4,1.0,0.2),vec3(0.7,0.65,1.0));
    for(int i=0;i<5;i++) {
        if(scintilla_mission_result.x>.5)break;
        vec4 r=scintilla_reticles[i];if(r.z<0.5)continue;
        vec2 p=(uvGL-vec2(r.x,1.0-r.y))*dimensions;
        float crossMark=max(step(abs(p.x),1.2)*step(abs(p.y),10.0),step(abs(p.y),1.2)*step(abs(p.x),10.0));
        // Number of dots encodes source slot independently of colour.
        float dots=0.0;
        for(int j=0;j<5;j++)if(j<=i)dots=max(dots,1.0-smoothstep(1.0,2.0,length(p-vec2((float(j)-float(i)*0.5)*5.0,15.0))));
        float tool=r.w>1.5&&scintilla_tools[i].x<0.5&&scintilla_mission_tools[i].x<0.5?1.0-smoothstep(0.8,1.8,abs(length(p)-13.0)):0.0;
        if(scintilla_defense_enabled!=0){
            int intent=int(scintilla_defense_tools[i].z);float icon=0.0;
            if(intent==4||intent==5)icon=min(scintillaSegmentDistance(p,vec2(-6,-6),vec2(6,6)),scintillaSegmentDistance(p,vec2(-6,6),vec2(6,-6)));
            else if(intent==2)icon=abs(max(abs(p.x)-7.0,abs(p.y)-6.0));
            else if(intent==3)icon=scintillaSegmentDistance(p,vec2(3,6),vec2(-3,-6));
            else icon=1000.0;
            crossMark=max(crossMark,1.0-smoothstep(.8,1.8,icon));
        }
        color=mix(color,palette[i],max(max(crossMark,dots),tool));
    }
    if(scintilla_crew_enabled!=0)return color;
    if(scintilla_mission_result.x<.5)color=scintillaFeedback(color,uvGL,dimensions);
    color=scintillaToolFeedback(color,uvGL,dimensions);
    return scintillaLatencyFlash(scintillaMissionHUD(color,uvGL,dimensions),uvGL,dimensions);
}
// Latency probe: a saturated disc on top of everything, filmed to time the
// optical path from a trigger press. Inactive outside that diagnostic profile.
vec3 scintillaLatencyFlash(vec3 color,vec2 uvGL,vec2 dimensions)
{
    for(int i=0;i<5;i++){
        vec4 f=scintilla_latency_flash[i];if(f.z<0.5)continue;
        if(length((uvGL-vec2(f.x,1.0-f.y))*dimensions)<=f.w)return vec3(1.0);
    }
    return color;
}
float scintillaAlpha(vec2 pixel,vec2 dimensions)
{
    if(scintilla_enabled==0 || pixel.x>=256.0 || (pixel.y>=1.0 && pixel.y<dimensions.y-1.0))return 1.0;
    uint bit=uint(pixel.x);
    return float((scintilla_stamp[bit/32u]>>(bit%32u))&1u);
}
