#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace dicom::test {

// Builds a valid DICOM Structured Report container with no PixelData or dimensions.
std::vector<std::uint8_t> make_header_only_sr_dcm();

// Builds an 8-bit uncompressed monochrome DICOM file in memory.
std::vector<std::uint8_t> make_monochrome_8bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& pixels,
    bool monochrome1 = false,
    std::optional<double> window_center = std::nullopt,
    std::optional<double> window_width = std::nullopt);

// Builds an 8-bit monochrome DICOM with optional FieldOfView orientation tags.
std::vector<std::uint8_t> make_oriented_monochrome_8bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& pixels,
    std::optional<double> fov_rotation = std::nullopt,
    std::optional<const char*> fov_horizontal_flip = std::nullopt);

// Builds a 16-bit allocated (12-bit or 16-bit stored) monochrome DICOM in memory.
std::vector<std::uint8_t> make_monochrome_16bit_dcm(
    int width, int height, int bits_stored, bool is_signed,
    const std::vector<std::uint16_t>& pixels,
    std::optional<double> window_center = std::nullopt,
    std::optional<double> window_width = std::nullopt,
    std::optional<double> rescale_slope = std::nullopt,
    std::optional<double> rescale_intercept = std::nullopt);

// Builds an 8-bit monochrome DICOM compressed with JPEG Lossless (Process 14 SV1).
std::vector<std::uint8_t> make_jpeg_lossless_8bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& pixels);

// Builds an 8-bit interleaved RGB DICOM file in memory.
std::vector<std::uint8_t> make_rgb_24bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& rgb_pixels);

}  // namespace dicom::test
