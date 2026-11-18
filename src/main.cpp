#include "MainWindow.h"

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>

#include <string>

namespace {
    const QString k_server_name = QStringLiteral("ImgViewPro.single-instance");

    QStringList imageFilesFromArgs(const QApplication& app) {
        QStringList files;
        const QStringList args = app.arguments();
        for (int i = 1; i < args.size(); ++i) {
            const QString& arg = args.at(i);
            if (arg.isEmpty() || arg.startsWith(QLatin1Char('-')))
                continue;
            const QFileInfo info(arg);
            if (info.isFile())
                files << info.absoluteFilePath();
        }
        return files;
    }
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ImgViewPro"));
    app.setOrganizationName(QStringLiteral("ImgViewPro"));

    const QStringList files = imageFilesFromArgs(app);

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
    if (!files.isEmpty())
        window.openImages(files);

    QObject::connect(&server, &QLocalServer::newConnection, [&server, &window]() {
        while (server.hasPendingConnections()) {
            QLocalSocket* client = server.nextPendingConnection();
            QObject::connect(client, &QLocalSocket::readyRead, client, [client, &window]() {
                QDataStream in(client);
                QStringList incoming;
                in >> incoming;
                window.openImages(incoming);
                client->deleteLater();
            });
        }
    });

    return app.exec();
}
