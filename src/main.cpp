#include "MainWindow.h"

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>

#include <ranges>
#include <string>

namespace {
    const QString k_server_name = QStringLiteral("ImgViewPro.single-instance");

    QStringList imageFilesFromArgs() {
        QStringList files;
        const QStringList args = QApplication::arguments();
        for (const QString& arg : args | std::views::drop(1)) {
            if (arg.isEmpty() || arg.startsWith(QLatin1Char('-'))) {
                continue;
            }
            const QFileInfo info(arg);
            if (info.isFile()) {
                files << info.absoluteFilePath();
            }
        }
        return files;
    }
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ImgViewPro"));
    QApplication::setOrganizationName(QStringLiteral("ImgViewPro"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    const QStringList files = imageFilesFromArgs();

    const QString server_name =
        k_server_name + QLatin1Char('.') +
        QString::fromStdString(std::string(qgetenv("USERNAME").constData()));

    QLocalSocket socket;
    socket.connectToServer(server_name);
    if (socket.waitForConnected(300)) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out << files;
        socket.write(block);
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0;
    }

    QLocalServer server;
    QLocalServer::removeServer(server_name);
    server.listen(server_name);
    MainWindow window;
    window.show();
    if (!files.isEmpty()) {
        window.openImages(files);
    }

    QObject::connect(&server, &QLocalServer::newConnection, [&server, &window]() {
        while (server.hasPendingConnections()) {
            auto* const client = server.nextPendingConnection();
            QObject::connect(client, &QLocalSocket::readyRead, client, [client, &window]() {
                QDataStream in(client);
                QStringList incoming;
                in >> incoming;
                window.openImages(incoming);
                client->deleteLater();
            });
        }
    });

    return QApplication::exec();
}
