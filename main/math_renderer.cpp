#include "math_renderer.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "epaper_hal.h"

namespace math {
namespace {

// ---------------------------------------------------------------------------
// Symbol table: LaTeX command → UTF-8 encoded Unicode string
// ---------------------------------------------------------------------------
struct SymbolEntry { const char *cmd; const char *utf8; };

constexpr SymbolEntry kSymbols[] = {
    // Greek lowercase
    {"alpha",          "α"}, {"beta",           "β"}, {"gamma",          "γ"},
    {"delta",          "δ"}, {"epsilon",        "ε"}, {"varepsilon",     "ε"},
    {"zeta",           "ζ"}, {"eta",            "η"}, {"theta",          "θ"},
    {"vartheta",       "ϑ"}, {"iota",           "ι"}, {"kappa",          "κ"},
    {"lambda",         "λ"}, {"mu",             "μ"}, {"nu",             "ν"},
    {"xi",             "ξ"}, {"pi",             "π"}, {"varpi",          "ϖ"},
    {"rho",            "ρ"}, {"varrho",         "ϱ"}, {"sigma",          "σ"},
    {"varsigma",       "ς"}, {"tau",            "τ"}, {"upsilon",        "υ"},
    {"phi",            "φ"}, {"varphi",         "ϕ"}, {"chi",            "χ"},
    {"psi",            "ψ"}, {"omega",          "ω"},
    // Greek uppercase
    {"Gamma",          "Γ"}, {"Delta",          "Δ"}, {"Theta",          "Θ"},
    {"Lambda",         "Λ"}, {"Xi",             "Ξ"}, {"Pi",             "Π"},
    {"Sigma",          "Σ"}, {"Upsilon",        "Υ"}, {"Phi",            "Φ"},
    {"Psi",            "Ψ"}, {"Omega",          "Ω"},
    // Arithmetic / relations
    {"pm",             "±"}, {"mp",             "∓"}, {"times",          "×"},
    {"div",            "÷"}, {"cdot",           "·"}, {"circ",           "∘"},
    {"leq",            "≤"}, {"geq",            "≥"}, {"neq",            "≠"},
    {"approx",         "≈"}, {"equiv",          "≡"}, {"sim",            "∼"},
    {"simeq",          "≃"}, {"cong",           "≅"}, {"propto",         "∝"},
    {"ll",             "≪"}, {"gg",             "≫"},
    // Sets / logic
    {"in",             "∈"}, {"notin",          "∉"}, {"subset",         "⊂"},
    {"subseteq",       "⊆"}, {"supset",         "⊃"}, {"supseteq",       "⊇"},
    {"cap",            "∩"}, {"cup",            "∪"}, {"emptyset",       "∅"},
    {"forall",         "∀"}, {"exists",         "∃"}, {"nexists",        "∄"},
    {"neg",            "¬"}, {"wedge",          "∧"}, {"vee",            "∨"},
    // Calculus / analysis
    {"infty",          "∞"}, {"partial",        "∂"}, {"nabla",          "∇"},
    {"sum",            "∑"}, {"prod",           "∏"}, {"int",            "∫"},
    {"oint",           "∮"}, {"iint",           "∬"},
    // Arrows
    {"to",             "→"}, {"rightarrow",     "→"}, {"leftarrow",      "←"},
    {"uparrow",        "↑"}, {"downarrow",      "↓"}, {"leftrightarrow", "↔"},
    {"Rightarrow",     "⇒"}, {"Leftarrow",      "⇐"}, {"Leftrightarrow", "⇔"},
    {"mapsto",         "↦"},
    // Misc
    {"ldots",          "…"}, {"cdots",          "⋯"}, {"vdots",          "⋮"},
    {"bullet",         "•"}, {"langle",         "⟨"}, {"rangle",         "⟩"},
    {"quad",           " "}, {"qquad",          "  "},{"hbar",           "ℏ"},
    {"ell",            "ℓ"}, {"Re",             "ℜ"}, {"Im",             "ℑ"},
    {" ",              " "}, {",",              " "}, {";",              " "},
    {":",              " "}, {"!",              ""},
    // Escaped symbols
    {"{",              "{"}, {"}",              "}"}, {"_",              "_"},
    {"^",              "^"}, {"&",              "&"}, {"#",              "#"},
    // Size decorators (ignored)
    {"left",           ""}, {"right",           ""}, {"big",             ""},
    {"Big",            ""}, {"bigg",            ""}, {"Bigg",            ""},
    {nullptr, nullptr}
};

const char *lookupSymbol(const std::string &cmd)
{
    for (int i = 0; kSymbols[i].cmd; ++i) {
        if (cmd == kSymbols[i].cmd) return kSymbols[i].utf8;
    }
    return nullptr;
}

bool isGreekCommand(const std::string &cmd)
{
    static const char *const kGreek[] = {
        "alpha", "beta", "gamma", "delta", "epsilon", "varepsilon", "zeta",
        "eta", "theta", "vartheta", "iota", "kappa", "lambda", "mu", "nu",
        "xi", "pi", "varpi", "rho", "varrho", "sigma", "varsigma", "tau",
        "upsilon", "phi", "varphi", "chi", "psi", "omega", "Gamma", "Delta",
        "Theta", "Lambda", "Xi", "Pi", "Sigma", "Upsilon", "Phi", "Psi",
        "Omega", nullptr
    };
    for (int i = 0; kGreek[i]; ++i) {
        if (cmd == kGreek[i]) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Lexer — index-based so positions can be saved/restored safely
// ---------------------------------------------------------------------------
enum class TokKind { Char, Command, LBrace, RBrace, Super, Sub, End };

struct Token {
    TokKind kind{TokKind::End};
    std::string val;
};

struct Lexer {
    const std::string &src;
    std::size_t pos{0};

    explicit Lexer(const std::string &s, std::size_t start = 0)
        : src(s), pos(start) {}

    bool done() const { return pos >= src.size(); }

    std::size_t tell() const { return pos; }
    void seek(std::size_t p) { pos = p; }

    Token peek() { auto p = pos; Token t = next(); pos = p; return t; }

    Token next()
    {
        while (pos < src.size() && src[pos] == ' ') ++pos;
        if (pos >= src.size()) return {TokKind::End, {}};

        char c = src[pos];
        if (c == '{') { ++pos; return {TokKind::LBrace, {}}; }
        if (c == '}') { ++pos; return {TokKind::RBrace, {}}; }
        if (c == '^') { ++pos; return {TokKind::Super,  {}}; }
        if (c == '_') { ++pos; return {TokKind::Sub,    {}}; }

        if (c == '\\') {
            ++pos;
            if (pos >= src.size()) return {TokKind::Char, "\\"};
            if (!std::isalpha(static_cast<unsigned char>(src[pos]))) {
                const std::string escaped(1, src[pos++]);
                return {TokKind::Command, escaped};
            }
            std::string name;
            while (pos < src.size() && std::isalpha(static_cast<unsigned char>(src[pos]))) {
                name += src[pos++];
            }
            if (pos < src.size() && src[pos] == ' ') ++pos;
            return {TokKind::Command, std::move(name)};
        }

        // UTF-8 sequence
        const unsigned char uc = static_cast<unsigned char>(c);
        std::size_t len = 1;
        if ((uc & 0xE0) == 0xC0)      len = 2;
        else if ((uc & 0xF0) == 0xE0) len = 3;
        else if ((uc & 0xF8) == 0xF0) len = 4;
        len = std::min(len, src.size() - pos);
        std::string ch = src.substr(pos, len);
        pos += len;
        return {TokKind::Char, std::move(ch)};
    }
};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static int renderAtom(Lexer &lex, int x, int baseline_y, int y_off,
                      bool measure_only, bool black);
static int renderGroup(Lexer &lex, int x, int baseline_y, int y_off,
                       bool measure_only, bool black);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void draw(int x, int y, const std::string &text, bool black)
{
    epaper::drawTextLine(x, y, text, black);
}

static int width(const std::string &text)
{
    return epaper::textWidth(text);
}

struct FontScope {
    explicit FontScope(const std::string *cmd)
    {
        if (cmd == nullptr) {
            epaper::setUiFont();
        } else if (isGreekCommand(*cmd)) {
            epaper::setMathGreekFont();
        } else {
            epaper::setMathSymbolFont();
        }
    }

    ~FontScope()
    {
        epaper::setUiFont();
    }
};

static void drawSymbol(int x, int y, const std::string &text, const std::string *cmd, bool black)
{
    FontScope scope(cmd);
    epaper::drawTextLine(x, y, text, black);
}

static int widthSymbol(const std::string &text, const std::string *cmd)
{
    FontScope scope(cmd);
    return epaper::textWidth(text);
}

// ---------------------------------------------------------------------------
// Render a { group } or single atom, returns x after rendering
// ---------------------------------------------------------------------------
static int renderGroup(Lexer &lex, int x, int baseline_y, int y_off,
                       bool measure_only, bool black)
{
    if (lex.peek().kind == TokKind::LBrace) {
        lex.next();  // consume '{'
        int cx = x;
        while (true) {
            TokKind pk = lex.peek().kind;
            if (pk == TokKind::RBrace || pk == TokKind::End) break;
            cx = renderAtom(lex, cx, baseline_y, y_off, measure_only, black);
        }
        if (lex.peek().kind == TokKind::RBrace) lex.next();
        return cx;
    }
    return renderAtom(lex, x, baseline_y, y_off, measure_only, black);
}

// ---------------------------------------------------------------------------
// \frac{num}{den}  — caller has already consumed the \frac command token
// ---------------------------------------------------------------------------
static int renderFrac(Lexer &lex, int x, int baseline_y,
                      bool measure_only, bool black)
{
    const int lh = epaper::lineHeight();

    // Always measure both groups first to compute layout
    const std::size_t start = lex.tell();
    const int num_w = renderGroup(lex, 0, 0, 0, /*measure_only=*/true, false);
    const std::size_t after_num = lex.tell();
    const int den_w = renderGroup(lex, 0, 0, 0, /*measure_only=*/true, false);
    const std::size_t after_den = lex.tell();

    const int frac_w = std::max(num_w, den_w) + 6;
    const int bar_y  = baseline_y - lh / 2 - 2;
    const int num_y  = bar_y - 3;
    const int den_y  = bar_y + lh + 3;
    const int num_x  = x + (frac_w - num_w) / 2;
    const int den_x  = x + (frac_w - den_w) / 2;

    if (!measure_only) {
        lex.seek(start);
        renderGroup(lex, num_x, num_y, 0, false, black);   // numerator
        lex.seek(after_num);
        renderGroup(lex, den_x, den_y, 0, false, black);   // denominator
        epaper::drawHLine(x + 1, bar_y, frac_w - 2, black);
    }

    lex.seek(after_den);
    return x + frac_w;
}

// ---------------------------------------------------------------------------
// \sqrt{arg}  — caller has already consumed the \sqrt command token
// ---------------------------------------------------------------------------
static int renderSqrt(Lexer &lex, int x, int baseline_y,
                      bool measure_only, bool black)
{
    const int lh = epaper::lineHeight();

    // Optional [n] exponent — skip if present
    if (!lex.done() && lex.peek().kind == TokKind::LBrace) {
        // standard \sqrt{arg} — no optional exponent token to skip
    }

    const std::size_t start = lex.tell();
    const int arg_w = renderGroup(lex, 0, 0, 0, /*measure_only=*/true, false);
    const std::size_t after_arg = lex.tell();

    const int sym_w   = width("√");
    const int total_w = sym_w + arg_w + 2;

    if (!measure_only) {
        draw(x, baseline_y, "√", black);
        // Overline above the argument
        epaper::drawHLine(x + sym_w, baseline_y - lh + 2, arg_w + 2, black);
        lex.seek(start);
        renderGroup(lex, x + sym_w, baseline_y, 0, false, black);
    }

    lex.seek(after_arg);
    return x + total_w;
}

// ---------------------------------------------------------------------------
// Render one atom, returns x after rendering
// ---------------------------------------------------------------------------
static int renderAtom(Lexer &lex, int x, int baseline_y, int y_off,
                      bool measure_only, bool black)
{
    const int lh = epaper::lineHeight();
    Token t = lex.next();

    switch (t.kind) {
    case TokKind::End:
        return x;

    case TokKind::RBrace:
        return x;

    case TokKind::LBrace: {
        // Bare '{' group
        int cx = x;
        while (true) {
            TokKind pk = lex.peek().kind;
            if (pk == TokKind::RBrace || pk == TokKind::End) break;
            cx = renderAtom(lex, cx, baseline_y, y_off, measure_only, black);
        }
        if (lex.peek().kind == TokKind::RBrace) lex.next();
        return cx;
    }

    case TokKind::Char: {
        const int w = width(t.val);
        if (!measure_only) {
            epaper::setUiFont();
            draw(x, baseline_y + y_off, t.val, black);
        }
        return x + w;
    }

    case TokKind::Command: {
        if (t.val == "frac") {
            return renderFrac(lex, x, baseline_y + y_off, measure_only, black);
        }
        if (t.val == "sqrt") {
            return renderSqrt(lex, x, baseline_y + y_off, measure_only, black);
        }
        // Ignore \text{...} — render inner content literally
        if (t.val == "text") {
            return renderGroup(lex, x, baseline_y, y_off, measure_only, black);
        }
        const char *sym = lookupSymbol(t.val);
        if (sym) {
            if (sym[0] == '\0') return x;  // intentionally empty (size decorators)
            const int w = widthSymbol(sym, &t.val);
            if (!measure_only) drawSymbol(x, baseline_y + y_off, sym, &t.val, black);
            return x + w;
        }
        // Unknown: show \name
        const std::string fb = "\\" + t.val;
        const int w = width(fb);
        if (!measure_only) {
            epaper::setUiFont();
            draw(x, baseline_y + y_off, fb, black);
        }
        return x + w;
    }

    case TokKind::Super:
        return renderGroup(lex, x, baseline_y, y_off - lh + 4, measure_only, black);

    case TokKind::Sub:
        return renderGroup(lex, x, baseline_y, y_off + 5, measure_only, black);
    }
    return x;
}

// ---------------------------------------------------------------------------
// Top-level driver
// ---------------------------------------------------------------------------
static int runRender(const std::string &inner, int x, int baseline_y,
                     bool measure_only, bool black)
{
    Lexer lex(inner);
    int cx = x;
    while (!lex.done()) {
        if (lex.peek().kind == TokKind::End) break;
        cx = renderAtom(lex, cx, baseline_y, 0, measure_only, black);
    }
    return cx;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string stripDelimiters(const std::string &expr)
{
    std::string s = expr;
    if (s.size() >= 4 && s.substr(0, 2) == "$$" && s.substr(s.size() - 2) == "$$") {
        s = s.substr(2, s.size() - 4);
    } else if (s.size() >= 2 && s.front() == '$' && s.back() == '$') {
        s = s.substr(1, s.size() - 2);
    }
    const auto ws0 = s.find_first_not_of(" \t");
    if (ws0 == std::string::npos) return {};
    const auto ws1 = s.find_last_not_of(" \t");
    return s.substr(ws0, ws1 - ws0 + 1);
}

int render(int x, int baseline_y, const std::string &expr, bool black)
{
    const std::string inner = stripDelimiters(expr);
    if (inner.empty()) return x;
    const int result = runRender(inner, x, baseline_y, false, black);
    epaper::setUiFont();
    return result;
}

int measure(const std::string &expr)
{
    const std::string inner = stripDelimiters(expr);
    if (inner.empty()) return 0;
    const int result = runRender(inner, 0, 0, true, false);
    epaper::setUiFont();
    return result;
}

}  // namespace math
