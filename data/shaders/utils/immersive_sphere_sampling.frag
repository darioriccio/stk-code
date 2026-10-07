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
uniform int has_skybox;
uniform float face_tan_half_fov;
uniform int use_multiview;

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
