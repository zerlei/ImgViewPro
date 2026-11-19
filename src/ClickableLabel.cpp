#include "ClickableLabel.h"

#include <QMouseEvent>

ClickableLabel::ClickableLabel(QWidget* parent)
    : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QObject::tr("点击输入像素坐标，定位并放大到该像素"));
}

void ClickableLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QLabel::mousePressEvent(event);
}
