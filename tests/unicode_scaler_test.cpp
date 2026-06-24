#include <gtest/gtest.h>
#include <ftxmodel/unicode_text_scaler.hpp>

using namespace ftxmodel;

// ============================================================================
// UNIT TESTS FOR StringWidth
// ============================================================================

TEST(UnicodeTextScalerTest, StringWidth_EmptyAndAscii) {
  // Empty strings take up zero terminal column cells
  EXPECT_EQ(UnicodeTextScaler::StringWidth(""), 0);

  // Standard alphanumeric ASCII: 1 character = 1 terminal column cell
  EXPECT_EQ(UnicodeTextScaler::StringWidth("Hello"), 5);
  EXPECT_EQ(UnicodeTextScaler::StringWidth("C++20 Engine"), 12);
}

TEST(UnicodeTextScalerTest, StringWidth_Utf8MultiByteAndEmojis) {
  // Standard Chinese / CJK characters are "Full-Width" (2 columns each)
  // "进程" is 2 characters, but takes up 4 terminal columns
  EXPECT_EQ(UnicodeTextScaler::StringWidth("进程"), 4);

  // Mixed ASCII and CJK
  EXPECT_EQ(UnicodeTextScaler::StringWidth("PID: 进程"),
            9);  // 5 (ASCII) + 4 (CJK)

  // Complex mixed telemetry string check
  // "📊 PID: 4021" -> 2 (Emoji) + 1 (Space) + 9 (ASCII) = 12
  EXPECT_EQ(UnicodeTextScaler::StringWidth("📊 PID: 4021"), 12);
}

// ============================================================================
// UNIT TESTS FOR GetTextBounds
// ============================================================================
TEST(UnicodeTextScalerTest, GetTextBounds_EmptyAndSingleLine) {
  // Empty inputs resolve to an empty bounding box dimensions layout
  ftxui::Dimensions empty_dims = UnicodeTextScaler::GetTextBounds("");
  EXPECT_EQ(empty_dims.dimx, 0);  // Width
  EXPECT_EQ(empty_dims.dimy, 0);  // Height

  // Standard single line without newlines
  const ftxui::Dimensions single_line =
      UnicodeTextScaler::GetTextBounds("Standard Row");
  EXPECT_EQ(single_line.dimx, 12);  // Width matches string footprint length
  EXPECT_EQ(single_line.dimy, 1);   // Exactly 1 vertical row tall
}

TEST(UnicodeTextScalerTest, GetTextBounds_MultiLineAscii) {
  // Explicit multi-line split validation
  std::string_view multiline_text = "Line 1\nLine Two\n3";
  ftxui::Dimensions bounds = UnicodeTextScaler::GetTextBounds(multiline_text);

  // Widest line determines X ("Line Two" = 8 columns)
  EXPECT_EQ(bounds.dimx, 8);
  // Number of splits determines Y
  EXPECT_EQ(bounds.dimy, 3);
}

TEST(UnicodeTextScalerTest, GetTextBounds_MultiLineWithUtf8) {
  // Multi-line layout combining variable-width UTF-8 code points
  std::string_view complex_text =
      "📊 Core Telemetry\n"
      "State: Active Running\n"
      "线程 Monitor Loop";  // "线程" takes 4 columns

  ftxui::Dimensions bounds = UnicodeTextScaler::GetTextBounds(complex_text);

  // Let's verify line widths manually to find the max:
  // Line 1: 📊 (2) + " Core Telemetry" (15) = 17
  // Line 2: "State: Active Running" (21) -> Widest
  // Line 3: 线程 (4) + " Monitor Loop" (13) = 17

  EXPECT_EQ(bounds.dimx, 21);  // Width clamped by Line 2
  EXPECT_EQ(bounds.dimy, 3);   // Height exactly 3 lines
}

TEST(UnicodeTextScalerTest, GetTextBounds_TrailingNewlineEdgeCase) {
  // Verifies that a trailing newline token creates an extra empty vertical line
  // row
  std::string_view text_with_trailing = "Line 1\n";
  ftxui::Dimensions bounds =
      UnicodeTextScaler::GetTextBounds(text_with_trailing);

  EXPECT_EQ(bounds.dimx, 6);  // Width of "Line 1"
  EXPECT_EQ(bounds.dimy,
            2);  // Height is 2 because row 2 is blank but initialized

  // Consecutive newline verification
  std::string_view empty_lines = "\n\n\n";
  ftxui::Dimensions empty_bounds =
      UnicodeTextScaler::GetTextBounds(empty_lines);
  EXPECT_EQ(empty_bounds.dimx,
            0);  // Completely unpopulated rows consume 0 columns width
  EXPECT_EQ(empty_bounds.dimy,
            4);  // 3 newlines map to exactly 4 distinct vertical grid blocks
}

// ============================================================================
// UNIT TESTS FOR DecodeUtf8
// ============================================================================

TEST(UnicodeTextScalerTest, DecodeUtf8_ValidSequences) {
  char32_t cp = 0;
  int bytes_consumed = 0;

  // Test Case A: 1-Byte ASCII ('A' -> U+0041)
  std::string_view ascii_str = "A";
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(ascii_str, cp);
  EXPECT_EQ(bytes_consumed, 1);
  EXPECT_EQ(cp, U'A');
  EXPECT_EQ(cp, 0x41);

  // Test Case B: 2-Byte Sequence ('¢' -> U+00A2)
  // UTF-8 representation: 0xC2 0xA2
  std::string_view two_byte_str = "¢";
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(two_byte_str, cp);
  EXPECT_EQ(bytes_consumed, 2);
  EXPECT_EQ(cp, U'¢');
  EXPECT_EQ(cp, 0xA2);

  // Test Case C: 3-Byte Sequence ('进' -> U+8FDB)
  // UTF-8 representation: 0xE8 0xBF 0x9B
  std::string_view three_byte_str = "进";
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(three_byte_str, cp);
  EXPECT_EQ(bytes_consumed, 3);
  EXPECT_EQ(cp, U'进');
  EXPECT_EQ(cp, 0x8FDB);

  // Test Case D: 4-Byte Sequence ('📊' -> U+1F4CA)
  // UTF-8 representation: 0xF0 0x9F 0x93 0x8A
  std::string_view four_byte_str = "📊";
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(four_byte_str, cp);
  EXPECT_EQ(bytes_consumed, 4);
  EXPECT_EQ(cp, U'📊');
  EXPECT_EQ(cp, 0x1F4CA);
}

TEST(UnicodeTextScalerTest, DecodeUtf8_StreamingAndOffsets) {
  char32_t cp = 0;
  int bytes_consumed = 0;

  // Verify slicing transitions within a compound text stream: "A进📊"
  std::string_view compound_stream = "A进📊";

  // 1st Glyph: 'A'
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(compound_stream, cp);
  EXPECT_EQ(bytes_consumed, 1);
  EXPECT_EQ(cp, U'A');

  // Advance the window slice forward past the 1st glyph
  compound_stream = compound_stream.substr(bytes_consumed);  // "进📊"

  // 2nd Glyph: '进'
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(compound_stream, cp);
  EXPECT_EQ(bytes_consumed, 3);
  EXPECT_EQ(cp, U'进');

  // Advance the window slice forward past the 2nd glyph
  compound_stream = compound_stream.substr(bytes_consumed);  // "📊"

  // 3rd Glyph: '📊'
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(compound_stream, cp);
  EXPECT_EQ(bytes_consumed, 4);
  EXPECT_EQ(cp, U'📊');
}

TEST(UnicodeTextScalerTest, DecodeUtf8_InvalidAndEdgeCases) {
  char32_t cp = 0;
  int bytes_consumed = 0;

  // Edge Case 1: Empty View
  EXPECT_EQ(UnicodeTextScaler::DecodeUtf8("", cp), 0);
  EXPECT_EQ(cp, 0);

  // Edge Case 2: Truncated multi-byte headers (Missing continuation bytes)
  // 0xE8 signals a 3-byte character, but we only supply 1 byte total.
  char truncated_three_byte[] = {static_cast<char>(0xE8)};
  std::string_view truncated_view(truncated_three_byte, 1);
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(truncated_view, cp);
  EXPECT_EQ(bytes_consumed, 0);  // Must gracefully fail and return 0
  EXPECT_EQ(cp, 0);

  // Edge Case 3: Invalid UTF-8 Continuation Byte
  // 0xC2 signals a 2-byte sequence; 0x41 ('A') is NOT a valid continuation byte
  // (must be 0x80-0xBF)
  char invalid_continuation[] = {static_cast<char>(0xC2), 0x41};
  std::string_view invalid_view(invalid_continuation, 2);
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(invalid_view, cp);
  EXPECT_EQ(bytes_consumed, 0);  // Must fail validation gracefully
  EXPECT_EQ(cp, 0);

  // Edge Case 4: Malformed Lead Byte Prefix
  // 0xFE and 0xFF are completely illegal lead bytes in standard UTF-8
  // specifications
  char illegal_lead[] = {static_cast<char>(0xFE), static_cast<char>(0xBF)};
  std::string_view illegal_view(illegal_lead, 2);
  bytes_consumed = UnicodeTextScaler::DecodeUtf8(illegal_view, cp);
  EXPECT_EQ(bytes_consumed, 0);
  EXPECT_EQ(cp, 0);
}

// ============================================================================
//  UNIT TESTS FOR ApplyVerticalConstraints
// ============================================================================

TEST(VerticalLayoutPipelineTest,
     ApplyVerticalConstraints_ZeroAllocationFastPath) {
  // Ground setup: 3 lines of raw data layout slices
  std::array<std::string_view, 3> lines = {"Line 1", "Line 2", "Line 3"};
  std::span<const std::string_view> input_span(lines.data(), lines.size());

  // Options match the exact height signature and stick to the default Top
  // anchor rule
  FormattingOptions options;
  options.min_height = 3;
  options.max_height = 3;
  options.vertical_alignment = VerticalAlignment::Top;

  const auto result =
      UnicodeTextScaler::ApplyVerticalConstraints(input_span, options);

  // CRITICAL TRACKING CHECK: Must evaluate directly to the zero-allocation span
  // variant branch!
  ASSERT_TRUE(
      std::holds_alternative<std::span<const std::string_view>>(result));

  const auto output_span = std::get<std::span<const std::string_view>>(result);
  EXPECT_EQ(output_span.size(), 3);
  EXPECT_EQ(output_span[0], "Line 1");
  EXPECT_EQ(output_span[1], "Line 2");
  EXPECT_EQ(output_span[2], "Line 3");

  // Verify it points to the exact same memory footprints address location
  EXPECT_EQ(output_span.data(), input_span.data());
}

TEST(VerticalLayoutPipelineTest, ApplyVerticalConstraints_HeightClamping) {
  std::array<std::string_view, 4> lines = {"Row 1", "Row 2", "Row 3", "Row 4"};
  std::span<const std::string_view> input_span(lines.data(), lines.size());

  // Clamp the layout dynamically to a maximum height limit boundary of 2
  FormattingOptions options;
  options.max_height = 2;
  options.vertical_alignment = VerticalAlignment::Top;

  const auto result =
      UnicodeTextScaler::ApplyVerticalConstraints(input_span, options);

  // Because it modifies the structure by dropping elements, it must switch
  // variant allocations
  ASSERT_TRUE(
      std::holds_alternative<std::vector<UnicodeTextScaler::VerticalLine>>(
          result));

  const auto& output_vec =
      std::get<std::vector<UnicodeTextScaler::VerticalLine>>(result);
  ASSERT_EQ(output_vec.size(), 2);  // Resized down precisely to 2 lines

  EXPECT_EQ(output_vec[0].text_slice, "Row 1");
  EXPECT_FALSE(output_vec[0].is_pure_padding);

  EXPECT_EQ(output_vec[1].text_slice, "Row 2");
  EXPECT_FALSE(output_vec[1].is_pure_padding);
}

TEST(VerticalLayoutPipelineTest, ApplyVerticalConstraints_TopAlignmentPadding) {
  std::array<std::string_view, 1> lines = {"Content"};
  std::span<const std::string_view> input_span(lines.data(), lines.size());

  // Target a minimum vertical grid window height of 3 with Top Alignment
  // (Padding below)
  FormattingOptions options;
  options.min_height = 3;
  options.vertical_alignment = VerticalAlignment::Top;

  auto result =
      UnicodeTextScaler::ApplyVerticalConstraints(input_span, options);

  ASSERT_TRUE(
      std::holds_alternative<std::vector<UnicodeTextScaler::VerticalLine>>(
          result));
  const auto& output_vec =
      std::get<std::vector<UnicodeTextScaler::VerticalLine>>(result);
  ASSERT_EQ(output_vec.size(), 3);

  // Row 0: Content text
  EXPECT_EQ(output_vec[0].text_slice, "Content");
  EXPECT_FALSE(output_vec[0].is_pure_padding);

  // Row 1 & 2: Empty padding allocations
  EXPECT_TRUE(output_vec[1].is_pure_padding);
  EXPECT_TRUE(output_vec[1].text_slice.empty());
  EXPECT_TRUE(output_vec[2].is_pure_padding);
}

TEST(VerticalLayoutPipelineTest,
     ApplyVerticalConstraints_BottomAlignmentPadding) {
  std::array<std::string_view, 1> lines = {"Content"};
  std::span<const std::string_view> input_span(lines.data(), lines.size());

  // Target min height of 3 with Bottom Alignment (Padding injected above)
  FormattingOptions options;
  options.min_height = 3;
  options.vertical_alignment = VerticalAlignment::Bottom;

  auto result =
      UnicodeTextScaler::ApplyVerticalConstraints(input_span, options);

  ASSERT_TRUE(
      std::holds_alternative<std::vector<UnicodeTextScaler::VerticalLine>>(
          result));
  const auto& output_vec =
      std::get<std::vector<UnicodeTextScaler::VerticalLine>>(result);
  ASSERT_EQ(output_vec.size(), 3);

  // Rows 0 & 1: Empty padding lines pushing data down
  EXPECT_TRUE(output_vec[0].is_pure_padding);
  EXPECT_TRUE(output_vec[1].is_pure_padding);

  // Row 2: Content text sitting safely at base floor
  EXPECT_EQ(output_vec[2].text_slice, "Content");
  EXPECT_FALSE(output_vec[2].is_pure_padding);
}

TEST(VerticalLayoutPipelineTest,
     ApplyVerticalConstraints_CenterAlignmentPadding) {
  std::array<std::string_view, 1> lines = {"Center text"};
  std::span<const std::string_view> input_span(lines.data(), lines.size());

  // Target min height of 3 with Center Alignment (Padding split evenly around
  // content)
  FormattingOptions options;
  options.min_height = 3;
  options.vertical_alignment = VerticalAlignment::Center;

  auto result =
      UnicodeTextScaler::ApplyVerticalConstraints(input_span, options);

  ASSERT_TRUE(
      std::holds_alternative<std::vector<UnicodeTextScaler::VerticalLine>>(
          result));
  const auto& output_vec =
      std::get<std::vector<UnicodeTextScaler::VerticalLine>>(result);
  ASSERT_EQ(output_vec.size(), 3);

  // Balanced bounding structure matching asymmetric division targets:
  EXPECT_TRUE(output_vec[0].is_pure_padding);  // Top Buffer Row

  EXPECT_EQ(output_vec[1].text_slice,
            "Center text");  // Focused Central Core Row
  EXPECT_FALSE(output_vec[1].is_pure_padding);

  EXPECT_TRUE(output_vec[2].is_pure_padding);  // Bottom Buffer Row
}

// ============================================================================
// UNIT TESTS FOR ApplyHorizontalPadding
// ============================================================================

TEST(UnicodeTextScalerTest, ApplyHorizontalPadding_ZeroAllocationFastPath) {
  // Arrange: Setup strings that already perfectly meet or exceed the target
  // width
  std::array<std::string_view, 2> lines = {"ExactWidthLine",
                                           "VeryLongLineString"};
  const std::span<const std::string_view> input_span(lines.data(),
                                                     lines.size());

  // Target width matches or is smaller than current widths, meaning no padding
  // is needed
  constexpr int target_width = 14;

  FormattingOptions options;
  options.alignment = Alignment::Left;

  // Act: Execute padding phase
  const auto result = UnicodeTextScaler::ApplyHorizontalPadding(
      input_span, options, target_width);

  // Assert: Verify it chose the zero-allocation span tracking path
  ASSERT_TRUE(
      std::holds_alternative<std::span<const std::string_view>>(result));

  const auto output_span = std::get<std::span<const std::string_view>>(result);
  EXPECT_EQ(output_span.size(), 2);
  EXPECT_EQ(output_span[0], "ExactWidthLine");
  EXPECT_EQ(output_span[1], "VeryLongLineString");

  // Memory verification: The returned span must point to the exact same input
  // buffer address
  EXPECT_EQ(output_span.data(), input_span.data());
}

TEST(UnicodeTextScalerTest, ApplyHorizontalPadding_LeftAlignment) {
  // 1. Arrange: Input string view shorter than the target width constraint
  std::array<std::string_view, 1> lines = {"Data"};  // Width = 4
  std::span<const std::string_view> input_span(lines.data(), lines.size());

  constexpr int target_width = 8;  // Requires 4 spaces of trailing padding

  FormattingOptions options;
  options.alignment = Alignment::Left;

  // Act: Execute the padding pipeline pass
  auto result = UnicodeTextScaler::ApplyHorizontalPadding(input_span, options,
                                                          target_width);

  // Assert: Must allocate modified structures since text changes occurred
  ASSERT_TRUE(
      std::holds_alternative<std::vector<UnicodeTextScaler::HorizontalLine>>(
          result));

  const auto& output_vec =
      std::get<std::vector<UnicodeTextScaler::HorizontalLine>>(result);
  ASSERT_EQ(output_vec.size(), 1);

  // VERIFY VARIANT TYPE AND VALUE:
  // Because padding spaces were added, this row MUST hold an owning std::string
  // allocation
  ASSERT_TRUE(std::holds_alternative<std::string>(output_vec[0]));

  const auto& padded_line = std::get<std::string>(output_vec[0]);
  EXPECT_EQ(padded_line, "Data    ");
  EXPECT_EQ(padded_line.size(), 8);
}

TEST(UnicodeTextScalerTest, HorizonalLine_ToStringViewConversion) {
  // Test Case 1: Validate when the underlying variant is a zero-copy
  // std::string_view
  std::string_view original_view = "ZeroCopyText";
  UnicodeTextScaler::HorizontalLine line_from_view(original_view);

  // Verify type integrity and that to_string_view() successfully resolves the
  // data
  ASSERT_TRUE(std::holds_alternative<std::string_view>(line_from_view));
  EXPECT_EQ(line_from_view.to_string_view(), "ZeroCopyText");

  // Explicit pointer sanity check: Ensure no copying or reallocation happened
  EXPECT_EQ(line_from_view.to_string_view().data(), original_view.data());

  // Test Case 2: Validate when the underlying variant is an owning padded
  // std::string
  std::string padded_string = "PaddedText    ";

  UnicodeTextScaler::HorizontalLine line_from_string(std::move(padded_string));

  // Verify type integrity and that to_string_view() tracks the internal string
  // buffer
  ASSERT_TRUE(std::holds_alternative<std::string>(line_from_string));
  EXPECT_EQ(line_from_string.to_string_view(), "PaddedText    ");

  // Explicit pointer sanity check: Ensure it points directly to the
  // vector-allocated string buffer
  EXPECT_EQ(line_from_string.to_string_view().data(),
            std::get<std::string>(line_from_string).data());
}
