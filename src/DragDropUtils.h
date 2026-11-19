#pragma once

#include <QDropEvent>
#include <QList>
#include <QMimeData>
#include <QStringList>
#include <QUrl>

#include <ranges>

namespace dragdrop {
    inline QStringList localFilesFromDrop(const QDropEvent* event) {
        QStringList files;
        if (const QMimeData* mime = event->mimeData(); mime && mime->hasUrls()) {
            const QList<QUrl> urls = mime->urls();
            for (const QUrl& url : urls | std::views::filter([](const QUrl& value) {
                     return value.isLocalFile();
                 })) {
                files << url.toLocalFile();
            }
        }
        return files;
    }
} // namespace dragdrop
