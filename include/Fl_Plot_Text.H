//
// Fl_Plot_Text.H -- minimal "mathtext" renderer for FLTK.
//
// Text outside $...$ is drawn as-is. Inside $...$ a small TeX subset is
// understood (in the spirit of matplotlib's mathtext):
//
//   x^2  x^{n+1}  a_i  a_{ij}  x_i^2     superscripts / subscripts (nestable)
//   \alpha ... \Omega                     greek letters
//   \pm \times \cdot \infty \partial ...  common symbols (see Fl_Plot_Text.cxx)
//   \mathrm{..} \mathit{..} \mathbf{..}   font changes (builtin FLTK fonts)
//   \bar{x} \hat{x} \dot{x} \vec{v}    accents; \sqrt{x}; \frac{a}{b} (inline a/b)
//   \sin \cos \log \exp \max ...       upright operator names
//   \, \; \quad \!                        spacing
//   \{ \} \$ \_ \^ \\                     escapes
//
// Outside math, \$ produces a literal dollar sign. Symbols are emitted as
// UTF-8, so the font in use must contain the glyphs (e.g. DejaVu Sans,
// which Xft/fontconfig provide on most Linux systems).
//

#ifndef Fl_Plot_Text_H
#define Fl_Plot_Text_H

#include <FL/Enumerations.H>
#include <string>
#include <vector>

enum Fl_Plot_HAlign { FL_PLOT_LEFT = 0, FL_PLOT_HCENTER = 1, FL_PLOT_RIGHT = 2 };
enum Fl_Plot_VAlign { FL_PLOT_TOP = 0, FL_PLOT_VCENTER = 1, FL_PLOT_BASELINE = 2,
                      FL_PLOT_BOTTOM = 3 };

class FL_EXPORT Fl_Plot_Text {
public:
  struct Run {
    std::string text;
    Fl_Font font;
    double size;
    double x;    // advance from the start of the string, pixels
    double dy;   // baseline shift, pixels, positive = up
    double w;    // advance width; a run with empty text is a rule of width w
  };

  // Lay out str with the given base font and size. Must be called with a
  // valid FLTK display (fl_width() is used for measuring).
  void layout(const char *str, Fl_Font font, int size);

  double width() const { return width_; }
  double ascent() const { return ascent_; }
  double descent() const { return descent_; }
  double height() const { return ascent_ + descent_; }
  const std::vector<Run> &runs() const { return runs_; }

  // Draw the laid-out text anchored at (x, y), rotated by angle degrees
  // counter-clockwise around the anchor.
  void draw(double x, double y, int angle = 0,
            Fl_Plot_HAlign ha = FL_PLOT_LEFT, Fl_Plot_VAlign va = FL_PLOT_BASELINE) const;

  // Screen-space corners of the text box (for backgrounds), with padding.
  void box(double x, double y, int angle, Fl_Plot_HAlign ha, Fl_Plot_VAlign va,
           double pad, double cx[4], double cy[4]) const;

  // Convenience one-shot helpers (use the current fl_color()).
  static void draw(const char *str, double x, double y, Fl_Font font, int size,
                   int angle = 0, Fl_Plot_HAlign ha = FL_PLOT_LEFT,
                   Fl_Plot_VAlign va = FL_PLOT_BASELINE);
  static double width(const char *str, Fl_Font font, int size);

  // UTF-8 for a symbol name without the backslash ("alpha" -> "α"), or 0.
  static const char *symbol(const char *name);

  // Font used for symbols and greek letters (-1 = the surrounding font).
  // Useful when the text font lacks glyphs, e.g.
  //   Fl::set_font(FL_FREE_FONT, "DejaVu Sans");
  //   Fl_Plot_Text::symbol_font(FL_FREE_FONT);
  static void symbol_font(Fl_Font f) { symbol_font_ = f; }
  static Fl_Font symbol_font() { return symbol_font_; }

private:
  void anchor(Fl_Plot_HAlign ha, Fl_Plot_VAlign va, double &ox, double &oy) const;
  std::vector<Run> runs_;
  double width_, ascent_, descent_;
  static Fl_Font symbol_font_;
};

#endif
