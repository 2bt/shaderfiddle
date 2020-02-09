#define E  0.001
#define PI 3.141592653589793

float smin(float a, float b) {
    return min(a, b) - pow(max(0, 1.0 - abs(a - b)) * $k(0, 0.1), $smooth);
    return min(a, b);
}

float max3(vec3 v) {
    return max(max(v.x, v.y), v.z);
}

float box(vec3 p, vec3 s) {
    return max3(abs(p) - s);
}

void rot(inout vec2 p, float v) {
    float si = sin(v);
    float co = cos(v);
    p = vec2(co * p.x + si * p.y, co * p.y - si * p.x);
}

float line2(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    vec2 ap = p - a;
    vec2 n = ab * clamp(dot(ab, ap) / dot(ab, ab), 0.0, 1.0);
    return distance(ap, n);
}

float lines(vec3 p) {
//    rot(p.xz, iTime);

    float d = 9e9;
    d = min(d, line2(p.xy, vec2(-2.0, 3.0), vec2(0.0, 5.0)));
    d = min(d, line2(p.xy, vec2(-2.0, 3.0), vec2(3.0, 1.0)));
    d = min(d, line2(p.xy, vec2(0.0, 5.0), vec2(3.0, 1.0)));

    d = abs(d - $t);

//    d = abs(d - 0.4);
//    return pow(pow(d, 8.0 * $a(0,2)) + pow(z, 8.0 * $a(0,2)), .125 * $r(0,2)) - $t;
    return max(d - 0.2, abs(p.z) - 0.5);
}

vec2 mmin(vec2 a, vec2 b) {
    return a.x < b.x ? a : b;
}
vec2 mmax(vec2 a, vec2 b) {
    return a.x > b.x ? a : b;
}


#define M_WALL   2.0
#define M_BOX    3.0
#define M_SKY    4.0
#define SIMPLE   0


vec2 map(vec3 p) {
    vec2 d = vec2(-p.y + 10.0, M_SKY);
    d = mmin(d, vec2(-box(p + vec3(0.0, -10.0, 0.0), vec3(10.0, 10.0, 10.0)), M_WALL));

    d = mmin(d, vec2(lines(p), M_BOX));


    return d;
}

vec3 normal(vec3 p) {
    float d = map(p).x;
    return normalize(vec3(
        map(p + vec3(E, 0.0, 0.0)).x - d,
        map(p + vec3(0.0, E, 0.0)).x - d,
        map(p + vec3(0.0, 0.0, E)).x - d));
}



vec2 march(vec3 o, vec3 d) {
    float t = 0.0;
    vec3 p;
    for (int i = 0; i < 100 && t < 50.0; ++i) {
        p = o + d * t;
        vec2 s = map(p);
        if (s.x < E) return vec2(t, s.y);
        t += s.x;
    }
    return vec2(0.0, 0.0);
}

float rand3dTo1d(vec3 value, vec3 dotDir) {
    float random = dot(sin(value + iFrame), dotDir) + iFrame;
    return fract(sin(random) * 143758.5453);
}
vec3 rand3dTo3d(vec3 value) {
    return vec3(
        rand3dTo1d(value, vec3(12.989, 78.233, 37.719)),
        rand3dTo1d(value, vec3(39.346, 11.135, 83.155)),
        rand3dTo1d(value, vec3(73.156, 52.235,  9.151))
    );
}

vec3 rand_dir(vec3 p) {
    vec3 v;
    for (int i = 0; i < 4; ++i) {
        v = rand3dTo3d(p + vec3(i)) * 2.0 - vec3(1.0);
        if (dot(v, v) < 1.01) break;
    }
    return normalize(v);
}


vec3 trace(vec3 o, vec3 d) {
    vec3 light = normalize(vec3(1.0, 3.0, -2.0));
    vec3 attenuation = vec3(1.0);

    vec3 col = vec3(0.0);
    for (int i = 0; i < 3; ++i) {
        vec2 m = march(o, d);
        if (m.y == 0.0) break;
        vec3 p = o + d * m.x;
        if (m.y < M_SKY - 0.5) {
            vec3 n = normal(p);

            // material color
            vec3 c = vec3(0.2);
            if (m.y == M_BOX) c = vec3(0.5, 0.1, 0.1);

            if (SIMPLE != 0) {
                col += c * max(dot(n, light), 0.1);
                break;
            }

            vec3 r = rand_dir(p);
            if (dot(n, r) < 0.0) r = -r;
            d = r;
            o = p + n * E;
            attenuation *= c;
            continue;
        }
        if (m.y < M_SKY + 0.5) {
            col += vec3(2.0, 3.0, 4.0) * attenuation;
            break;
        }
    }
    return col;
}


void main() {

    int N = 2;
    vec3 col = vec3(0.0);
    for (int i = 0; i < N; ++i) {
        vec3 dir = normalize(iEye * vec3((gl_FragCoord.xy + vec2(i / float(N))) / iResolution.x * 2.0 - vec2(1.0, iResolution.y / iResolution.x), 1.5));
        col += trace(iPos, dir);
    }

    col /= N;

    gl_FragColor = vec4(col, 1.0) + texelFetch(iChannel0, ivec2(gl_FragCoord.xy), 0);
}

---


void main() {

    vec4 d = texelFetch(iChannel0, ivec2(gl_FragCoord.xy), 0);
    vec3 col = d.xyz / d.w;
    col /= col + vec3(1.0);

    gl_FragColor.rgba = vec4(col, 1.0);
}
