//
// Fl_Plot_Text.cxx -- minimal "mathtext" renderer for FLTK.
//

#include "Fl_Plot_Text.H"

#include <FL/fl_draw.H>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <algorithm>

namespace {

struct Sym { const char *name, *utf8; };

const Sym symbols[] = {
  // greek, lower case
  {"alpha", "α"}, {"beta", "β"}, {"gamma", "γ"}, {"delta", "δ"},
  {"epsilon", "ϵ"}, {"varepsilon", "ε"}, {"zeta", "ζ"}, {"eta", "η"},
  {"theta", "θ"}, {"vartheta", "ϑ"}, {"iota", "ι"}, {"kappa", "κ"},
  {"lambda", "λ"}, {"mu", "μ"}, {"nu", "ν"}, {"xi", "ξ"},
  {"omicron", "ο"}, {"pi", "π"}, {"varpi", "ϖ"}, {"rho", "ρ"},
  {"varrho", "ϱ"}, {"sigma", "σ"}, {"varsigma", "ς"}, {"tau", "τ"},
  {"upsilon", "υ"}, {"phi", "ϕ"}, {"varphi", "φ"}, {"chi", "χ"},
  {"psi", "ψ"}, {"omega", "ω"},
  // greek, upper case
  {"Gamma", "Γ"}, {"Delta", "Δ"}, {"Theta", "Θ"}, {"Lambda", "Λ"},
  {"Xi", "Ξ"}, {"Pi", "Π"}, {"Sigma", "Σ"}, {"Upsilon", "Υ"},
  {"Phi", "Φ"}, {"Psi", "Ψ"}, {"Omega", "Ω"},
  // operators and relations
  {"pm", "±"}, {"mp", "∓"}, {"times", "×"}, {"div", "÷"},
  {"cdot", "·"}, {"ast", "∗"}, {"star", "⋆"}, {"circ", "∘"},
  {"bullet", "•"}, {"oplus", "⊕"}, {"otimes", "⊗"},
  {"leq", "≤"}, {"le", "≤"}, {"geq", "≥"}, {"ge", "≥"},
  {"neq", "≠"}, {"ne", "≠"}, {"ll", "≪"}, {"gg", "≫"},
  {"approx", "≈"}, {"sim", "∼"}, {"simeq", "≃"}, {"cong", "≅"},
  {"equiv", "≡"}, {"propto", "∝"}, {"perp", "⊥"}, {"parallel", "∥"},
  {"in", "∈"}, {"notin", "∉"}, {"ni", "∋"}, {"subset", "⊂"},
  {"supset", "⊃"}, {"subseteq", "⊆"}, {"supseteq", "⊇"},
  {"cup", "∪"}, {"cap", "∩"}, {"wedge", "∧"}, {"vee", "∨"},
  {"neg", "¬"}, {"forall", "∀"}, {"exists", "∃"}, {"emptyset", "∅"},
  // big operators and calculus
  {"sum", "∑"}, {"prod", "∏"}, {"int", "∫"}, {"oint", "∮"},
  {"iint", "∬"}, {"partial", "∂"}, {"nabla", "∇"}, {"sqrt", "√"},
  {"infty", "∞"},
  // arrows
  {"leftarrow", "←"}, {"rightarrow", "→"}, {"to", "→"},
  {"uparrow", "↑"}, {"downarrow", "↓"}, {"leftrightarrow", "↔"},
  {"Leftarrow", "⇐"}, {"Rightarrow", "⇒"}, {"Leftrightarrow", "⇔"},
  {"mapsto", "↦"},
  // misc
  {"degree", "°"}, {"circ", "∘"}, {"angle", "∠"}, {"hbar", "ℏ"},
  {"ell", "ℓ"}, {"Re", "ℜ"}, {"Im", "ℑ"}, {"aleph", "ℵ"},
  {"AA", "Å"}, {"prime", "′"}, {"dagger", "†"}, {"ddagger", "‡"},
  {"ldots", "…"}, {"cdots", "⋯"}, {"vdots", "⋮"},
  {"langle", "⟨"}, {"rangle", "⟩"}, {"lfloor", "⌊"}, {"rfloor", "⌋"},
  {"lceil", "⌈"}, {"rceil", "⌉"}, {"S", "§"}, {"P", "¶"},
  {"minus", "−"},
  {0, 0}
};

inline Fl_Font with_style(Fl_Font base, int bits, bool set) {
  // only the 12 builtin families have style variants at fixed offsets
  if (base >= FL_SYMBOL) return base;
  return set ? (base | bits) : (base & ~bits);
}

class Builder {
public:
  explicit Builder(std::vector<Fl_Plot_Text::Run> &runs)
    : runs_(runs), x_(0), run_w_(0) {}

  struct State { Fl_Font font; double size; double dy; };

  double x() const { return x_; }

  void emit(const State &s, const char *text, size_t len) {
    if (!len) return;
    fl_font(s.font, (int)floor(s.size + 0.5));
    double w = fl_width(text, (int)len);
    if (!runs_.empty()) {
      Fl_Plot_Text::Run &r = runs_.back();
      if (r.font == s.font && r.size == s.size && r.dy == s.dy &&
          fabs(r.x + run_w_ - x_) < 1e-9) {
        r.text.append(text, len);
        run_w_ = r.w = fl_width(r.text.c_str(), (int)r.text.size());
        x_ = r.x + run_w_;
        return;
      }
    }
    Fl_Plot_Text::Run r;
    r.text.assign(text, len);
    r.font = s.font; r.size = s.size; r.x = x_; r.dy = s.dy; r.w = w;
    runs_.push_back(r);
    run_w_ = w;
    x_ += w;
  }
  // a run placed at x that does not advance (accents); text empty = rule of width w
  void emit_at(const State &s, const char *text, double x, double w) {
    Fl_Plot_Text::Run r;
    r.text = text;
    r.font = s.font; r.size = s.size; r.x = x; r.dy = s.dy; r.w = w;
    runs_.push_back(r);
    run_w_ = -1e30;   // never merge into this run
  }
  void space(double w) { x_ += w; }
  void set_x(double x) { x_ = x; }

  // plain text until '$' (unescaped) or end
  void text(const char *&p, const State &s) {
    std::string buf;
    while (*p && *p != '$') {
      if (*p == '\\' && p[1] == '$') { buf += '$'; p += 2; continue; }
      buf += *p++;
    }
    emit(s, buf.data(), buf.size());
  }

  // math until closing '$' or end
  void math(const char *&p, const State &s) {
    while (*p && *p != '$') {
      if (*p == '}') { p++; continue; }   // stray brace
      atom_with_scripts(p, s);
    }
    if (*p == '$') p++;
  }

private:
  void group(const char *&p, const State &s) {   // after '{'
    while (*p && *p != '}' && *p != '$') atom_with_scripts(p, s);
    if (*p == '}') p++;
  }

  void atom_with_scripts(const char *&p, const State &s) {
    atom(p, s);
    // scripts: x^a, x_b, x_b^a and x^a_b stack at the same x
    double start = x_, end = x_;
    int done = 0;
    while ((*p == '^' || *p == '_') && done < 2) {
      bool sup = *p == '^';
      p++;
      State ss = s;
      ss.size = std::max(6.0, s.size * 0.7);
      ss.dy = s.dy + (sup ? 0.42 * s.size : -0.22 * s.size);
      x_ = start;
      atom(p, ss);
      end = std::max(end, x_);
      done++;
    }
    x_ = end;
  }

  void atom(const char *&p, const State &s) {
    if (!*p || *p == '$') return;
    if (*p == '{') { p++; group(p, s); return; }
    if (*p == '\\') { p++; command(p, s); return; }
    // one UTF-8 code point
    const char *q = p + 1;
    while ((*q & 0xC0) == 0x80) q++;
    emit(s, p, q - p);
    p = q;
  }

  void command(const char *&p, const State &s) {
    if (!*p) return;
    if (!isalpha((unsigned char)*p)) {
      char c = *p++;
      fl_font(s.font, (int)floor(s.size + 0.5));
      double em = fl_width("M");
      switch (c) {
        case ',': space(em / 6); return;
        case ':': case '>': space(em * 2 / 9); return;
        case ';': space(em * 5 / 18); return;
        case '!': space(-em / 6); return;
        case ' ': space(fl_width(" ")); return;
        default: emit(s, &c, 1); return;   // \{ \} \$ \_ \^ \\ \% \# \&
      }
    }
    std::string name;
    while (isalpha((unsigned char)*p)) name += *p++;
    if (name == "quad" || name == "qquad") {
      fl_font(s.font, (int)floor(s.size + 0.5));
      space(fl_width("M") * (name == "quad" ? 1 : 2));
      return;
    }
    State fs = s;
    bool font_cmd = true;
    if (name == "mathrm" || name == "text" || name == "mathsf" || name == "rm")
      fs.font = with_style(s.font, FL_BOLD | FL_ITALIC, false);
    else if (name == "mathit" || name == "it")
      fs.font = with_style(with_style(s.font, FL_BOLD, false), FL_ITALIC, true);
    else if (name == "mathbf" || name == "bf")
      fs.font = with_style(with_style(s.font, FL_ITALIC, false), FL_BOLD, true);
    else if (name == "mathtt")
      fs.font = FL_COURIER;
    else
      font_cmd = false;
    if (font_cmd) {
      while (*p == ' ') p++;
      atom(p, fs);
      return;
    }
    if (name == "frac") {   // inline: {a}/{b}
      while (*p == ' ') p++;
      atom(p, s);
      emit(s, "/", 1);
      while (*p == ' ') p++;
      atom(p, s);
      return;
    }
    const char *acc = 0;   // accents: rule or spacing glyph above the atom
    if (name == "bar" || name == "overline" || name == "sqrt") acc = "";
    else if (name == "hat") acc = "\u02C6";
    else if (name == "tilde") acc = "\u02DC";
    else if (name == "dot") acc = "\u02D9";
    else if (name == "ddot") acc = "\u00A8";
    else if (name == "vec") acc = "\u2192";
    if (acc) {
      while (*p == ' ') p++;
      if (name == "sqrt") emit(s, "\u221A", 3);
      double x0 = x_;
      const char *a0 = p;
      atom(p, s);
      double x1 = x_;
      bool tall = false;
      for (const char *q = a0; q < p; q++)
        if (isupper((unsigned char)*q) || isdigit((unsigned char)*q) ||
            strchr("bdfhiklt\\(", *q) || (*q & 0x80)) tall = true;
      State as = s;
      if (!*acc) {
        as.dy = s.dy + (tall || name == "sqrt" ? 0.86 : 0.66) * s.size;
        emit_at(as, "", x0 + 0.5, x1 - x0 - 0.5);
      } else {
        if (name == "vec") as.size = s.size * 0.6;
        fl_font(as.font, (int)floor(as.size + 0.5));
        double aw = fl_width(acc);
        as.dy = s.dy + (tall ? 0.2 : 0.0) * s.size + (name == "vec" ? 0.45 * s.size : 0);
        emit_at(as, acc, (x0 + x1 - aw) / 2, aw);
      }
      x_ = x1;
      return;
    }
    static const char *const opnames[] = {
      "sin", "cos", "tan", "sec", "csc", "cot", "arcsin", "arccos", "arctan",
      "sinh", "cosh", "tanh", "coth", "exp", "log", "ln", "lg", "max", "min",
      "sup", "inf", "lim", "det", "dim", "ker", "arg", "deg", "gcd", "Pr", "mod", 0 };
    for (int i = 0; opnames[i]; i++)
      if (name == opnames[i]) {
        State os = s;
        os.font = with_style(s.font, FL_ITALIC, false);
        emit(os, name.data(), name.size());
        return;
      }
    const char *u = Fl_Plot_Text::symbol(name.c_str());
    if (u) {
      State ss = s;
      if (Fl_Plot_Text::symbol_font() >= 0) ss.font = Fl_Plot_Text::symbol_font();
      emit(ss, u, strlen(u));
    } else {
      std::string raw = "\\" + name;   // unknown: show literally
      emit(s, raw.data(), raw.size());
    }
    if (*p == ' ' && !u) p++;
  }

  std::vector<Fl_Plot_Text::Run> &runs_;
  double x_;
  double run_w_;
};

} // namespace

Fl_Font Fl_Plot_Text::symbol_font_ = -1;

const char *Fl_Plot_Text::symbol(const char *name) {
  for (const Sym *s = symbols; s->name; s++)
    if (!strcmp(s->name, name)) return s->utf8;
  return 0;
}

void Fl_Plot_Text::layout(const char *str, Fl_Font font, int size) {
  runs_.clear();
  width_ = ascent_ = descent_ = 0;
  if (!str) str = "";
  Builder b(runs_);
  Builder::State s;
  s.font = font; s.size = size; s.dy = 0;
  const char *p = str;
  while (*p) {
    if (*p == '$') { p++; b.math(p, s); }
    else b.text(p, s);
  }
  width_ = b.x();
  fl_font(font, size);
  ascent_ = fl_height() - fl_descent();
  descent_ = fl_descent();
  for (size_t i = 0; i < runs_.size(); i++) {
    const Run &r = runs_[i];
    if (r.text.empty()) {   // rule
      ascent_ = std::max(ascent_, r.dy + 2);
      width_ = std::max(width_, r.x + r.w);
      continue;
    }
    fl_font(r.font, (int)floor(r.size + 0.5));
    double a = fl_height() - fl_descent(), d = fl_descent();
    ascent_ = std::max(ascent_, r.dy + a);
    descent_ = std::max(descent_, d - r.dy);
    width_ = std::max(width_, r.x + r.w);
  }
}

void Fl_Plot_Text::anchor(Fl_Plot_HAlign ha, Fl_Plot_VAlign va,
                          double &ox, double &oy) const {
  ox = ha == FL_PLOT_HCENTER ? -width_ / 2 : ha == FL_PLOT_RIGHT ? -width_ : 0;
  switch (va) {
    case FL_PLOT_TOP:     oy = -ascent_; break;
    case FL_PLOT_VCENTER: oy = -(ascent_ - descent_) / 2; break;
    case FL_PLOT_BOTTOM:  oy = descent_; break;
    default:              oy = 0; break;
  }
}

// local (advance, up) -> screen, given rotation
static inline void to_screen(double x, double y, double, double s, double c,
                             double &sx, double &sy) {
  sx = x * c - y * s;   // u = (c, -s), v = (-s, -c)
  sy = -x * s - y * c;
}

void Fl_Plot_Text::draw(double x, double y, int angle,
                        Fl_Plot_HAlign ha, Fl_Plot_VAlign va) const {
  double ox, oy;
  anchor(ha, va, ox, oy);
  double a = angle * M_PI / 180.0, s = sin(a), c = cos(a);
  for (size_t i = 0; i < runs_.size(); i++) {
    const Run &r = runs_[i];
    double dx, dy;
    to_screen(ox + r.x, oy + r.dy, a, s, c, dx, dy);
    if (r.text.empty()) {
      double ex, ey;
      to_screen(ox + r.x + r.w, oy + r.dy, a, s, c, ex, ey);
      fl_line_style(FL_SOLID | FL_CAP_FLAT, std::max(1, (int)floor(r.size / 14 + 0.5)));
      fl_line((int)floor(x + dx + 0.5), (int)floor(y + dy + 0.5),
              (int)floor(x + ex + 0.5), (int)floor(y + ey + 0.5));
      fl_line_style(0);
      continue;
    }
    fl_font(r.font, (int)floor(r.size + 0.5));
    int ix = (int)floor(x + dx + 0.5), iy = (int)floor(y + dy + 0.5);
    if (angle == 0) fl_draw(r.text.c_str(), ix, iy);
    else fl_draw(angle, r.text.c_str(), ix, iy);
  }
}

void Fl_Plot_Text::box(double x, double y, int angle, Fl_Plot_HAlign ha,
                       Fl_Plot_VAlign va, double pad, double cx[4], double cy[4]) const {
  double ox, oy;
  anchor(ha, va, ox, oy);
  double a = angle * M_PI / 180.0, s = sin(a), c = cos(a);
  double lx[4] = { ox - pad, ox + width_ + pad, ox + width_ + pad, ox - pad };
  double ly[4] = { oy - descent_ - pad, oy - descent_ - pad,
                   oy + ascent_ + pad, oy + ascent_ + pad };
  for (int i = 0; i < 4; i++) {
    double dx, dy;
    to_screen(lx[i], ly[i], a, s, c, dx, dy);
    cx[i] = x + dx; cy[i] = y + dy;
  }
}

void Fl_Plot_Text::draw(const char *str, double x, double y, Fl_Font font, int size,
                        int angle, Fl_Plot_HAlign ha, Fl_Plot_VAlign va) {
  Fl_Plot_Text t;
  t.layout(str, font, size);
  t.draw(x, y, angle, ha, va);
}

double Fl_Plot_Text::width(const char *str, Fl_Font font, int size) {
  Fl_Plot_Text t;
  t.layout(str, font, size);
  return t.width();
}
