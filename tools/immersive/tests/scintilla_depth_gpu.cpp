// SPDX-License-Identifier: GPL-3.0-or-later
// macOS OpenGL 4.1 regression: compile/evaluate the production depth function.
// Uses an offscreen local GPU context; no app, STK assets or spatial input.
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <string>

static void require(bool condition, const std::string& message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
static GLuint compile(GLenum type, const std::string& source)
{
    GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, NULL); glCompileShader(shader);
    GLint okay = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (!okay) { char log[8192] = {}; glGetShaderInfoLog(shader, sizeof(log), NULL, log); require(false, log); }
    return shader;
}
static std::string functionSource(const std::string& source, const std::string& signature)
{
    const size_t start=source.find(signature);
    require(start!=std::string::npos,"production function missing: "+signature);
    const size_t body=source.find('{',start);require(body!=std::string::npos,"function body missing");
    size_t end=body;unsigned nesting=0;
    do{if(source[end]=='{')++nesting;if(source[end]=='}')--nesting;++end;}
    while(nesting&&end<source.size());
    require(!nesting,"unterminated production function: "+signature);
    return source.substr(start,end-start);
}
int main(int argc, char** argv)
{
    require(argc == 3, "usage: scintilla_depth_gpu production-shader cases.tsv");
    std::ifstream shader_file(argv[1]); require(shader_file.good(), "production shader missing");
    const std::string source((std::istreambuf_iterator<char>(shader_file)), std::istreambuf_iterator<char>());
    const std::string depthFunction=functionSource(source,"bool scintillaDepthVisible(");
    CGLPixelFormatAttribute attributes[] = { kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute)kCGLOGLPVersion_GL4_Core, kCGLPFAAccelerated,
        (CGLPixelFormatAttribute)0 };
    CGLPixelFormatObj format = NULL; GLint count = 0;
    require(CGLChoosePixelFormat(attributes, &format, &count) == kCGLNoError && format, "OpenGL 4.1 pixel format unavailable");
    CGLContextObj context = NULL;
    require(CGLCreateContext(format, NULL, &context) == kCGLNoError && context, "OpenGL context unavailable");
    CGLDestroyPixelFormat(format);
    require(CGLSetCurrentContext(context) == kCGLNoError, "cannot activate context");
    std::cout << "GL_VERSION=" << glGetString(GL_VERSION) << " GL_RENDERER=" << glGetString(GL_RENDERER) << '\n';
    const GLuint vertex = compile(GL_VERTEX_SHADER,
        "#version 410 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}");
    const GLuint fragment = compile(GL_FRAGMENT_SHADER,
        "#version 410 core\nuniform vec4 values;out vec4 result;\n" + depthFunction +
        "\nvoid main(){result=vec4(scintillaDepthVisible(values.x,values.y,values.z,values.w)?1.0:0.0,0,0,1);}");
    GLuint program = glCreateProgram(); glAttachShader(program, vertex); glAttachShader(program, fragment); glLinkProgram(program);
    GLint linked = 0; glGetProgramiv(program, GL_LINK_STATUS, &linked); require(linked, "depth program link failed");
    GLuint texture = 0, framebuffer = 0, vao = 0;
    glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "test framebuffer incomplete");
    glGenVertexArrays(1, &vao); glBindVertexArray(vao); glViewport(0, 0, 1, 1); glUseProgram(program);
    const GLint location = glGetUniformLocation(program, "values"); require(location >= 0, "uniform missing");
    std::ifstream cases(argv[2]); require(cases.good(), "cases file missing");
    std::string row; unsigned passed = 0;float auditWallDepth=-1,auditClearDepth=-1;
    while (std::getline(cases, row))
    {
        if (row.empty() || row[0] == '#') continue;
        std::istringstream fields(row); std::string name, scene, marker, near_plane, far_plane; int expected = -1;
        require(bool(fields >> name >> scene >> marker >> near_plane >> far_plane >> expected), "invalid fixture row");
        require(expected == 0 || expected == 1, "invalid expectation");
        if(name=="audit-wall")auditWallDepth=std::strtof(scene.c_str(),NULL);
        if(name=="audit-clear")auditClearDepth=std::strtof(scene.c_str(),NULL);
        glUniform4f(location, std::strtof(scene.c_str(), NULL), std::strtof(marker.c_str(), NULL),
            std::strtof(near_plane.c_str(), NULL), std::strtof(far_plane.c_str(), NULL));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        unsigned char pixel[4] = {}; glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        require(glGetError() == GL_NO_ERROR, name + " OpenGL error");
        require(pixel[0] == (expected ? 255 : 0) && pixel[3] == 255, name + " unexpected visibility");
        ++passed;
    }
    require(passed == 64, "fixture coverage unexpectedly empty/partial");
    require(auditWallDepth>=0&&auditClearDepth>=0,"mission occlusion fixtures missing");
    // Use the production tint and depth helpers together. These are rendering
    // regressions, not an optical/venue qualification or an input proof.
    const GLuint missionFragment=compile(GL_FRAGMENT_SHADER,
        "#version 410 core\nuniform vec4 values;uniform vec4 missionState;uniform vec4 samplePoint;"
        "uniform int cableMode;uniform vec4 cableStart;uniform vec4 cableEnd;uniform vec2 outputDimensions;"
        "uniform int scintilla_mission_hud_enabled;uniform vec4 scintilla_mission_slots[3];"
        "uniform vec4 scintilla_mission_result;uniform sampler2D scintilla_mission_result_text;out vec4 result;\n"+
        functionSource(source,"float scintillaSegmentDistance(")+functionSource(source,"vec3 scintillaIdentityColor(")+depthFunction+
        functionSource(source,"float scintillaCableSphereContact(")+functionSource(source,"float scintillaCableContact(")+
        functionSource(source,"vec3 scintillaMissionTint(")+functionSource(source,"vec3 scintillaMissionDecorationTint(")+
        functionSource(source,"float scintillaCostDigit(")+functionSource(source,"vec2 scintillaMissionHUDPoint(")+
        functionSource(source,"bool scintillaMissionHUDReserved(")+functionSource(source,"vec3 scintillaMissionSlotTint(")+
        functionSource(source,"vec3 scintillaMissionResultHUD(")+functionSource(source,"vec3 scintillaMissionHUD(")+
        "\nvoid main(){vec3 tint=vec3(0);if(cableMode==2){tint=scintillaMissionHUD(tint,samplePoint.xy,outputDimensions);"
        "result=vec4(tint,scintillaMissionHUDReserved(vec2(samplePoint.x,1.0-samplePoint.y),outputDimensions)?1.0:0.0);return;}"
        "if(cableMode==1){float contact=scintillaCableContact(vec3(0,0,1),cableStart.xyz,cableEnd.xyz,cableStart.w);"
        "if(scintillaDepthVisible(values.x,contact,values.z,values.w))tint=vec3(1);}"
        "else if(scintillaDepthVisible(values.x,values.y,values.z,values.w)){if(cableMode==3)"
        "tint=scintillaMissionDecorationTint(samplePoint.xy,samplePoint.z,missionState.z>.5,missionState.w>.5);"
        "else tint=scintillaMissionTint(samplePoint.xy,samplePoint.z,missionState,0);}result=vec4(tint,1);}");
    const GLuint missionProgram=glCreateProgram();glAttachShader(missionProgram,vertex);
    glAttachShader(missionProgram,missionFragment);glLinkProgram(missionProgram);
    glGetProgramiv(missionProgram,GL_LINK_STATUS,&linked);require(linked,"mission style program link failed");
    const GLint missionValues=glGetUniformLocation(missionProgram,"values");
    const GLint missionState=glGetUniformLocation(missionProgram,"missionState");
    const GLint samplePoint=glGetUniformLocation(missionProgram,"samplePoint");
    const GLint cableMode=glGetUniformLocation(missionProgram,"cableMode");
    const GLint cableStart=glGetUniformLocation(missionProgram,"cableStart");
    const GLint cableEnd=glGetUniformLocation(missionProgram,"cableEnd");
    const GLint outputDimensions=glGetUniformLocation(missionProgram,"outputDimensions");
    const GLint missionHUD=glGetUniformLocation(missionProgram,"scintilla_mission_hud_enabled");
    const GLint missionSlots=glGetUniformLocation(missionProgram,"scintilla_mission_slots[0]");
    const GLint missionResult=glGetUniformLocation(missionProgram,"scintilla_mission_result");
    const GLint missionResultText=glGetUniformLocation(missionProgram,"scintilla_mission_result_text");
    require(missionValues>=0&&missionState>=0&&samplePoint>=0,"mission style uniform missing");
    require(cableMode>=0&&cableStart>=0&&cableEnd>=0,"mission cable uniform missing");
    require(outputDimensions>=0&&missionHUD>=0&&missionSlots>=0&&missionResult>=0&&missionResultText>=0,"mission HUD uniform missing");
    glUseProgram(missionProgram);
    // Synthetic alpha rows exercise atlas addressing, not the production font
    // asset or its visual quality. Never sample from the attached output RTT.
    GLuint resultAtlas=0;glGenTextures(1,&resultAtlas);glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,resultAtlas);const unsigned char atlasRows[]={
        255,255,255,50,255,255,255,90,255,255,255,130,255,255,255,170,255,255,255,210};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,5,0,GL_RGBA,GL_UNSIGNED_BYTE,atlasRows);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glUniform1i(missionResultText,1);glActiveTexture(GL_TEXTURE0);
    const auto sample=[&](float scene,float z,int phase,float progress,float eligible,float offset,float x,float y)->std::array<unsigned char,4>{
        glUniform4f(missionValues,scene,z,.2f,1000.f);
        glUniform4f(missionState,phase>=4?2.f:1.f,float(phase),progress,eligible);
        glUniform4f(samplePoint,x,y,offset,0);
        glDrawArrays(GL_TRIANGLES,0,3);std::array<unsigned char,4> pixel={{0,0,0,0}};
        glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        require(glGetError()==GL_NO_ERROR,"mission style OpenGL error");
        require(pixel[3]==255,"mission style lost complete disc alpha");return pixel;
    };
    unsigned missionPassed=0;
    for(int phase:{0,1,4,5})for(float eligible:{0.f,1.f})for(float offset:{0.f,.25f,.6f,.9f,.99f}){
        const auto visible=sample(auditClearDepth,100.f,phase,.5f,eligible,offset,offset,0);
        require(visible[0]+visible[1]+visible[2]>30,"shell/core style created an invisible selectable region");
        const auto hidden=sample(auditWallDepth,100.f,phase,.5f,eligible,offset,offset,0);
        require(hidden[0]==0&&hidden[1]==0&&hidden[2]==0,"mission style appeared through wall");
        missionPassed+=2;
    }
    for(int phase:{0,1,4,5}){
        const auto clipped=sample(1.f,.2f,phase,0,1,0,0,0);
        require(clipped[0]==0&&clipped[1]==0&&clipped[2]==0,"mission style bypassed strict near clip");
        ++missionPassed;
    }
    require(sample(auditClearDepth,100.f,0,0,1,0,0,0)!=sample(auditClearDepth,100.f,1,0,1,0,0,0),
            "shell and open-core phases became visually identical");++missionPassed;
    require(sample(auditClearDepth,100.f,1,0,1,.85f,0,.85f)!=sample(auditClearDepth,100.f,1,.75f,1,.85f,0,.85f),
            "mission ownership progress ring missing");++missionPassed;
    require(sample(auditClearDepth,100.f,4,0,1,.5f,.5f,0)!=sample(auditClearDepth,100.f,5,0,1,.5f,.5f,0),
            "awaiting-presentation and recoverable tether styles identical");++missionPassed;
    require(missionPassed==87,"mission style fixture coverage partial");
    struct CableCase{const char* name;float ax,ay,az,bx,by,bz,radius,scene;bool visible;};
    const CableCase cableCases[]={
        {"transverse-clear",-1,0,100,1,0,100,.025f,auditClearDepth,true},
        {"transverse-wall",-1,0,100,1,0,100,.025f,auditWallDepth,false},
        {"parallel-endcap-clear",0,0,100,0,0,102,.025f,auditClearDepth,true},
        {"parallel-endcap-wall",0,0,100,0,0,102,.025f,auditWallDepth,false},
        {"near-parallel",-.001f,0,100,.001f,0,102,.025f,auditClearDepth,true},
        {"side-miss",-1,.03f,100,1,.03f,100,.025f,auditClearDepth,false},
        {"parallel-miss",.03f,0,100,.03f,0,102,.025f,auditClearDepth,false},
        {"zero-length-sphere",0,0,100,0,0,100,.025f,auditClearDepth,true},
        {"eye-inside",0,0,-1,0,0,1,.025f,1,false},
        {"behind-eye",0,0,-100,0,0,-102,.025f,1,false},
        {"strict-near-clip",-1,0,.2f,1,0,.2f,.025f,1,false},
        {"strict-far-clip",-1,0,1000.1f,1,0,1000.1f,.025f,1,false},
        {"invalid-radius",-1,0,100,1,0,100,-.025f,1,false}
    };
    glUniform1i(cableMode,1);unsigned cablePassed=0;
    for(const CableCase& c:cableCases){
        glUniform4f(missionValues,c.scene,0,.2f,1000.f);
        glUniform4f(cableStart,c.ax,c.ay,c.az,c.radius);glUniform4f(cableEnd,c.bx,c.by,c.bz,1);
        glDrawArrays(GL_TRIANGLES,0,3);unsigned char pixel[4]={};
        glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        require(glGetError()==GL_NO_ERROR,std::string(c.name)+" cable OpenGL error");
        require(pixel[0]==(c.visible?255:0)&&pixel[3]==255,std::string(c.name)+" cable geometry/depth mismatch");
        ++cablePassed;
    }
    require(cablePassed==13,"cable fixture coverage partial");
    const auto readPixel=[&]()->std::array<unsigned char,4>{
        glDrawArrays(GL_TRIANGLES,0,3);std::array<unsigned char,4> pixel={{0,0,0,0}};
        glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data());
        require(glGetError()==GL_NO_ERROR,"mission decoration/HUD OpenGL error");return pixel;
    };
    glUniform1i(cableMode,3);unsigned decorationPassed=0;
    const auto decorationSample=[&](bool lit,bool nucleus,bool clear)->std::array<unsigned char,4>{
        glUniform4f(missionValues,clear?auditClearDepth:auditWallDepth,100,.2f,1000.f);
        glUniform4f(missionState,0,0,lit?1.f:0.f,nucleus?1.f:0.f);glUniform4f(samplePoint,.6f,0,.6f,0);
        return readPixel();
    };
    for(bool lit:{false,true})for(bool nucleus:{false,true}){
        const auto shown=decorationSample(lit,nucleus,true),hidden=decorationSample(lit,nucleus,false);
        require(shown[0]+shown[1]+shown[2]>30,"decorative beacon/nucleus invisible on clear depth");
        require(hidden[0]==0&&hidden[1]==0&&hidden[2]==0,"decoration appeared through wall");decorationPassed+=2;
    }
    require(decorationSample(false,false,true)!=decorationSample(true,false,true),"unlit/lit beacon styles identical");++decorationPassed;
    require(decorationSample(true,false,true)!=decorationSample(true,true,true),"beacon/nucleus styles identical");++decorationPassed;
    require(decorationPassed==10,"decoration fixture coverage partial");
    glUniform1i(cableMode,2);glUniform1i(missionHUD,1);
    const float slotStates[]={0,0,0,0,1,0,1,1,2,2.3f,0,1};glUniform4fv(missionSlots,3,slotStates);
    const auto hudSample=[&](float width,float height,float x,float y)->std::array<unsigned char,4>{
        const float size=width<height?width:height;
        glUniform2f(outputDimensions,width,height);
        glUniform4f(samplePoint,.5f+x*size/(2*width),.5f-y*size/(2*height),0,0);return readPixel();
    };
    const float outputSizes[][2]={{256,256},{1536,1536},{3072,1536},{1024,256}};
    unsigned hudPassed=0;
    for(const auto& size:outputSizes)for(int slot=0;slot<3;slot++)for(int row=0;row<2;row++){
        const float x=(slot-1)*.18f,y=row==0?-.16f:-.27f,radius=row==0?.06f:.025f;
        const auto center=hudSample(size[0],size[1],x,y);
        require(center[3]==255&&center[0]+center[1]+center[2]>15,"cargo/beacon center missing from exact reserved circle");++hudPassed;
        const auto inside=hudSample(size[0],size[1],x+radius*(1.f-.0001f),y);
        require(inside[3]==255,"cargo/beacon inner boundary not reserved");++hudPassed;
        const auto outside=hudSample(size[0],size[1],x+radius*(1.f+.0001f),y);
        require(outside[0]==0&&outside[1]==0&&outside[2]==0&&outside[3]==0,"HUD stroke/glow escaped reserved circle");++hudPassed;
    }
    const auto freeWorld=hudSample(1536,1536,0,0);
    require(freeWorld[0]==0&&freeWorld[1]==0&&freeWorld[2]==0&&freeWorld[3]==0,"HUD expanded into free world");++hudPassed;
    glUniform1i(missionHUD,0);const auto disabled=hudSample(1536,1536,0,-.16f);
    require(disabled[0]==0&&disabled[1]==0&&disabled[2]==0&&disabled[3]==0,"disabled mission HUD changed vanilla output");++hudPassed;
    glUniform1i(missionHUD,1);glUniform4f(missionResult,1,0,0,0);
    struct PlatePoint{float x,y;bool reserved;};const PlatePoint platePoints[]={
        {-.7599f,.2f,true},{-.7601f,.2f,false},{.7599f,.2f,true},{.7601f,.2f,false},
        {0,-.0799f,true},{0,-.0801f,false},{0,.3999f,true},{0,.4001f,false}};
    for(const PlatePoint& p:platePoints){
        const auto pixel=hudSample(1536,1536,p.x,p.y);require(pixel[3]==(p.reserved?255:0),"terminal plate mask bounds diverged");
        if(!p.reserved)require(pixel[0]==0&&pixel[1]==0&&pixel[2]==0,"terminal plate painted outside ReservedHUD");++hudPassed;
    }
    unsigned char previous=0;
    for(int outcome=1;outcome<=5;outcome++){
        glUniform4f(missionResult,float(outcome),2,2,1);const auto text=hudSample(1536,1536,0,.03f);
        require(text[3]==255&&text[0]>previous,"terminal atlas outcome row addressing diverged");previous=text[0];++hudPassed;
    }
    glUniform4f(missionResult,5,0,1,1);const auto absentRank=hudSample(1536,1536,-.30f,.24f);
    glUniform4f(missionResult,5,1,1,1);const auto presentRank=hudSample(1536,1536,-.30f,.24f);
    require(absentRank!=presentRank,"absent official rank was rendered as a fabricated position");++hudPassed;
    const auto oneCargo=hudSample(1536,1536,.22f,.188f);glUniform4f(missionResult,5,1,3,1);
    const auto threeCargo=hudSample(1536,1536,.22f,.188f);
    require(oneCargo!=threeCargo,"terminal secured cargo count missing");++hudPassed;
    require(hudPassed==89,"HUD fixture coverage partial");
    glDeleteVertexArrays(1, &vao); glDeleteFramebuffers(1, &framebuffer); glDeleteTextures(1, &texture);
    glDeleteTextures(1,&resultAtlas);
    glDeleteProgram(program); glDeleteShader(vertex); glDeleteShader(fragment);
    glDeleteProgram(missionProgram);glDeleteShader(missionFragment);
    CGLSetCurrentContext(NULL); CGLDestroyContext(context);
    std::cout << "PASS " << passed << " actual GLSL depth cases, " <<missionPassed<< " mission style cases, "<<cablePassed
              <<" cable cases, "<<decorationPassed<<" decoration cases, "<<hudPassed<<" HUD cases\n";
    return 0;
}
