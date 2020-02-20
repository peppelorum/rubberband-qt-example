/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* Copyright 2020 Particular Programs Ltd. All Rights Reserved. */

#include "Interface.h"
#include "Processor.h"

#include <QFileDialog>
#include <QMessageBox>

#include <bqaudiostream/AudioReadStreamFactory.h>

using namespace std;

Interface::Interface(Processor *p, QWidget *parent) :
    QMainWindow(parent),
    m_processor(p)
{
}

Interface::~Interface()
{
}

void
Interface::open()
{
    auto extensions =
        breakfastquay::AudioReadStreamFactory().getSupportedFileExtensions();
    QStringList exts;
    for (auto e: extensions) exts.push_back(QString::fromUtf8(e.c_str()));
    QString extStr = exts.join(" *.");

    QString filename = QFileDialog::getOpenFileName
        (this, tr("Select audio file to open"), {},
         QString("Audio files (*.%1)\nAll files (*.*)").arg(extStr));

    if (filename == "") {
        return;
    }

    open(filename);
}

void
Interface::open(QString filename)
{
    try {
        m_processor->open(filename);
    } catch (const exception &f) {
        QMessageBox::critical
            (this, tr("Failed to open file"),
             tr("Could not open audio file \"%1\": %2")
             .arg(filename).arg(QString::fromUtf8(f.what())));
    }
}

void
Interface::play()
{
    m_processor->play();
}

void
Interface::pause()
{
    m_processor->pause();
}

void
Interface::rewind()
{
    m_processor->rewind();
}
/*
void
Interface::speedUp()
{
    int v = m_speed->value();
    v = (v/2) * 2 + 2; // target is always even
    m_speed->setValue(v);
    speedChange(m_speed->value());
}

void
Interface::slowDown()
{
    int v = m_speed->value();
    v = (v/2) * 2 - 2; // target is always even
    m_speed->setValue(v);
    speedChange(m_speed->value());
}
*/

