// plot_window: Fl_Plot_Window, the ready-made plot window, used two ways.
//
//   plot_window          blocking, like plt.show(): one window, run() returns
//                        when it is closed
//   plot_window --app    an application window whose button pops up
//                        self-deleting plot windows (fire and forget)

#include "demo_plots.H"
#include "Fl_Plot_Window.H"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static int open_windows = 0;
static Fl_Box *status;

static void update_status() {
  static char buf[64];
  snprintf(buf, sizeof(buf), "%d plot window%s open", open_windows, open_windows == 1 ? "" : "s");
  status->label(buf);
}

static void on_close(Fl_Plot_Window *, void *) {
  open_windows--;
  update_status();
}

// Pop up a damped oscillation with a random frequency. Nothing to clean
// up afterwards: the window deletes itself when the user closes it.
static void new_plot_cb(Fl_Widget *, void *) {
  static int count = 0;
  double f = 0.5 + 2.0 * rand() / RAND_MAX;
  std::vector<double> y(2000);
  for (size_t i = 0; i < y.size(); i++) {
    double t = i * 0.005;
    y[i] = exp(-0.3 * t) * sin(2 * M_PI * f * t);
  }
  char title[64];
  snprintf(title, sizeof(title), "Figure %d", ++count);

  Fl_Plot_Window *w = new Fl_Plot_Window(700, 480, title);
  w->delete_on_close(true);
  w->close_callback(on_close);
  Fl_Plot *p = w->axes();
  char lbl[64];
  snprintf(lbl, sizeof(lbl), "$f = %.2f$ Hz", f);
  p->plot_sampled(y, 0.0, 0.005)->label(lbl);
  p->title("Damped oscillation");
  p->xlabel("$t$ (s)");
  p->ylabel("$y(t)$");
  p->grid(true);
  p->legend(true);
  w->show();

  open_windows++;
  update_status();
}

int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "--app")) {
    Fl_Double_Window app(320, 90, "My application");
    Fl_Button *b = new Fl_Button(10, 10, 300, 30, "New plot window");
    b->callback(new_plot_cb);
    status = new Fl_Box(10, 50, 300, 30);
    app.end();
    update_status();
    app.show();
    return Fl::run();
  }

  // Blocking use: build, fill, run. The window is destroyed with the stack.
  Fl_Plot_Window win(800, 560, "Fl_Plot_Window");
  demo_lines(win.axes());
  win.run();
  printf("plot window closed\n");
  return 0;
}
