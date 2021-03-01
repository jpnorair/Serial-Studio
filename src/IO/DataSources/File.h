/*
 * Copyright (C) MATUSICA S.A. de C.V. - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Proprietary and confidential.
 *
 * Written by Alex Spataru <https://alex-spataru.com/>, February 2021
 */

#ifndef IO_DATA_SOURCES_FILE_H
#define IO_DATA_SOURCES_FILE_H

#include <QHostInfo>
#include <QByteArray>

namespace IO
{
namespace DataSources
{
class File : public QObject
{
    // clang-format off
    Q_OBJECT
    Q_PROPERTY(QString path
               READ path
               WRITE setPath
               NOTIFY pathChanged)
    Q_PROPERTY(QString defaultPath
               READ defaultPath
               CONSTANT)
    // clang-format on

signals:
    void pathChanged();

public:
    static File *getInstance();

    QString path() const;
    static QString defaultPath() { return "log.txt"; }

    //QIODevice *openNetworkPort();

public slots:
    void setPath(const QString &path);

private slots:
    //void onErrorOccurred(const QAbstractSocket::SocketError socketError);

private:
    File();
    ~File();

private:
    QString m_path;

};
}
}

#endif
