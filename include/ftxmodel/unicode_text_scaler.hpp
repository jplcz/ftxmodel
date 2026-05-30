// ReSharper disable CppTooWideScopeInitStatement
#pragma once
#include <algorithm>
#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace ftxmodel {

enum class Alignment { Left, Center, Right };
enum class VerticalAlignment { Top, Center, Bottom };

struct FormattingOptions {
  //! Maximum width of text. Values <= 0 mean no limit
  int max_width = 0;
  //! Minimum width. Values <= 0 mean there is no minimum width
  int min_width = 0;
  //! Preferred width of text. Values <= 0 mean there is no preferred width
  int preferred_width = 0;
  //! Maximum height of text. Values <= 0 mean there is no maximum height
  int max_height = 0;
  //! Minimum height of text. Values <= 0 mean there is no maximum height
  int min_height = 0;
  //! Horizontal alignment of text
  Alignment alignment = Alignment::Left;
  //! Vertical alignment of text
  VerticalAlignment vertical_alignment = VerticalAlignment::Top;
  //! True: splits/wraps words to new lines; False: cuts off with ellipsis
  bool wrap_lines = true;
  //! Custom unicode ellipsis glyph
  std::string ellipsis = "…";
};

class UnicodeTextScaler {
 public:
  [[nodiscard]] static int StringWidth(const std::string_view utf8_str) {
    return ftxui::string_width(utf8_str);
  }

  [[nodiscard]] static ftxui::Dimensions GetTextBounds(
      std::string_view utf8_str) noexcept {
    if (utf8_str.empty()) {
      return ftxui::Dimensions{0, 0};
    }

    int max_width = 0;
    int line_count = 0;

    size_t start = 0;
    while (start < utf8_str.size()) {
      size_t end = utf8_str.find('\n', start);
      if (end == std::string_view::npos) {
        end = utf8_str.size();
      }

      // Slice the line
      const std::string_view current_line = utf8_str.substr(start, end - start);
      const int line_width = StringWidth(current_line);

      if (line_width > max_width) {
        max_width = line_width;
      }

      line_count++;
      start = end + 1;  // Move past the newline character
    }

    // Handle trailing newline edge case (e.g., "Line 1\n" creates an empty
    // second line)
    if (utf8_str.back() == '\n') {
      line_count++;
    }

    return ftxui::Dimensions{max_width, line_count};
  }

  // ============================================================================
  // STRING_VIEW UTF-8 DECODER
  // ============================================================================
  // Decodes the FIRST code point of the provided string_view slice.
  // @param remaining_text: A view into the remaining unprocessed text stream.
  // @param out_code_point: Reference populated with the decoded 32-bit
  // character.
  // @return: Number of bytes consumed (1 to 4) by this glyph. Returns 0 if
  // invalid.
  [[nodiscard]] static int DecodeUtf8(const std::string_view remaining_text,
                                      char32_t& out_code_point) noexcept {
    const size_t available_bytes = remaining_text.size();
    if (available_bytes == 0) {
      out_code_point = 0;
      return 0;
    }

    // Access the first byte safely via string_view indexing
    const auto byte1 = static_cast<uint8_t>(remaining_text[0]);

    // 1-Byte Character: Standard ASCII (0xxxxxxx)
    if (byte1 < 0x80) {
      out_code_point = byte1;
      return 1;
    }

    // 2-Byte Character (110xxxxx 10xxxxxx)
    if ((byte1 & 0xE0) == 0xC0) {
      if (available_bytes < 2) {
        return 0;
      }

      const auto byte2 = static_cast<uint8_t>(remaining_text[1]);
      if ((byte2 & 0xC0) != 0x80) {
        return 0;  // Invalid continuation byte
      }

      out_code_point = (static_cast<char32_t>(byte1 & 0x1F) << 6) |
                       (static_cast<char32_t>(byte2 & 0x3F));
      return 2;
    }

    // 3-Byte Character (1110xxxx 10xxxxxx 10xxxxxx)
    if ((byte1 & 0xF0) == 0xE0) {
      if (available_bytes < 3) {
        return 0;
      }

      const auto byte2 = static_cast<uint8_t>(remaining_text[1]);
      const auto byte3 = static_cast<uint8_t>(remaining_text[2]);
      if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80) {
        return 0;
      }

      out_code_point = (static_cast<char32_t>(byte1 & 0x0F) << 12) |
                       (static_cast<char32_t>(byte2 & 0x3F) << 6) |
                       (static_cast<char32_t>(byte3 & 0x3F));
      return 3;
    }

    // 4-Byte Character (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
    if ((byte1 & 0xF8) == 0xF0) {
      if (available_bytes < 4) {
        return 0;
      }

      const auto byte2 = static_cast<uint8_t>(remaining_text[1]);
      const auto byte3 = static_cast<uint8_t>(remaining_text[2]);
      const auto byte4 = static_cast<uint8_t>(remaining_text[3]);
      if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 ||
          (byte4 & 0xC0) != 0x80) {
        return 0;
      }

      out_code_point = (static_cast<char32_t>(byte1 & 0x07) << 18) |
                       (static_cast<char32_t>(byte2 & 0x3F) << 12) |
                       (static_cast<char32_t>(byte3 & 0x3F) << 6) |
                       (static_cast<char32_t>(byte4 & 0x3F));
      return 4;
    }

    // Fallback: Malformed sequence or invalid overlong prefix
    out_code_point = 0;
    return 0;
  }

  struct VerticalLine {
    std::string_view text_slice;
    bool is_pure_padding = false;  // Tracks if this is an empty injected row
  };

  using VerticalLayoutResult =
      std::variant<std::span<const std::string_view>,  // Returning the exact
                                                       // untouched input span
                   std::vector<VerticalLine>  // Returning modified structure
                                              // with padding/clamping
                   >;

  [[nodiscard]] static VerticalLayoutResult ApplyVerticalConstraints(
      std::span<const std::string_view> input_lines,
      const FormattingOptions& options) {
    const int initial_height = static_cast<int>(input_lines.size());

    // 1. Resolve Target Height Boundaries
    int target_height = initial_height;
    if (options.max_height > 0 && target_height > options.max_height) {
      target_height = options.max_height;
    }
    if (options.min_height > 0 && target_height < options.min_height) {
      target_height = options.min_height;
    }

    // If the line count matches target boundaries and no top/center shifts are
    // needed, we change absolutely nothing. Return a copy of the input span
    // window!
    if (initial_height == target_height &&
        options.vertical_alignment == VerticalAlignment::Top) {
      return input_lines;
    }

    // Modifications required
    std::vector<VerticalLine> modified_layout;
    modified_layout.reserve(static_cast<size_t>(target_height));

    // Calculate available padding spaces
    const int active_content_lines = std::min(initial_height, target_height);
    const int total_padding_needed = target_height - active_content_lines;

    int top_padding = 0;
    int bottom_padding = 0;

    if (total_padding_needed > 0) {
      switch (options.vertical_alignment) {
        case VerticalAlignment::Top:
          bottom_padding = total_padding_needed;
          break;
        case VerticalAlignment::Bottom:
          top_padding = total_padding_needed;
          break;
        case VerticalAlignment::Center:
          top_padding = total_padding_needed / 2;
          bottom_padding = total_padding_needed - top_padding;
          break;
      }
    }

    // Phase A: Inject Top Padding Rows
    for (int i = 0; i < top_padding; ++i) {
      modified_layout.push_back({std::string_view(), true});
    }

    // Phase B: Copy Over Unclipped Core Contents Window
    for (int i = 0; i < active_content_lines; ++i) {
      modified_layout.push_back({input_lines[static_cast<size_t>(i)], false});
    }

    // Phase C: Inject Bottom Padding Rows
    for (int i = 0; i < bottom_padding; ++i) {
      modified_layout.push_back({std::string_view(), true});
    }

    return modified_layout;
  }

  // Simply flattens the resolved layout pipeline state back into a unified
  // string block.
  // @param layout_res: The structured vertical constraints pipeline output.
  // @param resolved_target_width: Precalculated bounding box width for row
  // padding strings.
  [[nodiscard]] static std::string AssembleParagraph(
      const VerticalLayoutResult& layout_res,
      const int resolved_target_width) {
    // If we only have 1 single line and it's from the zero-allocation span
    // stream, construct and return it instantly with zero loops or internal
    // tracking overhead.
    if (std::holds_alternative<std::span<const std::string_view>>(layout_res)) {
      const auto lines_span =
          std::get<std::span<const std::string_view>>(layout_res);
      if (lines_span.size() == 1) {
        return std::string(lines_span[0]);
      }
      if (lines_span.empty()) {
        return {};
      }
    }

    const size_t total_lines = std::visit(
        [](const auto& container) -> size_t { return container.size(); },
        layout_res);

    if (total_lines == 0) {
      return {};
    }

    const size_t exact_capacity =
        (total_lines * static_cast<size_t>(resolved_target_width)) +
        total_lines;

    std::string output_buffer;
    output_buffer.reserve(exact_capacity);

    // Lambda to write a single line, injecting padding block spaces if marked
    // empty
    auto write_row = [&](const std::string_view slice, const bool is_padding) {
      if (is_padding) {
        output_buffer.append(static_cast<size_t>(resolved_target_width), ' ');
      } else {
        output_buffer.append(slice.data(), slice.size());
      }
    };

    std::visit(
        [&](const auto& container) {
          for (size_t i = 0; i < container.size(); ++i) {
            if constexpr (std::is_same_v<std::decay_t<decltype(container[0])>,
                                         std::string_view>) {
              // Processing the std::span<const std::string_view>
              write_row(container[i], false);
            } else {
              // Processing the std::vector<VerticalLine>
              write_row(container[i].text_slice, container[i].is_pure_padding);
            }

            // Newline logic is completely unified
            if (i + 1 < container.size()) {
              output_buffer += "\n";
            }
          }
        },
        layout_res);

    return output_buffer;
  }

  // Represents a horizontally resolved line
  using HorizontalLine_base = std::variant<std::string_view, std::string>;

  struct HorizontalLine : HorizontalLine_base {
    using HorizontalLine_base::HorizontalLine_base;

    [[nodiscard]] std::string_view to_string_view() const noexcept {
      return std::visit(
          [](const auto& f) -> std::string_view { return std::string_view(f); },
          *this);
    }
  };

  // Zero-allocation fallback variant token
  using HorizontalPaddingResult =
      std::variant<std::span<const std::string_view>,  // Returns the unchanged
                                                       // input span if no
                                                       // padding was needed
                   std::vector<HorizontalLine>  // Returns a mix of views and
                                                // padded string allocations
                   >;

  // ============================================================================
  // HORIZONTAL PADDING STAGE (NON-OWNING STREAM INPUT)
  // ============================================================================
  [[nodiscard]] static HorizontalPaddingResult ApplyHorizontalPadding(
      std::span<const std::string_view> lines,
      const FormattingOptions& options,
      const int target_width) {
    // QUICK SCAN FOR THE ZERO-ALLOCATION FAST PATH
    // Check if all lines are already perfectly sized
    bool padding_required = false;
    for (const std::string_view line : lines) {
      if (StringWidth(line) < target_width) {
        padding_required = true;
        break;
      }
    }

    // Fast Path: If nothing needs resizing, forward the span right through
    if (!padding_required) {
      return lines;
    }

    // Padding is required
    std::vector<HorizontalLine> modified_lines;
    modified_lines.reserve(lines.size());

    for (std::string_view line : lines) {
      const int current_w = StringWidth(line);
      const int padding_needed = target_width - current_w;

      if (padding_needed <= 0) {
        // No padding needed for this specific row: store a direct, zero-copy
        // pointer view
        modified_lines.emplace_back(line);
        continue;
      }

      // Allocate a string only for rows that actually require padding spaces
      std::string padded_str;
      padded_str.reserve(static_cast<size_t>(target_width));

      switch (options.alignment) {
        case Alignment::Left:
          padded_str.append(line.data(), line.size());
          padded_str.append(static_cast<size_t>(padding_needed), ' ');
          break;

        case Alignment::Right:
          padded_str.append(static_cast<size_t>(padding_needed), ' ');
          padded_str.append(line.data(), line.size());
          break;

        case Alignment::Center: {
          const int left_pad = padding_needed / 2;
          const int right_pad = padding_needed - left_pad;
          padded_str.append(static_cast<size_t>(left_pad), ' ');
          padded_str.append(line.data(), line.size());
          padded_str.append(static_cast<size_t>(right_pad), ' ');
          break;
        }
      }

      // Capture the string buffer and point the active view straight inside it
      modified_lines.emplace_back(std::move(padded_str));
    }

    return modified_lines;
  }

  static std::string FormatText(const std::string_view value,
                                const FormattingOptions& options) {
    // Core Fast-Path Opt: Flat single row string, no vertical padding limits
    // requested
    if (value.find('\n') == std::string_view::npos && options.min_height <= 1) {
      const int visual_w = StringWidth(value);

      if (visual_w >= options.min_width &&
          (options.max_width <= 0 || visual_w <= options.max_width) &&
          (options.preferred_width <= 0 ||
           visual_w == options.preferred_width) &&
          options.alignment == Alignment::Left) {
        return std::string(value);  // Bypasses the layout engine completely
      }
    }

    return FullFormatPipelineFallback(value, options);
  }

 private:
  // ============================================================================
  // COMPLETE PIEPLINE EXECUTION ENGINE FOR RE-SHAPING OPERATIONS
  // ============================================================================
  static std::string FullFormatPipelineFallback(
      const std::string_view value,
      const FormattingOptions& options) {
    // Step 1: Target Width Resolution Pass
    const ftxui::Dimensions initial_bounds = GetTextBounds(value);
    int target_width = (options.preferred_width > 0) ? options.preferred_width
                                                     : initial_bounds.dimx;
    if (options.max_width > 0 && target_width > options.max_width) {
      target_width = options.max_width;
    }
    if (options.min_width > 0 && target_width < options.min_width) {
      target_width = options.min_width;
    }
    if (target_width <= 0) {
      target_width = 1;
    }

    // Step 2: Line Wrapping / Truncation Core Loop
    std::vector<std::string_view> horizontal_slices;
    std::vector<std::string>
        ellipsis_allocations_cache;  // Holds truncated strings securely

    size_t line_start = 0;
    size_t offset = 0;
    int current_line_w = 0;
    const size_t total_bytes = value.size();

    while (offset < total_bytes) {
      if (value[offset] == '\n') {
        horizontal_slices.push_back(
            value.substr(line_start, offset - line_start));
        offset++;
        line_start = offset;
        current_line_w = 0;
        continue;
      }

      char32_t cp = 0;
      std::string_view remaining = value.substr(offset);
      const int bytes_consumed = DecodeUtf8(remaining, cp);

      if (bytes_consumed == 0) {
        offset++;
        continue;
      }

      const std::string_view glyph =
          remaining.substr(0, static_cast<size_t>(bytes_consumed));
      const int glyph_w = StringWidth(glyph);

      if (current_line_w + glyph_w > target_width) {
        if (options.wrap_lines) {
          horizontal_slices.push_back(
              value.substr(line_start, offset - line_start));
          line_start = offset;
          current_line_w = glyph_w;
        } else {
          // Ellipsis truncation pass
          std::string truncated(value.substr(line_start, offset - line_start));
          const int ellipsis_w = StringWidth(options.ellipsis);
          while (!truncated.empty() &&
                 (current_line_w + ellipsis_w > target_width)) {
            truncated.pop_back();
            current_line_w = StringWidth(truncated);
          }
          ellipsis_allocations_cache.push_back(truncated + options.ellipsis);
          horizontal_slices.push_back(ellipsis_allocations_cache.back());
          line_start = total_bytes;  // Force breaks loop cleanly
          break;
        }
      } else {
        current_line_w += glyph_w;
      }
      offset += static_cast<size_t>(bytes_consumed);
    }

    if (line_start < total_bytes || (!value.empty() && value.back() == '\n')) {
      horizontal_slices.push_back(
          value.substr(line_start, total_bytes - line_start));
    }

    // Step 3: Run Horizontal Padding Phase
    std::span<const std::string_view> wrapping_span(horizontal_slices.data(),
                                                    horizontal_slices.size());
    HorizontalPaddingResult horiz_res =
        ApplyHorizontalPadding(wrapping_span, options, target_width);

    // Standardize views to clear raw structures before passing into vertical
    // constraints stage
    std::vector<std::string_view> vertical_input;
    if (std::holds_alternative<std::span<const std::string_view>>(horiz_res)) {
      const auto span_data =
          std::get<std::span<const std::string_view>>(horiz_res);
      vertical_input.assign(span_data.begin(), span_data.end());
    } else {
      const auto& vec_data = std::get<std::vector<HorizontalLine>>(horiz_res);
      vertical_input.reserve(vec_data.size());
      for (const auto& line : vec_data) {
        vertical_input.push_back(line.to_string_view());
      }
    }

    // Step 4: Run Vertical Constraints Phase
    const std::span<const std::string_view> vertical_span(
        vertical_input.data(), vertical_input.size());

    const VerticalLayoutResult vertical_res =
        ApplyVerticalConstraints(vertical_span, options);

    // Step 5: Final Row Assembly flattening
    return AssembleParagraph(vertical_res, target_width);
  }
};

}  // namespace ftxmodel
