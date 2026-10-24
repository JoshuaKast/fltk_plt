//
// Fl_Plot_Algo.cxx -- FLTK-independent algorithms used by Fl_Plot.
//

#include "Fl_Plot_Algo.H"

#include <math.h>
#include <stdio.h>
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

void compute_ticks(double lo, double hi, int max_ticks, Ticks &t) {
  t.major.clear();
  t.minor.clear();
  if (lo > hi) std::swap(lo, hi);
  if (!(hi > lo) || !is_finite(lo) || !is_finite(hi)) {
    t.step = 1; t.minor_step = 0.2; t.decimals = 0; t.exponent = 0;
    return;
  }
  double step = nice_step(hi - lo, max_ticks);
  t.step = step;
  const double eps = 1e-9;
  double k0 = ceil(lo / step - eps), k1 = floor(hi / step + eps);
  for (double k = k0; k <= k1; k += 1) {
    double v = k * step;
    if (fabs(v) < step * 1e-9) v = 0;
    t.major.push_back(v);
  }

  double mant = step / pow(10.0, floor(log10(step) + eps));
  int ndiv = (fabs(mant - 2.0) < 1e-6) ? 4 : 5;
  double ms = step / ndiv;
  t.minor_step = ms;
  if ((hi - lo) / ms < 5000) {
    double m0 = ceil(lo / ms - eps), m1 = floor(hi / ms + eps);
    for (double k = m0; k <= m1; k += 1) {
      long long ki = (long long)llround(k);
      if (ki % ndiv == 0) continue;
      t.minor.push_back(k * ms);
    }
  }

  // scale labels by 10^e for very large / small magnitudes
  double maxabs = std::max(fabs(lo), fabs(hi));
  int e = (int)floor(log10(maxabs) + eps);
  t.exponent = (e >= 5 || e <= -4) ? e : 0;

  double s = step / pow(10.0, t.exponent);
  int d = 0;
  for (; d < 15; d++) {
    double r = s * pow(10.0, d);
    if (fabs(r - floor(r + 0.5)) < 1e-6 * std::max(1.0, r)) break;
  }
  t.decimals = d;
}

std::string format_tick(double v, const Ticks &t) {
  char buf[64];
  double s = v / pow(10.0, t.exponent);
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
  else push_pt(out, xf.sx(x), xf.sy(y));
}

void decimate_sorted(const double *x, const double *y, size_t n,
                     const MinMax &mm, const Xform &xf, std::vector<Pt> &out) {
  out.clear();
  if (!n) return;
  size_t i0, i1;
  visible_range(x, n, xf.x0, xf.x1, i0, i1);
  if (!mm.built() || i1 - i0 <= (size_t)(4 * xf.pw) + 8) {
    for (size_t i = i0; i < i1; i++) push_data_pt(out, xf, x[i], y[i]);
    return;
  }
  size_t i = i0;
  while (i < i1) {
    double col = floor(xf.sx(x[i]));
    double xe = xf.dx(col + 1);
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
        if (is_finite(y[i])) push_pt(out, xf.sx(x[i]), xf.sy(y[i]));
        push_pt(out, cx, xf.sy(mn));
        push_pt(out, cx, xf.sy(mx));
        if (is_finite(y[j - 1])) push_pt(out, xf.sx(x[j - 1]), xf.sy(y[j - 1]));
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
    double sx = xf.sx(x[i]), sy = xf.sy(y[i]);
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
    double sx = xf.sx(x[i]), sy = xf.sy(y[i]);
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
  double d0 = y1a - y2a, d1 = y1b - y2b;
  if ((d0 < 0 && d1 > 0) || (d0 > 0 && d1 < 0)) {
    double t = d0 / (d0 - d1);
    double xm = xa + t * (xb - xa), ym = y1a + t * (y1b - y1a);
    push_pt(polys, xf.sx(xa), xf.sy(y1a));
    push_pt(polys, xf.sx(xm), xf.sy(ym));
    push_pt(polys, xf.sx(xa), xf.sy(y2a));
    polys.push_back(break_pt());
    push_pt(polys, xf.sx(xm), xf.sy(ym));
    push_pt(polys, xf.sx(xb), xf.sy(y1b));
    push_pt(polys, xf.sx(xb), xf.sy(y2b));
    polys.push_back(break_pt());
  } else {
    push_pt(polys, xf.sx(xa), xf.sy(y1a));
    push_pt(polys, xf.sx(xb), xf.sy(y1b));
    push_pt(polys, xf.sx(xb), xf.sy(y2b));
    push_pt(polys, xf.sx(xa), xf.sy(y2a));
    polys.push_back(break_pt());
  }
}

void fill_between_sorted(const double *x, size_t n, const MinMax &mlo, const MinMax &mhi,
                         const double *y1, const double *y2,
                         const Xform &xf, std::vector<Pt> &polys) {
  polys.clear();
  if (n < 2) return;
  size_t i0, i1;
  visible_range(x, n, xf.x0, xf.x1, i0, i1);
  bool dense_ok = mlo.built() && mhi.built();
  size_t i = i0;
  bool have_prev = false;
  size_t prev = 0;
  while (i < i1) {
    size_t j = i + 1;
    if (dense_ok) {
      double col = floor(xf.sx(x[i]));
      double xe = xf.dx(col + 1);
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
        double sx0 = xf.sx(x[i]), sx1 = std::max(xf.sx(x[j - 1]), sx0 + 1);
        double top = xf.sy(hi), bot = xf.sy(lo);
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
