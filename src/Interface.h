/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/* Copyright 2020 Particular Programs Ltd. All Rights Reserved. */

#ifndef EXAMPLE_INTERFACE_H
#define EXAMPLE_INTERFACE_H

#include <QString>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QMainWindow>

class Processor;

class Interface : public QMainWindow
{
    Q_OBJECT

public:
    Interface(Processor *p, QWidget *parent = 0);
    virtual ~Interface();

    void open(QString);

protected slots:
    void open();
    void play();
    void pause();
    void rewind();
//    void speedUp();
//    void slowDown();
    
private:
    Processor *m_processor;
    QDoubleSpinBox *m_speedSpin;
};


#endif
