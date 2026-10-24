// custom_bindings: no default interaction; everything is wired by hand.
//
//  - left click in the top plot drops a marker + label at the data point
//  - ctrl+drag draws a zoom box (via the zoom_box_* API)
//  - wheel zoom on x only, linked to the bottom plot through view_callback
//  - application buttons call the plot actions directly
//  - mouse position is shown in both screen and data coordinates

#include "Fl_Plot.H"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <math.h>
#include <stdio.h>
#include <vector>

static Fl_Plot *top, *bottom;
static Fl_Box *info;
static Fl_Plot_Line *picks;
static std::vector<double> px, py;

static int on_event(Fl_Plot *p, const Fl_Plot_Event &e, void *) {
  static char buf[160];
  snprintf(buf, sizeof(buf), "screen (%d, %d)   data (%.4g, %.4g)   region %d",
           e.sx, e.sy, e.x, e.y, (int)e.region);
  info->copy_label(buf);
  if (e.region != FL_PLOT_IN_AXES && e.event != FL_DRAG && e.event != FL_RELEASE) return 0;
  switch (e.event) {
    case FL_PUSH:
      if (e.button == FL_LEFT_MOUSE && (e.state & FL_CTRL)) {
        p->zoom_box_begin(e.sx, e.sy);
        return 1;
      }
      if (e.button == FL_LEFT_MOUSE) {
        px.push_back(e.x);
        py.push_back(e.y);
        picks->data(&px[0], &py[0], px.size());
        char lbl[64];
        snprintf(lbl, sizeof(lbl), "  $P_{%d}$", (int)px.size());
        Fl_Plot_Annotation *a = p->text(e.x, e.y, lbl);
        a->align(FL_PLOT_LEFT, FL_PLOT_VCENTER);
        p->redraw();
        return 1;
      }
      return 0;
    case FL_DRAG:
      if (p->zoom_box_active()) { p->zoom_box_update(e.sx, e.sy); return 1; }
      return 0;
    case FL_RELEASE:
      if (p->zoom_box_active()) { p->zoom_box_end(); return 1; }
      return 0;
    case FL_MOUSEWHEEL:
      p->zoom(pow(1.1, e.wheel_dy), e.x, e.y, FL_PLOT_X);
      return 1;
  }
  return 0;
}

// keep the bottom plot's x range equal to the top plot's
static void on_view(Fl_Plot *p, void *) {
  if (bottom->xmin() != p->xmin() || bottom->xmax() != p->xmax())
    bottom->view(p->xmin(), p->xmax(), bottom->ymin(), bottom->ymax());
}

static void zoom_in_cb(Fl_Widget *, void *) { top->zoom_in(FL_PLOT_X); }
static void zoom_out_cb(Fl_Widget *, void *) { top->zoom_out(FL_PLOT_X); }
static void reset_cb(Fl_Widget *, void *) { top->reset_zoom(); bottom->reset_zoom(); }
static void clear_cb(Fl_Widget *, void *) {
  px.clear(); py.clear();
  for (int i = top->items() - 1; i >= 0; i--)
    if (dynamic_cast<Fl_Plot_Annotation *>(top->item(i))) top->remove(top->item(i));
  picks->data(0, 0, 0);
  top->redraw();
}
static void png_cb(Fl_Widget *, void *) {
  if (top->save_png("custom_bindings.png") == 0) info->label("saved custom_bindings.png");
}

int main(int argc, char **argv) {
  Fl_Double_Window win(800, 640, "Fl_Plot: custom bindings");
  Fl_Button *b;
  b = new Fl_Button(5, 5, 80, 25, "X zoom +"); b->callback(zoom_in_cb);
  b = new Fl_Button(90, 5, 80, 25, "X zoom -"); b->callback(zoom_out_cb);
  b = new Fl_Button(175, 5, 80, 25, "Reset"); b->callback(reset_cb);
  b = new Fl_Button(260, 5, 80, 25, "Clear"); b->callback(clear_cb);
  b = new Fl_Button(345, 5, 80, 25, "PNG"); b->callback(png_cb);

  const int n = 2000;
  std::vector<double> x(n), y(n), dy(n);
  for (int i = 0; i < n; i++) {
    x[i] = i * 0.01;
    y[i] = sin(x[i]) * cos(3.1 * x[i]);
    dy[i] = cos(x[i]) * cos(3.1 * x[i]) - 3.1 * sin(x[i]) * sin(3.1 * x[i]);
  }
  top = new Fl_Plot(0, 35, 800, 300);
  top->plot(x, y);
  picks = top->plot(0, 0, 0, "ks");
  picks->marker_size(7);
  top->ylabel("$y(t)$");
  top->title("click: pick point,  ctrl+drag: zoom box,  wheel: x zoom");
  top->event_callback(on_event);
  top->view_callback(on_view);

  bottom = new Fl_Plot(0, 335, 800, 275);
  bottom->plot(x, dy, "r");
  bottom->xlabel("$t$");
  bottom->ylabel("$\\dot{y}(t) = dy/dt$");

  info = new Fl_Box(0, 612, 800, 25);
  info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  win.end();
  win.resizable(win);
  win.show(argc, argv);
  return Fl::run();
}
