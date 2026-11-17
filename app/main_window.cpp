#include "main_window.h"
#include "ui_main_window.h"

#include <filesystem>
#include <utility>
#include <variant>

#include <QColor>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>

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

    connect(ui->actionSelect_image, &QAction::triggered,
            this, &MainWindow::open_dicom_file);
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
    auto render_result = image.render();
    if (const auto *error = std::get_if<dicom::Error>(&render_result)) {
        show_dicom_error(this, tr("Failed to Render DICOM"), *error);
        return;
    }

    const auto &buffer = std::get<dicom::ImageBuffer>(render_result);
    display_buffer(buffer, image.is_monochrome());
    current_image_ = std::move(image);
}

void MainWindow::display_buffer(const dicom::ImageBuffer &buffer,
                                bool is_monochrome)
{
    const bool grayscale = buffer.format == dicom::ImageFormat::kGrayscale8;
    const QImage::Format qt_format =
        grayscale ? QImage::Format_Grayscale8 : QImage::Format_RGB888;
    const qsizetype bytes_per_line =
        static_cast<qsizetype>(grayscale ? buffer.width : buffer.width * 3);

    const QImage image(buffer.data.data(), buffer.width, buffer.height,
                       bytes_per_line, qt_format);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Display Error"),
                             tr("Failed to construct Qt image from pixel buffer."));
        return;
    }

    const QPixmap pixmap = QPixmap::fromImage(image);
    if (pixmap_item_ == nullptr) {
        pixmap_item_ = scene_->addPixmap(pixmap);
    } else {
        pixmap_item_->setPixmap(pixmap);
    }

    scene_->setSceneRect(pixmap.rect());
    ui->graphicsView->fitInView(pixmap_item_, Qt::KeepAspectRatio);
    setWindowTitle(
        QString("DICOM Viewer - %1 x %2 | %3")
            .arg(buffer.width)
            .arg(buffer.height)
            .arg(is_monochrome ? tr("Monochrome") : tr("RGB")));
}
