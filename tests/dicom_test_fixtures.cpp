#include "dicom_test_fixtures.h"

#include <cstddef>
#include <stdexcept>
#include <string>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcwcache.h>
#include <dcmtk/dcmjpeg/djencode.h>
#include <dcmtk/dcmjpeg/djrplol.h>

namespace dicom::test {
namespace {

struct EncoderGuard {
  EncoderGuard() { DJEncoderRegistration::registerCodecs(); }
  ~EncoderGuard() { DJEncoderRegistration::cleanup(); }
};

void ensure_encoders_registered() {
  static const EncoderGuard kGuard;
}

void populate_common_sop_tags(DcmDataset& dataset, const char* sop_class_uid) {
  char uid_buf[128]{};
  dcmGenerateUniqueIdentifier(uid_buf, SITE_INSTANCE_UID_ROOT);
  dataset.putAndInsertString(DCM_SOPClassUID, sop_class_uid);
  dataset.putAndInsertString(DCM_SOPInstanceUID, uid_buf);
  dataset.putAndInsertString(DCM_PatientName, "Test^Patient");
}

void populate_window_and_rescale(
    DcmDataset& dataset, std::optional<double> window_center,
    std::optional<double> window_width,
    std::optional<double> rescale_slope = std::nullopt,
    std::optional<double> rescale_intercept = std::nullopt) {
  if (window_center.has_value() && window_width.has_value()) {
    dataset.putAndInsertString(DCM_WindowCenter,
                               std::to_string(*window_center).c_str());
    dataset.putAndInsertString(DCM_WindowWidth,
                               std::to_string(*window_width).c_str());
  }
  if (rescale_slope.has_value()) {
    dataset.putAndInsertString(DCM_RescaleSlope,
                               std::to_string(*rescale_slope).c_str());
  }
  if (rescale_intercept.has_value()) {
    dataset.putAndInsertString(DCM_RescaleIntercept,
                               std::to_string(*rescale_intercept).c_str());
  }
}

std::vector<std::uint8_t> serialize_dicom(DcmFileFormat& file_format,
                                          E_TransferSyntax xfer) {
  constexpr std::size_t kChunkSize = 65536U;
  std::vector<std::uint8_t> scratch(kChunkSize);
  std::vector<std::uint8_t> output;

  DcmOutputBufferStream stream(scratch.data(),
                               static_cast<offile_off_t>(scratch.size()));
  DcmWriteCache write_cache;

  file_format.transferInit();
  OFCondition status = EC_Normal;
  do {
    status = file_format.write(stream, xfer, EET_ExplicitLength, &write_cache);
    stream.flush();

    void* flushed_ptr = nullptr;
    offile_off_t flushed_len = 0;
    stream.flushBuffer(flushed_ptr, flushed_len);
    if (flushed_ptr != nullptr && flushed_len > 0) {
      const auto* bytes = static_cast<const std::uint8_t*>(flushed_ptr);
      output.insert(output.end(), bytes,
                    bytes + static_cast<std::size_t>(flushed_len));
    }
  } while (status == EC_StreamNotifyClient);
  file_format.transferEnd();

  if (status.bad()) {
    throw std::runtime_error(std::string("Failed to serialize test DICOM: ") +
                             status.text());
  }
  return output;
}

DcmFileFormat build_monochrome_8bit_file_format(
    int width, int height, const std::vector<std::uint8_t>& pixels,
    bool monochrome1, std::optional<double> window_center,
    std::optional<double> window_width) {
  DcmFileFormat file_format;
  DcmDataset* dataset = file_format.getDataset();
  populate_common_sop_tags(*dataset, UID_SecondaryCaptureImageStorage);

  dataset->putAndInsertString(DCM_Modality, "OT");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
  dataset->putAndInsertString(DCM_PhotometricInterpretation,
                              monochrome1 ? "MONOCHROME1" : "MONOCHROME2");
  dataset->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(height));
  dataset->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(width));
  dataset->putAndInsertUint16(DCM_BitsAllocated, 8);
  dataset->putAndInsertUint16(DCM_BitsStored, 8);
  dataset->putAndInsertUint16(DCM_HighBit, 7);
  dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
  populate_window_and_rescale(*dataset, window_center, window_width);
  dataset->putAndInsertUint8Array(DCM_PixelData, pixels.data(),
                                  static_cast<unsigned long>(pixels.size()));
  return file_format;
}

DcmFileFormat build_monochrome_16bit_file_format(
    int width, int height, int bits_stored, bool is_signed,
    const std::vector<std::uint16_t>& pixels,
    std::optional<double> window_center, std::optional<double> window_width,
    std::optional<double> rescale_slope,
    std::optional<double> rescale_intercept) {
  DcmFileFormat file_format;
  DcmDataset* dataset = file_format.getDataset();
  populate_common_sop_tags(*dataset, UID_CTImageStorage);

  dataset->putAndInsertString(DCM_Modality, "CT");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
  dataset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
  dataset->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(height));
  dataset->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(width));
  dataset->putAndInsertUint16(DCM_BitsAllocated, 16);
  dataset->putAndInsertUint16(DCM_BitsStored, static_cast<Uint16>(bits_stored));
  dataset->putAndInsertUint16(DCM_HighBit, static_cast<Uint16>(bits_stored - 1));
  dataset->putAndInsertUint16(DCM_PixelRepresentation, is_signed ? 1 : 0);
  populate_window_and_rescale(*dataset, window_center, window_width,
                              rescale_slope, rescale_intercept);
  dataset->putAndInsertUint16Array(DCM_PixelData, pixels.data(),
                                   static_cast<unsigned long>(pixels.size()));
  return file_format;
}

}  // namespace

std::vector<std::uint8_t> make_header_only_sr_dcm() {
  DcmFileFormat file_format;
  DcmDataset* dataset = file_format.getDataset();
  populate_common_sop_tags(*dataset, UID_BasicTextSRStorage);
  dataset->putAndInsertString(DCM_Modality, "SR");
  return serialize_dicom(file_format, EXS_LittleEndianExplicit);
}

std::vector<std::uint8_t> make_monochrome_8bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& pixels,
    bool monochrome1, std::optional<double> window_center,
    std::optional<double> window_width) {
  DcmFileFormat file_format = build_monochrome_8bit_file_format(
      width, height, pixels, monochrome1, window_center, window_width);
  return serialize_dicom(file_format, EXS_LittleEndianExplicit);
}

std::vector<std::uint8_t> make_oriented_monochrome_8bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& pixels,
    std::optional<double> fov_rotation,
    std::optional<const char*> fov_horizontal_flip) {
  DcmFileFormat file_format = build_monochrome_8bit_file_format(
      width, height, pixels, /*monochrome1=*/false, std::nullopt, std::nullopt);
  DcmDataset* dataset = file_format.getDataset();
  if (fov_rotation.has_value()) {
    dataset->putAndInsertString(DCM_FieldOfViewRotation,
                               std::to_string(*fov_rotation).c_str());
  }
  if (fov_horizontal_flip.has_value()) {
    dataset->putAndInsertString(DCM_FieldOfViewHorizontalFlip,
                               *fov_horizontal_flip);
  }
  return serialize_dicom(file_format, EXS_LittleEndianExplicit);
}

std::vector<std::uint8_t> make_monochrome_16bit_dcm(
    int width, int height, int bits_stored, bool is_signed,
    const std::vector<std::uint16_t>& pixels,
    std::optional<double> window_center, std::optional<double> window_width,
    std::optional<double> rescale_slope,
    std::optional<double> rescale_intercept) {
  DcmFileFormat file_format = build_monochrome_16bit_file_format(
      width, height, bits_stored, is_signed, pixels, window_center,
      window_width, rescale_slope, rescale_intercept);
  return serialize_dicom(file_format, EXS_LittleEndianExplicit);
}

std::vector<std::uint8_t> make_window_presets_16bit_dcm(
    int width, int height, const std::vector<std::uint16_t>& pixels,
    const std::string& window_centers, const std::string& window_widths,
    std::optional<std::string> window_explanations) {
  DcmFileFormat file_format = build_monochrome_16bit_file_format(
      width, height, /*bits_stored=*/12, /*is_signed=*/false, pixels,
      std::nullopt, std::nullopt, std::nullopt, std::nullopt);
  DcmDataset* dataset = file_format.getDataset();
  dataset->putAndInsertString(DCM_WindowCenter, window_centers.c_str());
  dataset->putAndInsertString(DCM_WindowWidth, window_widths.c_str());
  if (window_explanations.has_value()) {
    dataset->putAndInsertString(DCM_WindowCenterWidthExplanation,
                                window_explanations->c_str());
  }
  return serialize_dicom(file_format, EXS_LittleEndianExplicit);
}

std::vector<std::uint8_t> make_jpeg_lossless_8bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& pixels) {
  ensure_encoders_registered();
  DcmFileFormat file_format = build_monochrome_8bit_file_format(
      width, height, pixels, /*monochrome1=*/false, std::nullopt, std::nullopt);

  DJ_RPLossless lossless_params;
  const OFCondition status = file_format.getDataset()->chooseRepresentation(
      EXS_JPEGProcess14SV1, &lossless_params);
  if (status.bad()) {
    throw std::runtime_error(
        std::string("Failed to compress JPEG Lossless test fixture: ") +
        status.text());
  }
  return serialize_dicom(file_format, EXS_JPEGProcess14SV1);
}

std::vector<std::uint8_t> make_rgb_24bit_dcm(
    int width, int height, const std::vector<std::uint8_t>& rgb_pixels) {
  DcmFileFormat file_format;
  DcmDataset* dataset = file_format.getDataset();
  populate_common_sop_tags(*dataset, UID_SecondaryCaptureImageStorage);

  dataset->putAndInsertString(DCM_Modality, "OT");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 3);
  dataset->putAndInsertString(DCM_PhotometricInterpretation, "RGB");
  dataset->putAndInsertUint16(DCM_PlanarConfiguration, 0);
  dataset->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(height));
  dataset->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(width));
  dataset->putAndInsertUint16(DCM_BitsAllocated, 8);
  dataset->putAndInsertUint16(DCM_BitsStored, 8);
  dataset->putAndInsertUint16(DCM_HighBit, 7);
  dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
  dataset->putAndInsertUint8Array(
      DCM_PixelData, rgb_pixels.data(),
      static_cast<unsigned long>(rgb_pixels.size()));
  return serialize_dicom(file_format, EXS_LittleEndianExplicit);
}

}  // namespace dicom::test
