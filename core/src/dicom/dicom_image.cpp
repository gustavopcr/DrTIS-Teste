#include "dicom/dicom_image.h"

#include <cmath>
#include <limits>
#include <memory>
#include <system_error>
#include <utility>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcerror.h>
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

dicom::Error make_error(dicom::ErrorCode code, std::string details) {
  switch (code) {
    case dicom::ErrorCode::kFileNotFound:
      return {code, "The selected file could not be found.", std::move(details)};
    case dicom::ErrorCode::kInvalidDicomFile:
      return {code, "The selected file is not a valid DICOM file.",
              std::move(details)};
    case dicom::ErrorCode::kMissingImageData:
      return {code, "The DICOM file does not contain image pixel data.",
              std::move(details)};
    case dicom::ErrorCode::kUnsupportedFormat:
      return {code, "This DICOM image format is not supported.",
              std::move(details)};
    case dicom::ErrorCode::kRenderFailed:
      return {code, "Failed to render the DICOM image.", std::move(details)};
  }
  return {code, "An unexpected DICOM error occurred.", std::move(details)};
}

dicom::Error make_error(const OFCondition& status) {
  const std::string details =
      status.text() != nullptr ? status.text() : "Unknown DICOM parser error";

  if (status == EC_UnsupportedEncoding || status == EC_UnknownTransferSyntax) {
    return make_error(dicom::ErrorCode::kUnsupportedFormat, details);
  }
  if (status == EC_MissingAttribute) {
    return make_error(dicom::ErrorCode::kMissingImageData, details);
  }
  return make_error(dicom::ErrorCode::kInvalidDicomFile, details);
}

dicom::Error make_error(EI_Status status) {
  const char* status_text = ::DicomImage::getString(status);
  const std::string details =
      status_text != nullptr ? status_text : "Unknown DICOM image error";

  switch (status) {
    case EIS_MissingAttribute:
      return make_error(dicom::ErrorCode::kMissingImageData, details);
    case EIS_NotSupportedValue:
      return make_error(dicom::ErrorCode::kUnsupportedFormat, details);
    case EIS_InvalidDocument:
    case EIS_InvalidValue:
    case EIS_InvalidImage:
      return make_error(dicom::ErrorCode::kInvalidDicomFile, details);
    case EIS_NoDataDictionary:
    case EIS_MemoryFailure:
    case EIS_OtherError:
    case EIS_Normal:
      break;
  }
  return make_error(dicom::ErrorCode::kRenderFailed, details);
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

void apply_fov_orientation(DcmDataset& dataset, ::DicomImage& image) {
  Float64 rotation = 0.0;
  if (dataset.findAndGetFloat64(DCM_FieldOfViewRotation, rotation).good() &&
      std::isfinite(rotation) && std::abs(rotation) <= 360.0) {
    const int degrees = static_cast<int>(std::lround(rotation)) % 360;
    if (degrees != 0 && degrees % 90 == 0) {
      image.rotateImage(degrees);
    }
  }

  OFString horizontal_flip;
  if (dataset.findAndGetOFString(DCM_FieldOfViewHorizontalFlip, horizontal_flip)
          .good() &&
      horizontal_flip == "YES") {
    image.flipImage(1, 0);
  }
}

}  // namespace

namespace dicom {

struct DicomImage::Impl {
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
    return make_error(ErrorCode::kInvalidDicomFile,
                      "DcmFileFormat::getDataset() returned nullptr");
  }

  impl->dcm_image =
      std::make_unique<::DicomImage>(&impl->file_format, EXS_Unknown);
  const EI_Status image_status = impl->dcm_image->getStatus();
  if (image_status != EIS_Normal) {
    return make_error(image_status);
  }

  if (impl->dcm_image->getWidth() == 0UL ||
      impl->dcm_image->getHeight() == 0UL ||
      impl->dcm_image->getFrameCount() == 0UL) {
    return make_error(ErrorCode::kMissingImageData,
                      "Decoded image has zero width, height, or frame count");
  }

  apply_fov_orientation(*impl->file_format.getDataset(), *impl->dcm_image);
  apply_default_voi(*impl->dcm_image);
  return DicomImage(std::move(impl));
}

std::variant<DicomImage, Error> DicomImage::load_from_file(
    const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    return make_error(
        ErrorCode::kFileNotFound,
        ec ? ec.message() : ("File does not exist: " + path.string()));
  }
  if (!std::filesystem::is_regular_file(path, ec) || ec) {
    return make_error(
        ErrorCode::kInvalidDicomFile,
        ec ? ec.message() : ("Path is not a regular file: " + path.string()));
  }

  ensure_codecs_registered();

  auto impl = std::make_unique<Impl>();
  const OFCondition load_status = impl->file_format.loadFile(path.c_str());
  if (load_status.bad()) {
    return make_error(load_status);
  }

  return decode_image(std::move(impl));
}

std::variant<DicomImage, Error> DicomImage::load_from_memory(
    const std::uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0U) {
    return make_error(ErrorCode::kInvalidDicomFile,
                      "Memory buffer pointer is null or size is 0");
  }
  if (size > static_cast<std::size_t>(std::numeric_limits<offile_off_t>::max())) {
    return make_error(ErrorCode::kInvalidDicomFile,
                      "Memory buffer exceeds maximum stream size");
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
    return make_error(read_status);
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
    return make_error(ErrorCode::kRenderFailed,
                      "DicomImage::getOutputDataSize() returned 0");
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
    const EI_Status status = impl_->dcm_image->getStatus();
    return status != EIS_Normal
               ? make_error(status)
               : make_error(ErrorCode::kRenderFailed,
                            "DicomImage::getOutputData() returned 0");
  }

  return buffer;
}

}  // namespace dicom
