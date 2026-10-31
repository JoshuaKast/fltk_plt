// Render test: draws the demo plots offscreen, saves PNGs and checks that
// something sensible was drawn. Also times redraws of 1.2e6 points.
// Needs a display; on headless Linux run it under xvfb-run.
//
//   test_render [output_dir]

#include "../examples/demo_plots.H"
#include "Fl_Plot_Window.H"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <string.h>
#include <stdio.h>
#include <chrono>
#include <string>

static int failures = 0;

// Deliver a synthetic mouse event straight to a widget's handle().
static int send(Fl_Widget *w, int ev, int x, int y, int button = 1, int state = 0,
                int clicks = 0, int wheel_dy = 0) {
  Fl::e_x = x; Fl::e_y = y;
  Fl::e_keysym = FL_Button + button;
  Fl::e_state = state;
  Fl::e_clicks = clicks;
  Fl::e_dx = 0; Fl::e_dy = wheel_dy;
  return w->handle(ev);
}

static int send_key(Fl_Widget *w, int key, const char *text) {
  Fl::e_keysym = key;
  Fl::e_text = (char *)text;
  Fl::e_length = (int)strlen(text);
  Fl::e_state = 0;
  return w->handle(FL_KEYDOWN);
}

static int hook_calls = 0;
static int hook(Fl_Plot *, const Fl_Plot_Event &e, void *) {
  hook_calls++;
  return e.event == FL_PUSH && e.button == FL_RIGHT_MOUSE;   // swallow right clicks
}
static void close_window(void *w) { ((Fl_Plot_Window *)w)->close(); }
static int closed_calls = 0;
static void on_close(Fl_Plot_Window *, void *) { closed_calls++; }
static bool pick(Fl_Plot_Window *w, const char *path) {
  const Fl_Menu_Item *it = w->menubar()->find_item(path);
  if (!it) return false;
  w->menubar()->picked(it);
  return true;
}
// Drives the modal axes dialog: new title, fixed x limits [1, 2], then OK.
static void fill_axes_dialog(void *) {
  Fl_Window *d = Fl::modal();
  if (!d || d->children() < 11) return;
  ((Fl_Input *)d->child(0))->value("new title");
  ((Fl_Check_Button *)d->child(5))->value(0);
  d->child(5)->do_callback();
  ((Fl_Input *)d->child(3))->value("1");
  ((Fl_Input *)d->child(4))->value("2");
  d->child(10)->do_callback();   // OK
}
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

// fraction of pixels that differ from the corner (background) color
static double ink(Fl_RGB_Image *img) {
  const unsigned char *d = (const unsigned char *)img->data()[0];
  int W = img->w(), H = img->h(), D = img->d();
  int ld = img->ld() ? img->ld() : W * D;
  long n = 0;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      const unsigned char *p = d + y * ld + x * D;
      if (p[0] != d[0] || p[1] != d[1] || p[2] != d[2]) n++;
    }
  return n / (double)(W * H);
}

int main(int argc, char **argv) {
  std::string dir = argc > 1 ? argv[1] : ".";
  Fl::visual(FL_RGB);
  Fl_Double_Window win(1300, 900, "render test");
  Fl_Plot *p[8];
  p[0] = new Fl_Plot(0, 0, 640, 440);
  p[1] = new Fl_Plot(640, 0, 640, 440);
  p[2] = new Fl_Plot(0, 440, 640, 440);
  p[3] = new Fl_Plot(640, 440, 640, 440);
  win.end();
  demo_lines(p[0]);
  demo_fill(p[1]);
  demo_shapes(p[2]);
  demo_markers(p[3]);
  p[0]->toolbar(true);
  win.show();
  Fl::flush();

  // offscreen plot that is never shown
  p[4] = new Fl_Plot(0, 0, 800, 480);
  demo_bigdata(p[4]);
  p[5] = new Fl_Plot(0, 0, 700, 420);
  demo_mathtext(p[5]);
  p[6] = new Fl_Plot(0, 0, 600, 460);
  demo_log(p[6]);
  p[7] = new Fl_Plot(0, 0, 600, 460);
  demo_offset(p[7]);

  const char *names[] = { "lines", "fill", "shapes", "markers", "bigdata", "mathtext", "log",
                          "offset" };
  for (int i = 0; i < 8; i++) {
    std::string f = dir + "/" + names[i] + ".png";
    CHECK(p[i]->save_png(f.c_str()) == 0);
    Fl_RGB_Image *img = p[i]->image();
    CHECK(img && img->w() == p[i]->w() && img->h() == p[i]->h());
    double k = img ? ink(img) : 0;
    printf("%-8s %s  ink %.1f%%\n", names[i], f.c_str(), 100 * k);
    CHECK(k > 0.02);
    delete img;
  }

  // zoom: the view changes, and survives a redraw (no re-autoscale)
  p[4]->zoom_to(40, 41, -1, 1);
  Fl_RGB_Image *z = p[4]->image();
  CHECK(p[4]->xmin() == 40 && p[4]->xmax() == 41);
  delete z;
  p[4]->save_png((dir + "/bigdata_zoom.png").c_str());
  p[4]->reset_zoom();
  CHECK(p[4]->xmin() < 0 && p[4]->xmax() > 99);

  // coordinate round trip
  double sx, sy, dx, dy;
  p[0]->data_to_screen(3.0, 0.5, sx, sy);
  p[0]->screen_to_data(sx, sy, dx, dy);
  CHECK(fabs(dx - 3.0) < 1e-9 && fabs(dy - 0.5) < 1e-9);
  int ax, ay, aw, ah;
  p[0]->axes_box(ax, ay, aw, ah);
  CHECK(p[0]->region(ax + aw / 2, ay + ah / 2) == FL_PLOT_IN_AXES);
  CHECK(p[0]->region(ax + aw / 2, ay + ah + 10) == FL_PLOT_IN_XAXIS);
  CHECK(p[0]->region(ax - 10, ay + ah / 2) == FL_PLOT_IN_YAXIS);

  // redraw timing for 1.2e6 points, full view and after a pan (cache miss)
  Fl_Plot_Line *big = (Fl_Plot_Line *)p[4]->item(0);
  for (int pass = 0; pass < 2; pass++) {
    const int reps = 20;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; r++) {
      if (pass) p[4]->pan_pixels(3, 0);
      Fl_RGB_Image *img = p[4]->image();
      delete img;
    }
    double ms = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - t0).count() / reps;
    printf("bigdata %s: %.2f ms per offscreen render, %zu line vertices, %zu markers\n",
           pass ? "panning (recompute)" : "static (cached)  ", ms,
           big->drawn_vertices(), ((Fl_Plot_Line *)p[4]->item(1))->drawn_markers());
    CHECK(big->drawn_vertices() < 4 * 800);
  }

  // ---- interaction through handle() ----
  Fl_Plot *q = p[0];
  q->default_interaction();
  q->reset_zoom();
  Fl::flush();
  q->axes_box(ax, ay, aw, ah);
  double x0 = q->xmin(), x1 = q->xmax(), y0 = q->ymin(), y1 = q->ymax();

  // zoom box over the middle half
  CHECK(send(q, FL_PUSH, ax + aw / 4, ay + ah / 4));
  CHECK(q->zoom_box_active());
  send(q, FL_DRAG, ax + 3 * aw / 4, ay + 3 * ah / 4);
  send(q, FL_RELEASE, ax + 3 * aw / 4, ay + 3 * ah / 4);
  CHECK(!q->zoom_box_active());
  double span = (x1 - x0) / 2;
  CHECK(fabs((q->xmax() - q->xmin()) - span) < span * 0.02);

  // pan with the middle button: content follows the mouse
  double before = q->xmin();
  send(q, FL_PUSH, ax + aw / 2, ay + ah / 2, FL_MIDDLE_MOUSE);
  send(q, FL_DRAG, ax + aw / 2 + 100, ay + ah / 2, FL_MIDDLE_MOUSE);
  send(q, FL_RELEASE, ax + aw / 2 + 100, ay + ah / 2, FL_MIDDLE_MOUSE);
  CHECK(q->xmin() < before);

  // wheel over the x axis zooms x only
  double ys = q->ymax() - q->ymin(), xs = q->xmax() - q->xmin();
  CHECK(send(q, FL_MOUSEWHEEL, ax + aw / 2, ay + ah + 15, 0, 0, 0, 1));
  CHECK(fabs((q->ymax() - q->ymin()) - ys) < 1e-12);
  CHECK(fabs((q->xmax() - q->xmin()) - xs * 1.2) < 1e-9 * xs);

  // double click resets
  send(q, FL_PUSH, ax + aw / 2, ay + ah / 2, FL_LEFT_MOUSE, 0, 1);
  send(q, FL_RELEASE, ax + aw / 2, ay + ah / 2);
  CHECK(q->xmin() == x0 && q->xmax() == x1 && q->ymin() == y0 && q->ymax() == y1);

  // keyboard (needs focus)
  Fl::focus(q);
  CHECK(send_key(q, '+', "+"));
  CHECK(q->xmax() - q->xmin() < x1 - x0);
  CHECK(send_key(q, 'r', "r"));
  CHECK(q->xmin() == x0);

  // user hook sees events first and can consume them
  q->event_callback(hook);
  hook_calls = 0;
  send(q, FL_PUSH, ax + aw / 2, ay + ah / 2, FL_RIGHT_MOUSE);
  send(q, FL_DRAG, ax + aw / 2 + 50, ay + ah / 2, FL_RIGHT_MOUSE);
  send(q, FL_RELEASE, ax + aw / 2 + 50, ay + ah / 2, FL_RIGHT_MOUSE);
  CHECK(hook_calls == 3);
  CHECK(q->xmin() == x0);   // right-drag pan was swallowed at FL_PUSH
  q->event_callback(0);

  // no bindings: nothing moves
  q->no_interaction();
  send(q, FL_MOUSEWHEEL, ax + aw / 2, ay + ah / 2, 0, 0, 0, 1);
  CHECK(q->xmin() == x0);

  // whole window, toolbar included
  {
    Fl_Image_Surface surf(win.w(), win.h());
    Fl_Surface_Device::push_current(&surf);
    surf.draw(&win);
    Fl_RGB_Image *img = surf.image();
    Fl_Surface_Device::pop_current();
    fl_write_png((dir + "/window.png").c_str(), img);
    delete img;
  }

  // ---- log axes ----
  {
    Fl_Plot *g = p[6];
    CHECK(g->xscale() == FL_PLOT_LOG && g->yscale() == FL_PLOT_LOG);
    // autoscaled to the positive data, with a margin
    CHECK(g->xmin() > 0.05 && g->xmin() < 0.1 && g->xmax() > 1e5 && g->xmax() < 2e5);
    CHECK(g->ymin() > 0);
    // round trip through the log transform
    double sx, sy, dx, dy;
    g->data_to_screen(1000, 0.01, sx, sy);
    g->screen_to_data(sx, sy, dx, dy);
    CHECK(fabs(dx / 1000 - 1) < 1e-9 && fabs(dy / 0.01 - 1) < 1e-9);
    // decades are evenly spaced on screen
    double s1, s2, s3, t;
    g->data_to_screen(1, 1, s1, t);
    g->data_to_screen(10, 1, s2, t);
    g->data_to_screen(100, 1, s3, t);
    CHECK(fabs((s2 - s1) - (s3 - s2)) < 1e-6);
    // zoom keeps the geometric center, pan keeps the ratio
    double c = sqrt(g->xmin() * g->xmax()), ratio = g->xmax() / g->xmin();
    g->zoom_in();
    CHECK(fabs(sqrt(g->xmin() * g->xmax()) / c - 1) < 1e-9);
    ratio = g->xmax() / g->xmin();
    g->pan_pixels(-120, 0);
    CHECK(g->xmin() > c / sqrt(ratio) * 1.01);
    CHECK(fabs(g->xmax() / g->xmin() / ratio - 1) < 1e-9);
    delete g->image();
    // limits <= 0 are replaced on a log axis
    g->xlim(0, 1e3);
    CHECK(g->xmin() > 0 && g->xmax() == 1e3);
    g->view(-1, 10, -5, -1);
    CHECK(g->xmin() > 0 && g->ymin() > 0 && g->ymax() > g->ymin());
    g->xlim_auto();
    g->reset_zoom();
    // back to linear and log again
    g->xscale(FL_PLOT_LINEAR);
    CHECK(g->xscale() == FL_PLOT_LINEAR && g->xmin() < 0);
    delete g->image();
    g->xscale(FL_PLOT_LOG);
    CHECK(g->xmin() > 0);

    // no positive data at all: a default decade, no crash
    Fl_Plot e(0, 0, 400, 300);
    double zx[3] = { 1, 2, 3 }, zy[3] = { 0, -1, -2 };
    e.semilogy(zx, zy, 3);
    delete e.image();
    CHECK(e.ymin() > 0 && e.ymax() > e.ymin());
    for (int i = 0; i < 2000; i++) e.zoom_out();
    for (int i = 0; i < 4000; i++) e.zoom_in();
    delete e.image();
    CHECK(e.ymin() > 0 && e.ymax() > e.ymin());
  }

  // ---- small range, large offset ----
  {
    Fl_Plot *o = p[7];
    CHECK(o->xmax() - o->xmin() < 0.06 && o->xmin() > 1.7e9);
    // zoom to 0.1 ms of a Unix time axis: not widened, still drawn
    o->zoom_to(1730000000.01, 1730000000.0101, 101324.98, 101325.02);
    CHECK(fabs((o->xmax() - o->xmin()) - 1e-4) < 1e-6);
    delete o->image();
    CHECK(((Fl_Plot_Line *)o->item(0))->drawn_vertices() >= 10);
  }

  // ---- robustness ----
  {
    Fl_Plot r(0, 0, 400, 300);
    // style set after a draw reaches the next draw (cached geometry is rebuilt)
    std::vector<double> y(50);
    for (int i = 0; i < 50; i++) y[i] = sin(i * 0.2);
    Fl_Plot_Line *l = r.plot_sampled(y, 10.0, 0.5);
    CHECK(l->x_data()[0] == 10.0 && l->x_data()[49] == 10.0 + 49 * 0.5 && l->sorted());
    delete r.image();
    CHECK(l->drawn_markers() == 0);
    l->marker(FL_PLOT_CIRCLE);
    delete r.image();
    CHECK(l->drawn_markers() > 0);
    l->line_style(FL_PLOT_NO_LINE);
    delete r.image();
    CHECK(l->drawn_vertices() == 0);

    // deleting an item directly detaches it; add() moves an item between plots
    Fl_Plot_Line *l2 = r.plot(y);
    CHECK(r.items() == 2);
    delete l2;
    CHECK(r.items() == 1 && r.item(1) == 0);
    Fl_Plot r2(0, 0, 400, 300);
    r2.add(l);
    CHECK(r.items() == 0 && r2.items() == 1 && l->plot() == &r2);
    delete r.image();

    // NULL data and extreme values neither crash nor hang
    r.plot(0, 0, 10)->label("null");
    r.fill_between(0, &y[0], 0.0, y.size());
    double big[3] = { -1e308, 0, 1e308 };
    r.plot(0, big, 3);
    Fl_RGB_Image *img = r.image();
    CHECK(img != 0);
    delete img;
    CHECK(r.xmax() - r.xmin() < 1e301 && r.ymax() - r.ymin() < 1e301);
    for (int i = 0; i < 5000; i++) r.zoom_out();
    r.zoom(0, 0, 0);
    r.zoom(-2, 0, 0);
    delete r.image();
    CHECK(r.ymax() - r.ymin() > 0 && r.ymax() - r.ymin() < 1e301);
  }

  // the captured image does not include the toolbar band
  {
    Fl_Plot a(0, 0, 400, 300), b(0, 0, 400, 300);
    b.toolbar(true);
    a.title("t");
    b.title("t");
    Fl_RGB_Image *ia = a.image(), *ib = b.image();
    CHECK(ia && ib && ink(ia) == ink(ib));
    delete ia;
    delete ib;
  }

  // ---- Fl_Plot_Window ----
  {
    Fl_Plot_Window w(640, 480, "figure");
    Fl_Plot *a = w.axes();
    demo_lines(a);
    a->grid(false);
    CHECK(a && a->toolbar() && a->drag_mode(FL_LEFT_MOUSE) == FL_PLOT_DRAG_ZOOM_BOX);
    CHECK(a->y() == w.menubar()->h() && a->h() == w.h() - w.menubar()->h());
    w.show();
    Fl::flush();
    CHECK(w.shown() && Fl::focus() == a);

    // menu items act on the plot, and check marks follow the plot state
    const Fl_Menu_Item *grid = w.menubar()->find_item("&View/&Grid");
    const Fl_Menu_Item *pan = w.menubar()->find_item("&Tools/&Pan");
    const Fl_Menu_Item *zoom = w.menubar()->find_item("&Tools/&Zoom box");
    CHECK(grid && pan && zoom);
    CHECK(pick(&w, "&View/&Grid") && a->grid());
    CHECK(grid && grid->value());
    CHECK(pick(&w, "&Tools/&Pan") && a->drag_mode(FL_LEFT_MOUSE) == FL_PLOT_DRAG_PAN);
    CHECK(pan && zoom && pan->value() && !zoom->value());
    send_key(a, 'z', "z");                   // the plot's own key ...
    w.sync_menu();                           // ... shows up in the menu
    CHECK(pan && zoom && !pan->value() && zoom->value());
    CHECK(pick(&w, "&View/&Toolbar") && !a->toolbar());
    CHECK(pick(&w, "&View/&Toolbar") && a->toolbar());
    a->zoom_in();
    CHECK(pick(&w, "&View/&Reset view") && a->xmin() < 0.01);
    const Fl_Menu_Item *logy = w.menubar()->find_item("&View/Log &y axis");
    CHECK(pick(&w, "&View/Log &y axis") && a->yscale() == FL_PLOT_LOG);
    CHECK(logy && logy->value());
    CHECK(a->ymin() > 0);
    CHECK(pick(&w, "&View/Log &y axis") && a->yscale() == FL_PLOT_LINEAR);
    CHECK(logy && !logy->value());

    // axes dialog
    Fl::add_timeout(0.05, fill_axes_dialog);
    CHECK(pick(&w, "&Tools/&Axes..."));
    CHECK(!strcmp(a->title(), "new title"));
    CHECK(!a->xlim_is_auto() && a->ylim_is_auto());
    CHECK(a->xmin() == 1 && a->xmax() == 2);

    // Esc does not close the window; the close button does
    Fl::e_number = FL_SHORTCUT;
    Fl::e_keysym = FL_Escape;
    w.do_callback();
    CHECK(w.shown());

    // run() returns once the window is closed
    w.close_callback(on_close);
    closed_calls = 0;
    Fl::add_timeout(0.05, close_window, &w);
    w.run();
    CHECK(!w.shown() && closed_calls == 1);
  }
  {
    // fire and forget: the window deletes itself
    Fl_Plot_Window *w = new Fl_Plot_Window(400, 300);
    w->delete_on_close(true);
    w->axes()->plot_sampled(std::vector<double>(100, 1.0), 0, 1);
    Fl_Widget_Tracker wt(w);
    w->show();
    Fl::flush();
    CHECK(pick(w, "&File/&Close"));
    Fl::wait(0.01);
    CHECK(!wt.exists());

    // and run() copes with that
    w = new Fl_Plot_Window(400, 300);
    w->delete_on_close(true);
    Fl_Widget_Tracker wt2(w);
    Fl::add_timeout(0.05, close_window, w);
    w->run();
    Fl::wait(0.01);
    CHECK(!wt2.exists());
  }

  if (failures) { printf("%d failure(s)\n", failures); return 1; }
  printf("all render tests passed\n");
  return 0;
}
