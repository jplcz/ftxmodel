#include <ftxmodel/unicode_text_scaler.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <string>
#include <string_view>

#include "ftxui/component/component.hpp"

using namespace ftxui;
using namespace ftxmodel;

int main() {
  // ------------------------------------------------------------------------
  // MOCK DATA DATASTREAM SAMPLES
  // ------------------------------------------------------------------------
  constexpr std::string_view kAsciiShort = "Metrics OK";
  constexpr std::string_view kUtf8Short = "📊 进程 active";
  constexpr std::string_view kLongStream =
      "The quick brown fox jumps over the lazy dog splitting words perfectly "
      "across terminal grid matrices.";
  constexpr std::string_view kMultiLineRaw =
      "Warning!\nDatabase Connection Lost.\nRetrying authentication "
      "handshakes...";

  // ============================================================================
  // PIPELINE VARIANT GENERATIONS
  // ============================================================================

  // Case A: Standard Left-Aligned Data Row (Ultra Fast-Path Bypass)
  FormattingOptions opts_a;
  opts_a.min_width = 16;
  opts_a.alignment = Alignment::Left;
  std::string out_a = UnicodeTextScaler::FormatText(kAsciiShort, opts_a);

  // Case B: Right-Aligned Metric Cell with UTF-8 Icons
  FormattingOptions opts_b;
  opts_b.min_width = 16;
  opts_b.alignment = Alignment::Right;
  std::string out_b = UnicodeTextScaler::FormatText(kUtf8Short, opts_b);

  // Case C: Center-Aligned Text with Minimum Boundary Bounds
  FormattingOptions opts_c;
  opts_c.min_width = 22;
  opts_c.alignment = Alignment::Center;
  std::string out_c = UnicodeTextScaler::FormatText(kAsciiShort, opts_c);

  // Case D: Strict Word Wrapping with Preferred Width
  FormattingOptions opts_d;
  opts_d.preferred_width = 25;
  opts_d.wrap_lines = true;
  opts_d.alignment = Alignment::Left;
  std::string out_d = UnicodeTextScaler::FormatText(kLongStream, opts_d);

  // Case E: Hard Truncation with Custom Ellipsis Glyph
  FormattingOptions opts_e;
  opts_e.max_width = 30;
  opts_e.wrap_lines = false;
  opts_e.ellipsis = " [TRUNCATED]…";
  std::string out_e = UnicodeTextScaler::FormatText(kLongStream, opts_e);

  // Case F: Multi-Line Input Forced to Bottom-Right Anchor Matrix
  FormattingOptions opts_f;
  opts_f.min_width = 40;
  opts_f.min_height = 6;
  opts_f.alignment = Alignment::Right;
  opts_f.vertical_alignment = VerticalAlignment::Bottom;
  std::string out_f = UnicodeTextScaler::FormatText(kMultiLineRaw, opts_f);

  // Case G: Asymmetric Center Box Matrix (Dead Centered 34x5 Bounding Box)
  FormattingOptions opts_g;
  opts_g.min_width = 34;
  opts_g.min_height = 5;
  opts_g.alignment = Alignment::Center;
  opts_g.vertical_alignment = VerticalAlignment::Center;
  std::string out_g = UnicodeTextScaler::FormatText(kMultiLineRaw, opts_g);

  // ============================================================================
  // FIXED DECLARATIVE UI RENDERING TREE
  // ============================================================================
  auto document = vbox(
      {text(" UnicodeTextScaler Comprehensive Validation Harness ") | bold |
           hcenter,
       separator(),

       // Section 1: Standard Single-Line Alignments
       text(" 1. Single-Line Alignment Options (Width Clamped) ") | dim | bold,
       hbox(
           {window(text("Left Align"), text(out_a) | bgcolor(Color::DarkGreen) |
                                           color(Color::White)) |
                size(WIDTH, EQUAL, 18),  // 16 content + 2 border

            window(text("Right Align"),
                   text(out_b) | bgcolor(Color::Blue) | color(Color::White)) |
                size(WIDTH, EQUAL, 18),

            window(text("Center Align"), text(out_c) |
                                             bgcolor(Color::DarkMagenta) |
                                             color(Color::White)) |
                size(WIDTH, EQUAL, 24)}) |
           hcenter,

       separator(),

       // Section 2: Horizontal Paragraph Transformations
       text(" 2. Line Wrapping vs. Hard Truncation Bounds ") | dim | bold,
       hbox({window(text("Word Wrap (Width 25)"), paragraph(out_d) |
                                                      bgcolor(Color::Red) |
                                                      color(Color::White)) |
                 size(WIDTH, EQUAL, 27),

             window(text("Truncate (Width 30)"), text(out_e) |
                                                     bgcolor(Color::DarkRed) |
                                                     color(Color::White)) |
                 size(WIDTH, EQUAL, 32)}) |
           hcenter,

       separator(),

       // Section 3: Dual-Axis Complex Matrices
       text(" 3. Dual-Axis Matrix Alignment Controls ") | dim | bold,
       vbox({window(
                 text("Multi-Line Raw (Bottom-Right Anchor 40x6)"),
                 text(out_f) | bgcolor(Color::GrayDark) | color(Color::White)) |
                 size(WIDTH, EQUAL, 42) |
                 size(HEIGHT, EQUAL, 8),  // 6 content + 2 border

             vbox({window(text("Dead-Centered Matrix Box (34x5 Layout)"),
                          text(out_g) | bgcolor(Color::Blue) |
                              color(Color::White)) |
                   size(WIDTH, EQUAL, 36) | size(HEIGHT, EQUAL, 7)}) |
                 hcenter})});

  // Mount and print output to console
  auto screen = ftxui::ScreenInteractive::TerminalOutput();
  auto baseComponent = ftxui::Make<ftxui::ComponentBase>();

  auto appController = CatchEvent(baseComponent, [&](Event event) {
    if (event == Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  // Map application layout view drawing blocks
  auto uiRenderer = Renderer(appController, [&]() { return document; });

  screen.Loop(uiRenderer);
  return 0;
}
