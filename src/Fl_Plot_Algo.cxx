//
// Fl_Plot_Algo.cxx -- FLTK-independent algorithms used by Fl_Plot.
//

#include "Fl_Plot_Algo.H"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <limits>

namespace Fl_Plot_Algo {

static inline bool is_finite(double v) { return v - v == 0; }

Pt break_pt() {
  Pt p;
  p.x = p.y = std::numeric_limits<double>::quiet_NaN();
  return p;
}

static inline void push_break(std::vector<Pt> &out) {
  if (!out.empty() && !is_break(out.back())) out.push_back(break_pt());
}

static inline void push_pt(std::vector<Pt> &out, double x, double y) {
  Pt p; p.x = x; p.y = y;
  out.push_back(p);
}

// ---------------------------------------------------------------------------
// Ticks
// ---------------------------------------------------------------------------

double nice_step(double range, int max_ticks) {
  if (max_ticks < 1) max_ticks = 1;
  if (!(range > 0) || !is_finite(range)) return 1.0;
  double raw = range / max_ticks;
  double mag = pow(10.0, floor(log10(raw)));
  double r = raw / mag;
  static const double steps[] = { 1.0, 2.0, 2.5, 5.0, 10.0 };
  for (int i = 0; i < 5; i++)
    if (r <= steps[i] * (1 + 1e-9)) return steps[i] * mag;
  return 10.0 * mag;
}

static void clear_ticks(Ticks &t) {
  t.major.clear();
  t.minor.clear();
  t.step = 1; t.minor_step = 0.2; t.decimals = 0; t.exponent = 0;
  t.offset = 0; t.decades = false;
}

// Decimals needed to print every value of v / 10^exponent exactly.
static int label_decimals(const std::vector<double> &v, double offset, int exponent) {
  int d = 0;
  for (size_t i = 0; i < v.size(); i++) {
    double s = (v[i] - offset) / pow(10.0, exponent);
    for (; d < 15; d++) {
      double r = s * pow(10.0, d);
      if (fabs(r - floor(r + 0.5)) < 1e-6 * std::max(1.0, fabs(r))) break;
    }
  }
  return d;
}

// Exponent for labels whose largest magnitude is maxabs (0 = plain).
static int label_exponent(double maxabs) {
  if (!(maxabs > 0)) return 0;
  int e = (int)floor(log10(maxabs) + 1e-9);
  return (e >= 5 || e <= -4) ? e : 0;
}

// Linear ticks without an offset (the plain algorithm).
static void linear_ticks(double lo, double hi, int max_ticks, Ticks &t) {
  clear_ticks(t);
  double step = nice_step(hi - lo, max_ticks);
  t.step = step;
  const double eps = 1e-9;
  double k0 = ceil(lo / step - eps), k1 = floor(hi / step + eps);
  // a range that overflows (hi - lo = inf) or a max_ticks far too large
  // would otherwise produce an unbounded number of ticks
  if (!is_finite(hi - lo) || !(k1 - k0 < 10000)) {
    t.step = 1;
    return;
  }
  // count with an integer: past 2^53, k += 1 would no longer change k
  long long nk = (long long)(k1 - k0);
  for (long long i = 0; i <= nk; i++) {
    double v = (k0 + (double)i) * step;
    if (fabs(v) < step * 1e-9) v = 0;
    if (!t.major.empty() && v <= t.major.back()) continue;   // beyond double precision
    t.major.push_back(v);
  }

  double mant = step / pow(10.0, floor(log10(step) + eps));
  int ndiv = (fabs(mant - 2.0) < 1e-6) ? 4 : 5;
  double ms = step / ndiv;
  t.minor_step = ms;
  if ((hi - lo) / ms < 5000) {
    double m0 = ceil(lo / ms - eps), m1 = floor(hi / ms + eps);
    long long nm = (long long)(m1 - m0);
    for (long long i = 0; i <= nm; i++) {
      double k = m0 + (double)i;
      if (fmod(k, (double)ndiv) == 0) continue;
      double v = k * ms;
      if (!t.minor.empty() && v <= t.minor.back()) continue;
      t.minor.push_back(v);
    }
  }

  // scale labels by 10^e for very large / small magnitudes
  t.exponent = label_exponent(std::max(fabs(lo), fabs(hi)));
  double s = step / pow(10.0, t.exponent);
  int d = 0;
  for (; d < 15; d++) {
    double r = s * pow(10.0, d);
    if (fabs(r - floor(r + 0.5)) < 1e-6 * std::max(1.0, r)) break;
  }
  t.decimals = d;
}

void compute_ticks(double lo, double hi, int max_ticks, Ticks &t) {
  if (lo > hi) std::swap(lo, hi);
  if (!(hi > lo) || !is_finite(lo) || !is_finite(hi) || !is_finite(hi - lo)) {
    clear_ticks(t);
    return;
  }
  // When the labels would share 4 or more leading digits (e.g. [1e9, 1e9+1]),
  // label the distance from an offset instead. The offset is the roundest
  // number (largest power-of-ten multiple) that keeps every label within ten
  // times the range. That power of ten is larger than the range, so the
  // offset is a multiple of the tick step and the ticks stay on round values.
  double span = hi - lo, maxabs = std::max(fabs(lo), fabs(hi));
  if (maxabs >= 1e4 * span) {
    double pmin = pow(10.0, floor(log10(span)) + 1), limit = 10 * pmin;
    double mid = lo + span / 2, off = 0;
    for (double p = pow(10.0, floor(log10(maxabs))); ; p /= 10) {
      off = floor(mid / p + 0.5) * p;
      if ((fabs(lo - off) <= limit && fabs(hi - off) <= limit) || p <= pmin) break;
    }
    // compute in offset space, where lo - off and hi - off are exact
    linear_ticks(lo - off, hi - off, max_ticks, t);
    for (size_t i = 0; i < t.major.size(); i++) t.major[i] += off;
    for (size_t i = 0; i < t.minor.size(); i++) t.minor[i] += off;
    t.offset = off;
    return;
  }
  linear_ticks(lo, hi, max_ticks, t);
}

void compute_log_ticks(double lo, double hi, int max_ticks, Ticks &t) {
  if (lo > hi) std::swap(lo, hi);
  clear_ticks(t);
  if (!(lo > 0) || !(hi > lo) || !is_finite(lo) || !is_finite(hi)) return;
  if (max_ticks < 1) max_ticks = 1;
  const double eps = 1e-9;
  double la = log10(lo), lb = log10(hi), D = lb - la;
  double gap = D / max_ticks;          // smallest label spacing, in decades
  double k0 = ceil(la - eps), k1 = floor(lb + eps);

  if (k1 - k0 >= 1) {
    // two or more powers of ten in view: label decades, every s-th one
    // labels like 10^3 are short: let them come a little closer than
    // linear labels before skipping decades
    double s = 1;   // runs 1, 2, 5, 10, 20, 50, ...
    for (double m = 1; s < 0.7 * gap; ) {
      double r = s / m;
      if (r == 1) s = 2 * m; else if (r == 2) s = 5 * m; else { m *= 10; s = m; }
    }
    t.decades = true;
    t.step = s;
    double m0 = ceil(la / s - eps) * s;
    for (double k = m0; k <= lb + eps; k += s) t.major.push_back(pow(10.0, k));
    double ms = s < 10 ? 1 : s / 5;
    t.minor_step = ms;
    if (s == 1) {
      // 2..9 x 10^k
      for (double k = floor(la); k <= lb; k++)
        for (int m = 2; m <= 9; m++) {
          double v = m * pow(10.0, k);
          if (v >= lo * (1 - eps) && v <= hi * (1 + eps)) t.minor.push_back(v);
        }
    } else if (D / ms < 2000) {
      // the decades in between (every few of them, for very long ranges)
      for (double k = ceil(la / ms - eps) * ms; k <= lb + eps; k += ms)
        if (fmod(k, s) != 0) t.minor.push_back(pow(10.0, k));
    }
    return;
  }

  // Less than two decades: 1-2-5 ticks, or linear "nice" ticks, whichever
  // gives more labels while keeping them at least `gap` decades apart.
  std::vector<double> b;
  for (double k = floor(la); k <= lb; k++)
    for (int m = 1; m <= 5; m += (m == 1 ? 1 : 3)) {   // 1, 2, 5
      double v = m * pow(10.0, k);
      if (v >= lo * (1 - eps) && v <= hi * (1 + eps)) b.push_back(v);
    }
  // linear ticks: the gap is smallest at the top, so pick the step there
  double need = hi * (1 - pow(10.0, -gap));
  int lin_max = (int)std::min(1000.0, std::max(1.0, floor((hi - lo) / need)));
  compute_ticks(lo, hi, lin_max, t);
  if (b.size() >= 2 && b.size() >= t.major.size()) {
    std::vector<double> minor;
    for (double k = floor(la); k <= lb; k++)
      for (int m = 1; m <= 9; m++) {
        double v = m * pow(10.0, k);
        if (v < lo * (1 - eps) || v > hi * (1 + eps)) continue;
        if (std::find(b.begin(), b.end(), v) == b.end()) minor.push_back(v);
      }
    clear_ticks(t);
    t.major.swap(b);
    t.minor.swap(minor);
    t.exponent = label_exponent(hi);
    t.decimals = label_decimals(t.major, 0, t.exponent);
  }
}

std::string format_tick(double v, const Ticks &t) {
  char buf[64];
  if (t.decades) {
    snprintf(buf, sizeof(buf), "$10^{%d}$", (int)floor(log10(v) + 0.5));
    return buf;
  }
  double s = (v - t.offset) / pow(10.0, t.exponent);
  snprintf(buf, sizeof(buf), "%.*f", t.decimals, s);
  // "-0.00" -> "0.00"
  bool allzero = true;
  for (const char *p = buf; *p; p++)
    if (*p >= '1' && *p <= '9') { allzero = false; break; }
  const char *p = buf;
  if (allzero && *p == '-') p++;
  std::string out;
  if (*p == '-') { out = "\xE2\x88\x92"; p++; }   // U+2212 MINUS SIGN
  out += p;
  return out;
}

// v with `digits` decimals, trailing zeros removed.
static std::string trim_fixed(double v, int digits) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%.*f", std::max(0, std::min(40, digits)), v);
  std::string s = buf;
  if (s.find('.') != std::string::npos) {
    while (!s.empty() && s[s.size() - 1] == '0') s.erase(s.size() - 1);
    if (!s.empty() && s[s.size() - 1] == '.') s.erase(s.size() - 1);
  }
  return s;
}

std::string format_multiplier(const Ticks &t) {
  std::string out;
  char buf[64];
  if (t.exponent && !t.decades) {
    snprintf(buf, sizeof(buf), "\\times10^{%d}", t.exponent);
    out = buf;
  }
  if (t.offset != 0 && is_finite(t.offset)) {
    double a = fabs(t.offset);
    // the offset is a multiple of a power of ten no smaller than the step,
    // so digits below 10^q are zeros (trimmed if we print them anyway)
    int q = (int)floor(log10(t.step) + 1e-9);
    int e = (int)floor(log10(a) + 1e-9);
    // plain (101325, 0.0012) or scientific (1.73x10^9), whichever is
    // shorter; "x10^9" counts as about 4 characters
    std::string num = trim_fixed(a, -q);
    std::string mant = trim_fixed(a / pow(10.0, e), e - q);
    if (e <= -5 || mant.size() + 4 < num.size()) {
      snprintf(buf, sizeof(buf), "\\times10^{%d}", e);
      num = mant + buf;
    }
    if (!out.empty()) out += "\\;";
    out += t.offset < 0 ? "\xE2\x88\x92" : "+";
    out += num;
  }
  return out.empty() ? out : "$" + out + "$";
}

std::string format_exact(double v, int min_digits) {
  char buf[64];
  for (int d = std::max(1, min_digits); d <= 17; d++) {
    snprintf(buf, sizeof(buf), "%.*g", d, v);
    if (strtod(buf, 0) == v) break;
  }
  return buf;
}

std::string format_coord(double v, double resolution) {
  char buf[64];
  int d = 6;
  resolution = fabs(resolution);
  if (resolution > 0 && is_finite(resolution) && fabs(v) > resolution)
    d = std::max(6, std::min(17, (int)ceil(log10(fabs(v) / resolution)) + 1));
  snprintf(buf, sizeof(buf), "%.*g", d, v);
  return buf;
}

// ---------------------------------------------------------------------------
// Clipping
// ---------------------------------------------------------------------------

bool clip_segment(double &ax, double &ay, double &bx, double &by, const Rect &r) {
  double t0 = 0, t1 = 1;
  double dx = bx - ax, dy = by - ay;
  double p[4] = { -dx, dx, -dy, dy };
  double q[4] = { ax - r.x0, r.x1 - ax, ay - r.y0, r.y1 - ay };
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0) {
      if (q[i] < 0) return false;
    } else {
      double t = q[i] / p[i];
      if (p[i] < 0) { if (t > t1) return false; if (t > t0) t0 = t; }
      else          { if (t < t0) return false; if (t < t1) t1 = t; }
    }
  }
  double nax = ax + t0 * dx, nay = ay + t0 * dy;
  double nbx = ax + t1 * dx, nby = ay + t1 * dy;
  ax = nax; ay = nay; bx = nbx; by = nby;
  return true;
}

void clip_polyline(const std::vector<Pt> &in, const Rect &r, std::vector<Pt> &out) {
  out.clear();
  for (size_t i = 1; i < in.size(); i++) {
    const Pt &a = in[i - 1], &b = in[i];
    if (is_break(a) || is_break(b)) continue;
    double ax = a.x, ay = a.y, bx = b.x, by = b.y;
    if (!clip_segment(ax, ay, bx, by, r)) { push_break(out); continue; }
    bool joined = !out.empty() && !is_break(out.back()) &&
                  out.back().x == ax && out.back().y == ay;
    if (!joined) {
      push_break(out);
      push_pt(out, ax, ay);
    }
    push_pt(out, bx, by);
    if (bx != b.x || by != b.y) push_break(out);
  }
}

// One Sutherland-Hodgman pass against the half-plane given by edge/side.
static void clip_edge(const std::vector<Pt> &in, std::vector<Pt> &out,
                      int axis, double v, bool keep_greater) {
  out.clear();
  size_t n = in.size();
  if (!n) return;
  for (size_t i = 0; i < n; i++) {
    const Pt &a = in[i], &b = in[(i + 1) % n];
    double ca = axis ? a.y : a.x, cb = axis ? b.y : b.x;
    bool ina = keep_greater ? ca >= v : ca <= v;
    bool inb = keep_greater ? cb >= v : cb <= v;
    if (ina) out.push_back(a);
    if (ina != inb) {
      double t = (v - ca) / (cb - ca);
      Pt p;
      p.x = a.x + t * (b.x - a.x);
      p.y = a.y + t * (b.y - a.y);
      if (axis) p.y = v; else p.x = v;
      out.push_back(p);
    }
  }
}

void clip_polygon(const std::vector<Pt> &in, const Rect &r, std::vector<Pt> &out) {
  std::vector<Pt> tmp;
  clip_edge(in, out, 0, r.x0, true);
  clip_edge(out, tmp, 0, r.x1, false);
  clip_edge(tmp, out, 1, r.y0, true);
  clip_edge(out, tmp, 1, r.y1, false);
  out.swap(tmp);
}

// ---------------------------------------------------------------------------
// MinMax
// ---------------------------------------------------------------------------

void MinMax::build(const double *data, size_t n) {
  data_ = data;
  n_ = n;
  nb_ = (n + BLOCK - 1) / BLOCK;
  leaves_ = 1;
  while (leaves_ < nb_) leaves_ <<= 1;
  const double inf = std::numeric_limits<double>::infinity();
  mn_.assign(2 * leaves_, inf);
  mx_.assign(2 * leaves_, -inf);
  for (size_t b = 0; b < nb_; b++) {
    double lo = inf, hi = -inf;
    size_t e = std::min(n, (b + 1) * BLOCK);
    for (size_t i = b * BLOCK; i < e; i++) {
      double v = data[i];
      if (v < lo) lo = v;        // NaN compares false: ignored
      if (v > hi) hi = v;
    }
    mn_[leaves_ + b] = lo;
    mx_[leaves_ + b] = hi;
  }
  for (size_t k = leaves_ - 1; k >= 1; k--) {
    mn_[k] = std::min(mn_[2 * k], mn_[2 * k + 1]);
    mx_[k] = std::max(mx_[2 * k], mx_[2 * k + 1]);
  }
}

void MinMax::scan(size_t i, size_t j, double &mn, double &mx) const {
  for (; i < j; i++) {
    double v = data_[i];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
  }
}

bool MinMax::query(size_t i, size_t j, double &mn, double &mx) const {
  const double inf = std::numeric_limits<double>::infinity();
  mn = inf; mx = -inf;
  if (j > n_) j = n_;
  if (i >= j) return false;
  size_t bi = (i + BLOCK - 1) / BLOCK;   // first full block
  size_t bj = j / BLOCK;                 // one past last full block
  if (bi >= bj) {
    scan(i, j, mn, mx);
  } else {
    scan(i, bi * BLOCK, mn, mx);
    scan(bj * BLOCK, j, mn, mx);
    size_t l = bi + leaves_, r = bj + leaves_;
    while (l < r) {
      if (l & 1) { mn = std::min(mn, mn_[l]); mx = std::max(mx, mx_[l]); l++; }
      if (r & 1) { r--; mn = std::min(mn, mn_[r]); mx = std::max(mx, mx_[r]); }
      l >>= 1; r >>= 1;
    }
  }
  return mn <= mx;
}

// ---------------------------------------------------------------------------
// Decimation
// ---------------------------------------------------------------------------

bool is_sorted_finite(const double *x, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (!is_finite(x[i])) return false;
    if (i && x[i] < x[i - 1]) return false;
  }
  return true;
}

void visible_range(const double *x, size_t n, double xlo, double xhi,
                   size_t &i0, size_t &i1) {
  if (xlo > xhi) std::swap(xlo, xhi);
  i0 = std::lower_bound(x, x + n, xlo) - x;
  i1 = std::upper_bound(x, x + n, xhi) - x;
  if (i0 > 0) i0--;
  if (i1 < n) i1++;
}

static inline void push_data_pt(std::vector<Pt> &out, const Xform &xf,
                                double x, double y) {
  if (!is_finite(y)) push_break(out);
  else push_pt(out, xf.data_sx(x), xf.data_sy(y));
}

void decimate_sorted(const double *x, const double *y, size_t n,
                     const MinMax &mm, const Xform &xf, std::vector<Pt> &out) {
  out.clear();
  if (!n) return;
  size_t i0, i1;
  visible_range(x, n, xf.data_x0(), xf.data_x1(), i0, i1);
  if (!mm.built() || i1 - i0 <= (size_t)(4 * xf.pw) + 8) {
    for (size_t i = i0; i < i1; i++) push_data_pt(out, xf, x[i], y[i]);
    return;
  }
  size_t i = i0;
  while (i < i1) {
    double col = floor(xf.data_sx(x[i]));
    double xe = xf.data_dx(col + 1);
    size_t j = std::lower_bound(x + i + 1, x + i1, xe) - x;
    if (j <= i) j = i + 1;
    if (j - i <= 4) {
      for (size_t k = i; k < j; k++) push_data_pt(out, xf, x[k], y[k]);
    } else {
      double mn, mx;
      if (!mm.query(i, j, mn, mx)) {
        push_break(out);
      } else {
        double cx = col + 0.5;
        // log y is monotonic, so the column extremes map to the screen extremes
        if (is_finite(y[i])) push_pt(out, xf.data_sx(x[i]), xf.data_sy(y[i]));
        push_pt(out, cx, xf.data_sy(mn));
        push_pt(out, cx, xf.data_sy(mx));
        if (is_finite(y[j - 1])) push_pt(out, xf.data_sx(x[j - 1]), xf.data_sy(y[j - 1]));
      }
    }
    i = j;
  }
}

void decimate_unsorted(const double *x, const double *y, size_t n,
                       const Xform &xf, std::vector<Pt> &out) {
  out.clear();
  double lx = 0, ly = 0;
  bool have = false;
  for (size_t i = 0; i < n; i++) {
    if (!is_finite(x[i]) || !is_finite(y[i])) { push_break(out); have = false; continue; }
    double sx = xf.data_sx(x[i]), sy = xf.data_sy(y[i]);
    double rx = floor(sx), ry = floor(sy);
    if (have && rx == lx && ry == ly) continue;
    push_pt(out, sx, sy);
    lx = rx; ly = ry; have = true;
  }
}

void marker_points(const double *x, const double *y, size_t i0, size_t i1,
                   const Xform &xf, const Rect &r, std::vector<Pt> &out) {
  out.clear();
  int gx0 = (int)floor(r.x0), gy0 = (int)floor(r.y0);
  int gw = (int)ceil(r.x1) - gx0 + 1, gh = (int)ceil(r.y1) - gy0 + 1;
  if (gw <= 0 || gh <= 0) return;
  std::vector<unsigned char> seen((size_t)gw * gh, 0);
  for (size_t i = i0; i < i1; i++) {
    if (!is_finite(x[i]) || !is_finite(y[i])) continue;
    double sx = xf.data_sx(x[i]), sy = xf.data_sy(y[i]);
    if (!(sx >= r.x0 && sx <= r.x1 && sy >= r.y0 && sy <= r.y1)) continue;
    size_t cell = (size_t)((int)floor(sy) - gy0) * gw + ((int)floor(sx) - gx0);
    if (seen[cell]) continue;
    seen[cell] = 1;
    push_pt(out, sx, sy);
  }
}

// Fill between two points (a, b) of the two curves; split at a crossing.
static void fill_segment(double xa, double y1a, double y2a,
                         double xb, double y1b, double y2b,
                         const Xform &xf, std::vector<Pt> &polys) {
  if (!is_finite(y1a) || !is_finite(y2a) || !is_finite(y1b) || !is_finite(y2b)) return;
  // work in screen space, where the edges are the straight lines that get
  // drawn (on a log axis they are not straight in data space)
  double sxa = xf.data_sx(xa), sxb = xf.data_sx(xb);
  double s1a = xf.data_sy(y1a), s2a = xf.data_sy(y2a);
  double s1b = xf.data_sy(y1b), s2b = xf.data_sy(y2b);
  double d0 = s1a - s2a, d1 = s1b - s2b;
  if ((d0 < 0 && d1 > 0) || (d0 > 0 && d1 < 0)) {
    double t = d0 / (d0 - d1);
    double xm = sxa + t * (sxb - sxa), ym = s1a + t * (s1b - s1a);
    push_pt(polys, sxa, s1a);
    push_pt(polys, xm, ym);
    push_pt(polys, sxa, s2a);
    polys.push_back(break_pt());
    push_pt(polys, xm, ym);
    push_pt(polys, sxb, s1b);
    push_pt(polys, sxb, s2b);
    polys.push_back(break_pt());
  } else {
    push_pt(polys, sxa, s1a);
    push_pt(polys, sxb, s1b);
    push_pt(polys, sxb, s2b);
    push_pt(polys, sxa, s2a);
    polys.push_back(break_pt());
  }
}

void fill_between_sorted(const double *x, size_t n, const MinMax &mlo, const MinMax &mhi,
                         const double *y1, const double *y2,
                         const Xform &xf, std::vector<Pt> &polys) {
  polys.clear();
  if (n < 2) return;
  size_t i0, i1;
  visible_range(x, n, xf.data_x0(), xf.data_x1(), i0, i1);
  bool dense_ok = mlo.built() && mhi.built();
  size_t i = i0;
  bool have_prev = false;
  size_t prev = 0;
  while (i < i1) {
    size_t j = i + 1;
    if (dense_ok) {
      double col = floor(xf.data_sx(x[i]));
      double xe = xf.data_dx(col + 1);
      j = std::lower_bound(x + i + 1, x + i1, xe) - x;
      if (j <= i) j = i + 1;
    }
    if (have_prev)
      fill_segment(x[prev], y1[prev], y2[prev], x[i], y1[i], y2[i], xf, polys);
    if (j - i <= 4) {
      for (size_t k = i + 1; k < j; k++)
        fill_segment(x[k - 1], y1[k - 1], y2[k - 1], x[k], y1[k], y2[k], xf, polys);
    } else {
      double lo, hi, dummy;
      if (mlo.query(i, j, lo, dummy) && mhi.query(i, j, dummy, hi)) {
        double sx0 = xf.data_sx(x[i]), sx1 = std::max(xf.data_sx(x[j - 1]), sx0 + 1);
        double top = xf.data_sy(hi), bot = xf.data_sy(lo);
        push_pt(polys, sx0, top);
        push_pt(polys, sx1, top);
        push_pt(polys, sx1, bot);
        push_pt(polys, sx0, bot);
        polys.push_back(break_pt());
      }
    }
    prev = j - 1;
    have_prev = true;
    i = j;
  }
}

} // namespace Fl_Plot_Algo
