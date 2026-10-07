#stk_include "utils/immersive_sphere_sampling.frag"

uniform vec2 resolution;
uniform float dome_tilt_rad;
uniform int debug_grid;

out vec4 FragColor;

const float PI = 3.14159265358979323846;

vec3 rotateX(vec3 v, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(v.x, c * v.y - s * v.z, s * v.y + c * v.z);
}

float lineMask(float distance_to_line, float width)
{
    return 1.0 - smoothstep(width, width * 2.0, abs(distance_to_line));
}

float discMask(vec2 p, vec2 center, float radius)
{
    return 1.0 - smoothstep(radius, radius + 0.018, length(p - center));
}

float crossMask(vec2 p, vec2 center, float arm, float width)
{
    vec2 q = p - center;
    float vertical = lineMask(q.x, width) * (1.0 - step(arm, abs(q.y)));
    float horizontal = lineMask(q.y, width) * (1.0 - step(arm, abs(q.x)));
    return max(vertical, horizontal);
}

vec3 debugGrid(vec2 p, float r, float phi, float tilt)
{
    float rings = 0.0;
    for (float i = 0.2; i < 1.01; i += 0.2)
        rings = max(rings, lineMask(r - i, 0.0035));

    float radials = 1.0 - smoothstep(0.015, 0.035, abs(sin(phi * 8.0)));
    float horizon = lineMask(r - 1.0, 0.006);
    float forward_axis = lineMask(p.x, 0.004) *
                         smoothstep(0.15, 0.25, p.y) *
                         (1.0 - smoothstep(0.92, 1.0, r));
    float tilt_line = lineMask(sin(tilt) * p.x - cos(tilt) * p.y, 0.004) *
                      smoothstep(0.55, 0.65, r) * (1.0 - smoothstep(0.93, 1.0, r));

    vec3 color = vec3(0.0);
    color += rings * vec3(0.1, 0.8, 0.9);
    color += radials * vec3(0.1, 0.45, 1.0);
    color += horizon * vec3(1.0, 0.9, 0.05);
    color += forward_axis * vec3(1.0, 0.15, 0.05);
    color += tilt_line * vec3(0.9, 0.25, 1.0);

    float edge = lineMask(r - 1.0, 0.005);
    color += edge * vec3(1.0);

    float side_marker = smoothstep(0.90, 0.94, r) *
                        (1.0 - smoothstep(0.96, 1.0, r));
    color += side_marker * lineMask(abs(p.x) - 0.92, 0.008) *
             vec3(0.1, 1.0, 0.2);
    color += side_marker * lineMask(abs(p.y) - 0.92, 0.008) *
             vec3(1.0, 0.25, 0.1);

    float front_marker = max(discMask(p, vec2(0.0, 0.0), 0.060),
                             crossMask(p, vec2(0.0, 0.0), 0.13, 0.008));
    float up_marker = max(discMask(p, vec2(0.0, 0.78), 0.055),
                          crossMask(p, vec2(0.0, 0.78), 0.11, 0.008));
    float right_marker = max(discMask(p, vec2(0.78, 0.0), 0.055),
                             crossMask(p, vec2(0.78, 0.0), 0.11, 0.008));
    float left_marker = max(discMask(p, vec2(-0.78, 0.0), 0.055),
                            crossMask(p, vec2(-0.78, 0.0), 0.11, 0.008));
    float back_marker = max(discMask(p, vec2(0.0, -0.78), 0.055),
                            crossMask(p, vec2(0.0, -0.78), 0.11, 0.008));

    color = max(color, front_marker * vec3(1.0, 0.05, 0.02));
    color = max(color, up_marker * vec3(0.05, 0.95, 1.0));
    color = max(color, right_marker * vec3(0.05, 1.0, 0.1));
    color = max(color, left_marker * vec3(1.0, 0.05, 1.0));
    color = max(color, back_marker * vec3(1.0, 0.45, 0.02));
    return clamp(color, 0.0, 1.0);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    vec2 p = uv * 2.0 - 1.0;
    float r = length(p);
    if (r > 1.0)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float phi = atan(p.y, p.x);
    float theta = r * (PI * 0.5);

    vec3 dir;
    if (r < 0.00001)
    {
        dir = vec3(0.0, 0.0, 1.0);
    }
    else
    {
        vec2 disk_dir = p / r;
        float s = sin(theta);
        dir = vec3(s * disk_dir.x,
                   s * disk_dir.y,
                   cos(theta));
    }
    dir = normalize(rotateX(dir, dome_tilt_rad));

    vec4 color = use_multiview != 0 ? sampleMultiview(dir)
                                    : sampleEnvironment(dir);
    if (debug_grid != 0)
    {
        vec3 grid = debugGrid(p, r, phi, dome_tilt_rad);
        color.rgb = mix(color.rgb, grid, clamp(max(max(grid.r, grid.g), grid.b), 0.0, 0.85));
    }
    FragColor = vec4(color.rgb, 1.0);
}
