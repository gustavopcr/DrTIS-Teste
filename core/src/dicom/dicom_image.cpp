#include "dicom/dicom_image.h"

#include <limits>
#include <memory>
#include <system_error>
#include <utility>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmimage/diregist.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpls/djdecode.h>

namespace {

struct CodecGuard {
  CodecGuard() {
    DJDecoderRegistration::registerCodecs();
    DJLSDecoderRegistration::registerCodecs();
    DcmRLEDecoderRegistration::registerCodecs();
  }

  ~CodecGuard() {
    DcmRLEDecoderRegistration::cleanup();
    DJLSDecoderRegistration::cleanup();
    DJDecoderRegistration::cleanup();
  }
};

void ensure_codecs_registered() {
  static const CodecGuard kGuard;
}

dicom::Error map_image_status_error(EI_Status status) {
  const char* detail = ::DicomImage::getString(status);
  const std::string message =
      detail != nullptr ? detail : "Unknown DICOM image error";

  switch (status) {
    case EIS_MissingAttribute:
      return dicom::Error{dicom::ErrorCode::kMissingImageData, message};
    case EIS_NotSupportedValue:
      return dicom::Error{dicom::ErrorCode::kUnsupportedFormat, message};
    case EIS_InvalidDocument:
    case EIS_InvalidValue:
    case EIS_InvalidImage:
      return dicom::Error{dicom::ErrorCode::kInvalidDicomFile, message};
    case EIS_NoDataDictionary:
    case EIS_MemoryFailure:
    case EIS_OtherError:
    case EIS_Normal:
      return dicom::Error{dicom::ErrorCode::kRenderFailed, message};
  }
  return dicom::Error{dicom::ErrorCode::kRenderFailed, message};
}

void apply_default_voi(::DicomImage& image) {
  if (!image.isMonochrome()) {
    return;
  }
  if (image.getWindowCount() > 0UL) {
    image.setWindow(0UL);
    return;
  }
  if (image.getVoiLutCount() > 0UL) {
    image.setVoiLut(0UL);
    return;
  }
  image.setMinMaxWindow();
}

}  // namespace

namespace dicom {

struct DicomImage::Impl {
  // Declared first so ::DicomImage is destroyed before the DcmFileFormat it references.
  DcmFileFormat file_format;
  std::unique_ptr<::DicomImage> dcm_image;
};

DicomImage::DicomImage(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

DicomImage::~DicomImage() = default;
DicomImage::DicomImage(DicomImage&&) noexcept = default;
DicomImage& DicomImage::operator=(DicomImage&&) noexcept = default;

std::variant<DicomImage, Error> DicomImage::decode_image(
    std::unique_ptr<Impl> impl) {
  if (impl->file_format.getDataset() == nullptr) {
    return Error{ErrorCode::kInvalidDicomFile,
                 "DICOM file does not contain a dataset"};
  }

  impl->dcm_image =
      std::make_unique<::DicomImage>(&impl->file_format, EXS_Unknown);
  const EI_Status image_status = impl->dcm_image->getStatus();
  if (image_status != EIS_Normal) {
    return map_image_status_error(image_status);
  }

  if (impl->dcm_image->getWidth() == 0UL ||
      impl->dcm_image->getHeight() == 0UL ||
      impl->dcm_image->getFrameCount() == 0UL) {
    return Error{ErrorCode::kMissingImageData,
                 "DICOM image has empty dimensions or zero frames"};
  }

  apply_default_voi(*impl->dcm_image);
  return DicomImage(std::move(impl));
}

std::variant<DicomImage, Error> DicomImage::load_from_file(
    const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return Error{ErrorCode::kFileNotFound,
                 "File not found: " + path.string()};
  }
  if (!std::filesystem::is_regular_file(path, ec) || ec) {
    return Error{ErrorCode::kInvalidDicomFile,
                 "Path is not a regular file: " + path.string()};
  }

  ensure_codecs_registered();

  auto impl = std::make_unique<Impl>();
  const OFCondition load_status = impl->file_format.loadFile(path.c_str());
  if (load_status.bad()) {
    return Error{ErrorCode::kInvalidDicomFile, load_status.text()};
  }

  return decode_image(std::move(impl));
}

std::variant<DicomImage, Error> DicomImage::load_from_memory(
    const std::uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0U) {
    return Error{ErrorCode::kInvalidDicomFile, "Memory buffer is empty"};
  }
  if (size > static_cast<std::size_t>(std::numeric_limits<offile_off_t>::max())) {
    return Error{ErrorCode::kInvalidDicomFile,
                 "Memory buffer exceeds maximum stream size"};
  }

  ensure_codecs_registered();

  DcmInputBufferStream stream;
  stream.setBuffer(data, static_cast<offile_off_t>(size));
  stream.setEos();

  auto impl = std::make_unique<Impl>();
  impl->file_format.transferInit();
  const OFCondition read_status = impl->file_format.read(stream);
  impl->file_format.transferEnd();

  if (read_status.bad()) {
    return Error{ErrorCode::kInvalidDicomFile, read_status.text()};
  }

  return decode_image(std::move(impl));
}

int DicomImage::get_height() const {
  return static_cast<int>(impl_->dcm_image->getHeight());
}

int DicomImage::get_width() const {
  return static_cast<int>(impl_->dcm_image->getWidth());
}

bool DicomImage::is_monochrome() const {
  return impl_->dcm_image->isMonochrome() != 0;
}

std::variant<ImageBuffer, Error> DicomImage::render() {
  constexpr int kOutputBits = 8;
  const unsigned long byte_count =
      impl_->dcm_image->getOutputDataSize(kOutputBits);
  if (byte_count == 0UL) {
    return Error{ErrorCode::kRenderFailed,
                 "Failed to compute rendered pixel buffer size"};
  }

  ImageBuffer buffer{};
  buffer.width = get_width();
  buffer.height = get_height();
  buffer.format =
      is_monochrome() ? ImageFormat::kGrayscale8 : ImageFormat::kRgb24;
  buffer.data.resize(static_cast<std::size_t>(byte_count));

  const int ok = impl_->dcm_image->getOutputData(buffer.data.data(), byte_count,
                                                 kOutputBits);
  if (ok == 0) {
    return Error{ErrorCode::kRenderFailed, "Failed to render DICOM image data"};
  }

  return buffer;
}

}  // namespace dicom
