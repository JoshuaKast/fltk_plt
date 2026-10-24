//
// Fl_Plot.cxx -- the plot widget: layout, axes, legend, interaction, output.
//

#include "Fl_Plot.H"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_draw.H>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <limits>

using namespace Fl_Plot_Algo;

static inline bool is_finite(double v) { return v - v == 0; }

Fl_Plot::Fl_Plot(int X, int Y, int W, int H, const char *L)
  : Fl_Group(X, Y, W, H, L),
    color_index_(0),
    font_(FL_HELVETICA), fontsize_(12), axlabel_size_(13), title_size_(15),
    axes_bg_(FL_WHITE), frame_color_(FL_BLACK),
    grid_color_(fl_rgb_color(215, 215, 215)), grid_minor_color_(fl_rgb_color(235, 235, 235)),
    grid_(false), grid_minor_(false), minor_ticks_(true), ticks_inside_(false), legend_(false),
    grid_style_(FL_SOLID), grid_minor_style_(FL_DOT),
    tick_len_(6), minor_len_(3), xtick_px_(90), ytick_px_(50),
    xfmt_(0), yfmt_(0), xfmt_data_(0), yfmt_data_(0),
    legend_pos_(FL_PLOT_LEGEND_NE),
    vx0_(0), vx1_(1), vy0_(0), vy1_(1), ux0_(0), ux1_(1), uy0_(0), uy1_(1),
    xauto_(true), yauto_(true), view_set_(false), margin_(0.05), zoom_step_(1.5),
    top_(Y), bottom_(Y + H), left_(X), right_(X + W),
    wheel_zoom_(false), dblclick_reset_(false), keyboard_(false),
    drag_kind_(0), drag_axes_(FL_PLOT_XY), last_x_(0), last_y_(0),
    box_active_(false), box_x0_(0), box_y0_(0), box_x1_(0), box_y1_(0),
    ev_cb_(0), ev_data_(0), view_cb_(0), view_data_(0),
    capturing_(false), toolbar_(false), toolbar_h_(0) {
  box(FL_FLAT_BOX);
  for (int i = 0; i < 4; i++) drag_[i] = FL_PLOT_DRAG_NONE;
  readout_buf_[0] = 0;

  // toolbar children (hidden until toolbar(true))
  readout_ = new Fl_Box(X, Y, 10, 22);
  readout_->box(FL_FLAT_BOX);
  readout_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
  readout_->labelsize(11);
  readout_->labelfont(FL_COURIER);
  b_home_ = new Fl_Button(X, Y, 10, 22, "Home");
  b_in_   = new Fl_Button(X, Y, 10, 22, "+");
  b_out_  = new Fl_Button(X, Y, 10, 22, "\xE2\x88\x92");   // U+2212
  b_pan_  = new Fl_Button(X, Y, 10, 22, "Pan");
  b_zoom_ = new Fl_Button(X, Y, 10, 22, "Zoom");
  b_save_ = new Fl_Button(X, Y, 10, 22, "Save");
  b_home_->tooltip("Reset zoom (r)");
  b_in_->tooltip("Zoom in (+)");
  b_out_->tooltip("Zoom out (-)");
  b_pan_->tooltip("Left drag pans (p)");
  b_zoom_->tooltip("Left drag draws a zoom box (z)");
  b_save_->tooltip("Save as PNG");
  b_pan_->type(FL_TOGGLE_BUTTON);
  b_zoom_->type(FL_TOGGLE_BUTTON);
  Fl_Widget *bs[] = { b_home_, b_in_, b_out_, b_pan_, b_zoom_, b_save_ };
  for (int i = 0; i < 6; i++) {
    bs[i]->labelsize(11);
    bs[i]->callback(tb_cb, this);
    bs[i]->clear_visible_focus();
  }
  end();
  resizable(0);
  layout_toolbar();
}

Fl_Plot::~Fl_Plot() {
  for (size_t i = 0; i < items_.size(); i++) delete items_[i];
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------

Fl_Plot_Item *Fl_Plot::add(Fl_Plot_Item *item) {
  if (!item) return 0;
  if (std::find(items_.begin(), items_.end(), item) == items_.end()) {
    items_.push_back(item);
    item->plot_ = this;
  }
  return item;
}

void Fl_Plot::remove(Fl_Plot_Item *item) {
  std::vector<Fl_Plot_Item *>::iterator it = std::find(items_.begin(), items_.end(), item);
  if (it == items_.end()) return;
  items_.erase(it);
  delete item;
}

void Fl_Plot::clear_items() {
  for (size_t i = 0; i < items_.size(); i++) delete items_[i];
  items_.clear();
  color_index_ = 0;
}

Fl_Color Fl_Plot::cycle_color(int i) {
  static const unsigned char c[10][3] = {
    {31, 119, 180}, {255, 127, 14}, {44, 160, 44}, {214, 39, 40}, {148, 103, 189},
    {140, 86, 75}, {227, 119, 194}, {127, 127, 127}, {188, 189, 34}, {23, 190, 207}
  };
  i = ((i % 10) + 10) % 10;
  return fl_rgb_color(c[i][0], c[i][1], c[i][2]);
}

Fl_Color Fl_Plot::next_color() { return cycle_color(color_index_++); }

Fl_Plot_Line *Fl_Plot::plot(const double *x, const double *y, size_t n, const char *fmt) {
  Fl_Plot_Line *l = new Fl_Plot_Line;
  l->data(x, y, n);
  l->color(next_color());
  if (fmt) l->format(fmt);
  add(l);
  return l;
}

Fl_Plot_Line *Fl_Plot::plot(const std::vector<double> &x, const std::vector<double> &y,
                            const char *fmt) {
  size_t n = std::min(x.size(), y.size());
  return plot(n ? &x[0] : 0, n ? &y[0] : 0, n, fmt);
}

Fl_Plot_Line *Fl_Plot::plot(const std::vector<double> &y, const char *fmt) {
  return plot(0, y.empty() ? 0 : &y[0], y.size(), fmt);
}

Fl_Plot_Line *Fl_Plot::scatter(const double *x, const double *y, size_t n,
                               Fl_Plot_Marker m, int size) {
  Fl_Plot_Line *l = plot(x, y, n);
  l->line_style(FL_PLOT_NO_LINE);
  l->marker(m);
  l->marker_size(size);
  return l;
}

Fl_Plot_Fill *Fl_Plot::fill_between(const double *x, const double *y1, const double *y2,
                                    size_t n, double alpha) {
  Fl_Plot_Fill *f = new Fl_Plot_Fill;
  f->data(x, y1, y2, n);
  f->color(next_color());
  f->edge_color(f->color());
  f->alpha(alpha);
  add(f);
  return f;
}

Fl_Plot_Fill *Fl_Plot::fill_between(const double *x, const double *y1, double y2,
                                    size_t n, double alpha) {
  Fl_Plot_Fill *f = new Fl_Plot_Fill;
  f->data(x, y1, y2, n);
  f->color(next_color());
  f->edge_color(f->color());
  f->alpha(alpha);
  add(f);
  return f;
}

Fl_Plot_Polygon *Fl_Plot::polygon(const double *x, const double *y, size_t n, Fl_Color fill) {
  Fl_Plot_Polygon *p = new Fl_Plot_Polygon;
  p->data(x, y, n);
  p->fill_color(fill);
  add(p);
  return p;
}

Fl_Plot_Circle *Fl_Plot::circle(double cx, double cy, double r, Fl_Color fill,
                                Fl_Plot_Coords units) {
  Fl_Plot_Circle *c = new Fl_Plot_Circle(cx, cy, r, units);
  c->fill_color(fill);
  add(c);
  return c;
}

Fl_Plot_Ref_Line *Fl_Plot::axhline(double y, Fl_Color c, int style) {
  Fl_Plot_Ref_Line *l = new Fl_Plot_Ref_Line(Fl_Plot_Ref_Line::HORIZONTAL, y);
  l->color(c);
  l->line_style(style);
  add(l);
  return l;
}

Fl_Plot_Ref_Line *Fl_Plot::axvline(double x, Fl_Color c, int style) {
  Fl_Plot_Ref_Line *l = new Fl_Plot_Ref_Line(Fl_Plot_Ref_Line::VERTICAL, x);
  l->color(c);
  l->line_style(style);
  add(l);
  return l;
}

Fl_Plot_Annotation *Fl_Plot::text(double x, double y, const char *s, Fl_Plot_Coords coords) {
  Fl_Plot_Annotation *a = new Fl_Plot_Annotation(x, y, s, coords);
  add(a);
  return a;
}

// ---------------------------------------------------------------------------
// Limits
// ---------------------------------------------------------------------------

static void sanitize_range(double &a, double &b) {
  if (a > b) std::swap(a, b);
  if (!is_finite(a) || !is_finite(b)) { a = 0; b = 1; return; }
  double m = std::max(fabs(a), fabs(b));
  double minspan = std::max(m * 1e-12, 1e-300);
  if (b - a < minspan) {
    double c = (a + b) / 2, d = m > 0 ? m * 0.05 : 0.5;
    if (b - a == 0) { a = c - d; b = c + d; }
    else { a = c - minspan / 2; b = c + minspan / 2; }
  }
}

void Fl_Plot::compute_auto_limits(double &x0, double &x1, double &y0, double &y1) const {
  const double inf = std::numeric_limits<double>::infinity();
  x0 = y0 = inf; x1 = y1 = -inf;
  for (size_t i = 0; i < items_.size(); i++) {
    const Fl_Plot_Item *it = items_[i];
    if (!it->visible() || !it->autoscale()) continue;
    double a, b, c, d;
    if (!it->bounds(a, b, c, d)) continue;
    x0 = std::min(x0, a); x1 = std::max(x1, b);
    y0 = std::min(y0, c); y1 = std::max(y1, d);
  }
  if (!(x0 <= x1)) { x0 = 0; x1 = 1; }
  if (!(y0 <= y1)) { y0 = 0; y1 = 1; }
  sanitize_range(x0, x1);
  sanitize_range(y0, y1);
  double mx = (x1 - x0) * margin_, my = (y1 - y0) * margin_;
  x0 -= mx; x1 += mx; y0 -= my; y1 += my;
}

void Fl_Plot::apply_autoscale() {
  if (view_set_) return;
  double x0, x1, y0, y1;
  compute_auto_limits(x0, x1, y0, y1);
  if (!xauto_) { x0 = ux0_; x1 = ux1_; }
  if (!yauto_) { y0 = uy0_; y1 = uy1_; }
  bool changed = x0 != vx0_ || x1 != vx1_ || y0 != vy0_ || y1 != vy1_;
  vx0_ = x0; vx1_ = x1; vy0_ = y0; vy1_ = y1;
  if (changed && view_cb_) view_cb_(this, view_data_);
}

void Fl_Plot::set_view(double x0, double x1, double y0, double y1, bool notify) {
  sanitize_range(x0, x1);
  sanitize_range(y0, y1);
  vx0_ = x0; vx1_ = x1; vy0_ = y0; vy1_ = y1;
  view_set_ = true;
  xf_.x0 = x0; xf_.x1 = x1; xf_.y0 = y0; xf_.y1 = y1;
  if (notify && view_cb_) view_cb_(this, view_data_);
  redraw();
}

void Fl_Plot::xlim(double lo, double hi) {
  sanitize_range(lo, hi);
  ux0_ = lo; ux1_ = hi; xauto_ = false;
  vx0_ = lo; vx1_ = hi;
  xf_.x0 = lo; xf_.x1 = hi;
  if (view_cb_) view_cb_(this, view_data_);
  redraw();
}

void Fl_Plot::ylim(double lo, double hi) {
  sanitize_range(lo, hi);
  uy0_ = lo; uy1_ = hi; yauto_ = false;
  vy0_ = lo; vy1_ = hi;
  xf_.y0 = lo; xf_.y1 = hi;
  if (view_cb_) view_cb_(this, view_data_);
  redraw();
}

void Fl_Plot::xlim_auto() { xauto_ = true; redraw(); }
void Fl_Plot::ylim_auto() { yauto_ = true; redraw(); }

void Fl_Plot::view(double x0, double x1, double y0, double y1) { set_view(x0, x1, y0, y1); }

void Fl_Plot::reset_zoom() {
  view_set_ = false;
  apply_autoscale();
  xf_.x0 = vx0_; xf_.x1 = vx1_; xf_.y0 = vy0_; xf_.y1 = vy1_;
  if (view_cb_) view_cb_(this, view_data_);
  redraw();
}

void Fl_Plot::zoom(double f, double cx, double cy, int axes) {
  double x0 = vx0_, x1 = vx1_, y0 = vy0_, y1 = vy1_;
  if (axes & FL_PLOT_X) { x0 = cx + (x0 - cx) * f; x1 = cx + (x1 - cx) * f; }
  if (axes & FL_PLOT_Y) { y0 = cy + (y0 - cy) * f; y1 = cy + (y1 - cy) * f; }
  set_view(x0, x1, y0, y1);
}

void Fl_Plot::zoom_in(int axes) {
  zoom(1.0 / zoom_step_, (vx0_ + vx1_) / 2, (vy0_ + vy1_) / 2, axes);
}

void Fl_Plot::zoom_out(int axes) {
  zoom(zoom_step_, (vx0_ + vx1_) / 2, (vy0_ + vy1_) / 2, axes);
}

void Fl_Plot::zoom_to(double x0, double x1, double y0, double y1) {
  set_view(x0, x1, y0, y1);
}

void Fl_Plot::pan_pixels(double dx, double dy, int axes) {
  double sx = (vx1_ - vx0_) / xf_.pw, sy = (vy1_ - vy0_) / xf_.ph;
  double x0 = vx0_, x1 = vx1_, y0 = vy0_, y1 = vy1_;
  if (axes & FL_PLOT_X) { x0 -= dx * sx; x1 -= dx * sx; }
  if (axes & FL_PLOT_Y) { y0 += dy * sy; y1 += dy * sy; }
  set_view(x0, x1, y0, y1);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void Fl_Plot::zoom_box_begin(int sx, int sy) {
  box_active_ = true;
  box_x0_ = box_x1_ = clampi(sx, left_, right_);
  box_y0_ = box_y1_ = clampi(sy, top_, bottom_);
  redraw();
}

void Fl_Plot::zoom_box_update(int sx, int sy) {
  if (!box_active_) return;
  box_x1_ = clampi(sx, left_, right_);
  box_y1_ = clampi(sy, top_, bottom_);
  redraw();
}

void Fl_Plot::zoom_box_end(bool apply) {
  if (!box_active_) return;
  box_active_ = false;
  int w = abs(box_x1_ - box_x0_), h = abs(box_y1_ - box_y0_);
  if (apply && (w >= 5 || h >= 5)) {
    double x0 = vx0_, x1 = vx1_, y0 = vy0_, y1 = vy1_;
    if (w >= 5) { x0 = xf_.dx(box_x0_); x1 = xf_.dx(box_x1_); }
    if (h >= 5) { y0 = xf_.dy(box_y0_); y1 = xf_.dy(box_y1_); }
    zoom_to(x0, x1, y0, y1);
  }
  redraw();
}

// ---------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------

void Fl_Plot::axes_box(int &X, int &Y, int &W, int &H) const {
  X = left_; Y = top_; W = right_ - left_; H = bottom_ - top_;
}

void Fl_Plot::data_to_screen(double x, double y, double &sx, double &sy) const {
  sx = xf_.sx(x); sy = xf_.sy(y);
}

void Fl_Plot::screen_to_data(double sx, double sy, double &x, double &y) const {
  x = xf_.dx(sx); y = xf_.dy(sy);
}

Fl_Plot_Region Fl_Plot::region(int sx, int sy) const {
  if (sx >= left_ && sx < right_ && sy >= top_ && sy < bottom_) return FL_PLOT_IN_AXES;
  bool inside = sx >= x() && sx < x() + w() && sy >= y() + toolbar_h_ && sy < y() + h();
  if (!inside) return FL_PLOT_OUTSIDE;
  if (sx >= left_ && sx < right_ && sy >= bottom_) return FL_PLOT_IN_XAXIS;
  if (sy >= top_ && sy < bottom_ && sx < left_) return FL_PLOT_IN_YAXIS;
  return FL_PLOT_OUTSIDE;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void Fl_Plot::make_ticks(bool xaxis, double lo, double hi, int len_px, Axis_Ticks &out) const {
  const std::vector<double> &user = xaxis ? xticks_user_ : yticks_user_;
  Fl_Plot_Tick_Format fmt = xaxis ? xfmt_ : yfmt_;
  void *fdata = xaxis ? xfmt_data_ : yfmt_data_;
  int spacing = xaxis ? xtick_px_ : ytick_px_;
  int maxt = std::max(2, len_px / std::max(10, spacing));
  for (;;) {
    compute_ticks(lo, hi, maxt, out.t);
    if (!user.empty()) {
      out.t.major.clear();
      out.t.minor.clear();
      for (size_t i = 0; i < user.size(); i++)
        if (user[i] >= std::min(lo, hi) && user[i] <= std::max(lo, hi))
          out.t.major.push_back(user[i]);
    }
    out.labels.clear();
    for (size_t i = 0; i < out.t.major.size(); i++) {
      if (fmt) {
        char buf[128];
        buf[0] = 0;
        fmt(out.t.major[i], buf, sizeof(buf), fdata);
        out.labels.push_back(buf);
      } else {
        out.labels.push_back(format_tick(out.t.major[i], out.t));
      }
    }
    if (!xaxis || !user.empty() || maxt <= 2 || out.t.major.size() < 2) break;
    // shrink the tick count until x labels don't collide
    double maxw = 0;
    for (size_t i = 0; i < out.labels.size(); i++)
      maxw = std::max(maxw, Fl_Plot_Text::width(out.labels[i].c_str(), font_, fontsize_));
    double px_step = out.t.step / (hi - lo) * len_px;
    if (maxw + 10 <= px_step) break;
    maxt--;
  }
}

void Fl_Plot::layout_toolbar() {
  toolbar_h_ = toolbar_ ? 26 : 0;
  Fl_Widget *bs[] = { b_home_, b_in_, b_out_, b_pan_, b_zoom_, b_save_ };
  int widths[] = { 44, 24, 24, 36, 42, 40 };
  int total = 0;
  for (int i = 0; i < 6; i++) total += widths[i] + 2;
  int bx = x() + w() - total - 2, by = y() + 2;
  for (int i = 0; i < 6; i++) {
    bs[i]->resize(bx, by, widths[i], 22);
    bx += widths[i] + 2;
    if (toolbar_) bs[i]->show(); else bs[i]->hide();
  }
  readout_->resize(x() + 4, by, std::max(0, x() + w() - total - 10 - x()), 22);
  if (toolbar_) readout_->show(); else readout_->hide();
  readout_->color(color());
  b_pan_->value(drag_[FL_LEFT_MOUSE] == FL_PLOT_DRAG_PAN);
  b_zoom_->value(drag_[FL_LEFT_MOUSE] == FL_PLOT_DRAG_ZOOM_BOX);
}

void Fl_Plot::update_layout() {
  apply_autoscale();
  fl_font(font_, fontsize_);
  int th = fl_height();
  int out_len = ticks_inside_ ? 0 : tick_len_;

  int top = y() + toolbar_h_ + 8;
  if (!title_.empty()) {
    Fl_Plot_Text t;
    t.layout(title_.c_str(), font_ < FL_SYMBOL ? (font_ | FL_BOLD) : font_, title_size_);
    top += (int)ceil(t.height()) + 6;
  }
  int bottom = y() + h() - 6 - out_len - th - 2;
  Fl_Plot_Text xl, yl;
  if (!xlabel_.empty()) {
    xl.layout(xlabel_.c_str(), font_, axlabel_size_);
    bottom -= (int)ceil(xl.height()) + 4;
  }
  int left = x() + 6;
  if (!ylabel_.empty()) {
    yl.layout(ylabel_.c_str(), font_, axlabel_size_);
    left += (int)ceil(yl.height()) + 4;
  }

  // y ticks determine the left margin; the exponent label needs space on top
  for (int pass = 0; pass < 2; pass++) {
    make_ticks(false, vy0_, vy1_, std::max(10, bottom - top), yt_);
    if (yt_.t.exponent && top < y() + toolbar_h_ + th + 8) top = y() + toolbar_h_ + th + 8;
  }
  if (xlabel_.empty()) {
    // reserve a row for the x exponent label if needed (checked below)
  }
  double maxw = 0;
  for (size_t i = 0; i < yt_.labels.size(); i++)
    maxw = std::max(maxw, Fl_Plot_Text::width(yt_.labels[i].c_str(), font_, fontsize_));
  left += (int)ceil(maxw) + 4 + out_len;

  int right = x() + w() - 12;
  make_ticks(true, vx0_, vx1_, std::max(10, right - left), xt_);
  if (!xt_.labels.empty()) {
    double lw = Fl_Plot_Text::width(xt_.labels.back().c_str(), font_, fontsize_);
    double sx = left + (xt_.t.major.back() - vx0_) / (vx1_ - vx0_) * (right - left);
    if (sx + lw / 2 > x() + w() - 4) right -= (int)ceil(sx + lw / 2 - (x() + w() - 4));
  }
  if (xt_.t.exponent && xlabel_.empty()) bottom -= th + 2;

  if (right - left < 10) right = left + 10;
  if (bottom - top < 10) bottom = top + 10;
  top_ = top; bottom_ = bottom; left_ = left; right_ = right;
  xf_.x0 = vx0_; xf_.x1 = vx1_; xf_.y0 = vy0_; xf_.y1 = vy1_;
  xf_.px = left; xf_.py = top; xf_.pw = right - left; xf_.ph = bottom - top;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Fl_Plot::draw_grid(const Axis_Ticks &xt, const Axis_Ticks &yt) {
  for (int pass = 0; pass < 2; pass++) {
    bool minor = pass == 0;
    if (minor ? !grid_minor_ : !grid_) continue;
    fl_color(minor ? grid_minor_color_ : grid_color_);
    fl_line_style(minor ? grid_minor_style_ : grid_style_);
    const std::vector<double> &xv = minor ? xt.t.minor : xt.t.major;
    const std::vector<double> &yv = minor ? yt.t.minor : yt.t.major;
    for (size_t i = 0; i < xv.size(); i++) {
      int sx = (int)floor(xf_.sx(xv[i]) + 0.5);
      if (sx <= left_ || sx >= right_) continue;
      fl_line(sx, top_, sx, bottom_ - 1);
    }
    for (size_t i = 0; i < yv.size(); i++) {
      int sy = (int)floor(xf_.sy(yv[i]) + 0.5);
      if (sy <= top_ || sy >= bottom_ - 1) continue;
      fl_line(left_, sy, right_ - 1, sy);
    }
  }
  fl_line_style(0);
}

void Fl_Plot::draw_ticks(const Axis_Ticks &xt, const Axis_Ticks &yt) {
  fl_color(frame_color_);
  int dir = ticks_inside_ ? -1 : 1;
  int b = bottom_ - 1, l = left_;
  for (int pass = 0; pass < 2; pass++) {
    bool minor = pass == 1;
    if (minor && !minor_ticks_) continue;
    int len = minor ? minor_len_ : tick_len_;
    const std::vector<double> &xv = minor ? xt.t.minor : xt.t.major;
    const std::vector<double> &yv = minor ? yt.t.minor : yt.t.major;
    for (size_t i = 0; i < xv.size(); i++) {
      int sx = (int)floor(xf_.sx(xv[i]) + 0.5);
      if (sx < left_ || sx >= right_ + 1) continue;
      fl_line(sx, b, sx, b + dir * len);
    }
    for (size_t i = 0; i < yv.size(); i++) {
      int sy = (int)floor(xf_.sy(yv[i]) + 0.5);
      if (sy < top_ - 1 || sy > bottom_) continue;
      fl_line(l, sy, l - dir * len, sy);
    }
  }
  int out_len = ticks_inside_ ? 0 : tick_len_;
  for (size_t i = 0; i < xt.t.major.size(); i++) {
    double sx = floor(xf_.sx(xt.t.major[i]) + 0.5);
    if (sx < left_ - 1 || sx > right_ + 1) continue;
    Fl_Plot_Text::draw(xt.labels[i].c_str(), sx, bottom_ + out_len + 2, font_, fontsize_,
                       0, FL_PLOT_HCENTER, FL_PLOT_TOP);
  }
  for (size_t i = 0; i < yt.t.major.size(); i++) {
    double sy = floor(xf_.sy(yt.t.major[i]) + 0.5);
    if (sy < top_ - 1 || sy > bottom_ + 1) continue;
    Fl_Plot_Text::draw(yt.labels[i].c_str(), left_ - out_len - 3, sy, font_, fontsize_,
                       0, FL_PLOT_RIGHT, FL_PLOT_VCENTER);
  }
  char buf[64];
  if (yt.t.exponent && yfmt_ == 0) {
    snprintf(buf, sizeof(buf), "$\\times10^{%d}$", yt.t.exponent);
    Fl_Plot_Text::draw(buf, left_, top_ - 3, font_, fontsize_, 0, FL_PLOT_LEFT, FL_PLOT_BOTTOM);
  }
  if (xt.t.exponent && xfmt_ == 0) {
    fl_font(font_, fontsize_);
    snprintf(buf, sizeof(buf), "$\\times10^{%d}$", xt.t.exponent);
    Fl_Plot_Text::draw(buf, right_, bottom_ + out_len + fl_height() + 4, font_, fontsize_,
                       0, FL_PLOT_RIGHT, FL_PLOT_TOP);
  }
}

void Fl_Plot::draw_legend() {
  std::vector<Fl_Plot_Item *> li;
  for (size_t i = 0; i < items_.size(); i++)
    if (items_[i]->visible() && items_[i]->label()[0]) li.push_back(items_[i]);
  if (li.empty()) return;
  std::vector<Fl_Plot_Text> texts(li.size());
  double tw = 0, rowh = 0;
  for (size_t i = 0; i < li.size(); i++) {
    texts[i].layout(li[i]->label(), font_, fontsize_);
    tw = std::max(tw, texts[i].width());
    rowh = std::max(rowh, texts[i].height());
  }
  int rh = std::max(16, (int)ceil(rowh) + 4), iw = 32, pad = 6;
  int W = pad + iw + 6 + (int)ceil(tw) + pad, H = pad + rh * (int)li.size() + pad - 4;
  int X = (legend_pos_ == FL_PLOT_LEGEND_NW || legend_pos_ == FL_PLOT_LEGEND_SW)
              ? left_ + 8 : right_ - 8 - W;
  int Y = (legend_pos_ == FL_PLOT_LEGEND_NE || legend_pos_ == FL_PLOT_LEGEND_NW)
              ? top_ + 8 : bottom_ - 8 - H;
  fl_color(axes_bg_);
  fl_rectf(X, Y, W, H);
  fl_color(fl_rgb_color(160, 160, 160));
  fl_rect(X, Y, W, H);
  for (size_t i = 0; i < li.size(); i++) {
    int ry = Y + pad - 2 + rh * (int)i;
    fl_push_clip(X + pad, ry, iw, rh);
    li[i]->draw_legend_icon(X + pad, ry, iw, rh);
    fl_pop_clip();
    fl_color(frame_color_);
    texts[i].draw(X + pad + iw + 6, ry + rh / 2.0, 0, FL_PLOT_LEFT, FL_PLOT_VCENTER);
  }
}

void Fl_Plot::draw_contents(bool for_image) {
  update_layout();
  if (for_image) {
    fl_color(color());
    fl_rectf(x(), y(), w(), h());
  } else {
    draw_box();
  }
  int aw = right_ - left_, ah = bottom_ - top_;
  fl_color(axes_bg_);
  fl_rectf(left_, top_, aw, ah);

  fl_push_clip(left_, top_, aw, ah);
  draw_grid(xt_, yt_);
  for (size_t i = 0; i < items_.size(); i++)
    if (items_[i]->visible()) items_[i]->draw(xf_);
  if (legend_) draw_legend();
  if (box_active_) {
    int bx = std::min(box_x0_, box_x1_), by = std::min(box_y0_, box_y1_);
    int bw = abs(box_x1_ - box_x0_), bh = abs(box_y1_ - box_y0_);
    if (bw < 5) { bx = left_; bw = aw; }   // x-only / y-only zoom
    if (bh < 5) { by = top_; bh = ah; }
    fl_color(FL_BLACK);
    fl_line_style(FL_DOT);
    fl_rect(bx, by, std::max(1, bw), std::max(1, bh));
    fl_line_style(0);
  }
  fl_pop_clip();

  fl_color(frame_color_);
  fl_rect(left_ - 1, top_ - 1, aw + 2, ah + 2);
  draw_ticks(xt_, yt_);

  fl_color(frame_color_);
  fl_font(font_, fontsize_);
  int th = fl_height(), out_len = ticks_inside_ ? 0 : tick_len_;
  if (!title_.empty()) {
    Fl_Font tf = font_ < FL_SYMBOL ? (font_ | FL_BOLD) : font_;
    Fl_Plot_Text::draw(title_.c_str(), (left_ + right_) / 2.0, y() + toolbar_h_ + 6, tf,
                       title_size_, 0, FL_PLOT_HCENTER, FL_PLOT_TOP);
  }
  if (!xlabel_.empty())
    Fl_Plot_Text::draw(xlabel_.c_str(), (left_ + right_) / 2.0, bottom_ + out_len + th + 4,
                       font_, axlabel_size_, 0, FL_PLOT_HCENTER, FL_PLOT_TOP);
  if (!ylabel_.empty())
    Fl_Plot_Text::draw(ylabel_.c_str(), x() + 6, (top_ + bottom_) / 2.0, font_,
                       axlabel_size_, 90, FL_PLOT_HCENTER, FL_PLOT_TOP);
}

void Fl_Plot::draw() {
  if (capturing_ || (damage() & ~FL_DAMAGE_CHILD)) {
    draw_contents(false);
    if (!capturing_ && toolbar_) draw_children();
  } else if (toolbar_) {
    for (int i = 0; i < children(); i++) update_child(*child(i));
  }
}

void Fl_Plot::resize(int X, int Y, int W, int H) {
  Fl_Widget::resize(X, Y, W, H);
  layout_toolbar();
}

void Fl_Plot::toolbar(bool show) {
  toolbar_ = show;
  layout_toolbar();
  redraw();
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

Fl_RGB_Image *Fl_Plot::image() {
  Fl_Image_Surface surf(w(), h());
  Fl_Surface_Device::push_current(&surf);
  // draw as if the widget were at the surface origin (layout uses x(), y())
  int ox = x(), oy = y();
  Fl_Widget::resize(0, 0, w(), h());
  capturing_ = true;
  draw_contents(true);
  capturing_ = false;
  Fl_Widget::resize(ox, oy, w(), h());
  update_layout();
  Fl_RGB_Image *img = surf.image();
  Fl_Surface_Device::pop_current();
  return img;
}

int Fl_Plot::save_png(const char *filename) {
  Fl_RGB_Image *img = image();
  if (!img) return -1;
  int r = fl_write_png(filename, img);
  delete img;
  return r;
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

void Fl_Plot::default_interaction() {
  drag_[FL_LEFT_MOUSE] = FL_PLOT_DRAG_ZOOM_BOX;
  drag_[FL_MIDDLE_MOUSE] = FL_PLOT_DRAG_PAN;
  drag_[FL_RIGHT_MOUSE] = FL_PLOT_DRAG_PAN;
  wheel_zoom_ = dblclick_reset_ = keyboard_ = true;
  layout_toolbar();
}

void Fl_Plot::no_interaction() {
  for (int i = 0; i < 4; i++) drag_[i] = FL_PLOT_DRAG_NONE;
  wheel_zoom_ = dblclick_reset_ = keyboard_ = false;
  layout_toolbar();
}

void Fl_Plot::drag_mode(int button, Fl_Plot_Drag mode) {
  if (button < 1 || button > 3) return;
  drag_[button] = mode;
  b_pan_->value(drag_[FL_LEFT_MOUSE] == FL_PLOT_DRAG_PAN);
  b_zoom_->value(drag_[FL_LEFT_MOUSE] == FL_PLOT_DRAG_ZOOM_BOX);
}

Fl_Plot_Drag Fl_Plot::drag_mode(int button) const {
  if (button < 1 || button > 3) return FL_PLOT_DRAG_NONE;
  return drag_[button];
}

void Fl_Plot::tb_cb(Fl_Widget *w, void *data) {
  Fl_Plot *p = (Fl_Plot *)data;
  if (w == p->b_home_) p->reset_zoom();
  else if (w == p->b_in_) p->zoom_in();
  else if (w == p->b_out_) p->zoom_out();
  else if (w == p->b_pan_)
    p->drag_mode(FL_LEFT_MOUSE, p->b_pan_->value() ? FL_PLOT_DRAG_PAN : FL_PLOT_DRAG_NONE);
  else if (w == p->b_zoom_)
    p->drag_mode(FL_LEFT_MOUSE, p->b_zoom_->value() ? FL_PLOT_DRAG_ZOOM_BOX : FL_PLOT_DRAG_NONE);
  else if (w == p->b_save_) {
    Fl_Native_File_Chooser fc;
    fc.title("Save plot as PNG");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fc.filter("PNG\t*.png");
    fc.preset_file("plot.png");
    fc.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM | Fl_Native_File_Chooser::USE_FILTER_EXT);
    if (fc.show() == 0) p->save_png(fc.filename());
  }
}

void Fl_Plot::update_readout(int sx, int sy) {
  if (!toolbar_) return;
  if (region(sx, sy) == FL_PLOT_IN_AXES) {
    double dx, dy;
    screen_to_data(sx, sy, dx, dy);
    snprintf(readout_buf_, sizeof(readout_buf_), "x=%-11.6g y=%-11.6g", dx, dy);
  } else {
    readout_buf_[0] = 0;
  }
  if (strcmp(readout_buf_, readout_->label() ? readout_->label() : "")) {
    readout_->copy_label(readout_buf_);
    readout_->redraw();
  }
}

void Fl_Plot::fill_event(Fl_Plot_Event &e, int event) const {
  e.event = event;
  e.button = Fl::event_button();
  e.clicks = Fl::event_clicks();
  e.state = Fl::event_state();
  e.key = Fl::event_key();
  e.wheel_dx = Fl::event_dx();
  e.wheel_dy = Fl::event_dy();
  e.sx = Fl::event_x();
  e.sy = Fl::event_y();
  screen_to_data(e.sx, e.sy, e.x, e.y);
  e.region = region(e.sx, e.sy);
}

int Fl_Plot::handle(int event) {
  // toolbar buttons get their events first
  if (toolbar_ && (event == FL_PUSH || event == FL_MOVE || event == FL_ENTER ||
                   event == FL_MOUSEWHEEL)) {
    for (int i = 0; i < children(); i++) {
      Fl_Widget *c = child(i);
      if (c != readout_ && c->visible() && Fl::event_inside(c))
        return Fl_Group::handle(event);
    }
  }
  switch (event) {
    case FL_PUSH: case FL_DRAG: case FL_RELEASE: case FL_MOVE: case FL_ENTER:
    case FL_LEAVE: case FL_MOUSEWHEEL: case FL_KEYDOWN: {
      if (event == FL_MOVE || event == FL_ENTER) Fl::belowmouse(this);
      if (event == FL_KEYDOWN && Fl::focus() != this) return 0;
      Fl_Plot_Event e;
      fill_event(e, event);
      if (ev_cb_ && ev_cb_(this, e, ev_data_)) return 1;
      int r = default_handle(event, e);
      if (event == FL_PUSH && ev_cb_) r = 1;   // keep delivering drags to the hook
      return r;
    }
    case FL_FOCUS: case FL_UNFOCUS:
      return keyboard_ ? 1 : 0;
    default:
      return Fl_Group::handle(event);
  }
}

int Fl_Plot::default_handle(int event, const Fl_Plot_Event &e) {
  switch (event) {
    case FL_ENTER:
      update_readout(e.sx, e.sy);
      return 1;
    case FL_MOVE:
      update_readout(e.sx, e.sy);
      return 1;
    case FL_LEAVE:
      update_readout(-100000, -100000);
      return 1;

    case FL_PUSH: {
      if (e.region == FL_PLOT_OUTSIDE) return 0;
      if (keyboard_ && Fl::focus() != this) Fl::focus(this);
      int b = e.button;
      if (dblclick_reset_ && b == FL_LEFT_MOUSE && e.clicks > 0) {
        Fl::event_clicks(0);
        reset_zoom();
        return 1;
      }
      Fl_Plot_Drag mode = drag_mode(b);
      if (b == FL_LEFT_MOUSE && (e.state & FL_SHIFT) && mode != FL_PLOT_DRAG_NONE)
        mode = FL_PLOT_DRAG_PAN;
      if (mode == FL_PLOT_DRAG_NONE) return keyboard_ ? 1 : 0;
      last_x_ = e.sx; last_y_ = e.sy;
      if (e.region == FL_PLOT_IN_AXES) {
        if (mode == FL_PLOT_DRAG_ZOOM_BOX) { drag_kind_ = 1; zoom_box_begin(e.sx, e.sy); }
        else { drag_kind_ = 2; drag_axes_ = FL_PLOT_XY; }
      } else {
        // dragging on an axis pans just that axis
        drag_kind_ = 2;
        drag_axes_ = e.region == FL_PLOT_IN_XAXIS ? FL_PLOT_X : FL_PLOT_Y;
      }
      return 1;
    }
    case FL_DRAG:
      if (drag_kind_ == 1) zoom_box_update(e.sx, e.sy);
      else if (drag_kind_ == 2) {
        pan_pixels(e.sx - last_x_, e.sy - last_y_, drag_axes_);
        last_x_ = e.sx; last_y_ = e.sy;
      }
      update_readout(e.sx, e.sy);
      return drag_kind_ != 0;
    case FL_RELEASE:
      if (drag_kind_ == 1) zoom_box_end(true);
      drag_kind_ = 0;
      return 1;

    case FL_MOUSEWHEEL: {
      if (!wheel_zoom_ || e.region == FL_PLOT_OUTSIDE || e.wheel_dy == 0) return 0;
      int axes = FL_PLOT_XY;
      if (e.region == FL_PLOT_IN_XAXIS || (e.state & FL_SHIFT)) axes = FL_PLOT_X;
      else if (e.region == FL_PLOT_IN_YAXIS || (e.state & FL_CTRL)) axes = FL_PLOT_Y;
      zoom(pow(1.2, e.wheel_dy), e.x, e.y, axes);
      return 1;
    }

    case FL_KEYDOWN: {
      if (!keyboard_) return 0;
      int k = e.key;
      const char *t = Fl::event_text();
      char c = t && t[0] && !t[1] ? t[0] : 0;
      if (k == FL_Escape) {
        if (box_active_) { zoom_box_end(false); drag_kind_ = 0; return 1; }
        return 0;
      }
      if (k == FL_Home || c == 'r') { reset_zoom(); return 1; }
      if (c == '+' || c == '=' || k == FL_KP + '+') { zoom_in(); return 1; }
      if (c == '-' || k == FL_KP + '-') { zoom_out(); return 1; }
      if (k == FL_Left)  { pan_pixels(xf_.pw * 0.1, 0); return 1; }
      if (k == FL_Right) { pan_pixels(-xf_.pw * 0.1, 0); return 1; }
      if (k == FL_Up)    { pan_pixels(0, xf_.ph * 0.1); return 1; }
      if (k == FL_Down)  { pan_pixels(0, -xf_.ph * 0.1); return 1; }
      if (c == 'g') { grid(!grid_); redraw(); return 1; }
      if (c == 'l') { legend(!legend_); redraw(); return 1; }
      if (c == 'p') { drag_mode(FL_LEFT_MOUSE, FL_PLOT_DRAG_PAN); return 1; }
      if (c == 'z') { drag_mode(FL_LEFT_MOUSE, FL_PLOT_DRAG_ZOOM_BOX); return 1; }
      return 0;
    }
  }
  return 0;
}
