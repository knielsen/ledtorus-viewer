/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

#include <string.h>

#include <QApplication>
#include <QDesktopWidget>

#include "window.h"
#include "io.h"
#include "ledtorus.h"

#include <stdlib.h>
#include <stdio.h>

int ledtorus_ver = 1;
unsigned LEDS_X = 7;
unsigned LEDS_Y = 8;
unsigned LEDS_TANG = 205;


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    for (int i = 1; i < argc; ++i) {
      if (0==strcmp(argv[i], "-1")) {
        ledtorus_ver = 1;
        LEDS_X = 7;
        LEDS_Y = 8;
        LEDS_TANG = 205;
      } else if (0==strcmp(argv[i], "-2")) {
        ledtorus_ver = 2;
        LEDS_X = 14;
        LEDS_Y = 16;
        LEDS_TANG = 205;
      } else if (0==strcmp(argv[i], "-h") || 0==strcmp(argv[i], "--help")) {
        fprintf(stderr, "Usage: %s [-1] [-2] [-h|--help]\n", argv[0]);
        exit(1);
      } else {
        fprintf(stderr, "Invalid argument '%s' (%s -h for usage)\n", argv[i], argv[0]);
        exit(1);
      }
    }

    start_io_threads();

    Window window;
    window.resize(window.sizeHint());
    int desktopArea = QApplication::desktop()->width() *
                     QApplication::desktop()->height();
    int widgetArea = window.width() * window.height();
    if (((float)widgetArea / (float)desktopArea) < 0.75f)
        window.show();
    else
        window.showMaximized();

    return app.exec();
}
