#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
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

  std::variant<ImageBuffer, Error> render();

 private:
  struct Impl;
  explicit DicomImage(std::unique_ptr<Impl> impl);
  static std::variant<DicomImage, Error> decode_image(
      std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace dicom
