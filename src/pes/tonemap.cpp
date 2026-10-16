#include "pes/tonemap.hpp"

#include <algorithm>
#include <cmath>

namespace pes {
namespace {

// Constants are PES's own (.rdata, read from PES2021.exe; VAs in comments).
constexpr double kMidGrey = 0.18;               // 0x1425c4bf0: the film's log pivot
constexpr double kToeLogBranchMax = 0.8;        // 0x142572348: toe at or below takes the log branch
constexpr double kHalf = 0.5;                  // 0x142565600 (also 0x1425658b0, the tint's 0.5)
constexpr double kTemperatureScale = 1.00055635;  // 0x142a21de0: applied to T in the Judd branch
constexpr double kJuddSwitchK = 7000.0;         // 0x1425e98b8: the high Judd polynomial above this
constexpr double kDaylightSwitchK = 4000.0;     // 0x1425cb498: daylight locus at or above, Planckian below
constexpr double kTintScale = 0.05;             // 0x1425a8a28
constexpr double kChromaticityFloor = 1e-10;    // 0x1429ec7e8: floor for y in the XYZ conversion
constexpr double kD65X = 0.312700003;           // 0x142a21dd0
constexpr double kD65Y = 0.328999996;           // 0x142a21dd8
constexpr double kBloomKernelsPerSize = 10.0;   // 0x142588210
constexpr double kBloomCopyMin = 0.1;           // 0x142588200
constexpr double kBloomCopyMax = 0.2;           // 0x142598578

// sRGB (D65) -> XYZ and XYZ -> sRGB (0x1429fb7d0.., 0x142a21e40.., 0x142a21ea0..).
constexpr double kSrgbToXyz[3][3] = {{0.412456393, 0.357576102, 0.180437505},
                                     {0.212672904, 0.715152204, 0.0721750036},
                                     {0.0193339009, 0.119191997, 0.950304091}};
constexpr double kXyzToSrgb[3][3] = {{3.2409699, -1.5373832, -0.498610765},
                                     {-0.969243646, 1.8759675, 0.0415550582},
                                     {0.0556300804, -0.203976959, 1.05697155}};
// Bradford cone response and its inverse (0x142a21e00.. and 0x142a21e20..).
constexpr double kBradford[3][3] = {{0.895099998, 0.266400009, -0.161400005},
                                    {-0.750199974, 1.71350002, 0.0366999991},
                                    {0.0388999991, -0.0684999973, 1.02960002}};
constexpr double kBradfordInv[3][3] = {{0.986992896, -0.1470543, 0.159962699},
                                       {0.432305306, 0.518360317, 0.0492912009},
                                       {-0.0085287001, 0.040042799, 0.968486726}};

// Judd daylight locus x_D and its y(x) (FUN_14175f0d0; coefficients 0x142a21dc8..0x142a21dfc).
void JuddDaylightXy(double temperatureK, double* x, double* y) {
  const double t = temperatureK * kTemperatureScale;
  double xd;
  if (kJuddSwitchK < t)
    xd = 1901800.0 / (t * t) - 2.0064e9 / (t * t * t) + 247.479996 / t + 0.237039998;
  else
    xd = 2967800.0 / (t * t) - 4.60700006e9 / (t * t * t) + 99.1100006 / t + 0.244063005;
  *x = xd;
  *y = 2.86999989 * xd - 3.0 * xd * xd - 0.275000006;  // 0x142a21de4, 0x14258c264, 0x1425e4a24
}

// Krystek's Planckian uv (FUN_14175f340; coefficients 0x142a21d80..0x142a21ddc).
void KrystekUv(double temperatureK, double* u, double* v) {
  const double t = temperatureK;
  *u = (1.28641219e-07 * t * t + 1.54118257e-04 * t + 0.860117733) /
       (7.08145137e-07 * t * t + 8.4242021e-04 * t + 1.0);
  *v = (4.20481676e-08 * t * t + 4.22806261e-05 * t + 0.317398727) /
       ((1.0 - t * 2.8974182e-05) + 1.61456057e-07 * t * t);
}

// CIE 1960 uv to CIE 1931 xy (3u, 2v over 2u − 8v + 4).
void UvToXy(double u, double v, double* x, double* y) {
  const double den = (u + u) - v * 8.0 + 4.0;  // 0x14259bdf4 = 8, 0x1425656c0 = 4
  *x = 3.0 * u / den;                          // 0x14258c264 = 3
  *y = (v + v) / den;
}

void KrystekXy(double temperatureK, double* x, double* y) {
  double u, v;
  KrystekUv(temperatureK, &u, &v);
  UvToXy(u, v, x, y);
}

// The tint offset (FUN_14175f190): the Krystek uv point moved by tint·0.05
// along its radius's perpendicular, i.e. by the rotated unit vector (−v, u)/|uv|.
void TintXy(double temperatureK, double tint, double* x, double* y) {
  double u, v;
  KrystekUv(temperatureK, &u, &v);
  double cu = 0.0, sv = 0.0;
  if (u != 0.0 || v != 0.0) {
    const double r2 = u * u + v * v;
    const double rinv = 1.0 / std::sqrt(r2);
    cu = (3.0 - r2 * rinv * rinv) * kHalf * rinv * u;  // Newton-refined u/|uv|
    sv = (3.0 - r2 * rinv * rinv) * kHalf * rinv * v;  // v/|uv|
  }
  const double v2 = cu * tint * kTintScale + v;
  const double u2 = (0.0 - sv) * tint * kTintScale + u;
  UvToXy(u2, v2, x, y);
}

// Bradford chromatic adaptation from white point src to dst (FUN_14175eaf0):
// C = Binv · diag(R) · B with R_k = (B_k·XYZ_dst)/(B_k·XYZ_src), so C·XYZ_src = XYZ_dst.
void BradfordCat(double sx, double sy, double dx, double dy, double c[3][3]) {
  auto xyz = [](double x, double y, double out[3]) {
    const double yy = std::max(y, kChromaticityFloor);
    out[0] = x / yy;
    out[1] = 1.0;
    out[2] = (1.0 - x - y) / yy;
  };
  double s[3], d[3];
  xyz(sx, sy, s);
  xyz(dx, dy, d);
  double r[3];
  for (int k = 0; k < 3; ++k) {
    double num = 0.0, den = 0.0;
    for (int j = 0; j < 3; ++j) {
      num += kBradford[k][j] * d[j];
      den += kBradford[k][j] * s[j];
    }
    r[k] = num / den;
  }
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      c[i][j] = 0.0;
      for (int k = 0; k < 3; ++k) c[i][j] += kBradfordInv[i][k] * r[k] * kBradford[k][j];
    }
}

}  // namespace

FilmCurve MakeFilmCurve(double slope, double toe, double shoulder, double black, double white) {
  FilmCurve f;
  f.slope = slope;
  f.black = black;
  f.white = white;
  f.toeScale = (black + 1.0) - toe;
  f.shoulderScale = (white + 1.0) - shoulder;
  const double logMid = std::log10(kMidGrey);
  if (toe <= kToeLogBranchMax) {
    const double x = (black + kMidGrey) / f.toeScale - 1.0;
    f.toeMatch = logMid - std::log((x + 1.0) / (1.0 - x)) * kHalf * (f.toeScale / slope);
  } else {
    f.toeMatch = logMid + ((1.0 - toe) - kMidGrey) / slope;
  }
  f.straightMatch = (1.0 - toe) / slope - f.toeMatch;
  f.shoulderMatch = shoulder / slope - f.straightMatch;
  return f;
}

Mat3 WhiteBalanceRgb(double temperatureK, double tint) {
  double jx, jy, kx, ky;
  JuddDaylightXy(temperatureK, &jx, &jy);
  KrystekXy(temperatureK, &kx, &ky);
  // The source illuminant: the daylight locus (or Planckian below 4000 K),
  // shifted by the tint's offset relative to the Planckian point.
  double sx = kx, sy = ky;
  if (kDaylightSwitchK <= temperatureK) {
    sx = jx;
    sy = jy;
  }
  double tx, ty;
  TintXy(temperatureK, tint, &tx, &ty);
  const double srcX = sx + (tx - kx);
  const double srcY = sy + (ty - ky);
  double c[3][3];
  BradfordCat(srcX, srcY, kD65X, kD65Y, c);
  // m = XYZ->sRGB · C · sRGB->XYZ (column convention).
  Mat3 m{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      double acc = 0.0;
      for (int k = 0; k < 3; ++k) {
        double cs = 0.0;
        for (int l = 0; l < 3; ++l) cs += c[k][l] * kSrgbToXyz[l][j];
        acc += kXyzToSrgb[i][k] * cs;
      }
      m[i][j] = acc;
    }
  return m;
}

int BloomKernelCount(double bloomSize) {
  return std::max(1, static_cast<int>(std::floor(kBloomKernelsPerSize * bloomSize)));
}

bool BloomCopyBranch(double bloomSize) {
  return kBloomCopyMin <= bloomSize && bloomSize < kBloomCopyMax;
}

}  // namespace pes
