#version 450
// PES post chain (docs/re/pes-re-lighting.md Part C; the grading pair is
// docs/render/pes-render-grading-lut.md §1.3 and §3). One kernel per PASS_* define, compiled by
// pes_post_effect.gd. Per frame, in order: REDUCE (tile sums of the pre-exposed luminance over
// PES's Tonemap_Lum cb5[12] sub-rectangle), LAW (one group: the exposure step, EV state), COMPOSE
// (the exposure, then PES's grading pair: the ColorCorrection_CubeLut stub and SdrDisplayMapping),
// written back to the colour buffer.
//
// Params buffer (10 floats, std430; written by src/gdext/pes_lighting.cpp):
//   [0..3]  exposure: key, compensation, minEV, maxEV
//   [4..5]  tonemapSpeed, stepGain (1 − 0.98^(60·dt), dt = 1/60)
//   [6..9]  metering rect cb5[12] (xy offset, zw scale): FUN_141805840's upload inputs
//            (ComposePath §4: xy = (0.5/W, 0.5/H) + comp[0x8c..]*bss, zw = comp[0x94..] clamped [0,1]).
//            // ponytail: full-frame static inputs until the live comp-block rect is read.
// Push constants: slots = (EV read slot, EV write slot), ext = (pixel count, partial count).
//
// ponytail: not applied: AA (FXAA/TAA), sharpen and grain (Part C §5). The 33³ grading LUT is
// bound by PES's stub but never sampled (grading-lut §1.3), so there is nothing to port. The
// reduction omits the 32·a² alpha term (Godot's alpha is not PES's bright-pixel code).

layout(push_constant, std430) uniform Push {
	vec2 slots;
	vec2 ext;
} pc;

// Tonemap_Lum weights (OBS, pes-re-lighting.md §1).
const vec3 LUMA = vec3(0.2125, 0.7154, 0.0721);
const float LUMA_FLOOR = 0.001;        // Tonemap_Lum's floor
const float ADAPT_KEY_FLOOR = 0.001;   // log2(key + 0.001) (§1)
const float ADAPT_MAX_STEP = 0.5;      // min(|Δ|, 0.5) (§1)
const float ADAPT_TARGET_HALF = 0.5;   // the target takes half the log error (§1)

// The grading pair's literals, read from the install's shader bytecode (GrSystemShaders_dx11.fsop:
// ColorCorrection_CubeLut.ps and disp/SdrDisplayMapping.ps). Each names its asm line in those
// dumps (vkd3d d3d-asm), with the immediate as it prints.
const float SRGB_BREAK = 0.0031308;            // stub L22: 3.13080009e-03, the linear/power select
const float SRGB_LINEAR_SLOPE = 12.92;         // stub L21: 1.29200001e+01
const float SRGB_SCALE = 1.055;                // stub L19: 1.05499995
const float SRGB_OFFSET = 0.055;               // stub L20: -5.49999997e-02; display L11: +5.49999997e-02
const float SRGB_POW_EXPONENT = 2.4;           // stub L17 (divide), display L15 (multiply): 2.40000010
const float SRGB_POW_FLOOR = 1e-5;             // stub L15, display L13: 9.99999975e-06
const float CUBE_LUT_GAIN = 0.25;              // stub L24: 2.50000000e-01
const float DISPLAY_DECODE_GAIN = 4.0;         // display L11 mad: 4.0
const float DISPLAY_INV_SCALE = 0.947867334;   // display L12: 9.47867334e-01 (1/1.055)
const float DISPLAY_DARK_BREAK = 0.00982;      // display L17: 9.82000027e-03
const float DISPLAY_DARK_SLOPE = 4.0 / 12.92;  // display L23: 3.09597522e-01

vec3 srgb_encode_piecewise(vec3 scene) {
	return mix(SRGB_LINEAR_SLOPE * scene,
		SRGB_SCALE * exp(log(max(scene, SRGB_POW_FLOOR)) / SRGB_POW_EXPONENT) - SRGB_OFFSET,
		greaterThanEqual(scene, vec3(SRGB_BREAK)));
}

// ColorCorrection_CubeLut as the match binds it: the piecewise sRGB encode of the point-sampled
// scene, then x0.25. Its 33³ LUT is bound and never sampled.
vec3 cube_lut_stub(vec3 scene) {
	// ponytail: the bypass (cb5[10].x = 1 when (vp+0x650) & 0xf == 2; upload at 0x1418044bf-0x141804515)
	// is not modelled. The live match reads 0, so the x0.25 always applies.
	return CUBE_LUT_GAIN * srgb_encode_piecewise(scene);
}

// SdrDisplayMapping: the sRGB decode of 4x its input, the x4 folded into both branches. Inputs at
// or below DISPLAY_DARK_BREAK take the linear branch. The result is linear light.
vec3 sdr_display_mapping(vec3 v) {
	vec3 decoded = exp(log(max((DISPLAY_DECODE_GAIN * v + SRGB_OFFSET) * DISPLAY_INV_SCALE, SRGB_POW_FLOOR))
		* SRGB_POW_EXPONENT);
	return mix(decoded, DISPLAY_DARK_SLOPE * v, lessThanEqual(v, vec3(DISPLAY_DARK_BREAK)));
}

#ifdef PASS_REDUCE
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(rgba16f, set = 0, binding = 0) uniform readonly image2D color;
layout(std430, set = 0, binding = 1) writeonly buffer Partials { float sums[]; };
layout(std430, set = 0, binding = 2) readonly buffer Ev { float ev[2]; };
layout(std430, set = 0, binding = 3) readonly buffer Params { float v[10]; } prm;

shared float tile[256];

void main() {
	ivec2 size = imageSize(color);
	ivec2 px = ivec2(gl_GlobalInvocationID.xy);
	float value = 0.0;
	if (all(lessThan(px, size))) {
		// Tonemap_Lum UV (asm): uv = (ndc*(0.5,-0.5)+0.5)*cb5[12].zw + cb5[12].xy, point-sampled
		// (s8) from the source. Every meter pixel gathers one rect texel; the mean over the
		// meter target is the rect content's mean.
		vec2 ndc = (vec2(px) + vec2(0.5)) / vec2(size) * 2.0 - 1.0;
		vec2 uv = (ndc * vec2(0.5, -0.5) + vec2(0.5)) * vec2(prm.v[8], prm.v[9])
			+ vec2(prm.v[6], prm.v[7]);
		ivec2 srcPx = ivec2(min(uv * vec2(size), vec2(size) - 1));
		srcPx = ivec2(max(srcPx, ivec2(0)));
		float scale = exp2(ev[int(pc.slots.x)]);
		value = max(dot(imageLoad(color, srcPx).rgb, LUMA) * scale, LUMA_FLOOR);
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
layout(std430, set = 0, binding = 2) readonly buffer Params { float v[10]; } prm;

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
	// pre-exposed luminance gathered from PES's metered rect; the EV state moves toward its
	// target by a fraction of the error.
	float lmeas = acc[0] / pc.ext.x;
	float key = prm.v[0];
	float compensation = prm.v[1];
	float minEv = prm.v[2];
	float maxEv = prm.v[3];
	float ev0 = clamp(ev[int(pc.slots.x)], minEv, maxEv);
	// ponytail: the EV→offset table g() is zero in the live state (its source is open,
	// §1), so Lc = Lmeas·2^−comp.
	float lc = lmeas * exp2(-compensation);
	float target = clamp(ev0 + ADAPT_TARGET_HALF * (log2(key + ADAPT_KEY_FLOOR) - log2(lc + ADAPT_KEY_FLOOR)), minEv, maxEv);
	float delta = target - ev0;
	float step = prm.v[5] * prm.v[4] * min(abs(delta), ADAPT_MAX_STEP) * delta;
	ev[int(pc.slots.y)] = (abs(step) < abs(delta)) ? ev0 + step : target;
}
#endif

#ifdef PASS_COMPOSE
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba16f, set = 0, binding = 0) uniform image2D color;
layout(std430, set = 0, binding = 1) readonly buffer Ev { float ev[2]; };

void main() {
	ivec2 px = ivec2(gl_GlobalInvocationID.xy);
	ivec2 size = imageSize(color);
	if (any(greaterThanEqual(px, size))) return;
	vec4 src = imageLoad(color, px);
	vec3 scene = src.rgb * exp2(ev[int(pc.slots.x)]);
	// The display's linear value is clipped to [0, 1]: highlights are the exposure plus this
	// clip. Godot's final pass sRGB-encodes the colour buffer, so the stored value is linear.
	vec3 display = clamp(sdr_display_mapping(cube_lut_stub(scene)), 0.0, 1.0);
	imageStore(color, px, vec4(display, src.a));
}
#endif
