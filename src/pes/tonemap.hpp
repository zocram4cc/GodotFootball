#pragma once

#include <array>

namespace pes {

// The film curve the tonemap shader evaluates (docs/re/pes-re-lighting.md
// Part C §3). The break points are PES's FilmToneMap parameters; the CPU helper
// that builds them is FUN_14175f430.
struct FilmCurve {
  double slope = 0;
  double toeMatch = 0, straightMatch = 0, shoulderMatch = 0;
  double black = 0, white = 0;
  double toeScale = 0, shoulderScale = 0;
};

// The rig's acesFilm* group (report §4.1) as the shader's curve.
FilmCurve MakeFilmCurve(double slope, double toe, double shoulder, double black, double white);

// Row-major 3x3, applied to linear sRGB column vectors: out = m * in.
using Mat3 = std::array<std::array<double, 3>, 3>;

// PES's white balance (FUN_14175f590): the illuminant chromaticity of the
// rig's whiteTemperature/whiteTint, Bradford-adapted to D65. Returned as the
// matrix PES's composition applies (out = m * in) with the stored live rows as
// its rows; that is the transpose of the Bradford matrix. Daylight locus (Judd)
// at or above 4000 K, Planckian (Krystek) below; the tint offsets the Krystek uv
// point along its radius.
Mat3 WhiteBalanceRgb(double temperatureK, double tint);

// PES's bloom loop (FUN_141804af0): one Kawase pass per kernel, floor(10·size)
// of them, at least one.
int BloomKernelCount(double bloomSize);
// PES's single copy branch for 0.1 <= size < 0.2 (zero offsets, one pass).
bool BloomCopyBranch(double bloomSize);

}  // namespace pes
