#pragma once

#include <QSize>
#include <QString>

class QMdiSubWindow;

struct ViewerState {
    QMdiSubWindow* sub_window = nullptr;
    QString file_path;
    QString file_name;
    QSize image_size;
    double zoom = 1.0;
    QString pixel_info;
    bool pixel_mode = false;
};
