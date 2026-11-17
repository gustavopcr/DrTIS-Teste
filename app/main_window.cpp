#include "main_window.h"
#include "ui_main_window.h"

#include <filesystem>
#include <utility>
#include <variant>

#include <QColor>
#include <QComboBox>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>

namespace {

void show_dicom_error(QWidget *parent, const QString &title,
                      const dicom::Error &error)
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(error.message.empty()
                    ? QObject::tr("An unexpected DICOM error occurred.")
                    : QString::fromStdString(error.message));
    if (!error.details.empty()) {
        box.setDetailedText(QString::fromStdString(error.details));
    }
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

QString format_preset_label(const dicom::WindowPreset &preset)
{
    if (!preset.explanation.empty()) {
        return QString::fromStdString(preset.explanation);
    }
    return QString("%1 / %2")
        .arg(preset.window.center, 0, 'g', 6)
        .arg(preset.window.width, 0, 'g', 6);
}

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , scene_(new QGraphicsScene(this))
{
    ui->setupUi(this);
    scene_->setBackgroundBrush(QColor(30, 30, 30));
    ui->graphicsView->setScene(scene_);
    ui->graphicsView->setRenderHint(QPainter::SmoothPixmapTransform, true);
    ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->windowPresetcomboBox->setEnabled(false);
    ui->resetButton->setEnabled(false);

    connect(ui->actionSelect_image, &QAction::triggered,
            this, &MainWindow::open_dicom_file);
    connect(ui->windowPresetcomboBox, &QComboBox::currentIndexChanged,
            this, &MainWindow::on_window_preset_changed);
    connect(ui->resetButton, &QPushButton::clicked,
            this, &MainWindow::on_reset_window_clicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (pixmap_item_ != nullptr) {
        ui->graphicsView->fitInView(pixmap_item_, Qt::KeepAspectRatio);
    }
}

void MainWindow::open_dicom_file()
{
    const QString file_path = QFileDialog::getOpenFileName(
        this,
        tr("Select DICOM Image"),
        QString(),
        tr("DICOM Files (*.dcm);;All Files (*)"));

    if (file_path.isEmpty()) {
        return;
    }

    auto load_result = dicom::DicomImage::load_from_file(
        std::filesystem::path(file_path.toStdU16String()));
    if (const auto *error = std::get_if<dicom::Error>(&load_result)) {
        show_dicom_error(this, tr("Unable to Open DICOM File"), *error);
        return;
    }

    dicom::DicomImage image = std::move(std::get<dicom::DicomImage>(load_result));
    if (!render_and_display(image, true)) {
        return;
    }

    current_image_ = std::move(image);
    populate_window_presets(*current_image_);
}

void MainWindow::on_window_preset_changed(int index)
{
    if (!current_image_.has_value() || index < 0) {
        return;
    }

    if (const auto error =
            current_image_->apply_window_preset(static_cast<std::size_t>(index))) {
        show_dicom_error(this, tr("Window Preset Error"), *error);
        return;
    }

    render_and_display(*current_image_, false);
}

void MainWindow::on_reset_window_clicked()
{
    if (!current_image_.has_value()) {
        return;
    }

    current_image_->reset_window();
    {
        const QSignalBlocker blocker(ui->windowPresetcomboBox);
        if (ui->windowPresetcomboBox->count() > 0) {
            ui->windowPresetcomboBox->setCurrentIndex(0);
        }
    }
    render_and_display(*current_image_, false);
}

void MainWindow::populate_window_presets(const dicom::DicomImage &image)
{
    const QSignalBlocker blocker(ui->windowPresetcomboBox);
    ui->windowPresetcomboBox->clear();

    const auto &presets = image.get_window_presets();
    const bool has_presets = !presets.empty();
    for (const auto &preset : presets) {
        ui->windowPresetcomboBox->addItem(format_preset_label(preset));
    }

    ui->windowPresetcomboBox->setEnabled(has_presets);
    ui->resetButton->setEnabled(has_presets);
}

bool MainWindow::render_and_display(dicom::DicomImage &image, bool fit_view)
{
    auto render_result = image.render();
    if (const auto *error = std::get_if<dicom::Error>(&render_result)) {
        show_dicom_error(this, tr("Failed to Render DICOM"), *error);
        return false;
    }

    const auto &buffer = std::get<dicom::ImageBuffer>(render_result);
    const bool grayscale = buffer.format == dicom::ImageFormat::kGrayscale8;
    const QImage::Format qt_format =
        grayscale ? QImage::Format_Grayscale8 : QImage::Format_RGB888;
    const qsizetype bytes_per_line =
        static_cast<qsizetype>(grayscale ? buffer.width : buffer.width * 3);

    const QImage qt_image(buffer.data.data(), buffer.width, buffer.height,
                          bytes_per_line, qt_format);
    if (qt_image.isNull()) {
        QMessageBox::warning(this, tr("Display Error"),
                             tr("Failed to construct Qt image from pixel buffer."));
        return false;
    }

    const QPixmap pixmap = QPixmap::fromImage(qt_image);
    if (pixmap_item_ == nullptr) {
        pixmap_item_ = scene_->addPixmap(pixmap);
    } else {
        pixmap_item_->setPixmap(pixmap);
    }

    scene_->setSceneRect(pixmap.rect());
    if (fit_view) {
        ui->graphicsView->fitInView(pixmap_item_, Qt::KeepAspectRatio);
    }

    QString title = QString("DICOM Viewer - %1 x %2 | %3")
        .arg(buffer.width)
        .arg(buffer.height)
        .arg(image.is_monochrome() ? tr("Monochrome") : tr("RGB"));
    if (const auto window = image.get_window()) {
        title += QString(" | WL: %1 / WW: %2")
                     .arg(window->center, 0, 'g', 6)
                     .arg(window->width, 0, 'g', 6);
    }
    setWindowTitle(title);
    return true;
}
