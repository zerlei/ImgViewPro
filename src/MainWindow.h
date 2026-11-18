#pragma once

#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMdiArea>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>

class ImageViewer;
class QAction;
class QDockWidget;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QListWidgetItem;
class QMdiSubWindow;
class QMouseEvent;

class DropMdiArea : public QMdiArea {
    Q_OBJECT

      public:
        explicit DropMdiArea(QWidget* parent = nullptr);

      signals:
        void filesDropped(const QStringList& files);

      protected:
        void dragEnterEvent(QDragEnterEvent * event) override;
        void dragMoveEvent(QDragMoveEvent * event) override;
        void dropEvent(QDropEvent * event) override;
};

class DropListWidget : public QListWidget {
    Q_OBJECT

      public:
        explicit DropListWidget(QWidget* parent = nullptr);

      signals:
        void filesDropped(const QStringList& files);

      protected:
        void dragEnterEvent(QDragEnterEvent * event) override;
        void dragMoveEvent(QDragMoveEvent * event) override;
        void dropEvent(QDropEvent * event) override;
};

class ClickableLabel : public QLabel {
    Q_OBJECT

      public:
        explicit ClickableLabel(QWidget* parent = nullptr);

      signals:
        void clicked();

      protected:
        void mousePressEvent(QMouseEvent * event) override;
};

struct ViewerState {
    QMdiSubWindow* sub_window = nullptr;
    QString file_path;
    QString file_name;
    QSize image_size;
    double zoom = 1.0;
    QString pixel_info;
    bool pixel_mode = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

      public:
        MainWindow();
        ~MainWindow() override;

      public slots:
        void openImages(const QStringList& files);

      private slots:
        void onOpen();
        void onAbout();
        void onSubWindowActivated(QMdiSubWindow * sub);
        void onTile();
        void onCascade();
        void onCloseCurrent();
        void onCloseAll();
        void onUserZoomed(double zoom, const QPointF& centerDelta);
        void onUserPanned(const QPointF& delta);
        void onSetDefault();
        void onRemoveDefault();
        void onPixelLabelClicked();

      private:
        ImageViewer* activeViewer() const;
        void addImageWindow(const QString& file_path);
        void updateStatusBar();
        void updateDockSelection();
        void createActions();
        void createDockWidget();
        void createMenus();
        void createToolBar();
        void createStatusBar();

        DropMdiArea* m_mdi_area = nullptr;
        QDockWidget* m_dock = nullptr;
        DropListWidget* m_image_list = nullptr;

        QHash<QListWidgetItem*, QMdiSubWindow*> m_item_to_sub;
        QHash<ImageViewer*, ViewerState> m_states;

        QAction* m_open_act = nullptr;
        QAction* m_exit_act = nullptr;
        QAction* m_zoom_in_act = nullptr;
        QAction* m_zoom_out_act = nullptr;
        QAction* m_fit_act = nullptr;
        QAction* m_zoom100_act = nullptr;
        QAction* m_show_rgb_act = nullptr;
        QAction* m_sync_act = nullptr;
        bool m_sync_enabled = false;
        QAction* m_tile_act = nullptr;
        QAction* m_cascade_act = nullptr;
        QAction* m_close_act = nullptr;
        QAction* m_close_all_act = nullptr;
        QAction* m_set_default_act = nullptr;
        QAction* m_remove_default_act = nullptr;
        QAction* m_about_act = nullptr;

        QLabel* m_count_label = nullptr;
        QLabel* m_image_label = nullptr;
        QLabel* m_zoom_label = nullptr;
        QLabel* m_mode_label = nullptr;
        QLabel* m_sync_label = nullptr;
        ClickableLabel* m_pixel_label = nullptr;
};
