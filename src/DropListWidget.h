#pragma once

#include <QListWidget>
#include <QStringList>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

class DropListWidget : public QListWidget {
    Q_OBJECT

      public:
        explicit DropListWidget(QWidget* parent = nullptr);

      signals:
        void filesDropped(const QStringList& files);

      protected:
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent* event) override;
};
