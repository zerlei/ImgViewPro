#pragma once

#include <QMdiArea>
#include <QStringList>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class DropMdiArea : public QMdiArea {
    Q_OBJECT

      public:
        explicit DropMdiArea(QWidget* parent = nullptr);

      signals:
        void filesDropped(const QStringList& files);

      protected:
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent* event) override;
};
