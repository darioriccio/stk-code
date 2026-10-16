// Native full-sphere projection. gl_FragCoord is bottom-left; the receiver's
// top-left convention is v_top = 1-v_gl. Syphon keeps flipped:NO as before.
#stk_include "utils/immersive_sphere_sampling.frag"
#stk_include "utils/immersive_scintilla.frag"
uniform vec2 resolution;
uniform int debug_grid;
out vec4 FragColor;
const float PI = 3.14159265358979323846;
float gridLine(float d, float w) { return 1.0-smoothstep(w, w*2.0, abs(d)); }
void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    float longitude = (uv.x - 0.5) * 2.0 * PI;
    float latitude = (uv.y - 0.5) * PI;
    vec3 dir = vec3(sin(longitude)*cos(latitude), sin(latitude),
                    cos(longitude)*cos(latitude));
    if(scintilla_measure_mode!=0){
        if(!scintillaMeasurementMask(dir,uv,resolution,scintilla_measure_index,scintilla_measure_mode))discard;
        FragColor=vec4(0);return;
    }
    vec4 color = use_multiview != 0 ? sampleMultiview(dir)
                                    : sampleEnvironment(dir);
    if (debug_grid != 0)
    {
        vec2 pixel = 2.0 / resolution;
        float meridians = gridLine(sin(longitude*4.0), 0.012);
        float parallels = gridLine(sin(latitude*6.0), 0.012);
        vec3 grid = vec3(0.05, 0.65, 0.85)*max(meridians, parallels);
        float equator = gridLine(uv.y-0.5, pixel.y);
        // Cardinal colours: front red, right green, left magenta, back orange;
        // zenith cyan and nadir yellow. White centre cross identifies front.
        for (int i=0; i<5; i++)
        {
            float u = float(i)*0.25;
            float marker = (1.0-smoothstep(0.016,0.025,length((uv-vec2(u,0.5))*vec2(2.0,1.0))));
            vec3 c = i==2 ? vec3(1,0.05,0.02) : i==3 ? vec3(0.05,1,0.1)
                : i==1 ? vec3(1,0.05,1) : vec3(1,0.45,0.02);
            grid=max(grid,c*marker);
        }
        grid=max(grid,vec3(0.05,0.95,1)*step(0.985,uv.y));
        grid=max(grid,vec3(1,0.9,0.05)*(1.0-step(0.015,uv.y)));
        grid=max(grid,vec3(1,0.9,0.05)*equator*0.65);
        color.rgb=mix(color.rgb,grid,clamp(max(max(grid.r,grid.g),grid.b),0.0,0.85));
    }
    FragColor=vec4(scintillaOverlay(color.rgb,dir,uv,resolution),scintillaAlpha(gl_FragCoord.xy,resolution));
}
