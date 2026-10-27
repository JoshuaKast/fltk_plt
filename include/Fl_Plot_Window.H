//
// Fl_Plot_Window.H -- a ready-made top-level window holding one Fl_Plot,
// with a menu bar, the plot toolbar and the default mouse/keyboard bindings.
//
// For applications that want to pop up a plot and leave the rest to the
// window, much like a matplotlib figure window:
//
//   // blocking, like plt.show(): returns when the window is closed
//   Fl_Plot_Window win(800, 560, "Response");
//   win.axes()->plot(t, y, "b-")->label("output");
//   win.axes()->xlabel("$t$ (s)");
//   win.run();
//
//   // fire and forget from an application that already runs Fl::run()
//   Fl_Plot_Window *w = new Fl_Plot_Window(800, 560, "Spectrum");
//   w->delete_on_close(true);          // the window deletes itself
//   w->axes()->plot(f, mag);
//   w->show();
//
// Menus: File (Save as PNG, Close), View (reset/zoom, grid, minor grid,
// legend, toolbar), Tools (zoom box / pan mode, axes dialog), Help.
// menubar() returns the Fl_Menu_Bar so you can add your own entries.
//

#ifndef Fl_Plot_Window_H
#define Fl_Plot_Window_H

#include <FL/Fl_Double_Window.H>

#include "Fl_Plot.H"

class Fl_Menu_Bar;
class Fl_Plot_Window;

// Called when the window is closed (close button, File/Close or close()),
// before it is hidden and, with delete_on_close(true), deleted.
typedef void (*Fl_Plot_Window_Close_Cb)(Fl_Plot_Window *w, void *data);

class FL_EXPORT Fl_Plot_Window : public Fl_Double_Window {
public:
  Fl_Plot_Window(int W, int H, const char *title = 0);
  Fl_Plot_Window(int X, int Y, int W, int H, const char *title = 0);

  // The plot (one set of axes) filling the window below the menu bar.
  // It has its toolbar on and default_interaction() installed; change
  // either as you like.
  Fl_Plot *axes() const { return plot_; }
  // The menu bar, for adding application entries ("Analysis/FFT", ...).
  Fl_Menu_Bar *menubar() const { return menu_; }

  // ---- lifetime ----------------------------------------------------------
  // Show the window and process events until it is closed (like plt.show()).
  // Other windows keep working meanwhile. Safe with delete_on_close(true):
  // the window is gone when run() returns.
  void run();
  // Delete the window when it is closed. Only for windows created with new;
  // the default is false (the creator owns the window, as usual in FLTK).
  void delete_on_close(bool on) { delete_on_close_ = on; }
  bool delete_on_close() const { return delete_on_close_; }
  void close_callback(Fl_Plot_Window_Close_Cb cb, void *data = 0) { close_cb_ = cb; close_data_ = data; }
  // Close as if by the window's close button.
  void close();

  // ---- dialogs (also reachable from the menus) ---------------------------
  // Save the plot as PNG; see Fl_Plot::save_png_dialog() for return values.
  int save_dialog() { return plot_->save_png_dialog(); }
  // Modal dialog for title, axis labels and axis limits.
  void axes_dialog();
  // Lists the mouse and keyboard bindings.
  void help_dialog();

  // Update menu check marks from the plot state. Done automatically when a
  // menu opens or a shortcut is used; call it if you need it sooner.
  void sync_menu();

  // ---- Fl_Widget ---------------------------------------------------------
  using Fl_Double_Window::show;      // keep show(argc, argv)
  void show();
  int handle(int event);

private:
  void init(const char *title);
  static void window_cb(Fl_Widget *w, void *data);
  static void menu_cb(Fl_Widget *w, void *data);

  Fl_Plot *plot_;
  Fl_Menu_Bar *menu_;
  bool delete_on_close_;
  Fl_Plot_Window_Close_Cb close_cb_;
  void *close_data_;
};

#endif
