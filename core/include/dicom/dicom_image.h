#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace dicom {

enum class ErrorCode {
  kFileNotFound,
  kInvalidDicomFile,
  kMissingImageData,
  kUnsupportedFormat,
  kRenderFailed
};

struct Error {
  ErrorCode code;
  std::string message;
  std::string details;
};

enum class ImageFormat {
  kGrayscale8,
  kRgb24,
};

struct ImageBuffer {
  int width = 0;
  int height = 0;
  ImageFormat format = ImageFormat::kGrayscale8;
  std::vector<std::uint8_t> data;
};

struct WindowLevel {
  double center = 0.0;
  double width = 1.0;
};

struct WindowPreset {
  std::string explanation;
  WindowLevel window;
};

struct DicomMetadata {
  std::string patient_name;
  std::string patient_id;
  std::string study_date;
  std::string study_description;
  std::string series_description;
  std::string modality;
  std::string view_position;
  std::string laterality;
  std::string patient_orientation;
  std::string photometric_interpretation;
  std::string pixel_spacing;
  std::string transfer_syntax;
  int bits_allocated = 0;
  int bits_stored = 0;
};

class DicomImage {
 public:
  static std::variant<DicomImage, Error> load_from_file(
      const std::filesystem::path& path);
  static std::variant<DicomImage, Error> load_from_memory(
      const std::uint8_t* data, std::size_t size);

  ~DicomImage();
  DicomImage(DicomImage&&) noexcept;
  DicomImage& operator=(DicomImage&&) noexcept;
  DicomImage(const DicomImage&) = delete;
  DicomImage& operator=(const DicomImage&) = delete;

  int get_width() const;
  int get_height() const;
  bool is_monochrome() const;
  const std::vector<WindowPreset>& get_window_presets() const;
  std::optional<WindowLevel> get_window() const;
  const DicomMetadata& get_metadata() const;

  std::optional<Error> apply_window_preset(std::size_t index);
  void reset_window();

  std::variant<ImageBuffer, Error> render();

 private:
  struct Impl;
  explicit DicomImage(std::unique_ptr<Impl> impl);
  static std::variant<DicomImage, Error> decode_image(
      std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace dicom
