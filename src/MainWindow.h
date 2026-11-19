#pragma once

#include "ViewerState.h"

#include <QHash>
#include <QMainWindow>
#include <QPointF>
#include <QStringList>

class ClickableLabel;
class DropListWidget;
class DropMdiArea;
class ImageViewer;
class QAction;
class QDockWidget;
class QLabel;
class QListWidgetItem;
class QMdiSubWindow;

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
        void onSubWindowActivated(const QMdiSubWindow* sub);
        void onTile();
        void onCascade();
        void onCloseCurrent() const;
        void onCloseAll();
        void onUserZoomed(double zoom, const QPointF& centerDelta) const;
        void onUserPanned(const QPointF& delta) const;
        void onSetDefault();
        void onRemoveDefault();
        void onPixelLabelClicked();

      private:
        ImageViewer* activeViewer() const;
        void addImageWindow(const QString& file_path);
        void updateStatusBar();
        void updateDockSelection() const;
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
