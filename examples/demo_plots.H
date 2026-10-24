// Plot contents shared by examples/demo.cxx and tests/test_render.cxx.

#ifndef demo_plots_H
#define demo_plots_H

#include "Fl_Plot.H"

#include <math.h>
#include <stdlib.h>
#include <vector>

// Lines with different styles and markers, legend, grid, mathtext labels.
inline void demo_lines(Fl_Plot *p) {
  const int n = 200;
  std::vector<double> x(n), a(n), b(n), c(n);
  for (int i = 0; i < n; i++) {
    x[i] = i * 4 * M_PI / (n - 1);
    a[i] = sin(x[i]);
    b[i] = 0.8 * cos(x[i]) * exp(-0.1 * x[i]);
    c[i] = 0.5 * sin(2 * x[i] + 0.5);
  }
  p->plot(x, a)->label("$\\sin(\\omega t)$");
  Fl_Plot_Line *l = p->plot(x, b, "--");
  l->line_width(2);
  l->label("$0.8\\cos(t)\\,e^{-t/10}$");
  std::vector<double> xs, cs;
  for (int i = 0; i < n; i += 8) { xs.push_back(x[i]); cs.push_back(c[i]); }
  l = p->plot(xs, cs, "-o");
  l->marker_size(7);
  l->marker_fill_color(FL_WHITE);
  l->label("$A_1 \\sin(2t+\\phi_0)$");
  p->title("Lines, markers and $\\mathrm{math}$: $\\alpha_i^2 + \\beta^{n+1}$");
  p->xlabel("$t$ (s)");
  p->ylabel("amplitude ($\\mu$V)");
  p->grid(true);
  p->grid_minor(true);
  p->legend(true);
}

// Mean with a +-1 sigma band, fill to zero, annotations.
inline void demo_fill(Fl_Plot *p) {
  const int n = 400;
  std::vector<double> x(n), m(n), lo(n), hi(n), g(n);
  for (int i = 0; i < n; i++) {
    x[i] = -3 + 6.0 * i / (n - 1);
    m[i] = 0.5 * x[i] + sin(2 * x[i]);
    double s = 0.3 + 0.2 * fabs(x[i]);
    lo[i] = m[i] - s;
    hi[i] = m[i] + s;
    g[i] = 2.5 * exp(-x[i] * x[i]) - 2.5;
  }
  Fl_Plot_Fill *f = p->fill_between(&x[0], &lo[0], &hi[0], n, 0.3);
  f->label("$\\pm1\\sigma$");
  Fl_Plot_Line *l = p->plot(x, m);
  l->color(f->color());
  l->line_width(2);
  l->label("mean $\\bar{x}$");
  Fl_Plot_Fill *f2 = p->fill_between(&x[0], &g[0], -2.5, n, 0.5);
  f2->edge_style(FL_SOLID);
  f2->label("$2.5\\,e^{-x^2}$");
  Fl_Plot_Annotation *a = p->text(0, -0.2, "peak $x_0 = 0$");
  a->align(FL_PLOT_HCENTER, FL_PLOT_TOP);
  a->box(true);
  a->box_color(fl_rgb_color(255, 255, 220));
  Fl_Plot_Annotation *b = p->text(0.02, 0.96, "$\\chi^2/\\nu = 1.04$", FL_PLOT_AXES);
  b->align(FL_PLOT_LEFT, FL_PLOT_TOP);
  p->title("fill_between, crossing curves, annotations");
  p->xlabel("$x$");
  p->ylabel("$f(x)$");
  p->legend(true);
  p->legend_position(FL_PLOT_LEGEND_SE);
  p->grid(true);
}

// Basic drawing: polygons, circles, reference lines.
inline void demo_shapes(Fl_Plot *p) {
  double hx[6], hy[6];
  for (int i = 0; i < 6; i++) {
    hx[i] = 2 + 1.2 * cos(i * M_PI / 3);
    hy[i] = 2 + 1.2 * sin(i * M_PI / 3);
  }
  Fl_Plot_Polygon *hex = p->polygon(hx, hy, 6, fl_rgb_color(255, 127, 14));
  hex->alpha(0.6);
  hex->label("hexagon");
  double sx[10], sy[10];
  for (int i = 0; i < 10; i++) {
    double r = (i % 2) ? 0.5 : 1.2, t = M_PI / 2 + i * M_PI / 5;
    sx[i] = 5.5 + r * cos(t);
    sy[i] = 2 + r * sin(t);
  }
  Fl_Plot_Polygon *star = p->polygon(sx, sy, 10, fl_rgb_color(44, 160, 44));
  star->label("star (non-convex)");
  Fl_Plot_Circle *c = p->circle(3.5, 4.5, 0.8, fl_rgb_color(31, 119, 180));
  c->alpha(0.5);
  c->label("circle, r = 0.8");
  Fl_Plot_Circle *d = p->circle(6.5, 4.5, 12, fl_rgb_color(214, 39, 40), FL_PLOT_PIXELS);
  d->edge_style(FL_PLOT_NO_LINE);
  d->label("dot, r = 12 px");
  p->axhline(1, fl_rgb_color(100, 100, 100))->label("$y = 1$");
  p->axvline(4.5, fl_rgb_color(148, 103, 189), FL_DOT);
  Fl_Plot_Annotation *t = p->text(4.5, 0.4, "$\\theta = 90\\degree$");
  t->angle(90);
  t->align(FL_PLOT_LEFT, FL_PLOT_BOTTOM);
  t->offset(-3, 0);
  p->xlim(0, 8);
  p->ylim(0, 6);
  p->title("Polygons, circles, reference lines");
  p->legend(true);
  p->legend_position(FL_PLOT_LEGEND_NW);
}

// Scatter with every marker type.
inline void demo_markers(Fl_Plot *p) {
  const Fl_Plot_Marker m[] = { FL_PLOT_CIRCLE, FL_PLOT_SQUARE, FL_PLOT_DIAMOND,
                               FL_PLOT_TRIANGLE_UP, FL_PLOT_TRIANGLE_DOWN, FL_PLOT_PLUS,
                               FL_PLOT_CROSS, FL_PLOT_STAR, FL_PLOT_POINT };
  const char *names[] = { "circle", "square", "diamond", "tri up", "tri down",
                          "plus", "cross", "star", "point" };
  srand(3);
  for (int k = 0; k < 9; k++) {
    double x[12], y[12];
    for (int i = 0; i < 12; i++) {
      x[i] = i + 0.3 * (rand() / (double)RAND_MAX);
      y[i] = k + 0.4 * (rand() / (double)RAND_MAX);
    }
    Fl_Plot_Line *l = p->scatter(x, y, 12, m[k], 8);
    if (k % 3 == 1) l->marker_filled(false);
    l->label(names[k]);
  }
  p->title("Scatter markers");
  p->xlim(-0.5, 15.5);
  p->legend(true);
  p->yticks(std::vector<double>());
  p->grid(true);
}

// Large data: a 1e6-point noisy signal (sorted x) and 2e5 random points.
inline void demo_bigdata(Fl_Plot *p, int n = 1000000) {
  std::vector<double> x(n), y(n);
  srand(4);
  for (int i = 0; i < n; i++) {
    x[i] = i * 1e-4;
    y[i] = sin(x[i]) + 0.2 * sin(37 * x[i]) + 0.15 * (rand() / (double)RAND_MAX - 0.5);
    if (i > n / 2 && i < n / 2 + n / 100) y[i] = NAN;   // a gap
  }
  p->plot(x, y)->label("$10^6$ samples");
  int m = 200000;
  std::vector<double> sx(m), sy(m);
  for (int i = 0; i < m; i++) {
    double r = sqrt(-2 * log((rand() + 1.0) / (RAND_MAX + 2.0)));
    double t = 2 * M_PI * rand() / (double)RAND_MAX;
    sx[i] = 50 + 12 * r * cos(t);
    sy[i] = 0.4 * r * sin(t);
  }
  p->scatter(&sx[0], &sy[0], m, FL_PLOT_POINT, 2)->label("$2\\times10^5$ random points");
  p->title("Large data (decimated per pixel)");
  p->legend(true);
  p->grid(true);
}

// Mathtext samples as annotations in axes coordinates.
inline void demo_mathtext(Fl_Plot *p) {
  const char *s[] = {
    "Greek: $\\alpha \\beta \\gamma \\delta \\epsilon \\theta \\lambda \\mu \\pi \\sigma \\phi \\omega$  $\\Gamma \\Delta \\Sigma \\Omega$",
    "Scripts: $x^2 + y_i^2 = r_{ij}^{n+1}$,  $e^{-t/\\tau}$,  $10^{-3}$,  $H_2O$",
    "Operators: $\\sin(\\omega t + \\phi)$,  $\\log_{10} x$,  $\\max_k |a_k|$",
    "Symbols: $\\pm \\times \\cdot \\leq \\geq \\neq \\approx \\propto \\infty \\partial \\nabla \\int \\sum$",
    "Accents: $\\bar{x}$, $\\hat{y}$, $\\dot{q}$, $\\ddot{q}$, $\\vec{F}$, $\\tilde{n}$, $\\bar{X}$, $\\sqrt{2}$",
    "Units: $T = 25\\degree$C,  $\\lambda = 532$ nm,  $\\Delta f = 3\\,\\mu$Hz,  5 \\$",
    "Fonts: $\\mathrm{rm}\\ \\mathit{italic}\\ \\mathbf{bold}\\ \\mathtt{mono}$,  $\\frac{dV}{dt}$",
  };
  for (int i = 0; i < 7; i++) {
    Fl_Plot_Annotation *a = p->text(0.03, 0.92 - i * 0.13, s[i], FL_PLOT_AXES);
    a->align(FL_PLOT_LEFT, FL_PLOT_VCENTER);
    a->textsize(15);
  }
  p->xticks(std::vector<double>(1, 1e9));   // no ticks
  p->yticks(std::vector<double>(1, 1e9));
  p->minor_ticks(false);
  p->title("Mathtext: $E = mc^2$");
  p->xlabel("$\\mathbf{x}$ axis, $\\Delta t$ (ms)");
  p->ylabel("$\\psi(x)^2$ / $\\AA^{-1}$");
}

#endif
