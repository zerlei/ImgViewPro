#include "MainWindow.h"
#include "DebugLog.h"
#include "FileAssociations.h"
#include "ImageViewer.h"

#include <QAction>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QLabel>
#include <QLibraryInfo>
#include <QListWidget>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {
    QString formatPixelLabel(const QString& text) {
        if (text.trimmed().isEmpty())
            return MainWindow::tr("点击此处输入像素坐标…");
        return text;
    }

    QString imageFileFilter() {
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        QStringList patterns;
        for (const QByteArray& format : formats)
            patterns << QStringLiteral("*.%1").arg(QString::fromLatin1(format));
        patterns << QStringLiteral("*.csv");
        return MainWindow::tr("图片/CSV 文件 (%1);;所有文件 (*)")
            .arg(patterns.join(QLatin1Char(' ')));
    }

    QStringList localFilesFromDrop(QDropEvent* event) {
        QStringList files;
        const QMimeData* mime = event->mimeData();
        if (mime && mime->hasUrls()) {
            const QList<QUrl> urls = mime->urls();
            for (const QUrl& url : urls) {
                if (url.isLocalFile())
                    files << url.toLocalFile();
            }
        }
        return files;
    }

    void logMdiState(DropMdiArea* area, const char* tag) {
        if (!dbg::loggingEnabled())
            return;

        const QList<QMdiSubWindow*> subs = area->subWindowList();
        QStringList parts;
        parts << QStringLiteral("n=%1").arg(subs.size());
        for (int i = 0; i < subs.size(); ++i) {
            QMdiSubWindow* sub = subs.at(i);
            ImageViewer* viewer = qobject_cast<ImageViewer*>(sub->widget());
            const QRect g = sub->geometry();
            parts << QStringLiteral("#%1 '%2' vis=%3 g=%4,%5,%6x%7 vp=%8x%9 zoom=%10 fit=%11")
                         .arg(i)
                         .arg(sub->windowTitle())
                         .arg(sub->isVisible() ? 1 : 0)
                         .arg(g.x())
                         .arg(g.y())
                         .arg(g.width())
                         .arg(g.height())
                         .arg(viewer ? viewer->viewport()->width() : -1)
                         .arg(viewer ? viewer->viewport()->height() : -1)
                         .arg(viewer ? viewer->currentZoom() : -1.0)
                         .arg(viewer && viewer->isFitMode() ? 1 : 0);
        }
        DBG_LOG(tag, parts.join(QStringLiteral(" | ")));
    }
} // namespace

DropMdiArea::DropMdiArea(QWidget* parent)
    : QMdiArea(parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

void DropMdiArea::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QMdiArea::dragEnterEvent(event);
}

void DropMdiArea::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QMdiArea::dragMoveEvent(event);
}

void DropMdiArea::dropEvent(QDropEvent* event) {
    const QStringList files = localFilesFromDrop(event);
    if (!files.isEmpty()) {
        emit filesDropped(files);
        event->acceptProposedAction();
        return;
    }
    QMdiArea::dropEvent(event);
}

DropListWidget::DropListWidget(QWidget* parent)
    : QListWidget(parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
}

void DropListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QListWidget::dragEnterEvent(event);
}

void DropListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QListWidget::dragMoveEvent(event);
}

void DropListWidget::dropEvent(QDropEvent* event) {
    const QStringList files = localFilesFromDrop(event);
    if (!files.isEmpty()) {
        emit filesDropped(files);
        event->acceptProposedAction();
        return;
    }
    QListWidget::dropEvent(event);
}

ClickableLabel::ClickableLabel(QWidget* parent)
    : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QObject::tr("点击输入像素坐标，定位并放大到该像素"));
}

void ClickableLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QLabel::mousePressEvent(event);
}

MainWindow::MainWindow() {
    setWindowTitle(tr("ImgViewPro - 图片查看器"));
    resize(1100, 750);

    m_mdi_area = new DropMdiArea(this);
    setCentralWidget(m_mdi_area);

    createActions();
    createDockWidget();
    createMenus();
    createToolBar();
    createStatusBar();

    connect(m_mdi_area, &DropMdiArea::filesDropped, this, &MainWindow::openImages);
    connect(m_mdi_area, &QMdiArea::subWindowActivated, this, &MainWindow::onSubWindowActivated);

    DBG_LOG("START", QStringLiteral("ver=%1 dir=%2 qtlib=%3 pid=%4")
                         .arg(QString::fromLatin1(qVersion()))
                         .arg(QCoreApplication::applicationDirPath())
                         .arg(QLibraryInfo::path(QLibraryInfo::LibrariesPath))
                         .arg(QCoreApplication::applicationPid()));

    if (dbg::loggingEnabled()) {
        QTimer* heartbeat = new QTimer(this);
        connect(heartbeat, &QTimer::timeout, this, [this]() {
            DBG_LOG("BEAT",
                    QStringLiteral("alive subs=%1").arg(m_mdi_area->subWindowList().size()));
        });
        heartbeat->start(1000);
    }

    updateStatusBar();
}

MainWindow::~MainWindow() {
    DBG_LOG("EXIT", "MainWindow destructor enter");
    if (m_mdi_area) {
        const QList<QMdiSubWindow*> subs = m_mdi_area->subWindowList();
        for (QMdiSubWindow* sub : subs)
            delete sub;
    }
}

void MainWindow::createActions() {
    m_open_act = new QAction(tr("打开图片..."), this);
    m_open_act->setShortcut(QKeySequence::Open);
    connect(m_open_act, &QAction::triggered, this, &MainWindow::onOpen);

    m_exit_act = new QAction(tr("退出"), this);
    m_exit_act->setShortcut(QKeySequence::Quit);
    connect(m_exit_act, &QAction::triggered, this, &QWidget::close);

    m_zoom_in_act = new QAction(tr("放大"), this);
    m_zoom_in_act->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoom_in_act, &QAction::triggered, this, [this]() {
        if (ImageViewer* viewer = activeViewer())
            viewer->zoomIn();
    });

    m_zoom_out_act = new QAction(tr("缩小"), this);
    m_zoom_out_act->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoom_out_act, &QAction::triggered, this, [this]() {
        if (ImageViewer* viewer = activeViewer())
            viewer->zoomOut();
    });

    m_fit_act = new QAction(tr("适应窗口"), this);
    m_fit_act->setShortcut(tr("Ctrl+0"));
    connect(m_fit_act, &QAction::triggered, this, [this]() {
        if (ImageViewer* viewer = activeViewer())
            viewer->zoomToFit();
    });

    m_zoom100_act = new QAction(tr("1:1 原始大小"), this);
    m_zoom100_act->setShortcut(tr("Ctrl+1"));
    connect(m_zoom100_act, &QAction::triggered, this, [this]() {
        if (ImageViewer* viewer = activeViewer())
            viewer->zoom100();
    });

    m_show_rgb_act = new QAction(tr("显示 RGB 数值"), this);
    m_show_rgb_act->setCheckable(true);
    m_show_rgb_act->setChecked(true);
    m_show_rgb_act->setShortcut(tr("Ctrl+R"));
    connect(m_show_rgb_act, &QAction::toggled, this, [this](bool enabled) {
        for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it)
            it.key()->setShowRgbValues(enabled);
    });

    m_sync_act = new QAction(tr("视图联动 (像素精确)"), this);
    m_sync_act->setCheckable(true);
    m_sync_act->setChecked(false);
    m_sync_act->setShortcut(tr("Ctrl+L"));
    connect(m_sync_act, &QAction::toggled, this, [this](bool enabled) {
        m_sync_enabled = enabled;
        if (m_sync_label)
            m_sync_label->setText(enabled ? tr("联动: 开") : tr("联动: 关"));
    });

    m_tile_act = new QAction(tr("平铺窗口"), this);
    connect(m_tile_act, &QAction::triggered, this, &MainWindow::onTile);

    m_cascade_act = new QAction(tr("层叠窗口"), this);
    connect(m_cascade_act, &QAction::triggered, this, &MainWindow::onCascade);

    m_close_act = new QAction(tr("关闭当前图片"), this);
    m_close_act->setShortcut(tr("Ctrl+W"));
    connect(m_close_act, &QAction::triggered, this, &MainWindow::onCloseCurrent);

    m_close_all_act = new QAction(tr("关闭全部图片"), this);
    connect(m_close_all_act, &QAction::triggered, this, &MainWindow::onCloseAll);

    m_set_default_act = new QAction(tr("设为默认图片查看器"), this);
    connect(m_set_default_act, &QAction::triggered, this, &MainWindow::onSetDefault);

    m_remove_default_act = new QAction(tr("取消图片文件关联"), this);
    connect(m_remove_default_act, &QAction::triggered, this, &MainWindow::onRemoveDefault);

    m_about_act = new QAction(tr("关于"), this);
    connect(m_about_act, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createDockWidget() {
    m_dock = new QDockWidget(tr("图片列表"), this);
    m_dock->setObjectName(QStringLiteral("imageListDock"));
    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_image_list = new DropListWidget(m_dock);
    connect(m_image_list, &DropListWidget::filesDropped, this, &MainWindow::openImages);
    m_image_list->setViewMode(QListWidget::IconMode);
    m_image_list->setIconSize(QSize(120, 90));
    m_image_list->setSpacing(8);
    m_image_list->setResizeMode(QListWidget::Adjust);
    m_image_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dock->setWidget(m_image_list);
    addDockWidget(Qt::LeftDockWidgetArea, m_dock);

    auto activate_item = [this](QListWidgetItem* item) {
        if (QMdiSubWindow* sub = m_item_to_sub.value(item))
            m_mdi_area->setActiveSubWindow(sub);
    };
    connect(m_image_list, &QListWidget::itemClicked, this, activate_item);
    connect(m_image_list, &QListWidget::itemDoubleClicked, this, activate_item);
}

void MainWindow::createMenus() {
    QMenu* file_menu = menuBar()->addMenu(tr("文件"));
    file_menu->addAction(m_open_act);
    file_menu->addSeparator();
    file_menu->addAction(m_close_act);
    file_menu->addAction(m_close_all_act);
    file_menu->addSeparator();
    file_menu->addAction(m_exit_act);

    QMenu* view_menu = menuBar()->addMenu(tr("视图"));
    view_menu->addAction(m_zoom_in_act);
    view_menu->addAction(m_zoom_out_act);
    view_menu->addSeparator();
    view_menu->addAction(m_fit_act);
    view_menu->addAction(m_zoom100_act);
    view_menu->addSeparator();
    view_menu->addAction(m_show_rgb_act);
    view_menu->addSeparator();
    view_menu->addAction(m_sync_act);
    view_menu->addSeparator();
    view_menu->addAction(m_dock->toggleViewAction());

    QMenu* window_menu = menuBar()->addMenu(tr("窗口"));
    window_menu->addAction(m_tile_act);
    window_menu->addAction(m_cascade_act);
    window_menu->addSeparator();
    window_menu->addAction(m_close_act);
    window_menu->addAction(m_close_all_act);

    QMenu* tools_menu = menuBar()->addMenu(tr("工具"));
    tools_menu->addAction(m_set_default_act);
    tools_menu->addAction(m_remove_default_act);

    QMenu* help_menu = menuBar()->addMenu(tr("帮助"));
    help_menu->addAction(m_about_act);
}

void MainWindow::createToolBar() {
    QToolBar* tool_bar = addToolBar(tr("工具栏"));
    tool_bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    tool_bar->addAction(m_open_act);
    tool_bar->addSeparator();
    tool_bar->addAction(m_zoom_in_act);
    tool_bar->addAction(m_zoom_out_act);
    tool_bar->addAction(m_fit_act);
    tool_bar->addAction(m_zoom100_act);
    tool_bar->addSeparator();
    tool_bar->addAction(m_show_rgb_act);
    tool_bar->addAction(m_sync_act);
    tool_bar->addAction(m_tile_act);
    tool_bar->addAction(m_cascade_act);
}

void MainWindow::createStatusBar() {
    m_count_label = new QLabel(this);
    m_image_label = new QLabel(this);
    m_zoom_label = new QLabel(this);
    m_mode_label = new QLabel(this);
    m_sync_label = new QLabel(this);
    m_pixel_label = new ClickableLabel(this);
    connect(m_pixel_label, &ClickableLabel::clicked, this, &MainWindow::onPixelLabelClicked);

    m_sync_label->setText(tr("联动: 关"));
    statusBar()->addWidget(m_count_label);
    statusBar()->addWidget(m_image_label, 1);
    statusBar()->addWidget(m_zoom_label);
    statusBar()->addWidget(m_mode_label);
    statusBar()->addWidget(m_sync_label);
    statusBar()->addWidget(m_pixel_label, 1);
}

void MainWindow::openImages(const QStringList& files) {
    for (const QString& file : files)
        addImageWindow(file);
    if (!files.isEmpty())
        updateStatusBar();
}

void MainWindow::addImageWindow(const QString& file_path) {
    QString canonical = QFileInfo(file_path).canonicalFilePath();
    if (canonical.isEmpty())
        canonical = QFileInfo(file_path).absoluteFilePath();

    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        if (it->file_path.compare(canonical, Qt::CaseInsensitive) == 0 && it->sub_window) {
            m_mdi_area->setActiveSubWindow(it->sub_window);
            return;
        }
    }

    ImageViewer* viewer = new ImageViewer(this);
    viewer->setShowRgbValues(m_show_rgb_act->isChecked());
    if (!viewer->openImage(file_path)) {
        delete viewer;
        return;
    }

    QMdiSubWindow* sub = m_mdi_area->addSubWindow(viewer);
    sub->setAttribute(Qt::WA_DeleteOnClose);
    const QString name = QFileInfo(file_path).fileName();
    sub->setWindowTitle(name);
    sub->resize(800, 600);

    ViewerState state;
    state.sub_window = sub;
    state.file_path = canonical;
    state.file_name = name;
    state.image_size = viewer->imageSize();
    state.zoom = viewer->currentZoom();
    state.pixel_mode = viewer->isPixelMode();
    m_states.insert(viewer, state);

    const QPixmap thumb =
        QPixmap::fromImage(viewer->image())
            .scaled(QSize(120, 90), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QListWidgetItem* item = new QListWidgetItem(thumb, name);
    item->setToolTip(file_path);
    m_image_list->addItem(item);
    m_item_to_sub.insert(item, sub);

    connect(viewer, &ImageViewer::zoomChanged, this, [this, viewer](double zoom) {
        auto it = m_states.find(viewer);
        if (it == m_states.end())
            return;
        it->zoom = zoom;
        if (activeViewer() == viewer)
            m_zoom_label->setText(tr("缩放: %1%").arg(zoom * 100.0, 0, 'f', 0));
    });
    connect(viewer, &ImageViewer::pixelModeChanged, this, [this, viewer](bool enabled) {
        auto it = m_states.find(viewer);
        if (it == m_states.end())
            return;
        it->pixel_mode = enabled;
        if (activeViewer() == viewer)
            updateStatusBar();
    });
    connect(viewer, &ImageViewer::pixelInfo, this, [this, viewer](const QString& text) {
        auto it = m_states.find(viewer);
        if (it == m_states.end())
            return;
        it->pixel_info = text;
        if (activeViewer() == viewer)
            m_pixel_label->setText(formatPixelLabel(text));
    });
    connect(viewer, &ImageViewer::filesDropped, this, &MainWindow::openImages);
    connect(viewer, &ImageViewer::userZoomed, this, &MainWindow::onUserZoomed);
    connect(viewer, &ImageViewer::userPanned, this, &MainWindow::onUserPanned);
    connect(viewer, &QObject::destroyed, this, [this, viewer]() { m_states.remove(viewer); });
    connect(sub, &QObject::destroyed, this, [this, item]() {
        m_item_to_sub.remove(item);
        const int row = m_image_list->row(item);
        if (row >= 0) {
            m_image_list->takeItem(row);
            delete item;
        }
    });

    sub->show();
    m_mdi_area->setActiveSubWindow(sub);

    if (m_sync_enabled && m_states.size() > 1) {
        ImageViewer* reference = nullptr;
        for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
            if (it.key() != viewer) {
                reference = it.key();
                break;
            }
        }
        if (reference)
            viewer->applyAbsoluteZoom(reference->currentZoom(), QPointF());
    }

    updateDockSelection();
}

ImageViewer* MainWindow::activeViewer() const {
    QMdiSubWindow* sub = m_mdi_area->activeSubWindow();
    return sub ? qobject_cast<ImageViewer*>(sub->widget()) : nullptr;
}

void MainWindow::updateStatusBar() {
    const QList<QMdiSubWindow*> subs = m_mdi_area->subWindowList();
    ImageViewer* viewer = activeViewer();
    if (!viewer || !m_states.contains(viewer)) {
        m_count_label->clear();
        m_image_label->clear();
        m_zoom_label->clear();
        m_mode_label->clear();
        m_pixel_label->clear();
        setWindowTitle(tr("ImgViewPro - 图片查看器"));
        updateDockSelection();
        return;
    }

    const ViewerState& state = m_states.value(viewer);
    m_count_label->setText(
        tr("图片 %1/%2").arg(subs.indexOf(state.sub_window) + 1).arg(subs.size()));
    m_image_label->setText(tr("%1  (%2 x %3)")
                               .arg(state.file_name)
                               .arg(state.image_size.width())
                               .arg(state.image_size.height()));
    m_zoom_label->setText(tr("缩放: %1%").arg(state.zoom * 100.0, 0, 'f', 0));
    m_mode_label->setText(state.pixel_mode ? tr("像素模式: 开 (真实像素块)")
                                           : tr("像素模式: 关 (平滑)"));
    m_pixel_label->setText(formatPixelLabel(state.pixel_info));
    setWindowTitle(tr("%1 - ImgViewPro").arg(state.file_name));
    updateDockSelection();
}

void MainWindow::updateDockSelection() {
    QMdiSubWindow* active = m_mdi_area->activeSubWindow();
    for (auto it = m_item_to_sub.constBegin(); it != m_item_to_sub.constEnd(); ++it)
        it.key()->setSelected(it.value() == active);
}

void MainWindow::onOpen() {
    const QStringList files =
        QFileDialog::getOpenFileNames(this, tr("打开图片"), QString(), imageFileFilter());
    openImages(files);
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this, tr("关于 ImgViewPro"),
        tr("ImgViewPro 是一个基于 Qt 6 的多图片查看器。\n\n"
           "功能：同时打开多张图片，左侧 Dock 显示缩略图；支持缩放、拖动平移，"
           "高倍放大时显示真实像素块并标注每个像素的 R/G/B 数值。"
           "勾选“视图 → 视图联动 (像素精确)”或按 Ctrl+L，可让所有图片以相同像素密度缩放、"
           "按相同图片像素数平移。\n\n"
           "操作：可一次选择多个文件，或把多个图片文件直接拖进窗口；"
           "滚轮缩放，鼠标左键拖动平移；“窗口 → 平铺窗口”可并排查看多张图片。"));
}

void MainWindow::onSubWindowActivated(QMdiSubWindow* sub) {
    DBG_LOG("ACTIVATE", sub ? sub->windowTitle() : QStringLiteral("<none>"));
    updateStatusBar();
}

void MainWindow::onTile() {
    DBG_LOG("TILE", "enter");
    logMdiState(m_mdi_area, "TILE-BEFORE");
    m_mdi_area->tileSubWindows();
    logMdiState(m_mdi_area, "TILE-AFTER");
    DBG_LOG("TILE", "exit");
}

void MainWindow::onCascade() {
    DBG_LOG("CASCADE", "enter");
    logMdiState(m_mdi_area, "CASCADE-BEFORE");
    m_mdi_area->cascadeSubWindows();
    logMdiState(m_mdi_area, "CASCADE-AFTER");
    DBG_LOG("CASCADE", "exit");
}

void MainWindow::onCloseCurrent() {
    if (QMdiSubWindow* sub = m_mdi_area->activeSubWindow())
        sub->close();
}

void MainWindow::onCloseAll() {
    m_mdi_area->closeAllSubWindows();
}

void MainWindow::onSetDefault() {
    QString error;
    if (FileAssociations::setAsDefault(&error)) {
        QMessageBox::information(
            this, tr("文件关联"),
            tr("已将 ImgViewPro 设为默认图片查看器。\n现在双击图片即可用本程序打开。"));
    } else {
        QMessageBox::warning(this, tr("文件关联"), tr("设置失败：%1").arg(error));
    }
}

void MainWindow::onRemoveDefault() {
    QString error;
    if (FileAssociations::removeAssociation(&error)) {
        QMessageBox::information(this, tr("文件关联"), tr("已取消 ImgViewPro 的图片文件关联。"));
    } else {
        QMessageBox::warning(this, tr("文件关联"), tr("取消失败：%1").arg(error));
    }
}

void MainWindow::onPixelLabelClicked() {
    ImageViewer* viewer = activeViewer();
    if (!viewer || !viewer->hasImage())
        return;

    const QSize size = viewer->imageSize();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("定位像素"));

    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* x_spin = new QSpinBox(&dialog);
    auto* y_spin = new QSpinBox(&dialog);
    x_spin->setRange(0, size.width() - 1);
    y_spin->setRange(0, size.height() - 1);
    x_spin->setToolTip(tr("X 范围: 0-%1").arg(size.width() - 1));
    y_spin->setToolTip(tr("Y 范围: 0-%1").arg(size.height() - 1));
    form->addRow(tr("X 坐标:"), x_spin);
    form->addRow(tr("Y 坐标:"), y_spin);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;

    viewer->focusOnPixel(x_spin->value(), y_spin->value());
}

void MainWindow::onUserZoomed(double zoom, const QPointF& centerDelta) {
    if (!m_sync_enabled)
        return;

    ImageViewer* source = qobject_cast<ImageViewer*>(sender());
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        ImageViewer* viewer = it.key();
        if (viewer != source)
            viewer->applyAbsoluteZoom(zoom, centerDelta);
    }
}

void MainWindow::onUserPanned(const QPointF& delta) {
    if (!m_sync_enabled)
        return;

    ImageViewer* source = qobject_cast<ImageViewer*>(sender());
    for (auto it = m_states.constBegin(); it != m_states.constEnd(); ++it) {
        ImageViewer* viewer = it.key();
        if (viewer != source)
            viewer->applyPanBy(delta);
    }
}
