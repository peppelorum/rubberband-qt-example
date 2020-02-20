/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* Copyright 2020 Particular Programs Ltd. All Rights Reserved. */

#include "Interface.h"
#include "Processor.h"

#include <QFileDialog>
#include <QGridLayout>
#include <QPushButton>
#include <QMessageBox>

#include <bqaudiostream/AudioReadStreamFactory.h>

using namespace std;

Interface::Interface(Processor *p, QWidget *parent) :
    QMainWindow(parent),
    m_processor(p)
{
    auto mainFrame = new QFrame;
    auto mainLayout = new QGridLayout;
    mainFrame->setLayout(mainLayout);

    auto open = new QPushButton(tr("Open"));
    mainLayout->addWidget(open, 0, 0);

    auto rewind = new QPushButton(tr("<<"));
    mainLayout->addWidget(rewind, 0, 1);

    auto play = new QPushButton(tr(">"));
    mainLayout->addWidget(play, 0, 2);

    auto pause = new QPushButton(tr("||"));
    mainLayout->addWidget(pause, 0, 3);

    auto speed = new QDoubleSpinBox;
    speed->setMinimum(0.1);
    speed->setMaximum(100.0);
    speed->setValue(1.0);
    speed->setDecimals(2);
    speed->setSingleStep(0.1);
    mainLayout->addWidget(speed, 0, 4);

    connect(open, SIGNAL(clicked()), this, SLOT(open()));
    connect(rewind, SIGNAL(clicked()), this, SLOT(rewind()));
    connect(play, SIGNAL(clicked()), this, SLOT(play()));
    connect(pause, SIGNAL(clicked()), this, SLOT(pause()));
    connect(speed, SIGNAL(valueChanged(double)), this, SLOT(speedChanged(double)));

    m_filenameLabel = new QLabel;
    m_filenameLabel->setText(tr("<no file loaded>"));
    mainLayout->addWidget(m_filenameLabel, 1, 0, 1, 5);

    setCentralWidget(mainFrame);
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
        m_filenameLabel->setText(m_processor->getTrackName());
    } catch (const exception &f) {
        QMessageBox::critical
            (this, tr("Failed to open file"),
             tr("Could not open audio file \"%1\": %2")
             .arg(filename).arg(QString::fromUtf8(f.what())));
        m_filenameLabel->setText(tr("<no file loaded>"));
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

void
Interface::speedChanged(double ratio)
{
    m_processor->setTimeRatio(ratio);
}

