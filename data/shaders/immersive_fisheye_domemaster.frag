uniform samplerCube environment_cube;
uniform samplerCube scene_depth_cube;
uniform samplerCube skybox_cube;
uniform sampler2D face_pos_x;
uniform sampler2D face_neg_x;
uniform sampler2D face_pos_y;
uniform sampler2D face_neg_y;
uniform sampler2D face_pos_z;
uniform sampler2D face_neg_z;
uniform sampler2D depth_face_pos_x;
uniform sampler2D depth_face_neg_x;
uniform sampler2D depth_face_pos_y;
uniform sampler2D depth_face_neg_y;
uniform sampler2D depth_face_pos_z;
uniform sampler2D depth_face_neg_z;
uniform vec2 resolution;
uniform float dome_tilt_rad;
uniform int debug_grid;
uniform int has_skybox;
uniform float face_tan_half_fov;
uniform int use_multiview;

out vec4 FragColor;

const float PI = 3.14159265358979323846;

vec3 rotateX(vec3 v, float a)
{
    float c = cos(a);
    float s = sin(a);
    return vec3(v.x, c * v.y - s * v.z, s * v.y + c * v.z);
}

vec4 sampleEnvironment(vec3 dir)
{
    vec4 base = texture(environment_cube, dir);

    vec3 ad = abs(dir);
    float major = max(max(ad.x, ad.y), ad.z);
    float minor = min(min(ad.x, ad.y), ad.z);
    float middle = ad.x + ad.y + ad.z - major - minor;

    float edge_proximity = major - middle;
    float seam = 1.0 - smoothstep(0.035, 0.18, edge_proximity);
    if (seam <= 0.001)
        return base;

    vec3 helper = ad.y < 0.92 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 t1 = normalize(cross(helper, dir));
    vec3 t2 = normalize(cross(dir, t1));
    float e = 0.035;

    vec4 avg = base;
    avg += texture(environment_cube, normalize(dir + t1 * e));
    avg += texture(environment_cube, normalize(dir - t1 * e));
    avg += texture(environment_cube, normalize(dir + t2 * e));
    avg += texture(environment_cube, normalize(dir - t2 * e));
    avg *= 0.2;
    return mix(base, avg, seam * 0.75);
}

vec4 sampleProjectedFace(sampler2D face_tex, float ma, float sc, float tc,
                         out float weight)
{
    weight = 0.0;
    if (ma <= 0.0001)
        return vec4(0.0);

    vec2 q = vec2(sc, tc) / (ma * face_tan_half_fov);
    vec2 edge = vec2(1.0) - abs(q);
    float inside = step(0.0, edge.x) * step(0.0, edge.y);
    if (inside <= 0.0)
        return vec4(0.0);

    float edge_fade = smoothstep(0.0, 0.16, min(edge.x, edge.y));
    weight = edge_fade * pow(clamp(ma, 0.0, 1.0), 4.0);
    vec2 uv = q * 0.5 + 0.5;
    return texture(face_tex, uv);
}

void addFaceSample(inout vec4 color_sum, inout float weight_sum,
                   sampler2D face_tex, float ma, float sc, float tc)
{
    float w = 0.0;
    vec4 sample_color = sampleProjectedFace(face_tex, ma, sc, tc, w);
    color_sum += sample_color * w;
    weight_sum += w;
}

vec4 sampleMultiview(vec3 dir)
{
    vec4 color_sum = vec4(0.0);
    float weight_sum = 0.0;

    addFaceSample(color_sum, weight_sum, face_pos_x,
                  dir.x, -dir.z, dir.y);
    addFaceSample(color_sum, weight_sum, face_neg_x,
                  -dir.x, dir.z, dir.y);
    addFaceSample(color_sum, weight_sum, face_pos_y,
                  dir.y, dir.x, -dir.z);
    addFaceSample(color_sum, weight_sum, face_neg_y,
                  -dir.y, dir.x, dir.z);
    addFaceSample(color_sum, weight_sum, face_pos_z,
                  dir.z, dir.x, dir.y);
    addFaceSample(color_sum, weight_sum, face_neg_z,
                  -dir.z, -dir.x, dir.y);

    if (weight_sum <= 0.0001)
        return sampleEnvironment(dir);
    return color_sum / weight_sum;
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
