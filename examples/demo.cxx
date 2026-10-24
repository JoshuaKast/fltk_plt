// demo: four plots showing most features, all with the default toolbar
// and mouse/keyboard bindings.

#include "demo_plots.H"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>

int main(int argc, char **argv) {
  Fl_Double_Window win(1200, 860, "Fl_Plot demo");
  Fl_Plot *p[4];
  p[0] = new Fl_Plot(0, 0, 600, 430);
  p[1] = new Fl_Plot(600, 0, 600, 430);
  p[2] = new Fl_Plot(0, 430, 600, 430);
  p[3] = new Fl_Plot(600, 430, 600, 430);
  demo_lines(p[0]);
  demo_fill(p[1]);
  demo_shapes(p[2]);
  demo_markers(p[3]);
  for (int i = 0; i < 4; i++) {
    p[i]->toolbar(true);
    p[i]->default_interaction();
  }
  win.end();
  win.resizable(win);
  win.show(argc, argv);
  return Fl::run();
}
