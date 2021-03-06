/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QPainter>
#include <QTcpSocket>
#include <arpa/inet.h>
#include <QBitmap>

#include <QDebug>

#include "qvnccursor.h"
#include "qvncserver.h"
#include "qvncscreen.h"

QT_BEGIN_NAMESPACE

QVNCCursor::QVNCCursor(QVNCServer * srvr, QVNCScreen *scr)
        :QFbCursor(scr), useVncCursor(false), server(srvr)
{
}

#ifndef QT_NO_CURSOR
void QVNCCursor::changeCursor(QCursor * widgetCursor, QWindow * window)
{
    QFbCursor::changeCursor(widgetCursor, window);
    if (useVncCursor) {
        server->setDirtyCursor();
    } else {
        setDirty();
    }
}
#endif

void QVNCCursor::setCursorMode(bool vnc)
{
    if (vnc) {
        setDirty();
        server->setDirtyCursor();
    } else {
        server->setDirtyCursor();
    }
    useVncCursor = vnc;
}

QRect QVNCCursor::drawCursor(QPainter & painter)
{
    if (useVncCursor)
        return QRect();

    return QFbCursor::drawCursor(painter);
}

void QVNCCursor::clearClientCursor()
{
    QVNCSocket *socket = server->clientSocket();
    if (!socket) {
        return;
    }
    // FramebufferUpdate header
    {
        const quint16 tmp[6] = { htons(0),
                                 htons(1),
                                 htons(0), htons(0),
                                 htons(0),
                                 htons(0) };
        socket->write((char*)tmp, sizeof(tmp));

        const quint32 encoding = htonl(-239);
        socket->write((char*)(&encoding), sizeof(encoding));
    }
}

void QVNCCursor::sendClientCursor()
{
    if (useVncCursor == false) {
        //clearClientCursor();
        return;
    }
    QImage *image = mGraphic->image();
    if (image->isNull())
        return;
    QVNCSocket *socket = server->clientSocket();
    if (!socket) {
        return;
    }
    // FramebufferUpdate header
    {
        const quint16 tmp[6] = { htons(0),
                                 htons(1),
                                 htons(mGraphic->hotspot().x()), htons(mGraphic->hotspot().y()),
                                 htons(image->width()),
                                 htons(image->height()) };
        socket->write((char*)tmp, sizeof(tmp));

        const quint32 encoding = htonl(-239);
        socket->write((char*)(&encoding), sizeof(encoding));
    }

    // write pixels
    //Q_ASSERT(cursor->hasAlphaChannel());
    const QImage img = image->convertToFormat(QImage::Format_RGB32);
    const int n = server->clientBytesPerPixel() * img.width();
    char *buffer = new char[n];
    for (int i = 0; i < img.height(); ++i) {
        server->convertPixels(buffer, (const char*)img.scanLine(i), img.width());
        socket->write(buffer, n);
    }
    delete[] buffer;

    // write mask
    const QImage bitmap = image->createAlphaMask().convertToFormat(QImage::Format_Mono);
    Q_ASSERT(bitmap.depth() == 1);
    Q_ASSERT(bitmap.size() == img.size());
    const int width = (bitmap.width() + 7) / 8;
    for (int i = 0; i < bitmap.height(); ++i)
        socket->write((const char*)bitmap.scanLine(i), width);
}

QT_END_NAMESPACE
