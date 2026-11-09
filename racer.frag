#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform RacerState
{
    vec2 resolution;
    float time;
    float steering;
    float speed;
    float score;
} pc;

float box2(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float sdTriangle(vec2 p, vec2 a, vec2 b, vec2 c)
{
    vec2 e0 = b - a;
    vec2 e1 = c - b;
    vec2 e2 = a - c;
    vec2 v0 = p - a;
    vec2 v1 = p - b;
    vec2 v2 = p - c;
    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
    float s = sign(e0.x * e2.y - e0.y * e2.x);
    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

vec3 carShape(vec2 p, vec3 body)
{
    float bodyD = box2(p, vec2(0.105, 0.205), 0.035);
    float cabinD = box2(p - vec2(0.0, 0.035), vec2(0.073, 0.095), 0.025);
    float glassD = box2(p - vec2(0.0, 0.045), vec2(0.061, 0.061), 0.018);
    float hoodD = box2(p - vec2(0.0, -0.125), vec2(0.078, 0.045), 0.018);
    float wheels = min(
        box2(p - vec2(-0.105, 0.105), vec2(0.023, 0.062), 0.009),
        box2(p - vec2(0.105, 0.105), vec2(0.023, 0.062), 0.009)
    );
    wheels = min(wheels, box2(p - vec2(-0.105, -0.105), vec2(0.023, 0.062), 0.009));
    wheels = min(wheels, box2(p - vec2(0.105, -0.105), vec2(0.023, 0.062), 0.009));

    vec3 color = vec3(0.025, 0.03, 0.04);
    color = mix(color, body, 1.0 - smoothstep(-0.004, 0.004, bodyD));
    color = mix(color, vec3(0.035, 0.13, 0.19), 1.0 - smoothstep(-0.004, 0.004, glassD));
    color = mix(color, body * 0.78, 1.0 - smoothstep(-0.004, 0.004, hoodD));
    color = mix(color, vec3(0.92, 0.96, 1.0), 1.0 - smoothstep(-0.004, 0.004, wheels));
    float tail = box2(p - vec2(0.0, 0.18), vec2(0.075, 0.012), 0.004);
    color = mix(color, vec3(1.0, 0.08, 0.035), 1.0 - smoothstep(-0.004, 0.004, tail));
    return color;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / pc.resolution;
    vec2 p = uv * 2.0 - 1.0;
    float horizon = 0.22;
    float depth = clamp((horizon - p.y) / (horizon + 1.0), 0.0, 1.0);
    float curve = sin(pc.time * 0.22 + depth * 3.2) * 0.13
                + sin(pc.time * 0.11 + depth * 7.0) * 0.045;
    float roadCenter = curve - pc.steering * 0.22;
    float roadHalf = mix(0.035, 0.79, pow(depth, 0.82));
    float localX = p.x - roadCenter;
    float roadEdge = abs(localX) - roadHalf;

    vec3 sky = mix(vec3(0.025, 0.055, 0.11), vec3(0.34, 0.52, 0.66),
                   smoothstep(-0.15, 0.65, p.y));
    vec3 ground = mix(vec3(0.035, 0.19, 0.075), vec3(0.08, 0.27, 0.11),
                      step(0.5, fract((p.y + pc.time * 0.018) * 18.0)));
    vec3 color = p.y > horizon ? sky : ground;

    float shoulder = abs(localX) - roadHalf * 1.035;
    float kerbPattern = step(0.5, fract(depth * 44.0 + pc.time * 0.12));
    vec3 kerb = mix(vec3(0.9, 0.88, 0.82), vec3(0.82, 0.08, 0.045), kerbPattern);
    color = mix(color, kerb, 1.0 - smoothstep(0.004, 0.012, abs(shoulder) - 0.012));

    color = mix(color, vec3(0.105, 0.115, 0.125), 1.0 - smoothstep(-0.004, 0.004, roadEdge));

    float laneX = abs(localX / max(roadHalf, 0.001));
    float dash = step(0.72, fract(depth * 23.0 + pc.time * (0.25 + pc.speed * 0.75)));
    float centerLine = 1.0 - smoothstep(0.006, 0.014, abs(localX));
    color = mix(color, vec3(0.92, 0.88, 0.68), centerLine * dash * step(0.18, depth));

    float sideLine = 1.0 - smoothstep(0.0, 0.012, abs(laneX - 0.91));
    color = mix(color, vec3(0.86, 0.86, 0.8), sideLine);

    for (int i = 0; i < 18; ++i)
    {
        float fi = float(i);
        float z = fract(fi * 0.137 + pc.time * (0.035 + pc.speed * 0.08));
        float y = horizon - z * (horizon + 1.0);
        float scale = mix(0.012, 0.13, z * z);
        float side = (i % 2 == 0) ? -1.0 : 1.0;
        float x = roadCenter + side * (roadHalf + mix(0.055, 0.24, z));
        vec2 tp = (p - vec2(x, y)) / max(scale, 0.001);
        float trunk = box2(tp - vec2(0.0, -0.18), vec2(0.12, 0.42), 0.025);
        float foliage = sdTriangle(tp, vec2(0.0, 1.25), vec2(-0.75, -0.15), vec2(0.75, -0.15));
        vec3 tree = mix(vec3(0.16, 0.075, 0.035), vec3(0.025, 0.22, 0.075),
                        1.0 - smoothstep(-0.015, 0.015, foliage));
        color = mix(color, tree, 1.0 - smoothstep(-0.015, 0.015, min(trunk, foliage)));
    }

    for (int i = 0; i < 3; ++i)
    {
        float fi = float(i);
        float z = fract(0.18 + fi * 0.31 + pc.time * (0.035 + pc.speed * 0.10));
        float y = mix(0.10, -0.42, z);
        float roadW = mix(0.06, 0.55, z);
        float lane = (i == 0) ? -0.43 : ((i == 1) ? 0.43 : 0.0);
        float x = roadCenter + lane * roadW;
        float size = mix(0.035, 0.15, z);
        vec2 cp = (p - vec2(x, y)) / max(size, 0.001);
        vec3 rival = (i == 0) ? vec3(0.8, 0.12, 0.045) :
                     ((i == 1) ? vec3(0.05, 0.34, 0.82) : vec3(0.86, 0.66, 0.08));
        vec3 rivalColor = carShape(cp, rival);
        float rivalMask = box2(cp, vec2(0.15, 0.24), 0.025);
        color = mix(color, rivalColor, 1.0 - smoothstep(-0.012, 0.012, rivalMask));
    }

    vec2 playerPos = vec2(pc.steering * 0.42, -0.64);
    vec2 carP = (p - playerPos) / vec2(1.0, 1.0);
    float carMask = box2(carP, vec2(0.13, 0.24), 0.035);
    vec3 playerColor = carShape(carP, vec3(0.86, 0.035, 0.025));
    color = mix(color, playerColor, 1.0 - smoothstep(-0.012, 0.012, carMask));

    float speedBar = smoothstep(-0.98, -0.96 + pc.speed * 0.65, p.x) *
                     (1.0 - smoothstep(-0.91, -0.89, p.x));
    float hud = step(0.82, uv.y) * step(uv.x, 0.28);
    color = mix(color, vec3(0.025, 0.035, 0.05), hud * 0.72);
    float bar = step(0.86, uv.y) * step(uv.y, 0.89) * step(0.04, uv.x) *
                (1.0 - step(0.04 + pc.speed * 0.20, uv.x));
    color = mix(color, vec3(0.12, 0.85, 0.92), bar);
    float scoreMarks = step(0.93, uv.y) * step(uv.y, 0.95) *
                       step(0.04, uv.x) * (1.0 - step(0.04 + fract(pc.score * 0.013) * 0.20, uv.x));
    color = mix(color, vec3(1.0, 0.72, 0.12), scoreMarks);

    float vignette = 1.0 - 0.32 * smoothstep(0.45, 1.45, length(p * vec2(0.8, 0.9)));
    color *= vignette;
    color = pow(max(color, vec3(0.0)), vec3(0.92));
    outColor = vec4(color, 1.0);
}
