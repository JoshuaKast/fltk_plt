//
// Fl_Plot_Window.cxx -- top-level plot window: menu bar, dialogs, lifetime.
//

#include "Fl_Plot_Window.H"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Return_Button.H>
#include <FL/fl_ask.H>
#include <stdio.h>
#include <stdlib.h>
#include <string>

static const int MENU_H = 25;

// menu actions, stored in each item's user_data
enum {
  A_SAVE, A_CLOSE,
  A_RESET, A_ZOOM_IN, A_ZOOM_OUT, A_GRID, A_GRID_MINOR, A_LEGEND, A_TOOLBAR,
  A_ZOOM_MODE, A_PAN_MODE, A_AXES,
  A_HELP
};

Fl_Plot_Window::Fl_Plot_Window(int W, int H, const char *title)
  : Fl_Double_Window(W, H) {
  init(title);
}

Fl_Plot_Window::Fl_Plot_Window(int X, int Y, int W, int H, const char *title)
  : Fl_Double_Window(X, Y, W, H) {
  init(title);
}

void Fl_Plot_Window::init(const char *title) {
  delete_on_close_ = false;
  close_cb_ = 0;
  close_data_ = 0;
  copy_label(title ? title : "Plot");

  menu_ = new Fl_Menu_Bar(0, 0, w(), MENU_H);
  menu_->box(FL_FLAT_BOX);
#define ACT(a) menu_cb, (void *)(fl_intptr_t)(a)
  menu_->add("&File/&Save as PNG...", FL_COMMAND + 's', ACT(A_SAVE), FL_MENU_DIVIDER);
  menu_->add("&File/&Close", FL_COMMAND + 'w', ACT(A_CLOSE));
  menu_->add("&View/&Reset view", 'r', ACT(A_RESET));
  menu_->add("&View/Zoom &in", '+', ACT(A_ZOOM_IN));
  menu_->add("&View/Zoom &out", '-', ACT(A_ZOOM_OUT), FL_MENU_DIVIDER);
  menu_->add("&View/&Grid", 'g', ACT(A_GRID), FL_MENU_TOGGLE);
  menu_->add("&View/&Minor grid", 0, ACT(A_GRID_MINOR), FL_MENU_TOGGLE);
  menu_->add("&View/&Legend", 'l', ACT(A_LEGEND), FL_MENU_TOGGLE | FL_MENU_DIVIDER);
  menu_->add("&View/&Toolbar", 0, ACT(A_TOOLBAR), FL_MENU_TOGGLE);
  menu_->add("&Tools/&Zoom box", 'z', ACT(A_ZOOM_MODE), FL_MENU_RADIO);
  menu_->add("&Tools/&Pan", 'p', ACT(A_PAN_MODE), FL_MENU_RADIO | FL_MENU_DIVIDER);
  menu_->add("&Tools/&Axes...", FL_COMMAND + 'e', ACT(A_AXES));
  menu_->add("&Help/&Mouse and keys...", 0, ACT(A_HELP));
#undef ACT

  plot_ = new Fl_Plot(0, MENU_H, w(), h() - MENU_H);
  plot_->toolbar(true);
  plot_->default_interaction();
  end();

  resizable(plot_);
  size_range(320, 240);
  callback(window_cb, this);
  sync_menu();
}

void Fl_Plot_Window::show() {
  bool first = !shown();
  Fl_Double_Window::show();
  sync_menu();
  if (first) plot_->take_focus();   // keyboard bindings work right away
}

int Fl_Plot_Window::handle(int event) {
  // the plot's own keys and toolbar change state behind the menu's back
  if (event == FL_PUSH || event == FL_SHORTCUT) sync_menu();
  return Fl_Double_Window::handle(event);
}

void Fl_Plot_Window::run() {
  Fl_Widget_Tracker wt(this);
  show();
  while (wt.exists() && shown()) Fl::wait();
}

void Fl_Plot_Window::close() {
  if (close_cb_) close_cb_(this, close_data_);
  hide();
  if (delete_on_close_) Fl::delete_widget(this);
}

void Fl_Plot_Window::window_cb(Fl_Widget *, void *data) {
  // Esc belongs to the plot (cancel zoom box), not to closing the window
  if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) return;
  ((Fl_Plot_Window *)data)->close();
}

void Fl_Plot_Window::sync_menu() {
  const Fl_Menu_Item *m = menu_->menu();
  if (!m) return;
  for (int i = 0; i < menu_->size(); i++) {
    Fl_Menu_Item &it = const_cast<Fl_Menu_Item &>(m[i]);
    if (it.callback() != menu_cb) continue;
    int on;
    switch ((int)(fl_intptr_t)it.user_data()) {
      case A_GRID:       on = plot_->grid(); break;
      case A_GRID_MINOR: on = plot_->grid_minor(); break;
      case A_LEGEND:     on = plot_->legend(); break;
      case A_TOOLBAR:    on = plot_->toolbar(); break;
      case A_ZOOM_MODE:  on = plot_->drag_mode(FL_LEFT_MOUSE) == FL_PLOT_DRAG_ZOOM_BOX; break;
      case A_PAN_MODE:   on = plot_->drag_mode(FL_LEFT_MOUSE) == FL_PLOT_DRAG_PAN; break;
      default: continue;
    }
    if (on) it.set(); else it.clear();
  }
}

void Fl_Plot_Window::menu_cb(Fl_Widget *w, void *data) {
  Fl_Plot_Window *win = (Fl_Plot_Window *)w->window();
  Fl_Plot *p = win->plot_;
  switch ((int)(fl_intptr_t)data) {
    case A_SAVE:       win->save_dialog(); break;
    case A_CLOSE:      win->close(); return;   // may be deleted from here on
    case A_RESET:      p->reset_zoom(); break;
    case A_ZOOM_IN:    p->zoom_in(); break;
    case A_ZOOM_OUT:   p->zoom_out(); break;
    case A_GRID:       p->grid(!p->grid()); break;
    case A_GRID_MINOR: p->grid_minor(!p->grid_minor()); break;
    case A_LEGEND:     p->legend(!p->legend()); break;
    case A_TOOLBAR:    p->toolbar(!p->toolbar()); break;
    case A_ZOOM_MODE:  p->drag_mode(FL_LEFT_MOUSE, FL_PLOT_DRAG_ZOOM_BOX); break;
    case A_PAN_MODE:   p->drag_mode(FL_LEFT_MOUSE, FL_PLOT_DRAG_PAN); break;
    case A_AXES:       win->axes_dialog(); break;
    case A_HELP:       win->help_dialog(); break;
  }
  p->redraw();
  win->sync_menu();
}

void Fl_Plot_Window::help_dialog() {
  fl_message_title("Mouse and keys");
  fl_message("%s",
    "Left drag\tzoom box (pan in pan mode)\n"
    "Shift+left, middle or right drag\tpan\n"
    "Drag on the x or y tick labels\tpan that axis\n"
    "Wheel\tzoom about the cursor\n"
    "Shift / Ctrl + wheel, or wheel over an axis\tzoom x / y only\n"
    "Double click, r, Home\treset view\n"
    "+ / -\tzoom in / out\n"
    "Arrow keys\tpan\n"
    "g / l\ttoggle grid / legend\n"
    "z / p\tleft drag zooms / pans\n"
    "Esc\tcancel zoom box");
}

// ---------------------------------------------------------------------------
// Axes dialog
// ---------------------------------------------------------------------------

namespace {
struct Axes_Dialog {
  Fl_Double_Window *win;
  Fl_Input *title, *xlabel, *ylabel;
  Fl_Float_Input *lim[4];            // x min, x max, y min, y max
  Fl_Check_Button *xauto, *yauto;
  double v[4];
  bool ok;
};
}

static bool parse(const char *s, double &v) {
  char *end;
  v = strtod(s, &end);
  while (*end == ' ') end++;
  return end != s && !*end && v - v == 0;   // number, nothing after, finite
}

static void axes_auto_cb(Fl_Widget *, void *data) {
  Axes_Dialog *d = (Axes_Dialog *)data;
  for (int i = 0; i < 4; i++) {
    bool a = (i < 2 ? d->xauto : d->yauto)->value() != 0;
    if (a) d->lim[i]->deactivate(); else d->lim[i]->activate();
  }
}

static void axes_ok_cb(Fl_Widget *, void *data) {
  Axes_Dialog *d = (Axes_Dialog *)data;
  for (int a = 0; a < 2; a++) {
    if ((a == 0 ? d->xauto : d->yauto)->value()) continue;
    const char *name = a == 0 ? "x" : "y";
    if (!parse(d->lim[2 * a]->value(), d->v[2 * a]) ||
        !parse(d->lim[2 * a + 1]->value(), d->v[2 * a + 1])) {
      fl_alert("The %s limits must be numbers.", name);
      return;
    }
    if (!(d->v[2 * a] < d->v[2 * a + 1])) {
      fl_alert("The %s minimum must be less than the maximum.", name);
      return;
    }
  }
  d->ok = true;
  d->win->hide();
}

static void axes_cancel_cb(Fl_Widget *, void *data) {
  ((Axes_Dialog *)data)->win->hide();
}

void Fl_Plot_Window::axes_dialog() {
  const int W = 380, H = 220;
  Axes_Dialog d;
  d.ok = false;
  Fl_Double_Window dlg(W, H, "Axes");
  d.win = &dlg;
  d.title = new Fl_Input(80, 10, 290, 25, "Title");
  d.xlabel = new Fl_Input(80, 40, 290, 25, "X label");
  d.ylabel = new Fl_Input(80, 70, 290, 25, "Y label");
  d.lim[0] = new Fl_Float_Input(80, 110, 100, 25, "X limits");
  d.lim[1] = new Fl_Float_Input(190, 110, 100, 25);
  d.xauto = new Fl_Check_Button(300, 110, 70, 25, "auto");
  d.lim[2] = new Fl_Float_Input(80, 140, 100, 25, "Y limits");
  d.lim[3] = new Fl_Float_Input(190, 140, 100, 25);
  d.yauto = new Fl_Check_Button(300, 140, 70, 25, "auto");
  Fl_Button *cancel = new Fl_Button(200, 185, 80, 25, "Cancel");
  Fl_Return_Button *ok = new Fl_Return_Button(290, 185, 80, 25, "OK");
  dlg.end();
  d.lim[0]->tooltip("minimum");
  d.lim[1]->tooltip("maximum");
  d.lim[2]->tooltip("minimum");
  d.lim[3]->tooltip("maximum");
  d.xauto->tooltip("Fit the x range to the data");
  d.yauto->tooltip("Fit the y range to the data");

  d.title->value(plot_->title());
  d.xlabel->value(plot_->xlabel());
  d.ylabel->value(plot_->ylabel());
  double cur[4] = { plot_->xmin(), plot_->xmax(), plot_->ymin(), plot_->ymax() };
  std::string start[4];
  for (int i = 0; i < 4; i++) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%.10g", cur[i]);
    d.lim[i]->value(buf);
    start[i] = buf;
  }
  d.xauto->value(plot_->xlim_is_auto());
  d.yauto->value(plot_->ylim_is_auto());
  d.xauto->callback(axes_auto_cb, &d);
  d.yauto->callback(axes_auto_cb, &d);
  axes_auto_cb(0, &d);
  ok->callback(axes_ok_cb, &d);
  cancel->callback(axes_cancel_cb, &d);

  if (shown()) dlg.position(x() + (w() - W) / 2, y() + (h() - H) / 2);
  dlg.set_modal();
  dlg.show();
  while (dlg.shown()) Fl::wait();
  if (!d.ok) return;

  plot_->title(d.title->value());
  plot_->xlabel(d.xlabel->value());
  plot_->ylabel(d.ylabel->value());
  // Touch the limits only if the user changed them, so that OK on an
  // untouched dialog keeps the current zoom.
  bool limits = false;
  for (int a = 0; a < 2; a++) {
    bool want_auto = (a == 0 ? d.xauto : d.yauto)->value() != 0;
    bool was_auto = a == 0 ? plot_->xlim_is_auto() : plot_->ylim_is_auto();
    if (want_auto) {
      if (was_auto) continue;
      if (a == 0) plot_->xlim_auto(); else plot_->ylim_auto();
    } else {
      if (!was_auto && start[2 * a] == d.lim[2 * a]->value() &&
          start[2 * a + 1] == d.lim[2 * a + 1]->value()) continue;
      if (a == 0) plot_->xlim(d.v[0], d.v[1]); else plot_->ylim(d.v[2], d.v[3]);
    }
    limits = true;
  }
  if (limits) plot_->reset_zoom();
  plot_->redraw();
}
