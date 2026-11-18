#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QTextStream>
#include <QtGlobal>

namespace dbg {

    inline bool loggingEnabled() {
        static const bool enabled = !qEnvironmentVariableIsEmpty("IMGVIEWPRO_LOG");
        return enabled;
    }

    inline QString logFilePath() {
        const QString app_path =
            QCoreApplication::applicationDirPath() + QStringLiteral("/imgviewpro-debug.log");
        QFile probe(app_path);
        if (probe.open(QIODevice::WriteOnly | QIODevice::Append))
            return app_path;
        return QDir::tempPath() + QStringLiteral("/imgviewpro-debug.log");
    }

    inline void log(const char* tag, const QString& message) {
        if (!loggingEnabled())
            return;

        static QMutex mutex;
        QMutexLocker locker(&mutex);

        QFile file(logFilePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            return;

        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")) << " ["
            << QString::fromLatin1(tag) << "] " << message << '\n';
        out.flush();
        file.flush();
    }

} // namespace dbg

#define DBG_LOG(tag, message)                                                                      \
    do {                                                                                           \
        if (dbg::loggingEnabled())                                                                 \
            dbg::log((tag), (message));                                                            \
    } while (false)
