// Headless tests for Fl_Plot_Algo (no FLTK needed).

#include "Fl_Plot_Algo.H"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <vector>

using namespace Fl_Plot_Algo;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static bool near(double a, double b, double eps = 1e-9) { return fabs(a - b) <= eps * (1 + fabs(a) + fabs(b)); }

static void test_ticks() {
  Ticks t;
  compute_ticks(0, 10, 5, t);
  CHECK(near(t.step, 2));
  CHECK(t.major.size() == 6);
  CHECK(t.minor.size() == 15);          // 4 subdivisions of a 2-step
  CHECK(format_tick(4, t) == "4");

  compute_ticks(-1, 1, 4, t);
  CHECK(near(t.step, 0.5));
  CHECK(t.decimals == 1);
  CHECK(format_tick(0, t) == "0.0");
  CHECK(format_tick(-0.5, t) == "\xE2\x88\x92" "0.5");

  compute_ticks(0, 10, 4, t);
  CHECK(near(t.step, 2.5));
  CHECK(t.decimals == 1);

  compute_ticks(0, 3e6, 5, t);
  CHECK(t.exponent == 6);
  CHECK(format_tick(1e6, t) == "1.0" || format_tick(1e6, t) == "1");

  compute_ticks(1e-6, 5e-6, 4, t);
  CHECK(t.exponent == -6);

  // a tick exactly at zero is printed as zero, not -0 or 1e-17
  compute_ticks(-0.3, 0.3, 6, t);
  bool has0 = false;
  for (size_t i = 0; i < t.major.size(); i++) if (t.major[i] == 0) has0 = true;
  CHECK(has0);
}

// ranges whose span overflows must not produce an unbounded tick list
static void test_ticks_extreme() {
  Ticks t;
  compute_ticks(-1e308, 1e308, 8, t);
  CHECK(t.major.size() < 10000);
  compute_ticks(0, 1, 1 << 30, t);
  CHECK(t.major.size() <= 10001);
  compute_ticks(1e15, 1e15 + 1, 8, t);
  CHECK(t.major.size() >= 2 && t.major.size() < 20);
}

// a small range far from zero: labels show the distance from a round offset
static void test_ticks_offset() {
  Ticks t;
  compute_ticks(1.7e9 - 1e-4, 1.7e9 + 1.1e-3, 4, t);
  CHECK(t.offset == 1.7e9);
  CHECK(near(t.step, 5e-4));
  CHECK(t.major.size() == 3);
  CHECK(format_tick(t.major[0], t) == "0.0000");
  CHECK(format_tick(t.major[1], t) == "0.0005");
  CHECK(format_multiplier(t) == "$+1.7\\times10^{9}$");
  // the ticks are on round absolute values
  for (size_t i = 0; i < t.major.size(); i++)
    CHECK(fabs((t.major[i] - 1.7e9) / 5e-4 - floor((t.major[i] - 1.7e9) / 5e-4 + 0.5)) < 1e-3);

  // negative values, labels straddling the offset
  compute_ticks(-12345.6791, -12345.669, 5, t);
  CHECK(t.offset == -12346);
  CHECK(format_multiplier(t) == "$\xE2\x88\x92" "12346$");
  CHECK(format_tick(t.major[0], t) == "0.3225");   // step 0.0025

  // plain offsets when they are short
  compute_ticks(101324.98, 101325.02, 5, t);
  CHECK(t.offset == 101325);
  CHECK(format_multiplier(t) == "$+101325$");
  compute_ticks(1e-20, 1e-20 + 1e-25, 5, t);
  CHECK(format_multiplier(t) == "$\\times10^{-25}\\;+1\\times10^{-20}$");

  // offset and a common factor together
  compute_ticks(0.5, 0.5 + 1.1e-5, 6, t);
  CHECK(t.offset == 0.5 && t.exponent == -5);
  CHECK(format_multiplier(t) == "$\\times10^{-5}\\;+0.5$");

  // ordinary ranges get no offset
  compute_ticks(1000, 1001, 5, t);
  CHECK(t.offset == 0);
  CHECK(format_multiplier(t) == "");
  compute_ticks(-1e-6, 1e-6, 5, t);
  CHECK(t.offset == 0 && t.exponent == -6);

  // the tightest range a plot allows (~1.5e-14 relative) still works
  compute_ticks(1e9, 1e9 * (1 + 1.5e-14), 5, t);
  CHECK(t.major.size() >= 2 && t.offset != 0);
  for (size_t i = 1; i < t.major.size(); i++) CHECK(t.major[i] > t.major[i - 1]);

  CHECK(format_exact(1e9 + 0.001) == "1000000000.001");
  CHECK(format_exact(0.1) == "0.1");
  CHECK(format_coord(1.7e9 + 0.00125, 1e-6) == "1700000000.00125");
  CHECK(format_coord(3.14159265, 0.01) == "3.14159");
}

static void test_log_ticks() {
  Ticks t;
  // several decades: powers of ten, 2..9 minors
  compute_log_ticks(1, 1e5, 8, t);
  CHECK(t.decades && t.major.size() == 6);
  CHECK(near(t.major[2], 100));
  CHECK(t.minor.size() == 5 * 8);
  CHECK(format_tick(1e-3, t) == "$10^{-3}$");
  CHECK(format_tick(1e5, t) == "$10^{5}$");
  CHECK(format_multiplier(t) == "");
  // very long ranges label every few decades
  compute_log_ticks(1e-30, 1e30, 8, t);
  CHECK(t.decades && t.major.size() >= 4 && t.major.size() <= 9);
  CHECK(near(t.step, 10));
  // under two decades: 1-2-5
  compute_log_ticks(5, 50, 8, t);
  CHECK(!t.decades && t.major.size() == 4);
  CHECK(format_tick(t.major[0], t) == "5" && format_tick(t.major[3], t) == "50");
  // tiny range: linear ticks inside a log axis
  compute_log_ticks(1000, 1001, 8, t);
  CHECK(!t.decades && t.major.size() >= 3);
  for (size_t i = 0; i < t.major.size(); i++) CHECK(t.major[i] >= 1000 && t.major[i] <= 1001);
  // invalid ranges give no ticks
  compute_log_ticks(-1, 10, 8, t);
  CHECK(t.major.empty());
}

static void column_extents(const std::vector<Pt> &pts, std::map<int, std::pair<double, double> > &ext);

static void test_log_xform() {
  Xform xf;
  xf.xlog = xf.ylog = true;
  xf.x0 = 0; xf.x1 = 2; xf.y0 = -3; xf.y1 = 0;   // [1, 100] x [1e-3, 1]
  xf.px = 0; xf.py = 0; xf.pw = 200; xf.ph = 300;
  CHECK(near(xf.data_sx(10), 100));
  CHECK(near(xf.data_sy(1e-2), 200));
  CHECK(near(xf.data_dx(150), sqrt(1000.0)));
  CHECK(near(xf.data_x0(), 1) && near(xf.data_x1(), 100));
  // values <= 0 are clipped far below the axis, NaN stays NaN
  CHECK(xf.data_sy(0) > 1e4 && xf.data_sy(-5) == xf.data_sy(0));
  CHECK(xf.data_sy(NAN) != xf.data_sy(NAN));
  Xform lin = xf;
  lin.xlog = false;
  CHECK(lin != xf);

  // decimation on a log axis matches transforming every point
  const size_t n = 200000;
  std::vector<double> x(n), y(n);
  for (size_t i = 0; i < n; i++) {
    x[i] = 1 + i * (99.0 / n);
    y[i] = (i % 1000 == 0) ? 0 : 0.01 * (1 + sin(i * 0.01)) + 1e-3;
  }
  MinMax mm;
  mm.build(&y[0], n);
  std::vector<Pt> dec, full;
  decimate_sorted(&x[0], &y[0], n, mm, xf, dec);
  CHECK(dec.size() <= 4 * 201 + 8);
  for (size_t i = 0; i < n; i++) { Pt p; p.x = xf.data_sx(x[i]); p.y = xf.data_sy(y[i]); full.push_back(p); }
  std::map<int, std::pair<double, double> > ef, ed;
  column_extents(full, ef);
  column_extents(dec, ed);
  CHECK(ef.size() == ed.size());
  for (std::map<int, std::pair<double, double> >::iterator it = ef.begin(); it != ef.end(); ++it) {
    CHECK(ed.count(it->first));
    CHECK(near(ed[it->first].first, it->second.first));
    CHECK(near(ed[it->first].second, it->second.second));
  }
  for (size_t i = 0; i < dec.size(); i++) CHECK(is_break(dec[i]) || dec[i].y == dec[i].y);
}

static void test_clip() {
  Rect r = { 0, 0, 10, 10 };
  double ax = -5, ay = 5, bx = 15, by = 5;
  CHECK(clip_segment(ax, ay, bx, by, r));
  CHECK(near(ax, 0) && near(bx, 10) && near(ay, 5));
  ax = -5; ay = -5; bx = -1; by = 20;
  CHECK(!clip_segment(ax, ay, bx, by, r));

  std::vector<Pt> poly(3), out;
  poly[0].x = -10; poly[0].y = 5;
  poly[1].x = 20; poly[1].y = 5;
  poly[2].x = 5; poly[2].y = 30;
  clip_polygon(poly, r, out);
  CHECK(out.size() >= 3);
  for (size_t i = 0; i < out.size(); i++)
    CHECK(out[i].x >= -1e-9 && out[i].x <= 10 + 1e-9 && out[i].y >= -1e-9 && out[i].y <= 10 + 1e-9);

  // polyline leaving and re-entering produces a break
  std::vector<Pt> pl(3), po;
  pl[0].x = 1; pl[0].y = 1;
  pl[1].x = 5; pl[1].y = 50;
  pl[2].x = 9; pl[2].y = 1;
  clip_polyline(pl, r, po);
  int breaks = 0;
  for (size_t i = 0; i < po.size(); i++) if (is_break(po[i])) breaks++;
  CHECK(breaks >= 1);
}

static void test_minmax() {
  srand(1);
  std::vector<double> v(10007);
  for (size_t i = 0; i < v.size(); i++) v[i] = rand() / (double)RAND_MAX - 0.5;
  v[500] = NAN;
  MinMax mm;
  mm.build(&v[0], v.size());
  for (int k = 0; k < 2000; k++) {
    size_t i = rand() % v.size(), j = rand() % v.size();
    if (i > j) std::swap(i, j);
    j++;
    double mn, mx, bmn = 1e300, bmx = -1e300;
    for (size_t q = i; q < j; q++) { if (v[q] < bmn) bmn = v[q]; if (v[q] > bmx) bmx = v[q]; }
    bool ok = mm.query(i, j, mn, mx);
    CHECK(ok == (bmn <= bmx));
    if (ok) { CHECK(mn == bmn); CHECK(mx == bmx); }
  }
}

// For every pixel column the decimated line must cover the same vertical
// extent as the full line.
static void column_extents(const std::vector<Pt> &pts, std::map<int, std::pair<double, double> > &ext) {
  ext.clear();
  for (size_t i = 0; i < pts.size(); i++) {
    if (is_break(pts[i])) continue;
    int c = (int)floor(pts[i].x);
    std::map<int, std::pair<double, double> >::iterator it = ext.find(c);
    if (it == ext.end()) ext[c] = std::make_pair(pts[i].y, pts[i].y);
    else { it->second.first = std::min(it->second.first, pts[i].y); it->second.second = std::max(it->second.second, pts[i].y); }
  }
}

static void test_decimation() {
  const size_t n = 1000000;
  std::vector<double> x(n), y(n);
  srand(2);
  for (size_t i = 0; i < n; i++) {
    x[i] = i * 1e-3;
    y[i] = sin(x[i]) + 0.3 * (rand() / (double)RAND_MAX - 0.5);
  }
  CHECK(is_sorted_finite(&x[0], n));
  MinMax mm;
  auto t0 = std::chrono::steady_clock::now();
  mm.build(&y[0], n);
  auto t1 = std::chrono::steady_clock::now();

  Xform xf;
  xf.x0 = 0; xf.x1 = x[n - 1]; xf.y0 = -2; xf.y1 = 2;
  xf.px = 50; xf.py = 20; xf.pw = 300; xf.ph = 200;
  std::vector<Pt> dec, full;
  decimate_sorted(&x[0], &y[0], n, mm, xf, dec);
  auto t2 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < n; i++) { Pt p; p.x = xf.sx(x[i]); p.y = xf.sy(y[i]); full.push_back(p); }

  printf("decimation: n=%zu -> %zu points (tree build %.2f ms, decimate %.3f ms)\n", n, dec.size(),
         std::chrono::duration<double, std::milli>(t1 - t0).count(),
         std::chrono::duration<double, std::milli>(t2 - t1).count());
  CHECK(dec.size() <= 4 * 301 + 8);

  std::map<int, std::pair<double, double> > ef, ed;
  column_extents(full, ef);
  column_extents(dec, ed);
  CHECK(ef.size() == ed.size());
  for (std::map<int, std::pair<double, double> >::iterator it = ef.begin(); it != ef.end(); ++it) {
    CHECK(ed.count(it->first));
    CHECK(near(ed[it->first].first, it->second.first));
    CHECK(near(ed[it->first].second, it->second.second));
  }

  // zoomed in: only visible points (+1 each side) are used, no decimation
  xf.x0 = 100; xf.x1 = 100.1;
  decimate_sorted(&x[0], &y[0], n, mm, xf, dec);
  CHECK(dec.size() >= 102 && dec.size() <= 104);

  // markers: one per occupied pixel at most
  Rect r = { xf.px, xf.py, xf.px + xf.pw, xf.py + xf.ph };
  xf.x0 = 0; xf.x1 = x[n - 1];
  std::vector<Pt> mk;
  auto t3 = std::chrono::steady_clock::now();
  marker_points(&x[0], &y[0], 0, n, xf, r, mk);
  auto t4 = std::chrono::steady_clock::now();
  printf("markers: n=%zu -> %zu markers (%.2f ms)\n", n, mk.size(),
         std::chrono::duration<double, std::milli>(t4 - t3).count());
  CHECK(mk.size() <= (size_t)(301 * 201));

  // NaN gaps break the line
  std::vector<double> gx(10), gy(10);
  for (int i = 0; i < 10; i++) { gx[i] = i; gy[i] = i; }
  gy[5] = NAN;
  MinMax gm;
  gm.build(&gy[0], 10);
  xf.x0 = 0; xf.x1 = 9;
  decimate_sorted(&gx[0], &gy[0], 10, gm, xf, dec);
  int breaks = 0;
  for (size_t i = 0; i < dec.size(); i++) if (is_break(dec[i])) breaks++;
  CHECK(breaks == 1);
}

static void test_fill() {
  // crossing curves: y1 = x, y2 = -x on [-1, 1]
  double x[3] = { -1, 0.5, 1 }, y1[3] = { -1, 0.5, 1 }, y2[3] = { 1, -0.5, -1 };
  double lo[3], hi[3];
  for (int i = 0; i < 3; i++) { lo[i] = std::min(y1[i], y2[i]); hi[i] = std::max(y1[i], y2[i]); }
  MinMax ml, mh;
  ml.build(lo, 3); mh.build(hi, 3);
  Xform xf;
  xf.x0 = -1; xf.x1 = 1; xf.y0 = -1; xf.y1 = 1; xf.px = 0; xf.py = 0; xf.pw = 100; xf.ph = 100;
  std::vector<Pt> polys;
  fill_between_sorted(x, 3, ml, mh, y1, y2, xf, polys);
  int npoly = 0;
  for (size_t i = 0; i < polys.size(); i++) if (is_break(polys[i])) npoly++;
  CHECK(npoly == 3);   // two triangles at the crossing + one quad
}

int main() {
  test_ticks();
  test_ticks_extreme();
  test_ticks_offset();
  test_log_ticks();
  test_clip();
  test_minmax();
  test_decimation();
  test_fill();
  test_log_xform();
  if (failures) { printf("%d failure(s)\n", failures); return 1; }
  printf("all algorithm tests passed\n");
  return 0;
}
