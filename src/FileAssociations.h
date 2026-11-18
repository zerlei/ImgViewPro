#pragma once

#include <QString>
#include <QStringList>

class FileAssociations {
  public:
    static QStringList imageExtensions();
    static bool setAsDefault(QString* error = nullptr);
    static bool removeAssociation(QString* error = nullptr);
    static bool isDefault();

  private:
    static QString progId();
    static QString classesPath();
    static void refreshShell();
};
