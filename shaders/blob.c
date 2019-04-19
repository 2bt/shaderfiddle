#define		E		0.0001

mat3 eye_dir = mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);
vec3 eye_pos = vec3(0, 0, 1);

float smin(float a, float b) {
	return min(a, b) - pow(max(0, 1.0 - abs(a - b)) * $k(0, 0.1), $smooth);
	return min(a, b);
}

float map(vec3 p) {

    float time = iTime * $time_factor + $time_offset(0, 10);

    float d = min(p.y + 3.0, 2.0 - p.z);

	d = min(d,
		smin(
			length(
				p + vec3(sin(time * 1.1) * 4.9,
				cos(time * 0.9) * 1.8 + 0.4,
				0.5 + cos(time * 2.1) * 0.5)) - 1.3,
			smin(
				length(p) - 2.0,
				length(p + vec3(sin(time * 1.3) * 4.2, cos(time * 1.3) * 1.3 + 0.4, 0.0)) - 2.0)));
    return d;
}

vec3 normal(vec3 p) {
	float d = map(p);
	return normalize(vec3(
		map(p + vec3(E, 0.0, 0.0)) - d,
		map(p + vec3(0.0, E, 0.0)) - d,
		map(p + vec3(0.0, 0.0, E)) - d));
}

vec3 light_dir;

float light(vec3 p) {
	vec3 n = normal(p);
	float dp = dot(n, light_dir);

	float c = max(dp, 0.0);


	float ao = 0.4;
	{
		// ambient occlusion
		float a = 0.0;
		vec3 o = p;
		for (int i = 0; i < 10; i++) {
			a = max(a, map(o));
			o += n * 0.05;
		}
		ao *= (1.0 - pow(1.0 - a, 3.0));
	}


	if (dp > 0.0) {
		vec3 o = p;
		float m = 1.0;
		float t = E;
		for (int i = 0; i < 100; i++) {
			float d = map(o + t * light_dir);
			t += d * 0.9;
			m = min(m, 10.0 * d / t);
			if (m < E) break;
		}
		c *= pow(clamp(m, 0.0, 1.0), 0.8);
	}


	return c + ao;
}


void main() {

	vec3 dir = vec3(gl_FragCoord.xy / iResolution.x * 2.0
		- vec2(1.0, iResolution.y / iResolution.x), 1.0);
    dir = eye_dir * dir;
	dir = normalize(dir);


	light_dir = normalize(vec3(1.0, 2.0, -0.3));

	vec3 pos = vec3(0.0, 0.0, -10.0) + eye_pos;
    vec3 p;
    float t = 0;
    float t_prev = 0;
    float factor = 0.9;
	for (int i = 0; i < 100; i++) {

		p = pos + dir * t;
		float d = map(p);

        if (d < 0) {
            t = t_prev;
            factor *= 0.9;
            continue;
        }
		if (d <= E) break;

        t_prev = t;
		t += d * factor;
	}

//    p += normal(p) * map(p);
//    p += normal(p) * map(p);

    gl_FragColor.xyz = vec3(0.7, 0.7, 1.0) * light(p);

	// background
//	gl_FragColor.xyz = vec3(0.05);

}
