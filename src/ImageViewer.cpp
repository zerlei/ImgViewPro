#include "ImageViewer.h"
#include "DebugLog.h"

#include <QColor>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRectF>
#include <QResizeEvent>
#include <QUrl>
#include <QWheelEvent>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <ranges>
#include <vector>

namespace {
    constexpr double k_zoom_step = 1.25;
    constexpr double k_min_zoom = 0.02;
    constexpr double k_max_zoom = 128.0;
    constexpr double k_pixel_zoom_threshold = 8.0;
    constexpr double k_rgb_label_threshold = 24.0;
    constexpr int k_value_digits = 10;

    void copyMatRowsToQImage(const cv::Mat& mat, QImage& image) {
        for (const int y : std::views::iota(0, mat.rows)) {
            std::memcpy(image.scanLine(y), mat.ptr<const uchar>(y),
                        static_cast<size_t>(mat.cols) * mat.elemSize());
        }
    }

    QImage matToQImage(const cv::Mat& mat) {
        if (mat.empty()) {
            return {};
        }

        if (mat.depth() == CV_8U && mat.channels() == 1) {
            QImage image(mat.cols, mat.rows, QImage::Format_Grayscale8);
            copyMatRowsToQImage(mat, image);
            return image;
        }
        if (mat.depth() == CV_8U && mat.channels() == 3) {
            cv::Mat rgb;
            cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
            QImage image(rgb.cols, rgb.rows, QImage::Format_RGB888);
            copyMatRowsToQImage(rgb, image);
            return image;
        }
        if (mat.depth() == CV_8U && mat.channels() == 4) {
            cv::Mat rgba;
            cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
            QImage image(rgba.cols, rgba.rows, QImage::Format_RGBA8888);
            copyMatRowsToQImage(rgba, image);
            return image;
        }

        double min_value = 0.0;
        double max_value = 0.0;
        if (mat.channels() == 1) {
            cv::minMaxLoc(mat, &min_value, &max_value);
        } else {
            std::vector<cv::Mat> planes;
            cv::split(mat, planes);
            min_value = std::numeric_limits<double>::max();
            max_value = std::numeric_limits<double>::lowest();
            for (const cv::Mat& plane : planes) {
                double channel_min = 0.0;
                double channel_max = 0.0;
                cv::minMaxLoc(plane, &channel_min, &channel_max);
                min_value = std::min(min_value, channel_min);
                max_value = std::max(max_value, channel_max);
            }
        }
        const double scale = max_value > min_value ? 255.0 / (max_value - min_value) : 1.0;
        const double shift = -min_value * scale;

        cv::Mat normalized;
        mat.convertTo(normalized, CV_8U, scale, shift);
        if (normalized.channels() == 1) {
            QImage image(normalized.cols, normalized.rows, QImage::Format_Grayscale8);
            copyMatRowsToQImage(normalized, image);
            return image;
        }
        if (normalized.channels() == 3) {
            cv::Mat rgb;
            cv::cvtColor(normalized, rgb, cv::COLOR_BGR2RGB);
            QImage image(rgb.cols, rgb.rows, QImage::Format_RGB888);
            copyMatRowsToQImage(rgb, image);
            return image;
        }
        if (normalized.channels() == 4) {
            cv::Mat rgba;
            cv::cvtColor(normalized, rgba, cv::COLOR_BGRA2RGBA);
            QImage image(rgba.cols, rgba.rows, QImage::Format_RGBA8888);
            copyMatRowsToQImage(rgba, image);
            return image;
        }
        return {};
    }

    QStringList valuesFromMat(const cv::Mat& mat, int x, int y) {
        QStringList values;
        if (mat.empty() || x < 0 || y < 0 || x >= mat.cols || y >= mat.rows) {
            return values;
        }

        const int channels = mat.channels();
        const int depth = mat.depth();
        const auto value_at = [&](int channel) -> QString {
            if (depth == CV_8U) {
                return QString::number(mat.ptr<uchar>(y)[x * channels + channel]);
            }
            if (depth == CV_16U) {
                return QString::number(mat.ptr<ushort>(y)[x * channels + channel]);
            }
            if (depth == CV_32F) {
                return QString::number(
                    static_cast<double>(mat.ptr<float>(y)[x * channels + channel]), 'g',
                    k_value_digits);
            }
            if (depth == CV_64F) {
                return QString::number(mat.ptr<double>(y)[x * channels + channel], 'g',
                                       k_value_digits);
            }
            return QStringLiteral("?");
        };

        if (channels == 1) {
            values << value_at(0);
        } else if (channels == 3) {
            values << value_at(2) << value_at(1) << value_at(0);
        } else if (channels == 4) {
            values << value_at(2) << value_at(1) << value_at(0) << value_at(3);
        } else {
            for (const int channel : std::views::iota(0, channels)) {
                values << value_at(channel);
            }
        }
        return values;
    }
} // namespace

struct ImageViewer::SourcePixels {
    cv::Mat mat;
};

ImageViewer::~ImageViewer() {
    releaseSourcePixels();
}

void ImageViewer::releaseSourcePixels() {
    delete m_source_pixels;
    m_source_pixels = nullptr;
}

ImageViewer::ImageViewer(QWidget* parent)
    : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this);
    m_item = new QGraphicsPixmapItem();
    m_item->setTransformationMode(Qt::SmoothTransformation);
    m_scene->addItem(m_item);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing, false);
    setRenderHint(QPainter::SmoothPixmapTransform, false);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setBackgroundBrush(QColor(38, 38, 38));
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ImageViewer::updateScrollBarPolicy() {
    const Qt::ScrollBarPolicy policy = m_fit_mode ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded;
    if (horizontalScrollBarPolicy() != policy) {
        setHorizontalScrollBarPolicy(policy);
    }
    if (verticalScrollBarPolicy() != policy) {
        setVerticalScrollBarPolicy(policy);
    }
}

bool ImageViewer::openImage(const QString& file_path) {
    releaseSourcePixels();

    const QString suffix = QFileInfo(file_path).suffix().toLower();
    const bool is_csv = (suffix == QStringLiteral("csv"));
    const bool prefer_opencv =
        (suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff"));

    QImage image;
    bool used_opencv = false;

    if (is_csv) {
        if (!loadCsvImage(file_path, &image)) {
            emit openFailed(tr("无法解析 CSV 文件：\n%1").arg(file_path));
            return false;
        }
    } else {
        if (!prefer_opencv) {
            image = QImage(file_path);
        }
    }

    if (image.isNull()) {
        QFile file(file_path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray bytes = file.readAll();
            const cv::Mat buffer(1, static_cast<int>(bytes.size()), CV_8UC1,
                                 reinterpret_cast<uchar*>(bytes.data()));
            const cv::Mat decoded = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
            if (!decoded.empty()) {
                image = matToQImage(decoded);
                if (!image.isNull()) {
                    if (decoded.depth() != CV_8U || decoded.channels() > 4) {
                        m_source_pixels = new SourcePixels();
                        m_source_pixels->mat = decoded;
                    }
                    used_opencv = true;
                }
            }
        }
    }

    if (image.isNull()) {
        emit openFailed(tr("无法打开图片：\n%1").arg(file_path));
        return false;
    }

    DBG_LOG("IMAGE", QStringLiteral("opened %1 size=%2x%3 opencv=%4")
                         .arg(QFileInfo(file_path).fileName())
                         .arg(image.width())
                         .arg(image.height())
                         .arg(used_opencv ? 1 : 0));

    m_image = image;
    m_item->setPixmap(QPixmap::fromImage(image));
    m_scene->setSceneRect(m_item->boundingRect());
    m_zoom = 1.0;
    m_fit_mode = true;
    centerOn(m_item);
    zoomToFit();

    emit pixelInfo(QString());
    emit imageLoaded(QFileInfo(file_path).fileName(), image.size());
    return true;
}

bool ImageViewer::loadCsvImage(const QString& file_path, QImage* out_image) {
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    int columns = 0;
    int rows = 0;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int count = static_cast<int>(std::ranges::count(line, ',')) + 1;
        columns = std::max(columns, count);
        ++rows;
    }
    if (rows <= 0 || columns <= 0) {
        return false;
    }

    cv::Mat values(rows, columns, CV_32FC1, cv::Scalar::all(0.0));
    file.seek(0);
    int row = 0;
    while (!file.atEnd() && row < rows) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QList<QByteArray> tokens = line.split(',');
        const int column_count = std::min(columns, static_cast<int>(tokens.size()));
        for (const int col : std::views::iota(0, column_count)) {
            bool ok = false;
            const float value = tokens.at(col).trimmed().toFloat(&ok);
            values.at<float>(row, col) = ok ? value : 0.0f;
        }
        ++row;
    }

    double min_value = 0.0;
    double max_value = 0.0;
    cv::minMaxLoc(values, &min_value, &max_value);
    const double scale = max_value > min_value ? 255.0 / (max_value - min_value) : 1.0;
    const double shift = -min_value * scale;

    cv::Mat normalized;
    values.convertTo(normalized, CV_8U, scale, shift);
    QImage image(normalized.cols, normalized.rows, QImage::Format_Grayscale8);
    copyMatRowsToQImage(normalized, image);

    m_source_pixels = new SourcePixels();
    m_source_pixels->mat = values;
    *out_image = image;
    DBG_LOG("CSV", QStringLiteral("loaded %1 size=%2x%3 range=%4..%5")
                       .arg(QFileInfo(file_path).fileName())
                       .arg(columns)
                       .arg(rows)
                       .arg(min_value)
                       .arg(max_value));
    return true;
}

void ImageViewer::zoomIn() {
    scaleBy(k_zoom_step, QGraphicsView::AnchorViewCenter);
}

void ImageViewer::zoomOut() {
    scaleBy(1.0 / k_zoom_step, QGraphicsView::AnchorViewCenter);
}

void ImageViewer::scaleBy(double factor, QGraphicsView::ViewportAnchor anchor) {
    if (!hasImage()) {
        return;
    }

    const double new_zoom = m_zoom * factor;
    if (new_zoom < k_min_zoom || new_zoom > k_max_zoom) {
        return;
    }

    const QPointF before = sceneCenter();
    setTransformationAnchor(anchor);
    scale(factor, factor);
    m_zoom = new_zoom;
    m_fit_mode = false;
    updateScrollBarPolicy();
    updateSmoothMode();
    emit zoomChanged(m_zoom);
    emit userZoomed(m_zoom, sceneCenter() - before);
}

void ImageViewer::zoomToFit() {
    if (!hasImage()) {
        return;
    }

    m_fit_mode = true;
    updateScrollBarPolicy();
    fitInView(m_item, Qt::KeepAspectRatio);
    m_zoom = transform().m11();
    updateSmoothMode();
    emit zoomChanged(m_zoom);
}

void ImageViewer::zoom100() {
    if (!hasImage()) {
        return;
    }

    resetTransform();
    centerOn(m_item);
    m_zoom = 1.0;
    m_fit_mode = false;
    updateScrollBarPolicy();
    updateSmoothMode();
    emit zoomChanged(m_zoom);
}

void ImageViewer::updateSmoothMode() {
    if (!m_item) {
        return;
    }

    const bool show_pixels = (m_zoom >= k_pixel_zoom_threshold);
    if (show_pixels != m_pixel_mode) {
        m_pixel_mode = show_pixels;
        emit pixelModeChanged(show_pixels);
    }
    m_item->setTransformationMode(show_pixels ? Qt::FastTransformation : Qt::SmoothTransformation);
    m_item->update();
}

void ImageViewer::wheelEvent(QWheelEvent* event) {
    if (!hasImage()) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const double factor = (event->angleDelta().y() > 0) ? k_zoom_step : 1.0 / k_zoom_step;
    scaleBy(factor, QGraphicsView::AnchorUnderMouse);
    event->accept();
}

void ImageViewer::mousePressEvent(QMouseEvent* event) {
    QGraphicsView::mousePressEvent(event);
    if (event->button() == Qt::LeftButton && dragMode() == QGraphicsView::ScrollHandDrag) {
        m_panning = true;
        m_last_pan_center = sceneCenter();
    }
}

void ImageViewer::mouseMoveEvent(QMouseEvent* event) {
    QGraphicsView::mouseMoveEvent(event);
    updatePixelInfo(mapToScene(event->pos()));

    if (m_panning) {
        if (!(event->buttons() & Qt::LeftButton)) {
            m_panning = false;
            return;
        }
        const QPointF now = sceneCenter();
        const QPointF delta = now - m_last_pan_center;
        if (qAbs(delta.x()) > 0.001 || qAbs(delta.y()) > 0.001) {
            m_last_pan_center = now;
            emit userPanned(delta);
        }
    }
}

void ImageViewer::mouseReleaseEvent(QMouseEvent* event) {
    QGraphicsView::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        m_panning = false;
    }
}

void ImageViewer::updatePixelInfo(const QPointF& scenePos) {
    if (!hasImage()) {
        emit pixelInfo(QString());
        return;
    }

    const auto x = static_cast<int>(std::floor(scenePos.x()));
    const auto y = static_cast<int>(std::floor(scenePos.y()));
    if (x < 0 || y < 0 || x >= m_image.width() || y >= m_image.height()) {
        emit pixelInfo(QString());
        return;
    }

    const QStringList values = pixelValueText(x, y);
    if (values.isEmpty()) {
        emit pixelInfo(QString());
        return;
    }

    QString text = tr("像素: %1, %2 | ").arg(x).arg(y);
    if (values.size() == 1) {
        text += tr("灰度: %1").arg(values.first());
    } else if (values.size() == 3) {
        text += tr("RGB: %1, %2, %3").arg(values.at(0), values.at(1), values.at(2));
    } else if (values.size() == 4) {
        text +=
            tr("RGBA: %1, %2, %3, %4").arg(values.at(0), values.at(1), values.at(2), values.at(3));
    } else {
        text += tr("通道: %1").arg(values.join(QStringLiteral(", ")));
    }
    emit pixelInfo(text);
}

QStringList ImageViewer::pixelValueText(int x, int y) const {
    if (!hasImage()) {
        return {};
    }

    if (m_source_pixels && !m_source_pixels->mat.empty()) {
        return valuesFromMat(m_source_pixels->mat, x, y);
    }

    const QColor color = m_image.pixelColor(x, y);
    if (m_image.format() == QImage::Format_Grayscale8 ||
        m_image.format() == QImage::Format_Grayscale16) {
        return {QString::number(color.red())};
    }
    return {QString::number(color.red()), QString::number(color.green()),
            QString::number(color.blue()), QString::number(color.alpha())};
}

void ImageViewer::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragEnterEvent(event);
    }
}

void ImageViewer::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void ImageViewer::dropEvent(QDropEvent* event) {
    if (const QMimeData* mime = event->mimeData(); mime && mime->hasUrls()) {
        QStringList files;
        const QList<QUrl> urls = mime->urls();
        for (const QUrl& url : urls | std::views::filter([](const QUrl& value) {
                 return value.isLocalFile();
             })) {
            files << url.toLocalFile();
        }
        if (!files.isEmpty()) {
            emit filesDropped(files);
            event->acceptProposedAction();
            return;
        }
    }
    QGraphicsView::dropEvent(event);
}

void ImageViewer::resizeEvent(QResizeEvent* event) {
    DBG_LOG("RESIZE", QStringLiteral("enter old=%1x%2 new=%3x%4 fit=%5 zoom=%6")
                          .arg(event->oldSize().width())
                          .arg(event->oldSize().height())
                          .arg(event->size().width())
                          .arg(event->size().height())
                          .arg(m_fit_mode)
                          .arg(m_zoom));
    QGraphicsView::resizeEvent(event);
    if (m_fit_mode && hasImage()) {
        fitInView(m_item, Qt::KeepAspectRatio);
        m_zoom = transform().m11();
        updateSmoothMode();
        emit zoomChanged(m_zoom);
    }
    DBG_LOG("RESIZE", QStringLiteral("exit vp=%1x%2 zoom=%3")
                          .arg(viewport()->width())
                          .arg(viewport()->height())
                          .arg(m_zoom));
}

void ImageViewer::setShowRgbValues(bool enabled) {
    if (m_show_rgb_values == enabled) {
        return;
    }
    m_show_rgb_values = enabled;
    viewport()->update();
}

QPointF ImageViewer::sceneCenter() const {
    return mapToScene(viewport()->rect().center());
}

void ImageViewer::applyAbsoluteZoom(double zoom, const QPointF& centerDelta) {
    if (!hasImage() || zoom <= 0.0) {
        return;
    }
    if (const double target = qBound(k_min_zoom, zoom, k_max_zoom);
        !qFuzzyCompare(target, m_zoom)) {
        const double factor = target / m_zoom;
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        scale(factor, factor);
        m_zoom = target;
        m_fit_mode = false;
        updateScrollBarPolicy();
        updateSmoothMode();
        emit zoomChanged(m_zoom);
    }
    if (qAbs(centerDelta.x()) > 0.001 || qAbs(centerDelta.y()) > 0.001) {
        applyPanBy(centerDelta);
    }
}

void ImageViewer::applyPanBy(const QPointF& delta) {
    if (!hasImage()) {
        return;
    }

    centerOn(sceneCenter() + delta);
}

void ImageViewer::focusOnPixel(int x, int y) {
    if (!hasImage() || x < 0 || y < 0 || x >= m_image.width() || y >= m_image.height()) {
        return;
    }

    if (!qFuzzyCompare(m_zoom, k_max_zoom)) {
        const double factor = k_max_zoom / m_zoom;
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        scale(factor, factor);
        m_zoom = k_max_zoom;
        m_fit_mode = false;
        updateScrollBarPolicy();
        updateSmoothMode();
        emit zoomChanged(m_zoom);
    }

    centerOn(QPointF(x + 0.5, y + 0.5));
    updatePixelInfo(QPointF(x + 0.5, y + 0.5));
}

void ImageViewer::drawForeground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawForeground(painter, rect);
    if (!m_show_rgb_values || !hasImage() || m_zoom < k_rgb_label_threshold) {
        return;
    }

    const QRectF visible = mapToScene(viewport()->rect()).boundingRect();
    const int x0 = qMax(0, static_cast<int>(std::floor(visible.left())));
    const int y0 = qMax(0, static_cast<int>(std::floor(visible.top())));
    const int x1 = qMin(m_image.width() - 1, static_cast<int>(std::ceil(visible.right())) - 1);
    const int y1 = qMin(m_image.height() - 1, static_cast<int>(std::ceil(visible.bottom())) - 1);

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool log_this = (now - m_last_fg_log_ms >= 1000);
    if (log_this) {
        m_last_fg_log_ms = now;
        DBG_LOG("FG", QStringLiteral("enter zoom=%1 visible=%2,%3 %4x%5 range=%6..%7 %8..%9")
                          .arg(m_zoom)
                          .arg(visible.left())
                          .arg(visible.top())
                          .arg(visible.width())
                          .arg(visible.height())
                          .arg(x0)
                          .arg(x1)
                          .arg(y0)
                          .arg(y1));
    }

    painter->save();
    painter->resetTransform();

    QFont font = painter->font();
    font.setPixelSize(qBound(6, static_cast<int>(m_zoom / 6.0), 10));
    font.setBold(true);
    painter->setFont(font);

    for (const int y : std::views::iota(y0, y1 + 1)) {
        for (const int x : std::views::iota(x0, x1 + 1)) {
            const QRectF cell = mapFromScene(QRectF(x, y, 1, 1)).boundingRect();
            const QColor color = m_image.pixelColor(x, y);
            const int luminance =
                (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
            painter->setPen(luminance > 128 ? QColor(0, 0, 0) : QColor(255, 255, 255));
            painter->drawText(cell, Qt::AlignCenter, pixelValueText(x, y).join(QLatin1Char('\n')));
        }
    }

    painter->restore();

    if (log_this) {
        DBG_LOG("FG", QStringLiteral("exit range=%1..%2 %3..%4").arg(x0).arg(x1).arg(y0).arg(y1));
    }
}
