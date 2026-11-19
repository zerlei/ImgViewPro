#include "FileAssociations.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QSettings>

namespace {
    const QString k_prog_id = QStringLiteral("ImgViewPro.Image.1");
    const QString k_friendly_name = QStringLiteral("ImgViewPro Image");
} // namespace

QString FileAssociations::progId() {
    return k_prog_id;
}

QString FileAssociations::classesPath() {
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes");
}

QStringList FileAssociations::imageExtensions() {
    return {QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"),
            QStringLiteral("bmp"),  QStringLiteral("gif"),  QStringLiteral("webp"),
            QStringLiteral("tif"),  QStringLiteral("tiff"), QStringLiteral("ico"),
            QStringLiteral("pnm"),  QStringLiteral("pbm"),  QStringLiteral("pgm"),
            QStringLiteral("ppm"),  QStringLiteral("tga"),  QStringLiteral("icns"),
            QStringLiteral("wbmp"), QStringLiteral("svg")};
}

void FileAssociations::refreshShell() {
#ifdef Q_OS_WIN
    using SHChangeNotifyFn = void(__stdcall*)(int, unsigned int, const void*, const void*);
    const auto fn = reinterpret_cast<SHChangeNotifyFn>(
        QLibrary::resolve(QStringLiteral("shell32.dll"), "SHChangeNotify"));
    if (fn) {
        const int shcne_assocchanged = 0x08000000;
        const unsigned int shcnf_idlist = 0x0000;
        fn(shcne_assocchanged, shcnf_idlist, nullptr, nullptr);
    }
#endif
}

bool FileAssociations::setAsDefault(QString* error) {
#ifdef Q_OS_WIN
    const QString exe_path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    if (exe_path.isEmpty()) {
        if (error) {
            *error = QStringLiteral("无法获取程序路径");
        }
        return false;
    }

    QSettings settings(classesPath(), QSettings::NativeFormat);

    const QString command = QStringLiteral("\"%1\" \"%2\"").arg(exe_path, QStringLiteral("%1"));
    settings.setValue(k_prog_id + QStringLiteral("/."), k_friendly_name);
    settings.setValue(k_prog_id + QStringLiteral("/DefaultIcon/."),
                      QStringLiteral("%1,0").arg(exe_path));
    settings.setValue(k_prog_id + QStringLiteral("/shell/open/command/."), command);

    for (const QString& ext : imageExtensions()) {
        const QString ext_key = QStringLiteral(".%1").arg(ext);
        settings.setValue(ext_key + QStringLiteral("/."), k_prog_id);
        settings.setValue(ext_key + QStringLiteral("/OpenWithProgids/") + k_prog_id, QString());
    }

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("写入注册表失败，请检查权限");
        }
        return false;
    }

    refreshShell();
    return true;
#else
    if (error) {
        *error = QStringLiteral("仅支持 Windows");
    }
    return false;
#endif
}

bool FileAssociations::removeAssociation(QString* error) {
#ifdef Q_OS_WIN
    QSettings settings(classesPath(), QSettings::NativeFormat);

    for (const QString& ext : imageExtensions()) {
        const QString ext_key = QStringLiteral(".%1").arg(ext);
        if (settings.value(ext_key + QStringLiteral("/.")).toString() == k_prog_id) {
            settings.remove(ext_key + QStringLiteral("/."));
        }
        settings.remove(ext_key + QStringLiteral("/OpenWithProgids/") + k_prog_id);
    }
    settings.remove(k_prog_id);

    settings.sync();
    refreshShell();
    return true;
#else
    if (error) {
        *error = QStringLiteral("仅支持 Windows");
    }
    return false;
#endif
}

bool FileAssociations::isDefault() {
#ifdef Q_OS_WIN
    QSettings settings(classesPath(), QSettings::NativeFormat);
    return settings.value(QStringLiteral(".png/.")).toString() == k_prog_id;
#else
    return false;
#endif
}
