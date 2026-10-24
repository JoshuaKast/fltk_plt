//
// Fl_Plot_Algo.H -- FLTK-independent algorithms used by Fl_Plot.
//
// Everything here is plain C++ (no FLTK calls), so it can be unit tested
// headless and reused elsewhere: tick generation, the data->screen
// transform, segment/polygon clipping and large-data decimation.
//

#ifndef Fl_Plot_Algo_H
#define Fl_Plot_Algo_H

#include <stddef.h>
#include <string>
#include <vector>

namespace Fl_Plot_Algo {

// ---------------------------------------------------------------------------
// Transform between data coordinates and (sub)pixel screen coordinates.
// Screen y grows downwards; data y grows upwards.
// ---------------------------------------------------------------------------
struct Xform {
  double x0, x1, y0, y1;   // visible data limits
  double px, py, pw, ph;   // axes rectangle in pixels

  Xform() : x0(0), x1(1), y0(0), y1(1), px(0), py(0), pw(1), ph(1) {}
  double sx(double x) const { return px + (x - x0) * (pw / (x1 - x0)); }
  double sy(double y) const { return py + ph - (y - y0) * (ph / (y1 - y0)); }
  double dx(double s) const { return x0 + (s - px) * ((x1 - x0) / pw); }
  double dy(double s) const { return y0 + (py + ph - s) * ((y1 - y0) / ph); }
  bool operator==(const Xform &o) const {
    return x0 == o.x0 && x1 == o.x1 && y0 == o.y0 && y1 == o.y1 &&
           px == o.px && py == o.py && pw == o.pw && ph == o.ph;
  }
  bool operator!=(const Xform &o) const { return !(*this == o); }
};

struct Pt { double x, y; };

// A point with x = NaN marks a break in a polyline.
inline bool is_break(const Pt &p) { return p.x != p.x; }
Pt break_pt();

// ---------------------------------------------------------------------------
// Ticks
// ---------------------------------------------------------------------------
struct Ticks {
  std::vector<double> major;
  std::vector<double> minor;
  double step;        // major step
  double minor_step;
  int decimals;       // decimals needed to print major ticks (after scaling)
  int exponent;       // labels are shown as value / 10^exponent (0 = none)
  Ticks() : step(1), minor_step(0.2), decimals(0), exponent(0) {}
};

// "Nice" step (1, 2, 2.5, 5 x 10^k) giving at most ~max_ticks intervals.
double nice_step(double range, int max_ticks);

// Compute major/minor ticks for [lo, hi] and the label format.
void compute_ticks(double lo, double hi, int max_ticks, Ticks &out);

// Format a tick value with the precision/scaling chosen by compute_ticks.
std::string format_tick(double v, const Ticks &t);

// ---------------------------------------------------------------------------
// Clipping (all in screen space)
// ---------------------------------------------------------------------------
struct Rect { double x0, y0, x1, y1; };

// Liang-Barsky. Returns false if the segment is fully outside.
bool clip_segment(double &ax, double &ay, double &bx, double &by, const Rect &r);

// Sutherland-Hodgman against an axis-aligned rectangle (works for
// non-convex input; result may contain degenerate edges on the border).
void clip_polygon(const std::vector<Pt> &in, const Rect &r, std::vector<Pt> &out);

// Clip a polyline (with break points) to r. Output is a polyline with
// break points wherever the line leaves the rectangle.
void clip_polyline(const std::vector<Pt> &in, const Rect &r, std::vector<Pt> &out);

// ---------------------------------------------------------------------------
// Range min/max queries over a fixed array (NaNs are ignored).
// Block decomposition: O(n/B) memory, O(B + log n) query.
// ---------------------------------------------------------------------------
class MinMax {
public:
  enum { BLOCK = 64 };
  MinMax() : data_(0), n_(0), nb_(0), leaves_(0) {}
  void build(const double *data, size_t n);
  void clear() { data_ = 0; n_ = 0; mn_.clear(); mx_.clear(); }
  bool built() const { return data_ != 0; }
  // min/max over [i, j). Returns false if the range holds no finite value.
  bool query(size_t i, size_t j, double &mn, double &mx) const;
private:
  void scan(size_t i, size_t j, double &mn, double &mx) const;
  const double *data_;
  size_t n_, nb_, leaves_;
  std::vector<double> mn_, mx_;   // segment tree over block min/max
};

// True if x is finite and non-decreasing (enables the fast paths).
bool is_sorted_finite(const double *x, size_t n);

// Index range [i0, i1) of sorted x needed to draw [xlo, xhi]: includes one
// point beyond each end so lines run to the edge of the axes.
void visible_range(const double *x, size_t n, double xlo, double xhi,
                   size_t &i0, size_t &i1);

// Polyline decimation for sorted x (min/max per pixel column, "M4").
// Produces screen points; NaN y values break the line. At most ~4 points
// per pixel column regardless of n, and visually identical to drawing
// every point (each column still spans exactly [min, max]).
void decimate_sorted(const double *x, const double *y, size_t n,
                     const MinMax &mm, const Xform &xf, std::vector<Pt> &out);

// Unsorted polyline: transform every point, drop consecutive points that
// land in the same pixel. O(n).
void decimate_unsorted(const double *x, const double *y, size_t n,
                       const Xform &xf, std::vector<Pt> &out);

// Marker positions: one marker per occupied pixel inside r (later markers
// at the same pixel would overdraw identically). [i0, i1) restricts the scan.
void marker_points(const double *x, const double *y, size_t i0, size_t i1,
                   const Xform &xf, const Rect &r, std::vector<Pt> &out);

// Fill-between geometry for sorted x. Emits convex polygons (quads,
// triangles at crossings, and 1px-wide column rectangles where the data is
// denser than the pixels) into polys, each terminated by a break point.
void fill_between_sorted(const double *x, size_t n, const MinMax &mlo, const MinMax &mhi,
                         const double *y1, const double *y2,
                         const Xform &xf, std::vector<Pt> &polys);

} // namespace Fl_Plot_Algo

#endif
