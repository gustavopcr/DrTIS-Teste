#include "dicom/dicom_image.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "dicom_test_fixtures.h"

namespace dicom {
namespace {

// RAII temporary file helper for testing load_from_file without repo fixtures.
class TempFile {
 public:
  explicit TempFile(const std::vector<std::uint8_t>& bytes) {
    static std::atomic<int> counter{0};
    path_ = std::filesystem::temp_directory_path() /
            ("drtis_dicom_test_" + std::to_string(counter++) + ".dcm");
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

// Expects variant<DicomImage, Error> to hold DicomImage and fails the test cleanly otherwise.
DicomImage unwrap_image(std::variant<DicomImage, Error>&& result) {
  if (auto* err = std::get_if<Error>(&result)) {
    ADD_FAILURE() << "Expected DicomImage, got Error("
                  << static_cast<int>(err->code) << "): " << err->message
                  << " [" << err->details << "]";
    throw std::runtime_error(err->message);
  }
  return std::move(std::get<DicomImage>(result));
}

ImageBuffer unwrap_buffer(std::variant<ImageBuffer, Error>&& result) {
  if (auto* err = std::get_if<Error>(&result)) {
    ADD_FAILURE() << "Expected ImageBuffer, got Error("
                  << static_cast<int>(err->code) << "): " << err->message
                  << " [" << err->details << "]";
    throw std::runtime_error(err->message);
  }
  return std::move(std::get<ImageBuffer>(result));
}

TEST(DicomImageLoadTest, LoadFromFileFailsWhenMissing) {
  const auto missing =
      std::filesystem::temp_directory_path() / "drtis_nonexistent_file_12345.dcm";
  auto result = DicomImage::load_from_file(missing);

  ASSERT_TRUE(std::holds_alternative<Error>(result));
  const auto& err = std::get<Error>(result);
  EXPECT_EQ(err.code, ErrorCode::kFileNotFound);
  EXPECT_EQ(err.message, "The selected file could not be found.");
  EXPECT_FALSE(err.details.empty());
}

TEST(DicomImageLoadTest, LoadFromFileFailsWhenPathIsDirectory) {
  auto result =
      DicomImage::load_from_file(std::filesystem::temp_directory_path());

  ASSERT_TRUE(std::holds_alternative<Error>(result));
  const auto& err = std::get<Error>(result);
  EXPECT_EQ(err.code, ErrorCode::kInvalidDicomFile);
  EXPECT_EQ(err.message, "The selected file is not a valid DICOM file.");
  EXPECT_FALSE(err.details.empty());
}

TEST(DicomImageLoadTest, LoadFromFileLoadsValidDicomFromDisk) {
  const std::vector<std::uint8_t> raw_pixels = {0, 85, 170, 255};
  const auto dcm_bytes =
      test::make_monochrome_8bit_dcm(/*width=*/2, /*height=*/2, raw_pixels);
  const TempFile temp_file(dcm_bytes);

  DicomImage image = unwrap_image(DicomImage::load_from_file(temp_file.path()));
  EXPECT_EQ(image.get_width(), 2);
  EXPECT_EQ(image.get_height(), 2);
  EXPECT_TRUE(image.is_monochrome());

  const ImageBuffer buffer = unwrap_buffer(image.render());
  EXPECT_EQ(buffer.width, 2);
  EXPECT_EQ(buffer.height, 2);
  EXPECT_EQ(buffer.format, ImageFormat::kGrayscale8);
  EXPECT_EQ(buffer.data, raw_pixels);
}

TEST(DicomImageLoadTest, LoadFromMemoryFailsOnNullOrEmptyBuffer) {
  const std::uint8_t dummy = 0x42U;
  const std::pair<const std::uint8_t*, std::size_t> empty_cases[] = {
      {nullptr, 0U},
      {nullptr, 16U},
      {&dummy, 0U},
  };

  for (const auto& [ptr, size] : empty_cases) {
    SCOPED_TRACE("size=" + std::to_string(size));
    auto result = DicomImage::load_from_memory(ptr, size);
    ASSERT_TRUE(std::holds_alternative<Error>(result));
    const auto& err = std::get<Error>(result);
    EXPECT_EQ(err.code, ErrorCode::kInvalidDicomFile);
    EXPECT_EQ(err.message, "The selected file is not a valid DICOM file.");
    EXPECT_FALSE(err.details.empty());
  }
}

TEST(DicomImageLoadTest, LoadFromMemoryFailsOnGarbageBytes) {
  const std::string garbage = "not a valid dicom file at all";
  auto result = DicomImage::load_from_memory(
      reinterpret_cast<const std::uint8_t*>(garbage.data()), garbage.size());

  ASSERT_TRUE(std::holds_alternative<Error>(result));
  const auto& err = std::get<Error>(result);
  EXPECT_EQ(err.code, ErrorCode::kInvalidDicomFile);
  EXPECT_EQ(err.message, "The selected file is not a valid DICOM file.");
  EXPECT_FALSE(err.details.empty());
}

TEST(DicomImageLoadTest, LoadFromMemoryFailsOnTruncatedDicom) {
  const std::vector<std::uint8_t> pixels(64U, 128U);
  auto dcm_bytes = test::make_monochrome_8bit_dcm(8, 8, pixels);
  dcm_bytes.resize((dcm_bytes.size() * 6U) / 10U);

  auto result =
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size());
  ASSERT_TRUE(std::holds_alternative<Error>(result));
  const auto& err = std::get<Error>(result);
  EXPECT_EQ(err.code, ErrorCode::kInvalidDicomFile);
  EXPECT_EQ(err.message, "The selected file is not a valid DICOM file.");
  EXPECT_FALSE(err.details.empty());
}

TEST(DicomImageLoadTest, LoadFromMemoryFailsWhenPixelDataMissing) {
  const auto sr_bytes = test::make_header_only_sr_dcm();
  auto result = DicomImage::load_from_memory(sr_bytes.data(), sr_bytes.size());

  ASSERT_TRUE(std::holds_alternative<Error>(result));
  const auto& err = std::get<Error>(result);
  EXPECT_EQ(err.code, ErrorCode::kMissingImageData);
  EXPECT_EQ(err.message, "The DICOM file does not contain image pixel data.");
  EXPECT_FALSE(err.details.empty());
}

TEST(DicomImageRenderTest, RendersMonochrome2DimensionsAndGradient) {
  const std::vector<std::uint8_t> pixels = {0, 64, 128, 255, 10, 20, 30, 40};
  const auto dcm_bytes =
      test::make_monochrome_8bit_dcm(/*width=*/4, /*height=*/2, pixels);

  DicomImage image = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  EXPECT_EQ(image.get_width(), 4);
  EXPECT_EQ(image.get_height(), 2);
  EXPECT_TRUE(image.is_monochrome());

  const ImageBuffer buffer = unwrap_buffer(image.render());
  EXPECT_EQ(buffer.width, 4);
  EXPECT_EQ(buffer.height, 2);
  EXPECT_EQ(buffer.format, ImageFormat::kGrayscale8);
  ASSERT_EQ(buffer.data.size(), 8U);
  EXPECT_EQ(buffer.data[0], 0U);
  EXPECT_EQ(buffer.data[3], 255U);
  EXPECT_LT(buffer.data[0], buffer.data[1]);
  EXPECT_LT(buffer.data[1], buffer.data[2]);
  EXPECT_LT(buffer.data[2], buffer.data[3]);
}

TEST(DicomImageRenderTest, InvertsMonochrome1ToDisplayScale) {
  const std::vector<std::uint8_t> pixels = {0, 255};
  const auto dcm_bytes = test::make_monochrome_8bit_dcm(
      /*width=*/2, /*height=*/1, pixels, /*monochrome1=*/true);

  DicomImage image = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  EXPECT_TRUE(image.is_monochrome());

  const ImageBuffer buffer = unwrap_buffer(image.render());
  ASSERT_EQ(buffer.data.size(), 2U);
  EXPECT_EQ(buffer.data[0], 255U);
  EXPECT_EQ(buffer.data[1], 0U);
}

TEST(DicomImageRenderTest, AppliesEmbeddedWindowPresetInsteadOfMinMax) {
  // 12-bit stored pixels; embedded window [2000..3000] clamps 2000->0 and 3000->255.
  // If setMinMaxWindow() were used instead, 2000 would render near mid-gray (~127).
  const std::vector<std::uint16_t> pixels = {0U, 2000U, 3000U, 4000U};
  const auto dcm_bytes = test::make_monochrome_16bit_dcm(
      /*width=*/4, /*height=*/1, /*bits_stored=*/12, /*is_signed=*/false,
      pixels, /*window_center=*/2500.0, /*window_width=*/1000.0);

  DicomImage image = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  const ImageBuffer buffer = unwrap_buffer(image.render());
  ASSERT_EQ(buffer.data.size(), 4U);
  EXPECT_EQ(buffer.data[0], 0U);
  EXPECT_LE(buffer.data[1], 5U);
  EXPECT_GE(buffer.data[2], 250U);
  EXPECT_EQ(buffer.data[3], 255U);
}

TEST(DicomImageRenderTest, RendersSigned16BitWithRescaleSlopeAndIntercept) {
  // Raw stored [0, 1000, 1040, 2000] + intercept -1000 => [-1000, 0, 40, 1000] HU.
  // With window center=20, width=80 ([-20..60] HU), 0->~64, 40->~191, extremes clamp.
  const std::vector<std::uint16_t> pixels = {
      static_cast<std::uint16_t>(0),
      static_cast<std::uint16_t>(1000),
      static_cast<std::uint16_t>(1040),
      static_cast<std::uint16_t>(2000),
  };
  const auto dcm_bytes = test::make_monochrome_16bit_dcm(
      /*width=*/4, /*height=*/1, /*bits_stored=*/16, /*is_signed=*/true, pixels,
      /*window_center=*/20.0, /*window_width=*/80.0,
      /*rescale_slope=*/1.0, /*rescale_intercept=*/-1000.0);

  DicomImage image = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  const ImageBuffer buffer = unwrap_buffer(image.render());
  ASSERT_EQ(buffer.data.size(), 4U);
  EXPECT_EQ(buffer.data[0], 0U);
  EXPECT_GT(buffer.data[1], 0U);
  EXPECT_LT(buffer.data[1], buffer.data[2]);
  EXPECT_LT(buffer.data[2], 255U);
  EXPECT_EQ(buffer.data[3], 255U);
}

TEST(DicomImageRenderTest, DecodesJpegLosslessCompressedDicom) {
  std::vector<std::uint8_t> pixels(64U);
  for (std::size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = static_cast<std::uint8_t>(i * 4U);
  }
  const auto dcm_bytes = test::make_jpeg_lossless_8bit_dcm(8, 8, pixels);

  DicomImage image = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  EXPECT_EQ(image.get_width(), 8);
  EXPECT_EQ(image.get_height(), 8);

  const ImageBuffer buffer = unwrap_buffer(image.render());
  ASSERT_EQ(buffer.data.size(), 64U);
  EXPECT_EQ(buffer.data.front(), 0U);
  EXPECT_EQ(buffer.data.back(), 255U);
}

TEST(DicomImageRenderTest, RendersRgb24WithCorrectChannelOrder) {
  const std::vector<std::uint8_t> rgb_pixels = {255U, 0U, 0U, 0U, 255U, 0U};
  const auto dcm_bytes =
      test::make_rgb_24bit_dcm(/*width=*/2, /*height=*/1, rgb_pixels);

  DicomImage image = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  EXPECT_EQ(image.get_width(), 2);
  EXPECT_EQ(image.get_height(), 1);
  EXPECT_FALSE(image.is_monochrome());

  const ImageBuffer buffer = unwrap_buffer(image.render());
  EXPECT_EQ(buffer.width, 2);
  EXPECT_EQ(buffer.height, 1);
  EXPECT_EQ(buffer.format, ImageFormat::kRgb24);
  EXPECT_EQ(buffer.data, rgb_pixels);
}

TEST(DicomImageLifecycleTest, SupportsMoveConstructionAndAssignment) {
  const std::vector<std::uint8_t> pixels = {10U, 200U};
  const auto dcm_bytes = test::make_monochrome_8bit_dcm(2, 1, pixels);

  DicomImage first = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  const ImageBuffer expected_buffer = unwrap_buffer(first.render());

  DicomImage moved_ctor(std::move(first));
  EXPECT_EQ(moved_ctor.get_width(), 2);

  DicomImage moved_assign = unwrap_image(
      DicomImage::load_from_memory(dcm_bytes.data(), dcm_bytes.size()));
  moved_assign = std::move(moved_ctor);
  EXPECT_EQ(moved_assign.get_width(), 2);
  EXPECT_EQ(moved_assign.get_height(), 1);

  const ImageBuffer actual_buffer = unwrap_buffer(moved_assign.render());
  EXPECT_EQ(actual_buffer.width, expected_buffer.width);
  EXPECT_EQ(actual_buffer.height, expected_buffer.height);
  EXPECT_EQ(actual_buffer.format, expected_buffer.format);
  EXPECT_EQ(actual_buffer.data, expected_buffer.data);
}

}  // namespace
}  // namespace dicom
