#version 450
// PES post chain (docs/re/pes-re-lighting.md Part C). One kernel per PASS_* define,
// compiled by pes_post_effect.gd. Per frame, in order: REDUCE (tile sums of the
// pre-exposed luminance), LAW (one group: the exposure step, EV state), BRIGHT
// (quarter-resolution threshold), KAWASE (one dispatch per bloom kernel,
// accumulated), COMPOSE (bloom add, white balance, FilmToneMap, display encode,
// written back to the colour buffer).
//
// Params buffer (32 floats, std430; written by src/gdext/pes_lighting.cpp):
//   [0..11]  white balance, column j at [4j .. 4j+2] (linear sRGB, column vectors)
//   [12..19] film: toeMatch, straightMatch, shoulderMatch, slope, black, white, toeScale, shoulderScale
//   [20..23] exposure: key, compensation, minEV, maxEV
//   [24..25] tonemapSpeed, stepGain (1 − 0.98^(60·dt), dt = 1/60)
//   [26..29] bloom: threshold, weight, kernel count, offset scale (0 for the copy branch)
// Push constants (all floats): idx = (EV read slot, EV write slot, kernel index, first pass),
// kern = (offset in quarter-res texels, pass weight, copy scale), ext = (pixel count, partial count).

// ponytail: not applied, open in docs/re/pes-re-lighting.md. The 33³ grading LUT
// (ColorGrading; input domain and encoding unresolved, the shipped CubeLut body is a
// stub: dump ColorCorrection_CubeLut from the live process). AA (FXAA/TAA), sharpen
// and grain (Part C §5). The reduction is an arithmetic mean over the full frame
// (type open, §1) and omits the 32·a² alpha term (Godot's alpha is not PES's bright-pixel
// code). The bright pass samples the scene bilinearly at quarter-res texel centres (the
// PES downsample filter is not decoded, §2).

layout(push_constant, std430) uniform Push {
	vec4 idx;
	vec4 kern;
	vec4 ext;
} pc;

// Tonemap_Lum weights (OBS, pes-re-lighting.md §1).
const vec3 LUMA = vec3(0.2125, 0.7154, 0.0721);
const float LUMA_FLOOR = 0.001;        // Tonemap_Lum's floor
const float BRIGHT_CAP = 128.0;        // TonemapBrightpass's cap (§2)
const float RRT_SATURATION = 0.96;     // FilmToneMap pre-desaturation (§3)
const float POST_DESATURATION = 0.93;  // FilmToneMap post-desaturation (§3)
const float DISPLAY_GAMMA = 2.2;       // pow(1/2.2) display encode (§3)
const float ADAPT_KEY_FLOOR = 0.001;   // log2(key + 0.001) (§1)
const float ADAPT_MAX_STEP = 0.5;      // min(|Δ|, 0.5) (§1)
const float ADAPT_TARGET_HALF = 0.5;   // the target takes half the log error (§1)
const float BLOOM_CHROMA_TEXELS = 8.0; // composition's R/B fringe (§2)

// UE4 AP1 matrices (the first rows are OBS, §3; the rest are the same standard constants).
// Written as columns: each column is the matching column of UE4's row-major matrix.
const mat3 SRGB_TO_AP1 = mat3(
	vec3(0.613191485, 0.0699417293, 0.0206157697),
	vec3(0.33951208, 0.916338623, 0.109594166),
	vec3(0.0473663323, 0.0137248794, 0.869790077));
const mat3 AP1_TO_SRGB = mat3(
	vec3(1.70505154, -0.130256429, -0.0240032743),
	vec3(-0.621790707, 1.14080286, -0.128969777),
	vec3(-0.0832583979, -0.0105485261, 1.15297317));
const vec3 AP1_Y = vec3(0.272228718, 0.674081743, 0.0536895171);

vec3 srgb_to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), greaterThan(c, vec3(0.04045)));
}

#ifdef PASS_REDUCE
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(rgba16f, set = 0, binding = 0) uniform readonly image2D color;
layout(std430, set = 0, binding = 1) writeonly buffer Partials { float sums[]; };
layout(std430, set = 0, binding = 2) readonly buffer Ev { float ev[2]; };

shared float tile[256];

void main() {
	ivec2 size = imageSize(color);
	ivec2 px = ivec2(gl_GlobalInvocationID.xy);
	float value = 0.0;
	if (all(lessThan(px, size))) {
		float scale = exp2(ev[int(pc.idx.x)]);
		value = max(dot(imageLoad(color, px).rgb, LUMA) * scale, LUMA_FLOOR);
	}
	tile[gl_LocalInvocationIndex] = value;
	barrier();
	for (uint s = 128u; s > 0u; s >>= 1u) {
		if (gl_LocalInvocationIndex < s) tile[gl_LocalInvocationIndex] += tile[gl_LocalInvocationIndex + s];
		barrier();
	}
	if (gl_LocalInvocationIndex == 0u) {
		sums[gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x] = tile[0];
	}
}
#endif

#ifdef PASS_LAW
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
layout(std430, set = 0, binding = 0) readonly buffer Partials { float sums[]; };
layout(std430, set = 0, binding = 1) buffer Ev { float ev[2]; };
layout(std430, set = 0, binding = 2) readonly buffer Params { float v[32]; } prm;

shared float acc[256];

void main() {
	uint count = uint(pc.ext.y);
	float total = 0.0;
	for (uint i = gl_LocalInvocationIndex; i < count; i += 256u) total += sums[i];
	acc[gl_LocalInvocationIndex] = total;
	barrier();
	for (uint s = 128u; s > 0u; s >>= 1u) {
		if (gl_LocalInvocationIndex < s) acc[gl_LocalInvocationIndex] += acc[gl_LocalInvocationIndex + s];
		barrier();
	}
	if (gl_LocalInvocationIndex != 0u) return;
	// The adaptation law (OBS, FUN_1416c7930; §1). Lmeas is the mean of the
	// pre-exposed luminance; the EV state moves toward its target by a fraction of the error.
	float lmeas = acc[0] / pc.ext.x;
	float key = prm.v[20];
	float compensation = prm.v[21];
	float minEv = prm.v[22];
	float maxEv = prm.v[23];
	float ev0 = clamp(ev[int(pc.idx.x)], minEv, maxEv);
	// ponytail: the EV→offset table g() is zero in the live state (its source is open,
	// §1), so Lc = Lmeas·2^−comp.
	float lc = lmeas * exp2(-compensation);
	float target = clamp(ev0 + ADAPT_TARGET_HALF * (log2(key + ADAPT_KEY_FLOOR) - log2(lc + ADAPT_KEY_FLOOR)), minEv, maxEv);
	float delta = target - ev0;
	float step = prm.v[25] * prm.v[24] * min(abs(delta), ADAPT_MAX_STEP) * delta;
	ev[int(pc.idx.y)] = (abs(step) < abs(delta)) ? ev0 + step : target;
}
#endif

#ifdef PASS_BRIGHT
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D color_tex;
layout(rgba16f, set = 0, binding = 1) uniform writeonly image2D bright;
layout(std430, set = 0, binding = 2) readonly buffer Ev { float ev[2]; };
layout(std430, set = 0, binding = 3) readonly buffer Params { float v[32]; } prm;

void main() {
	ivec2 q = ivec2(gl_GlobalInvocationID.xy);
	ivec2 qsize = imageSize(bright);
	if (any(greaterThanEqual(q, qsize))) return;
	vec2 uv = (vec2(q) + 0.5) / vec2(qsize);
	vec3 pre = texture(color_tex, uv).rgb * exp2(ev[int(pc.idx.x)]);
	vec3 b = clamp(pre - prm.v[26], 0.0, BRIGHT_CAP);
	imageStore(bright, q, vec4(b, 1.0));
}
#endif

#ifdef PASS_KAWASE
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D bright_tex;
layout(rgba16f, set = 0, binding = 1) uniform image2D bloom;

void main() {
	ivec2 q = ivec2(gl_GlobalInvocationID.xy);
	ivec2 qsize = imageSize(bloom);
	if (any(greaterThanEqual(q, qsize))) return;
	vec2 texel = 1.0 / vec2(qsize);
	vec2 uv = (vec2(q) + 0.5) * texel;
	vec2 o = pc.kern.x * texel;
	vec3 k = texture(bright_tex, uv + vec2(-o.x, -o.y)).rgb + texture(bright_tex, uv + vec2(o.x, -o.y)).rgb
		+ texture(bright_tex, uv + vec2(-o.x, o.y)).rgb + texture(bright_tex, uv + vec2(o.x, o.y)).rgb;
	k *= 0.25 * pc.kern.y * pc.kern.z;
	vec3 base = (pc.idx.w != 0.0) ? vec3(0.0) : imageLoad(bloom, q).rgb;
	imageStore(bloom, q, vec4(base + k, 1.0));
}
#endif

#ifdef PASS_COMPOSE
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba16f, set = 0, binding = 0) uniform image2D color;
layout(set = 0, binding = 1) uniform sampler2D bloom_tex;
layout(std430, set = 0, binding = 2) readonly buffer Ev { float ev[2]; };
layout(std430, set = 0, binding = 3) readonly buffer Params { float v[32]; } prm;

void main() {
	ivec2 px = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(color);
	if (any(greaterThanEqual(px, size))) return;
	vec2 uv = (vec2(px) + 0.5) / vec2(size);
	vec4 src = imageLoad(color, px);
	vec3 scene = src.rgb * exp2(ev[int(pc.idx.x)]);
	// Bloom is added unweighted, with its R and B fringed by 8 texels (§2).
	vec2 d = BLOOM_CHROMA_TEXELS / vec2(size);
	vec3 bloom = vec3(texture(bloom_tex, uv + d).r, texture(bloom_tex, uv).g, texture(bloom_tex, uv - d).b);
	mat3 wb = mat3(vec3(prm.v[0], prm.v[1], prm.v[2]), vec3(prm.v[4], prm.v[5], prm.v[6]), vec3(prm.v[8], prm.v[9], prm.v[10]));
	vec3 c = wb * (scene + bloom);
	vec3 a = max(SRGB_TO_AP1 * c, 0.0);
	a = mix(vec3(dot(a, AP1_Y)), a, RRT_SATURATION);
	vec3 L = log2(a) * 0.30103001;  // log10 via log2 (§3)
	float toeMatch = prm.v[12];
	float straightMatch = prm.v[13];
	float shoulderMatch = prm.v[14];
	float slope = prm.v[15];
	float black = prm.v[16];
	float white = prm.v[17];
	float toeScale = prm.v[18];
	float shoulderScale = prm.v[19];
	vec3 straight = slope * (L + straightMatch);
	vec3 toe = -black + 2.0 * toeScale / (1.0 + exp(-2.0 * slope / toeScale * (L - toeMatch)));
	vec3 shoulder = (1.0 + white) - 2.0 * shoulderScale / (1.0 + exp(2.0 * slope / shoulderScale * (L - shoulderMatch)));
	toe = mix(straight, toe, lessThan(L, vec3(toeMatch)));
	shoulder = mix(straight, shoulder, greaterThan(L, vec3(shoulderMatch)));
	vec3 t;
	if (shoulderMatch != toeMatch) {
		t = clamp((L - toeMatch) / (shoulderMatch - toeMatch), 0.0, 1.0);
	} else {
		// Toe + shoulder = 1: the blend divides by zero, so it is a hard switch at the toe match.
		t = vec3(greaterThan(L, vec3(toeMatch)));
	}
	if (shoulderMatch < toeMatch) t = 1.0 - t;
	t = (3.0 - 2.0 * t) * t * t;
	vec3 tone = mix(toe, shoulder, t);
	tone = mix(vec3(dot(tone, AP1_Y)), tone, POST_DESATURATION);
	vec3 display = clamp(pow(max(AP1_TO_SRGB * tone, 0.0), vec3(1.0 / DISPLAY_GAMMA)), 0.0, 1.0);
	// Godot's final pass sRGB-encodes the colour buffer: store the linear value whose
	// encode is PES's display value.
	imageStore(color, px, vec4(srgb_to_linear(display), src.a));
}
#endif
