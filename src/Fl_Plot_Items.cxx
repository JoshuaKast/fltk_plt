//
// Fl_Plot_Items.cxx -- items drawn inside Fl_Plot axes.
//

#include "Fl_Plot.H"

#include <FL/fl_draw.H>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <limits>

using namespace Fl_Plot_Algo;

static inline bool is_finite(double v) { return v - v == 0; }

// Draw a screen-space polyline with break points.
static size_t draw_polyline(const std::vector<Pt> &pts) {
  bool open = false;
  size_t n = 0;
  for (size_t i = 0; i < pts.size(); i++) {
    if (is_break(pts[i])) {
      if (open) { fl_end_line(); open = false; }
      continue;
    }
    if (!open) { fl_begin_line(); open = true; }
    fl_vertex(pts[i].x, pts[i].y);
    n++;
  }
  if (open) fl_end_line();
  return n;
}

// Fill a list of convex polygons separated by break points, clipped to r.
static void fill_convex_list(const std::vector<Pt> &polys, const Rect &r) {
  std::vector<Pt> poly, clipped;
  for (size_t i = 0; i <= polys.size(); i++) {
    if (i == polys.size() || is_break(polys[i])) {
      if (poly.size() >= 3) {
        clip_polygon(poly, r, clipped);
        if (clipped.size() >= 3) {
          fl_begin_polygon();
          for (size_t k = 0; k < clipped.size(); k++) fl_vertex(clipped[k].x, clipped[k].y);
          fl_end_polygon();
        }
      }
      poly.clear();
      continue;
    }
    poly.push_back(polys[i]);
  }
}

static Fl_Color blend_bg(Fl_Color c, double alpha, const Fl_Plot *p) {
  if (alpha >= 1) return c;
  Fl_Color bg = p ? p->axes_color() : FL_WHITE;
  return fl_color_average(c, bg, (float)alpha);
}

static void set_style(int style, int width) {
  fl_line_style(style | (width > 1 ? FL_JOIN_ROUND : 0), width);
}

// ---------------------------------------------------------------------------
// Fl_Plot_Item
// ---------------------------------------------------------------------------

Fl_Plot_Item::Fl_Plot_Item()
  : has_b_(false), visible_(true), autoscale_(true), plot_(0) {
  b_[0] = b_[1] = b_[2] = b_[3] = 0;
  pos_[0] = pos_[1] = 0;
}

Fl_Plot_Item::~Fl_Plot_Item() {
  if (plot_) plot_->detach(this);   // deleted directly by the application
}

bool Fl_Plot_Item::bounds(double &x0, double &x1, double &y0, double &y1) const {
  if (!has_b_) return false;
  x0 = b_[0]; x1 = b_[1]; y0 = b_[2]; y1 = b_[3];
  return true;
}

bool Fl_Plot_Item::min_positive(double &x, double &y) const {
  if (has_b_) { x = pos_[0]; y = pos_[1]; return true; }
  double x0, x1, y0, y1;
  if (!bounds(x0, x1, y0, y1)) return false;
  x = x0 > 0 ? x0 : x1;   // the caller ignores values <= 0
  y = y0 > 0 ? y0 : y1;
  return true;
}

bool Fl_Plot_Item::draw_legend_icon(int, int, int, int) { return false; }

Rect Fl_Plot_Item::clip_rect(const Xform &xf, double m) {
  Rect r;
  r.x0 = xf.px - m; r.y0 = xf.py - m;
  r.x1 = xf.px + xf.pw + m; r.y1 = xf.py + xf.ph + m;
  return r;
}

void Fl_Plot_Item::set_bounds(const double *x, const double *y, size_t n) {
  const double inf = std::numeric_limits<double>::infinity();
  double x0 = inf, x1 = -inf, y0 = inf, y1 = -inf, px = inf, py = inf;
  for (size_t i = 0; i < n; i++) {
    if (!is_finite(x[i]) || !is_finite(y[i])) continue;
    x0 = std::min(x0, x[i]); x1 = std::max(x1, x[i]);
    y0 = std::min(y0, y[i]); y1 = std::max(y1, y[i]);
    if (x[i] > 0) px = std::min(px, x[i]);
    if (y[i] > 0) py = std::min(py, y[i]);
  }
  has_b_ = x0 <= x1;
  b_[0] = x0; b_[1] = x1; b_[2] = y0; b_[3] = y1;
  pos_[0] = px; pos_[1] = py;
}

// ---------------------------------------------------------------------------
// Fl_Plot_Line
// ---------------------------------------------------------------------------

Fl_Plot_Line::Fl_Plot_Line()
  : sorted_(true), color_(FL_BLUE), marker_color_(FL_BLUE), marker_fill_(FL_BLUE),
    marker_color_set_(false), marker_fill_set_(false), marker_filled_(true),
    line_style_(FL_SOLID), line_width_(1), marker_size_(6), marker_line_width_(1),
    marker_(FL_PLOT_MARKER_NONE), cache_valid_(false),
    drawn_vertices_(0), drawn_markers_(0) {}

void Fl_Plot_Line::data(const double *x, const double *y, size_t n) {
  if (!y) n = 0;
  y_.assign(y, y + n);
  if (x) x_.assign(x, x + n);
  else {
    x_.resize(n);
    for (size_t i = 0; i < n; i++) x_[i] = (double)i;
  }
  sorted_ = is_sorted_finite(x_data(), n);
  tree_.clear();
  if (sorted_ && n) tree_.build(y_data(), n);
  set_bounds(x_data(), y_data(), n);
  cache_valid_ = false;
}

void Fl_Plot_Line::data(const std::vector<double> &x, const std::vector<double> &y) {
  size_t n = std::min(x.size(), y.size());
  data(n ? &x[0] : 0, n ? &y[0] : 0, n);
}

void Fl_Plot_Line::data_sampled(const double *y, size_t n, double x0, double dx) {
  if (!y) n = 0;
  std::vector<double> x(n);
  for (size_t i = 0; i < n; i++) x[i] = x0 + (double)i * dx;
  data(n ? &x[0] : 0, y, n);
}

void Fl_Plot_Line::format(const char *fmt) {
  if (!fmt) return;
  bool have_line = false, have_marker = false;
  for (const char *p = fmt; *p; p++) {
    switch (*p) {
      case 'b': color_ = fl_rgb_color(0, 0, 255); break;
      case 'g': color_ = fl_rgb_color(0, 128, 0); break;
      case 'r': color_ = fl_rgb_color(255, 0, 0); break;
      case 'c': color_ = fl_rgb_color(0, 191, 191); break;
      case 'm': color_ = fl_rgb_color(191, 0, 191); break;
      case 'y': color_ = fl_rgb_color(191, 191, 0); break;
      case 'k': color_ = FL_BLACK; break;
      case 'w': color_ = FL_WHITE; break;
      case '-':
        have_line = true;
        if (p[1] == '-') { line_style_ = FL_DASH; p++; }
        else if (p[1] == '.') { line_style_ = FL_DASHDOT; p++; }
        else line_style_ = FL_SOLID;
        break;
      case ':': have_line = true; line_style_ = FL_DOT; break;
      case 'o': have_marker = true; marker_ = FL_PLOT_CIRCLE; break;
      case 's': have_marker = true; marker_ = FL_PLOT_SQUARE; break;
      case 'd': have_marker = true; marker_ = FL_PLOT_DIAMOND; break;
      case '^': have_marker = true; marker_ = FL_PLOT_TRIANGLE_UP; break;
      case 'v': have_marker = true; marker_ = FL_PLOT_TRIANGLE_DOWN; break;
      case '+': have_marker = true; marker_ = FL_PLOT_PLUS; break;
      case 'x': have_marker = true; marker_ = FL_PLOT_CROSS; break;
      case '*': have_marker = true; marker_ = FL_PLOT_STAR; break;
      case '.': have_marker = true; marker_ = FL_PLOT_POINT; break;
      default: break;
    }
  }
  if (have_marker && !have_line) line_style_ = FL_PLOT_NO_LINE;
  if (!have_marker && have_line) marker_ = FL_PLOT_MARKER_NONE;
  cache_valid_ = false;
}

void Fl_Plot_Line::update_cache(const Xform &xf) {
  if (cache_valid_ && cache_xf_ == xf) return;
  size_t n = y_.size();
  line_pts_.clear();
  marker_pts_.clear();
  if (line_style_ != FL_PLOT_NO_LINE && n >= 2) {
    std::vector<Pt> raw;
    if (sorted_) decimate_sorted(x_data(), y_data(), n, tree_, xf, raw);
    else decimate_unsorted(x_data(), y_data(), n, xf, raw);
    clip_polyline(raw, clip_rect(xf, line_width_ + 2), line_pts_);
  }
  if (marker_ != FL_PLOT_MARKER_NONE && n) {
    size_t i0 = 0, i1 = n;
    if (sorted_) visible_range(x_data(), n, xf.data_x0(), xf.data_x1(), i0, i1);
    marker_points(x_data(), y_data(), i0, i1, xf,
                  clip_rect(xf, marker_size_ / 2.0 + marker_line_width_ + 1), marker_pts_);
  }
  cache_xf_ = xf;
  cache_valid_ = true;
}

void Fl_Plot_Line::draw(const Xform &xf) {
  update_cache(xf);
  drawn_vertices_ = drawn_markers_ = 0;
  if (!line_pts_.empty()) {
    fl_color(color_);
    set_style(line_style_, line_width_);
    drawn_vertices_ = draw_polyline(line_pts_);
    fl_line_style(0);
  }
  if (!marker_pts_.empty()) {
    draw_markers(marker_pts_);
    drawn_markers_ = marker_pts_.size();
  }
}

void Fl_Plot_Line::draw_marker(double sx, double sy) const {
  std::vector<Pt> one(1);
  one[0].x = sx; one[0].y = sy;
  draw_markers(one);
}

void Fl_Plot_Line::draw_markers(const std::vector<Pt> &pts) const {
  int s = std::max(1, marker_size_);
  double r = s / 2.0;
  bool closed = marker_ == FL_PLOT_CIRCLE || marker_ == FL_PLOT_SQUARE ||
                marker_ == FL_PLOT_DIAMOND || marker_ == FL_PLOT_TRIANGLE_UP ||
                marker_ == FL_PLOT_TRIANGLE_DOWN;
  // pass 0: faces, pass 1: edges
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 0) {
      if (marker_ == FL_PLOT_POINT) fl_color(marker_color());
      else if (!closed || !marker_filled_) continue;
      else fl_color(marker_fill_color());
    } else {
      if (marker_ == FL_PLOT_POINT) break;
      fl_color(marker_color());
      fl_line_style(FL_SOLID, marker_line_width_);
    }
    for (size_t i = 0; i < pts.size(); i++) {
      int ix = (int)floor(pts[i].x + 0.5), iy = (int)floor(pts[i].y + 0.5);
      int x0 = ix - s / 2, y0 = iy - s / 2;
      double cx = pts[i].x, cy = pts[i].y;
      switch (marker_) {
        case FL_PLOT_CIRCLE:
          if (pass == 0) fl_pie(x0, y0, s, s, 0, 360);
          else fl_arc(x0, y0, s, s, 0, 360);
          break;
        case FL_PLOT_SQUARE:
          if (pass == 0) fl_rectf(x0, y0, s, s);
          else fl_rect(x0, y0, s, s);
          break;
        case FL_PLOT_DIAMOND:
          if (pass == 0) { fl_begin_polygon(); }
          else fl_begin_loop();
          fl_vertex(cx, cy - r); fl_vertex(cx + r, cy);
          fl_vertex(cx, cy + r); fl_vertex(cx - r, cy);
          if (pass == 0) fl_end_polygon(); else fl_end_loop();
          break;
        case FL_PLOT_TRIANGLE_UP:
        case FL_PLOT_TRIANGLE_DOWN: {
          double d = marker_ == FL_PLOT_TRIANGLE_UP ? 1 : -1;
          if (pass == 0) fl_begin_polygon(); else fl_begin_loop();
          fl_vertex(cx, cy - d * r);
          fl_vertex(cx + r, cy + d * r * 0.7);
          fl_vertex(cx - r, cy + d * r * 0.7);
          if (pass == 0) fl_end_polygon(); else fl_end_loop();
          break;
        }
        case FL_PLOT_PLUS:
          fl_line(ix - s / 2, iy, ix + s / 2, iy);
          fl_line(ix, iy - s / 2, ix, iy + s / 2);
          break;
        case FL_PLOT_CROSS:
          fl_line(x0, y0, x0 + s - 1, y0 + s - 1);
          fl_line(x0, y0 + s - 1, x0 + s - 1, y0);
          break;
        case FL_PLOT_STAR:
          fl_line(ix - s / 2, iy, ix + s / 2, iy);
          fl_line(ix, iy - s / 2, ix, iy + s / 2);
          fl_line(x0 + 1, y0 + 1, x0 + s - 2, y0 + s - 2);
          fl_line(x0 + 1, y0 + s - 2, x0 + s - 2, y0 + 1);
          break;
        case FL_PLOT_POINT: {
          int d = std::max(2, s / 3);
          fl_rectf(ix - d / 2, iy - d / 2, d, d);
          break;
        }
        default: break;
      }
    }
  }
  fl_line_style(0);
}

bool Fl_Plot_Line::draw_legend_icon(int x, int y, int w, int h) {
  int cy = y + h / 2;
  if (line_style_ != FL_PLOT_NO_LINE) {
    fl_color(color_);
    set_style(line_style_, line_width_);
    fl_line(x + 2, cy, x + w - 3, cy);
    fl_line_style(0);
  }
  if (marker_ != FL_PLOT_MARKER_NONE) draw_marker(x + w / 2.0, cy);
  return true;
}

// ---------------------------------------------------------------------------
// Fl_Plot_Fill
// ---------------------------------------------------------------------------

Fl_Plot_Fill::Fl_Plot_Fill()
  : sorted_(true), color_(FL_BLUE), edge_color_(FL_BLUE), alpha_(1),
    edge_style_(FL_PLOT_NO_LINE), edge_width_(1), cache_valid_(false) {}

// x may be NULL (index), like Fl_Plot_Line; a NULL y1 or y2 means no data.
static void fill_x(std::vector<double> &v, const double *x, size_t n) {
  if (x) { v.assign(x, x + n); return; }
  v.resize(n);
  for (size_t i = 0; i < n; i++) v[i] = (double)i;
}

void Fl_Plot_Fill::data(const double *x, const double *y1, const double *y2, size_t n) {
  if (!y1 || !y2) n = 0;
  fill_x(x_, x, n);
  y1_.assign(y1, y1 + n);
  y2_.assign(y2, y2 + n);
  prepare();
}

void Fl_Plot_Fill::data(const double *x, const double *y1, double y2, size_t n) {
  if (!y1) n = 0;
  fill_x(x_, x, n);
  y1_.assign(y1, y1 + n);
  y2_.assign(n, y2);
  prepare();
}

void Fl_Plot_Fill::prepare() {
  size_t n = x_.size();
  lo_.resize(n);
  hi_.resize(n);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (size_t i = 0; i < n; i++) {
    double a = y1_[i], b = y2_[i];
    if (!is_finite(a) || !is_finite(b)) { lo_[i] = hi_[i] = nan; continue; }
    lo_[i] = std::min(a, b);
    hi_[i] = std::max(a, b);
  }
  sorted_ = n && is_sorted_finite(&x_[0], n);
  tlo_.clear(); thi_.clear(); t1_.clear(); t2_.clear();
  if (sorted_) {
    tlo_.build(&lo_[0], n);
    thi_.build(&hi_[0], n);
    t1_.build(&y1_[0], n);
    t2_.build(&y2_[0], n);
  }
  // bounds over both curves
  set_bounds(n ? &x_[0] : 0, n ? &lo_[0] : 0, n);
  if (has_b_) {
    double x0, x1, y0, y1, py = pos_[1];
    bounds(x0, x1, y0, y1);
    set_bounds(&x_[0], &hi_[0], n);
    b_[2] = std::min(b_[2], y0);
    b_[3] = std::max(b_[3], y1);
    // smallest positive value of either curve (lo may be 0 where hi is not)
    pos_[1] = std::min(pos_[1], py);
  }
  cache_valid_ = false;
}

Fl_Color Fl_Plot_Fill::fill_color() const { return blend_bg(color_, alpha_, plot()); }

void Fl_Plot_Fill::draw(const Xform &xf) {
  size_t n = x_.size();
  if (n < 2) return;
  if (!cache_valid_ || cache_xf_ != xf) {
    polys_.clear(); edge1_.clear(); edge2_.clear();
    if (sorted_) {
      fill_between_sorted(&x_[0], n, tlo_, thi_, &y1_[0], &y2_[0], xf, polys_);
    } else {
      // general case: one (possibly self-intersecting) polygon
      for (size_t i = 0; i < n; i++) {
        if (!is_finite(x_[i]) || !is_finite(y1_[i])) continue;
        Pt p; p.x = xf.data_sx(x_[i]); p.y = xf.data_sy(y1_[i]); polys_.push_back(p);
      }
      for (size_t i = n; i-- > 0;) {
        if (!is_finite(x_[i]) || !is_finite(y2_[i])) continue;
        Pt p; p.x = xf.data_sx(x_[i]); p.y = xf.data_sy(y2_[i]); polys_.push_back(p);
      }
    }
    if (edge_style_ != FL_PLOT_NO_LINE) {
      std::vector<Pt> raw;
      Rect r = clip_rect(xf, edge_width_ + 2);
      if (sorted_) decimate_sorted(&x_[0], &y1_[0], n, t1_, xf, raw);
      else decimate_unsorted(&x_[0], &y1_[0], n, xf, raw);
      clip_polyline(raw, r, edge1_);
      if (sorted_) decimate_sorted(&x_[0], &y2_[0], n, t2_, xf, raw);
      else decimate_unsorted(&x_[0], &y2_[0], n, xf, raw);
      clip_polyline(raw, r, edge2_);
    }
    cache_xf_ = xf;
    cache_valid_ = true;
  }
  Rect r = clip_rect(xf, 2);
  fl_color(fill_color());
  if (sorted_) {
    fill_convex_list(polys_, r);
  } else {
    std::vector<Pt> clipped;
    clip_polygon(polys_, r, clipped);
    if (clipped.size() >= 3) {
      fl_begin_complex_polygon();
      for (size_t k = 0; k < clipped.size(); k++) fl_vertex(clipped[k].x, clipped[k].y);
      fl_end_complex_polygon();
    }
  }
  if (edge_style_ != FL_PLOT_NO_LINE) {
    fl_color(edge_color_);
    set_style(edge_style_, edge_width_);
    draw_polyline(edge1_);
    draw_polyline(edge2_);
    fl_line_style(0);
  }
}

bool Fl_Plot_Fill::draw_legend_icon(int x, int y, int w, int h) {
  fl_color(fill_color());
  fl_rectf(x + 2, y + 2, w - 4, h - 4);
  if (edge_style_ != FL_PLOT_NO_LINE) {
    fl_color(edge_color_);
    fl_rect(x + 2, y + 2, w - 4, h - 4);
  }
  return true;
}

// ---------------------------------------------------------------------------
// Fl_Plot_Polygon
// ---------------------------------------------------------------------------

Fl_Plot_Polygon::Fl_Plot_Polygon()
  : coords_(FL_PLOT_DATA), fill_(FL_GRAY), edge_(FL_BLACK), filled_(true),
    alpha_(1), edge_style_(FL_SOLID), edge_width_(1) {}

void Fl_Plot_Polygon::data(const double *x, const double *y, size_t n) {
  if (!x || !y) n = 0;
  x_.assign(x, x + n);
  y_.assign(y, y + n);
  set_bounds(x, y, n);
}

void Fl_Plot_Polygon::coords(Fl_Plot_Coords c) {
  coords_ = c;
  if (c != FL_PLOT_DATA) has_b_ = false;
  else set_bounds(x_.empty() ? 0 : &x_[0], y_.empty() ? 0 : &y_[0], x_.size());
}

void Fl_Plot_Polygon::draw_screen_polygon(const std::vector<Pt> &pts, const Xform &xf) {
  if (pts.size() < 2) return;
  if (filled_ && pts.size() >= 3) {
    std::vector<Pt> clipped;
    clip_polygon(pts, clip_rect(xf, 2), clipped);
    if (clipped.size() >= 3) {
      fl_color(blend_bg(fill_, alpha_, plot()));
      fl_begin_complex_polygon();
      for (size_t k = 0; k < clipped.size(); k++) fl_vertex(clipped[k].x, clipped[k].y);
      fl_end_complex_polygon();
    }
  }
  if (edge_style_ != FL_PLOT_NO_LINE) {
    std::vector<Pt> loop(pts), clipped;
    loop.push_back(pts[0]);
    clip_polyline(loop, clip_rect(xf, edge_width_ + 2), clipped);
    fl_color(edge_);
    set_style(edge_style_, edge_width_);
    draw_polyline(clipped);
    fl_line_style(0);
  }
}

void Fl_Plot_Polygon::draw(const Xform &xf) {
  std::vector<Pt> pts(x_.size());
  for (size_t i = 0; i < x_.size(); i++) {
    if (coords_ == FL_PLOT_AXES) {
      pts[i].x = xf.px + x_[i] * xf.pw;
      pts[i].y = xf.py + xf.ph - y_[i] * xf.ph;
    } else {
      pts[i].x = xf.data_sx(x_[i]);
      pts[i].y = xf.data_sy(y_[i]);
    }
  }
  draw_screen_polygon(pts, xf);
}

bool Fl_Plot_Polygon::draw_legend_icon(int x, int y, int w, int h) {
  if (filled_) {
    fl_color(blend_bg(fill_, alpha_, plot()));
    fl_rectf(x + 2, y + 2, w - 4, h - 4);
  }
  if (edge_style_ != FL_PLOT_NO_LINE) {
    fl_color(edge_);
    fl_rect(x + 2, y + 2, w - 4, h - 4);
  }
  return true;
}

// ---------------------------------------------------------------------------
// Fl_Plot_Circle
// ---------------------------------------------------------------------------

Fl_Plot_Circle::Fl_Plot_Circle(double cx, double cy, double r, Fl_Plot_Coords units)
  : cx_(cx), cy_(cy), r_(r), units_(units) {}

bool Fl_Plot_Circle::bounds(double &x0, double &x1, double &y0, double &y1) const {
  double r = units_ == FL_PLOT_DATA ? r_ : 0;
  x0 = cx_ - r; x1 = cx_ + r; y0 = cy_ - r; y1 = cy_ + r;
  return true;
}

void Fl_Plot_Circle::draw(const Xform &xf) {
  double sx = xf.data_sx(cx_), sy = xf.data_sy(cy_);
  double rx = r_, ry = r_;
  if (units_ == FL_PLOT_DATA) {
    // screen extent of the circle (on log axes, the larger of the two sides)
    rx = std::max(fabs(xf.data_sx(cx_ + r_) - sx), fabs(xf.data_sx(cx_ - r_) - sx));
    ry = std::max(fabs(xf.data_sy(cy_ + r_) - sy), fabs(xf.data_sy(cy_ - r_) - sy));
  }
  double rmax = std::max(rx, ry);
  // skip if entirely outside the axes
  if (!(sx + rx >= xf.px - 2 && sx - rx <= xf.px + xf.pw + 2 &&
        sy + ry >= xf.py - 2 && sy - ry <= xf.py + xf.ph + 2)) return;
  int n = (int)std::min(4096.0, std::max(24.0, ceil(2 * M_PI * rmax / 3)));
  std::vector<Pt> pts(n);
  bool curved = units_ == FL_PLOT_DATA && (xf.xlog || xf.ylog);
  for (int i = 0; i < n; i++) {
    double a = 2 * M_PI * i / n;
    if (curved) {
      // a circle in data space, distorted by the log scale
      pts[i].x = xf.data_sx(cx_ + r_ * cos(a));
      pts[i].y = xf.data_sy(cy_ + r_ * sin(a));
    } else {
      pts[i].x = sx + rx * cos(a);
      pts[i].y = sy - ry * sin(a);
    }
  }
  draw_screen_polygon(pts, xf);
}

// ---------------------------------------------------------------------------
// Fl_Plot_Ref_Line
// ---------------------------------------------------------------------------

Fl_Plot_Ref_Line::Fl_Plot_Ref_Line(Orientation o, double v)
  : orient_(o), value_(v), color_(FL_BLACK), style_(FL_DASH), width_(1) {
  autoscale(false);
}

void Fl_Plot_Ref_Line::draw(const Xform &xf) {
  fl_color(color_);
  set_style(style_, width_);
  if (orient_ == HORIZONTAL) {
    double y = xf.data_sy(value_);
    if (y >= xf.py - width_ && y <= xf.py + xf.ph + width_) {
      fl_begin_line(); fl_vertex(xf.px, y); fl_vertex(xf.px + xf.pw, y); fl_end_line();
    }
  } else {
    double x = xf.data_sx(value_);
    if (x >= xf.px - width_ && x <= xf.px + xf.pw + width_) {
      fl_begin_line(); fl_vertex(x, xf.py); fl_vertex(x, xf.py + xf.ph); fl_end_line();
    }
  }
  fl_line_style(0);
}

bool Fl_Plot_Ref_Line::draw_legend_icon(int x, int y, int w, int h) {
  fl_color(color_);
  set_style(style_, width_);
  fl_line(x + 2, y + h / 2, x + w - 3, y + h / 2);
  fl_line_style(0);
  return true;
}

// ---------------------------------------------------------------------------
// Fl_Plot_Annotation
// ---------------------------------------------------------------------------

Fl_Plot_Annotation::Fl_Plot_Annotation(double x, double y, const char *text,
                                       Fl_Plot_Coords coords)
  : text_(text ? text : ""), x_(x), y_(y), coords_(coords),
    ha_(FL_PLOT_LEFT), va_(FL_PLOT_BASELINE), angle_(0), dx_(0), dy_(0),
    size_(0), pad_(3), font_(-1), color_(FL_BLACK), box_color_(FL_WHITE),
    box_edge_(FL_BLACK), box_(false) {
  autoscale(false);
}

void Fl_Plot_Annotation::draw(const Xform &xf) {
  double ax, ay;
  if (coords_ == FL_PLOT_AXES) {
    ax = xf.px + x_ * xf.pw;
    ay = xf.py + xf.ph - y_ * xf.ph;
  } else {
    ax = xf.data_sx(x_);
    ay = xf.data_sy(y_);
  }
  ax += dx_; ay += dy_;
  if (!is_finite(ax) || !is_finite(ay) || fabs(ax) > 1e6 || fabs(ay) > 1e6) return;
  Fl_Font f = font_ >= 0 ? font_ : (plot() ? plot()->textfont() : FL_HELVETICA);
  int s = size_ > 0 ? size_ : (plot() ? plot()->textsize() : 12);
  Fl_Plot_Text t;
  t.layout(text_.c_str(), f, s);
  if (box_) {
    double cx[4], cy[4];
    t.box(ax, ay, angle_, ha_, va_, pad_, cx, cy);
    fl_color(box_color_);
    fl_begin_polygon();
    for (int i = 0; i < 4; i++) fl_vertex(cx[i], cy[i]);
    fl_end_polygon();
    if (box_edge_ != (Fl_Color)FL_PLOT_NO_LINE) {
      fl_color(box_edge_);
      fl_begin_loop();
      for (int i = 0; i < 4; i++) fl_vertex(cx[i], cy[i]);
      fl_end_loop();
    }
  }
  fl_color(color_);
  t.draw(ax, ay, angle_, ha_, va_);
}
