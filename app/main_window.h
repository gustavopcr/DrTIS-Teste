#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <optional>

#include <QMainWindow>

#include "dicom/dicom_image.h"

class QGraphicsPixmapItem;
class QGraphicsScene;
class QResizeEvent;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void open_dicom_file();
    void on_window_preset_changed(int index);
    void on_reset_window_clicked();

private:
    void populate_window_presets(const dicom::DicomImage &image);
    void populate_metadata(const dicom::DicomImage &image);
    bool render_and_display(dicom::DicomImage &image, bool fit_view);

    Ui::MainWindow *ui;
    QGraphicsScene *scene_ = nullptr;
    QGraphicsPixmapItem *pixmap_item_ = nullptr;
    std::optional<dicom::DicomImage> current_image_;
};

#endif // MAIN_WINDOW_H
