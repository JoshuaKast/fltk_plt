// bigdata: 1e6-point line + 2e5-point scatter. Zoom/pan to see the
// decimation keep up; the status line shows the time of the last redraw.

#include "demo_plots.H"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <stdio.h>
#include <chrono>

class Timed_Plot : public Fl_Plot {
public:
  Fl_Box *status;
  Timed_Plot(int X, int Y, int W, int H) : Fl_Plot(X, Y, W, H), status(0) {}
  void draw() {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    Fl_Plot::draw();
    double ms = std::chrono::duration<double, std::milli>(
                  std::chrono::steady_clock::now() - t0).count();
    static char buf[200];
    size_t verts = 0, marks = 0;
    for (int i = 0; i < items(); i++) {
      Fl_Plot_Line *l = dynamic_cast<Fl_Plot_Line *>(item(i));
      if (l) { verts += l->drawn_vertices(); marks += l->drawn_markers(); }
    }
    snprintf(buf, sizeof(buf), "  draw %.2f ms   vertices sent %zu   markers sent %zu",
             ms, verts, marks);
    if (status) { status->label(buf); status->redraw(); }
  }
};

int main(int argc, char **argv) {
  Fl_Double_Window win(900, 560, "Fl_Plot: 1.2 million points");
  Timed_Plot *p = new Timed_Plot(0, 0, 900, 530);
  demo_bigdata(p);
  p->toolbar(true);
  p->default_interaction();
  p->status = new Fl_Box(0, 530, 900, 30);
  p->status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  win.end();
  win.resizable(p);
  win.show(argc, argv);
  return Fl::run();
}
