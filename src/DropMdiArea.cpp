#include "DropMdiArea.h"
#include "DragDropUtils.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

DropMdiArea::DropMdiArea(QWidget* parent)
    : QMdiArea(parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

void DropMdiArea::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QMdiArea::dragEnterEvent(event);
    }
}

void DropMdiArea::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QMdiArea::dragMoveEvent(event);
    }
}

void DropMdiArea::dropEvent(QDropEvent* event) {
    if (const QStringList files = dragdrop::localFilesFromDrop(event); !files.isEmpty()) {
        emit filesDropped(files);
        event->acceptProposedAction();
        return;
    }
    QMdiArea::dropEvent(event);
}
