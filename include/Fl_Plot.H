//
// Fl_Plot.H -- a lightweight, fast 2D plot widget for FLTK 1.4.
//
// One Fl_Plot is one set of axes holding any number of plot items
// (lines/scatter, fill-between, polygons, circles, reference lines, text).
// Fl_Plot is an Fl_Group only so that its optional toolbar can be made of
// ordinary FLTK buttons; do not add() your own widgets to it.
//
//   Fl_Plot *p = new Fl_Plot(10, 10, 600, 400);
//   p->plot(x, y, n, "r--o")->label("data");
//   p->title("Response");  p->xlabel("$t$ (s)");  p->ylabel("$\\omega_0$");
//   p->legend(true);  p->grid(true);
//   p->default_interaction();       // wheel zoom, box zoom, pan, keys...
//

#ifndef Fl_Plot_H
#define Fl_Plot_H

#include <FL/Fl_Group.H>
#include <FL/fl_draw.H>
#include <stddef.h>
#include <string>
#include <vector>

#include "Fl_Plot_Algo.H"
#include "Fl_Plot_Text.H"

class Fl_Plot;
class Fl_Button;
class Fl_Box;
class Fl_RGB_Image;

// Marker shapes
enum Fl_Plot_Marker {
  FL_PLOT_MARKER_NONE = 0,
  FL_PLOT_CIRCLE,
  FL_PLOT_SQUARE,
  FL_PLOT_DIAMOND,
  FL_PLOT_TRIANGLE_UP,
  FL_PLOT_TRIANGLE_DOWN,
  FL_PLOT_PLUS,
  FL_PLOT_CROSS,
  FL_PLOT_STAR,
  FL_PLOT_POINT
};

// Line style: FLTK's FL_SOLID, FL_DASH, FL_DOT, FL_DASHDOT, FL_DASHDOTDOT,
// or FL_PLOT_NO_LINE to draw markers only.
enum { FL_PLOT_NO_LINE = -1 };

// Where coordinates of an item are interpreted
enum Fl_Plot_Coords {
  FL_PLOT_DATA = 0,   // data units
  FL_PLOT_AXES = 1,   // fraction of the axes box: (0,0) lower left, (1,1) upper right
  FL_PLOT_PIXELS = 2  // (circle radius only) screen pixels
};

enum Fl_Plot_Legend_Pos {
  FL_PLOT_LEGEND_NE = 0, FL_PLOT_LEGEND_NW, FL_PLOT_LEGEND_SW, FL_PLOT_LEGEND_SE
};

// Mouse region reported in events
enum Fl_Plot_Region {
  FL_PLOT_OUTSIDE = 0,
  FL_PLOT_IN_AXES,     // inside the axes box
  FL_PLOT_IN_XAXIS,    // below the axes box (tick labels / xlabel)
  FL_PLOT_IN_YAXIS     // left of the axes box (tick labels / ylabel)
};

// What a mouse drag does
enum Fl_Plot_Drag {
  FL_PLOT_DRAG_NONE = 0,
  FL_PLOT_DRAG_ZOOM_BOX,
  FL_PLOT_DRAG_PAN
};

// Axis selection for zoom/pan
enum { FL_PLOT_X = 1, FL_PLOT_Y = 2, FL_PLOT_XY = 3 };

// ---------------------------------------------------------------------------
// Event information passed to the user event hook.
// ---------------------------------------------------------------------------
struct Fl_Plot_Event {
  int event;        // FL_PUSH, FL_DRAG, FL_RELEASE, FL_MOVE, FL_MOUSEWHEEL,
                    // FL_KEYDOWN, FL_ENTER, FL_LEAVE
  int button;       // Fl::event_button() for mouse events
  int clicks;       // Fl::event_clicks()
  int state;        // Fl::event_state() (modifiers)
  int key;          // Fl::event_key() for keyboard events
  int wheel_dx, wheel_dy;
  int sx, sy;       // mouse position in window coordinates (like Fl::event_x())
  double x, y;      // mouse position in data coordinates
  Fl_Plot_Region region;
};

// Return non-zero to consume the event (default handling is skipped).
typedef int (*Fl_Plot_Event_Cb)(Fl_Plot *plot, const Fl_Plot_Event &e, void *data);
// Called whenever the visible limits change (zoom, pan, reset, xlim...).
typedef void (*Fl_Plot_View_Cb)(Fl_Plot *plot, void *data);
// Custom tick label: write a label for value v into buf (mathtext allowed).
typedef void (*Fl_Plot_Tick_Format)(double v, char *buf, int bufsize, void *data);

// ---------------------------------------------------------------------------
// Base class of everything drawn inside the axes.
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot_Item {
public:
  Fl_Plot_Item();
  virtual ~Fl_Plot_Item();

  // Legend text (mathtext allowed). Items without a label are not shown
  // in the legend.
  void label(const char *s) { label_ = s ? s : ""; }
  const char *label() const { return label_.c_str(); }

  void visible(bool v) { visible_ = v; }
  bool visible() const { return visible_; }

  // Include in autoscaling (default: true for data items).
  void autoscale(bool v) { autoscale_ = v; }
  bool autoscale() const { return autoscale_; }

  Fl_Plot *plot() const { return plot_; }

  // --- interface for subclasses ---
  // Data bounds for autoscaling; return false if there are none.
  virtual bool bounds(double &x0, double &x1, double &y0, double &y1) const;
  // Draw into the axes; clipping to the axes box is already set.
  virtual void draw(const Fl_Plot_Algo::Xform &xf) = 0;
  // Draw a legend sample into the given box; return false for none.
  virtual bool draw_legend_icon(int x, int y, int w, int h);

protected:
  // helper for subclasses: axes rectangle expanded by m pixels
  static Fl_Plot_Algo::Rect clip_rect(const Fl_Plot_Algo::Xform &xf, double m);
  // helper for subclasses: cache finite data bounds for bounds()
  void set_bounds(const double *x, const double *y, size_t n);
  double b_[4];
  bool has_b_;

private:
  friend class Fl_Plot;
  std::string label_;
  bool visible_, autoscale_;
  Fl_Plot *plot_;
};

// ---------------------------------------------------------------------------
// Line and/or scatter plot. Data is copied. For large data sets with sorted
// x, drawing cost is O(width * log n) per view change: min/max per pixel
// column is taken from a block min/max tree. Unsorted data costs O(n).
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot_Line : public Fl_Plot_Item {
public:
  Fl_Plot_Line();

  // x may be NULL: y is then plotted against 0..n-1.
  void data(const double *x, const double *y, size_t n);
  void data(const std::vector<double> &x, const std::vector<double> &y);
  // Uniformly sampled: y[i] at x0 + i * dx.
  void data_sampled(const double *y, size_t n, double x0, double dx);
  size_t size() const { return y_.size(); }
  const double *x_data() const { return x_.empty() ? 0 : &x_[0]; }
  const double *y_data() const { return y_.empty() ? 0 : &y_[0]; }
  bool sorted() const { return sorted_; }   // x non-decreasing and finite

  // Matlab-like format string: [color][linestyle][marker], any order.
  // colors b g r c m y k w; styles - -- : -. ; markers o s d ^ v + x * .
  // A marker without a line style means "markers only" ("ro").
  void format(const char *fmt);

  void color(Fl_Color c) { color_ = c; }
  Fl_Color color() const { return color_; }
  void line_style(int s) { line_style_ = s; cache_valid_ = false; }   // FL_SOLID.. or FL_PLOT_NO_LINE
  int line_style() const { return line_style_; }
  void line_width(int w) { line_width_ = w; cache_valid_ = false; }
  int line_width() const { return line_width_; }

  void marker(Fl_Plot_Marker m) { marker_ = m; cache_valid_ = false; }
  Fl_Plot_Marker marker() const { return marker_; }
  void marker_size(int s) { marker_size_ = s; cache_valid_ = false; }   // pixels (diameter)
  int marker_size() const { return marker_size_; }
  // Marker edge and face colors default to color().
  void marker_color(Fl_Color c) { marker_color_ = c; marker_color_set_ = true; }
  Fl_Color marker_color() const { return marker_color_set_ ? marker_color_ : color_; }
  void marker_fill_color(Fl_Color c) { marker_fill_ = c; marker_fill_set_ = true; }
  Fl_Color marker_fill_color() const { return marker_fill_set_ ? marker_fill_ : marker_color(); }
  void marker_filled(bool f) { marker_filled_ = f; }
  bool marker_filled() const { return marker_filled_; }
  void marker_line_width(int w) { marker_line_width_ = w; cache_valid_ = false; }
  int marker_line_width() const { return marker_line_width_; }

  void draw(const Fl_Plot_Algo::Xform &xf);
  bool draw_legend_icon(int x, int y, int w, int h);

  // Draw a single marker at a screen position using this item's style.
  void draw_marker(double sx, double sy) const;

  // Number of vertices / markers sent to FLTK in the last draw (for
  // diagnostics and tests).
  size_t drawn_vertices() const { return drawn_vertices_; }
  size_t drawn_markers() const { return drawn_markers_; }

private:
  void update_cache(const Fl_Plot_Algo::Xform &xf);
  void draw_markers(const std::vector<Fl_Plot_Algo::Pt> &pts) const;
  std::vector<double> x_, y_;
  bool sorted_;
  Fl_Plot_Algo::MinMax tree_;
  Fl_Color color_, marker_color_, marker_fill_;
  bool marker_color_set_, marker_fill_set_, marker_filled_;
  int line_style_, line_width_, marker_size_, marker_line_width_;
  Fl_Plot_Marker marker_;
  // cache of screen geometry for the last transform
  bool cache_valid_;
  Fl_Plot_Algo::Xform cache_xf_;
  std::vector<Fl_Plot_Algo::Pt> line_pts_, marker_pts_;
  size_t drawn_vertices_, drawn_markers_;
};

// ---------------------------------------------------------------------------
// Area between two curves y1(x) and y2(x) sharing the same x. Crossing
// curves are handled. Transparency is emulated by blending color() with
// the axes background (alpha 1 = opaque).
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot_Fill : public Fl_Plot_Item {
public:
  Fl_Plot_Fill();
  // x may be NULL: the curves are then plotted against 0..n-1.
  void data(const double *x, const double *y1, const double *y2, size_t n);
  // y2 constant (e.g. 0 to fill down to the axis)
  void data(const double *x, const double *y1, double y2, size_t n);
  size_t size() const { return x_.size(); }

  void color(Fl_Color c) { color_ = c; }
  Fl_Color color() const { return color_; }
  void alpha(double a) { alpha_ = a < 0 ? 0 : a > 1 ? 1 : a; }
  double alpha() const { return alpha_; }
  // Optional outline along both curves (FL_PLOT_NO_LINE = none, default).
  void edge_style(int s) { edge_style_ = s; cache_valid_ = false; }
  int edge_style() const { return edge_style_; }
  void edge_color(Fl_Color c) { edge_color_ = c; }
  Fl_Color edge_color() const { return edge_color_; }
  void edge_width(int w) { edge_width_ = w; cache_valid_ = false; }

  void draw(const Fl_Plot_Algo::Xform &xf);
  bool draw_legend_icon(int x, int y, int w, int h);

private:
  void prepare();
  Fl_Color fill_color() const;
  std::vector<double> x_, y1_, y2_, lo_, hi_;
  bool sorted_;
  Fl_Plot_Algo::MinMax tlo_, thi_, t1_, t2_;
  Fl_Color color_, edge_color_;
  double alpha_;
  int edge_style_, edge_width_;
  bool cache_valid_;
  Fl_Plot_Algo::Xform cache_xf_;
  std::vector<Fl_Plot_Algo::Pt> polys_, edge1_, edge2_;
};

// ---------------------------------------------------------------------------
// Filled and/or outlined polygon (closed automatically, may be non-convex).
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot_Polygon : public Fl_Plot_Item {
public:
  Fl_Plot_Polygon();
  void data(const double *x, const double *y, size_t n);
  void coords(Fl_Plot_Coords c);   // FL_PLOT_DATA (default) or FL_PLOT_AXES

  void fill_color(Fl_Color c) { fill_ = c; filled_ = true; }
  Fl_Color fill_color() const { return fill_; }
  void filled(bool f) { filled_ = f; }
  bool filled() const { return filled_; }
  void alpha(double a) { alpha_ = a < 0 ? 0 : a > 1 ? 1 : a; }
  void edge_color(Fl_Color c) { edge_ = c; }
  Fl_Color edge_color() const { return edge_; }
  void edge_style(int s) { edge_style_ = s; }   // FL_PLOT_NO_LINE = no outline
  void edge_width(int w) { edge_width_ = w; }

  void draw(const Fl_Plot_Algo::Xform &xf);
  bool draw_legend_icon(int x, int y, int w, int h);

protected:
  void draw_screen_polygon(const std::vector<Fl_Plot_Algo::Pt> &pts,
                           const Fl_Plot_Algo::Xform &xf);
  std::vector<double> x_, y_;
  Fl_Plot_Coords coords_;
  Fl_Color fill_, edge_;
  bool filled_;
  double alpha_;
  int edge_style_, edge_width_;
};

// ---------------------------------------------------------------------------
// Circle (or ellipse, when the axes scales differ and units are data).
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot_Circle : public Fl_Plot_Polygon {
public:
  Fl_Plot_Circle(double cx = 0, double cy = 0, double r = 1,
                 Fl_Plot_Coords radius_units = FL_PLOT_DATA);
  void center(double cx, double cy) { cx_ = cx; cy_ = cy; }
  void radius(double r, Fl_Plot_Coords units = FL_PLOT_DATA) { r_ = r; units_ = units; }
  double cx() const { return cx_; }
  double cy() const { return cy_; }
  double radius() const { return r_; }

  bool bounds(double &x0, double &x1, double &y0, double &y1) const;
  void draw(const Fl_Plot_Algo::Xform &xf);
private:
  double cx_, cy_, r_;
  Fl_Plot_Coords units_;
};

// ---------------------------------------------------------------------------
// Infinite horizontal or vertical reference line (not autoscaled).
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot_Ref_Line : public Fl_Plot_Item {
public:
  enum Orientation { HORIZONTAL, VERTICAL };
  Fl_Plot_Ref_Line(Orientation o = HORIZONTAL, double v = 0);
  void value(double v) { value_ = v; }
  double value() const { return value_; }
  void color(Fl_Color c) { color_ = c; }
  void line_style(int s) { style_ = s; }
  void line_width(int w) { width_ = w; }
  void draw(const Fl_Plot_Algo::Xform &xf);
  bool draw_legend_icon(int x, int y, int w, int h);
private:
  Orientation orient_;
  double value_;
  Fl_Color color_;
  int style_, width_;
};

// ---------------------------------------------------------------------------
// Text annotation (mathtext allowed), optionally boxed, at data or axes
// coordinates. Not autoscaled.
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot_Annotation : public Fl_Plot_Item {
public:
  Fl_Plot_Annotation(double x = 0, double y = 0, const char *text = 0,
                     Fl_Plot_Coords coords = FL_PLOT_DATA);
  void text(const char *s) { text_ = s ? s : ""; }
  const char *text() const { return text_.c_str(); }
  void position(double x, double y, Fl_Plot_Coords c = FL_PLOT_DATA) { x_ = x; y_ = y; coords_ = c; }
  void align(Fl_Plot_HAlign h, Fl_Plot_VAlign v) { ha_ = h; va_ = v; }
  void angle(int a) { angle_ = a; }
  void offset(int dx, int dy) { dx_ = dx; dy_ = dy; }   // pixels
  void textfont(Fl_Font f) { font_ = f; }
  void textsize(int s) { size_ = s; }
  void textcolor(Fl_Color c) { color_ = c; }
  // Box behind the text: FL_PLOT_NO_LINE edge for no border.
  void box(bool b) { box_ = b; }
  void box_color(Fl_Color c) { box_color_ = c; box_ = true; }
  void box_edge_color(Fl_Color c) { box_edge_ = c; }
  void box_padding(int p) { pad_ = p; }
  void draw(const Fl_Plot_Algo::Xform &xf);
private:
  std::string text_;
  double x_, y_;
  Fl_Plot_Coords coords_;
  Fl_Plot_HAlign ha_;
  Fl_Plot_VAlign va_;
  int angle_, dx_, dy_, size_, pad_;
  Fl_Font font_;
  Fl_Color color_, box_color_, box_edge_;
  bool box_;
};

// ---------------------------------------------------------------------------
// The plot widget.
// ---------------------------------------------------------------------------
class FL_EXPORT Fl_Plot : public Fl_Group {
public:
  Fl_Plot(int X, int Y, int W, int H, const char *L = 0);
  ~Fl_Plot();

  // ---- items ----------------------------------------------------------
  // The plot owns added items and deletes them. Deleting an item yourself
  // also removes it from its plot. Adding an item that belongs to another
  // plot moves it here.
  Fl_Plot_Item *add(Fl_Plot_Item *item);
  void remove(Fl_Plot_Item *item);           // removes and deletes
  void clear_items();                        // removes and deletes all
  int items() const { return (int)items_.size(); }
  Fl_Plot_Item *item(int i) const { return i >= 0 && i < (int)items_.size() ? items_[i] : 0; }

  // Convenience creators (each returns the new, already added item).
  Fl_Plot_Line *plot(const double *x, const double *y, size_t n, const char *fmt = 0);
  Fl_Plot_Line *plot(const std::vector<double> &x, const std::vector<double> &y,
                     const char *fmt = 0);
  Fl_Plot_Line *plot(const std::vector<double> &y, const char *fmt = 0);
  // Uniformly sampled signal: y[i] is plotted at x0 + i * dx (e.g. a time
  // axis from a sample rate: dx = 1 / fs).
  Fl_Plot_Line *plot_sampled(const double *y, size_t n, double x0, double dx,
                             const char *fmt = 0);
  Fl_Plot_Line *plot_sampled(const std::vector<double> &y, double x0, double dx,
                             const char *fmt = 0);
  Fl_Plot_Line *scatter(const double *x, const double *y, size_t n,
                        Fl_Plot_Marker m = FL_PLOT_CIRCLE, int size = 5);
  Fl_Plot_Fill *fill_between(const double *x, const double *y1, const double *y2,
                             size_t n, double alpha = 0.35);
  Fl_Plot_Fill *fill_between(const double *x, const double *y1, double y2,
                             size_t n, double alpha = 0.35);
  Fl_Plot_Polygon *polygon(const double *x, const double *y, size_t n,
                           Fl_Color fill = FL_GRAY);
  Fl_Plot_Circle *circle(double cx, double cy, double r, Fl_Color fill = FL_GRAY,
                         Fl_Plot_Coords radius_units = FL_PLOT_DATA);
  Fl_Plot_Ref_Line *axhline(double y, Fl_Color c = FL_BLACK, int style = FL_DASH);
  Fl_Plot_Ref_Line *axvline(double x, Fl_Color c = FL_BLACK, int style = FL_DASH);
  Fl_Plot_Annotation *text(double x, double y, const char *s,
                           Fl_Plot_Coords coords = FL_PLOT_DATA);

  // Next color of the default color cycle (used by plot() etc.).
  Fl_Color next_color();
  static Fl_Color cycle_color(int i);

  // ---- labels (mathtext allowed) ---------------------------------------
  void title(const char *s) { title_ = s ? s : ""; }
  const char *title() const { return title_.c_str(); }
  void xlabel(const char *s) { xlabel_ = s ? s : ""; }
  const char *xlabel() const { return xlabel_.c_str(); }
  void ylabel(const char *s) { ylabel_ = s ? s : ""; }
  const char *ylabel() const { return ylabel_.c_str(); }
  void textfont(Fl_Font f) { font_ = f; }
  Fl_Font textfont() const { return font_; }
  void textsize(int s) { fontsize_ = s; }      // tick labels, legend
  int textsize() const { return fontsize_; }
  void labelsize_axes(int s) { axlabel_size_ = s; }   // xlabel/ylabel
  void titlesize(int s) { title_size_ = s; }

  // ---- appearance ------------------------------------------------------
  void axes_color(Fl_Color c) { axes_bg_ = c; }       // inside the axes box
  Fl_Color axes_color() const { return axes_bg_; }
  void frame_color(Fl_Color c) { frame_color_ = c; }  // axes box, ticks, labels

  void grid(bool major) { grid_ = major; }
  bool grid() const { return grid_; }
  void grid_minor(bool minor) { grid_minor_ = minor; }
  bool grid_minor() const { return grid_minor_; }
  void grid_color(Fl_Color major, Fl_Color minor) { grid_color_ = major; grid_minor_color_ = minor; }
  void grid_style(int major, int minor) { grid_style_ = major; grid_minor_style_ = minor; }

  void minor_ticks(bool on) { minor_ticks_ = on; }
  bool minor_ticks() const { return minor_ticks_; }
  void ticks_inside(bool in) { ticks_inside_ = in; }
  void tick_length(int major, int minor) { tick_len_ = major; minor_len_ = minor; }
  // Approximate pixel spacing between major ticks.
  void tick_spacing(int x_px, int y_px) { xtick_px_ = x_px; ytick_px_ = y_px; }
  // Explicit tick positions (empty vector = automatic).
  void xticks(const std::vector<double> &v) { xticks_user_ = v; }
  void yticks(const std::vector<double> &v) { yticks_user_ = v; }
  void xtick_format(Fl_Plot_Tick_Format f, void *data = 0) { xfmt_ = f; xfmt_data_ = data; }
  void ytick_format(Fl_Plot_Tick_Format f, void *data = 0) { yfmt_ = f; yfmt_data_ = data; }

  void legend(bool on) { legend_ = on; }
  bool legend() const { return legend_; }
  void legend_position(Fl_Plot_Legend_Pos p) { legend_pos_ = p; }
  Fl_Plot_Legend_Pos legend_position() const { return legend_pos_; }

  // ---- view limits -----------------------------------------------------
  // Setting limits turns autoscaling off for that axis.
  void xlim(double lo, double hi);
  void ylim(double lo, double hi);
  void xlim_auto();                  // autoscale x again
  void ylim_auto();
  double xmin() const { return vx0_; }
  double xmax() const { return vx1_; }
  double ymin() const { return vy0_; }
  double ymax() const { return vy1_; }
  // Set the view without changing autoscale state or the home view.
  void view(double x0, double x1, double y0, double y1);
  // Fraction of the data range added on each side when autoscaling.
  void margin(double m) { margin_ = m > 0 ? m : 0; }

  // ---- actions (wire these to your own buttons/keys if you like) ---------
  void reset_zoom();                 // back to home (autoscaled or xlim/ylim)
  void zoom(double factor, double cx, double cy, int axes = FL_PLOT_XY);
  void zoom_in(int axes = FL_PLOT_XY);   // about the center, by zoom_step()
  void zoom_out(int axes = FL_PLOT_XY);
  void zoom_step(double f) { if (f > 1) zoom_step_ = f; }   // > 1
  void zoom_to(double x0, double x1, double y0, double y1);
  void pan_pixels(double dx, double dy, int axes = FL_PLOT_XY);
  // Interactive zoom box in window coordinates.
  void zoom_box_begin(int sx, int sy);
  void zoom_box_update(int sx, int sy);
  void zoom_box_end(bool apply = true);
  bool zoom_box_active() const { return box_active_; }

  // ---- interaction -----------------------------------------------------
  // Install the default mouse/keyboard bindings:
  //   left drag: zoom box (or pan in pan mode), middle/right drag: pan,
  //   shift+left drag: pan, wheel: zoom about cursor (over an axis: that
  //   axis only), double click: reset, keys: r/Home reset, +/- zoom,
  //   arrows pan, g grid, l legend, p pan mode, z zoom mode, Esc cancel.
  void default_interaction();
  void no_interaction();             // clear every binding
  void drag_mode(int button, Fl_Plot_Drag mode);   // button = FL_LEFT_MOUSE..
  Fl_Plot_Drag drag_mode(int button) const;
  void wheel_zoom(bool on) { wheel_zoom_ = on; }
  void double_click_reset(bool on) { dblclick_reset_ = on; }
  void keyboard(bool on) { keyboard_ = on; }

  // Toolbar (Home, +, -, Pan, Zoom, Save, coordinate readout).
  void toolbar(bool show);
  bool toolbar() const { return toolbar_; }

  // User hook, called before default handling for every mouse/key event.
  void event_callback(Fl_Plot_Event_Cb cb, void *data = 0) { ev_cb_ = cb; ev_data_ = data; }
  void view_callback(Fl_Plot_View_Cb cb, void *data = 0) { view_cb_ = cb; view_data_ = data; }

  // ---- coordinates -----------------------------------------------------
  // Axes box in window coordinates (valid after the first draw/layout).
  void axes_box(int &X, int &Y, int &W, int &H) const;
  void data_to_screen(double x, double y, double &sx, double &sy) const;
  void screen_to_data(double sx, double sy, double &x, double &y) const;
  Fl_Plot_Region region(int sx, int sy) const;
  const Fl_Plot_Algo::Xform &xform() const { return xf_; }

  // ---- output ----------------------------------------------------------
  // Render the plot offscreen (toolbar excluded). Caller deletes the image.
  Fl_RGB_Image *image();
  // Save as PNG. Returns 0 on success.
  int save_png(const char *filename);

  // ---- Fl_Widget -------------------------------------------------------
  void draw();
  int handle(int event);
  void resize(int X, int Y, int W, int H);

  // Recompute layout and autoscaled limits (normally done in draw()).
  void update_layout();

private:
  friend class Fl_Plot_Item;
  void detach(Fl_Plot_Item *item);   // drop from items_ without deleting
  struct Axis_Ticks {
    Fl_Plot_Algo::Ticks t;
    std::vector<std::string> labels;
  };
  void layout_toolbar();
  void compute_auto_limits(double &x0, double &x1, double &y0, double &y1) const;
  void apply_autoscale();
  void set_view(double x0, double x1, double y0, double y1, bool notify = true);
  void make_ticks(bool xaxis, double lo, double hi, int len_px, Axis_Ticks &out) const;
  void draw_contents(bool for_image);
  void draw_grid(const Axis_Ticks &xt, const Axis_Ticks &yt);
  void draw_ticks(const Axis_Ticks &xt, const Axis_Ticks &yt);
  void draw_legend();
  void fill_event(Fl_Plot_Event &e, int event) const;
  int default_handle(int event, const Fl_Plot_Event &e);
  void update_readout(int sx, int sy);
  static void tb_cb(Fl_Widget *w, void *data);

  std::vector<Fl_Plot_Item *> items_;
  int color_index_;

  std::string title_, xlabel_, ylabel_;
  Fl_Font font_;
  int fontsize_, axlabel_size_, title_size_;

  Fl_Color axes_bg_, frame_color_, grid_color_, grid_minor_color_;
  bool grid_, grid_minor_, minor_ticks_, ticks_inside_, legend_;
  int grid_style_, grid_minor_style_;
  int tick_len_, minor_len_, xtick_px_, ytick_px_;
  std::vector<double> xticks_user_, yticks_user_;
  Fl_Plot_Tick_Format xfmt_, yfmt_;
  void *xfmt_data_, *yfmt_data_;
  Fl_Plot_Legend_Pos legend_pos_;

  // limits: view (current), user (xlim/ylim), auto flags
  double vx0_, vx1_, vy0_, vy1_;
  double ux0_, ux1_, uy0_, uy1_;
  bool xauto_, yauto_, view_set_;
  double margin_, zoom_step_;
  Fl_Plot_Algo::Xform xf_;
  Axis_Ticks xt_, yt_;
  int top_, bottom_, left_, right_;   // layout of the axes box

  // interaction state
  Fl_Plot_Drag drag_[4];             // indexed by button 1..3
  bool wheel_zoom_, dblclick_reset_, keyboard_;
  int drag_kind_;                    // 0 none, 1 box, 2 pan
  int drag_axes_;
  int last_x_, last_y_;
  bool box_active_;
  int box_x0_, box_y0_, box_x1_, box_y1_;
  Fl_Plot_Event_Cb ev_cb_;
  void *ev_data_;
  Fl_Plot_View_Cb view_cb_;
  void *view_data_;

  bool capturing_;                   // drawing into image()

  // toolbar
  bool toolbar_;
  int toolbar_h_;
  Fl_Box *readout_;
  Fl_Button *b_home_, *b_in_, *b_out_, *b_pan_, *b_zoom_, *b_save_;
  char readout_buf_[96];
};

#endif
