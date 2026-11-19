#include "DropListWidget.h"
#include "DragDropUtils.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

DropListWidget::DropListWidget(QWidget* parent)
    : QListWidget(parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
}

void DropListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QListWidget::dragEnterEvent(event);
    }
}

void DropListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QListWidget::dragMoveEvent(event);
    }
}

void DropListWidget::dropEvent(QDropEvent* event) {
    if (const QStringList files = dragdrop::localFilesFromDrop(event); !files.isEmpty()) {
        emit filesDropped(files);
        event->acceptProposedAction();
        return;
    }
    QListWidget::dropEvent(event);
}
