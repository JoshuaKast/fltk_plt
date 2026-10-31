// scales: log axes and a small range far from zero (tick label offsets).

#include "demo_plots.H"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>

int main(int argc, char **argv) {
  Fl_Double_Window win(1200, 460, "Fl_Plot scales");
  Fl_Plot *a = new Fl_Plot(0, 0, 600, 460);
  Fl_Plot *b = new Fl_Plot(600, 0, 600, 460);
  demo_log(a);
  demo_offset(b);
  a->toolbar(true);
  b->toolbar(true);
  a->default_interaction();
  b->default_interaction();
  win.end();
  win.resizable(win);
  win.show(argc, argv);
  return Fl::run();
}
