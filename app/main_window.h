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

private:
    void display_buffer(const dicom::ImageBuffer &buffer, bool is_monochrome);

    Ui::MainWindow *ui;
    QGraphicsScene *scene_ = nullptr;
    QGraphicsPixmapItem *pixmap_item_ = nullptr;
    std::optional<dicom::DicomImage> current_image_;
};

#endif // MAIN_WINDOW_H
