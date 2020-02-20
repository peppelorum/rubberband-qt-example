/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* Copyright 2020 Particular Programs Ltd. All Rights Reserved. */

#include "Processor.h"
#include "Interface.h"

#include <QApplication>
#include <QMessageBox>
#include <QTranslator>

#include <bqaudioio/AudioFactory.h>
#include <bqaudioio/SystemPlaybackTarget.h>

#include <rubberband/RubberBandStretcher.h>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName("Breakfast Quay");
    QApplication::setOrganizationDomain("breakfastquay.com");
    QApplication::setApplicationName("Rubber Band Qt Example");

    Processor *processor = new Processor();
    Interface *iface = new Interface(processor);
    iface->show();

    std::string error;
    breakfastquay::SystemPlaybackTarget *target =
	breakfastquay::AudioFactory::createCallbackPlayTarget
        (processor, {}, error);
    if (!target || !target->isTargetOK()) {
        QMessageBox::critical
            (iface,
             QObject::tr("Failed to open audio device"),
             error == "" ?
             QObject::tr("Failed to open audio device for playback.") :
             QObject::tr("Failed to open audio device for playback: %1")
             .arg(error.c_str()));
    }

    QStringList args = app.arguments();
    if (args.size() > 1) iface->open(args[1]);

    int rv = app.exec();

    delete target;
    delete iface;
    delete processor;

    return rv;
}


