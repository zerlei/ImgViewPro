#pragma once

#include <QGraphicsView>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtGlobal>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QGraphicsPixmapItem;
class QGraphicsScene;
class QMouseEvent;
class QPainter;
class QRectF;
class QResizeEvent;
class QWheelEvent;

class ImageViewer : public QGraphicsView {
    Q_OBJECT

      public:
        explicit ImageViewer(QWidget* parent = nullptr);
        ~ImageViewer() override;

        bool openImage(const QString& file_path);
        void zoomIn();
        void zoomOut();
        void zoomToFit();
        void zoom100();
        void setShowRgbValues(bool enabled);
        void applyAbsoluteZoom(double zoom, const QPointF& centerDelta);
        void applyPanBy(const QPointF& delta);
        void focusOnPixel(int x, int y);

        double currentZoom() const {
            return m_zoom;
        }
        QImage image() const {
            return m_image;
        }
        QSize imageSize() const {
            return m_image.size();
        }
        bool hasImage() const {
            return !m_image.isNull();
        }
        bool isPixelMode() const {
            return m_pixel_mode;
        }
        bool isFitMode() const {
            return m_fit_mode;
        }

      signals:
        void zoomChanged(double factor);
        void imageLoaded(const QString& file_name, const QSize& size);
        void pixelInfo(const QString& text);
        void pixelModeChanged(bool enabled);
        void openFailed(const QString& message);
        void filesDropped(const QStringList& files);
        void userZoomed(double zoom, const QPointF& centerDelta);
        void userPanned(const QPointF& delta);

      protected:
        void wheelEvent(QWheelEvent * event) override;
        void mousePressEvent(QMouseEvent * event) override;
        void mouseMoveEvent(QMouseEvent * event) override;
        void mouseReleaseEvent(QMouseEvent * event) override;
        void dragEnterEvent(QDragEnterEvent * event) override;
        void dragMoveEvent(QDragMoveEvent * event) override;
        void dropEvent(QDropEvent * event) override;
        void resizeEvent(QResizeEvent * event) override;
        void drawForeground(QPainter * painter, const QRectF& rect) override;

      private:
        void scaleBy(double factor, QGraphicsView::ViewportAnchor anchor);
        void updateScrollBarPolicy();
        void updateSmoothMode();
        void updatePixelInfo(const QPointF& scenePos);
        QPointF sceneCenter() const;
        QStringList pixelValueText(int x, int y) const;
        bool loadCsvImage(const QString& file_path, QImage* out_image);
        void releaseSourcePixels();

        struct SourcePixels;
        SourcePixels* m_source_pixels = nullptr;
        QGraphicsScene* m_scene = nullptr;
        QGraphicsPixmapItem* m_item = nullptr;
        QImage m_image;
        double m_zoom = 1.0;
        bool m_fit_mode = true;
        bool m_pixel_mode = false;
        bool m_show_rgb_values = true;
        bool m_panning = false;
        QPointF m_last_pan_center;
        qint64 m_last_fg_log_ms = 0;
};
