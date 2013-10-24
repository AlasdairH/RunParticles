#ifndef RUNPARTICLES_MAINWINDOW_H
#define RUNPARTICLES_MAINWINDOW_H

#include <QMainWindow>
#include "GLWidget.h"
#include <QAction>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    MainWindow(GLWidget *glWidget, QWidget * parent = 0, Qt::WindowFlags flags = 0);
    virtual ~MainWindow();
    
    void loadTcxFile(QFile *tcxFile);
    
protected:
    void _layout(QWidget*);
    
public slots:
    void slotLoadFile();
    
    void slotPlaybackRateChanged(const QString &newRate);
    
    void slotTimeChanged(double mapSeconds);
    
protected:
    QMenuBar *_menuBar;
    GLWidget *_glWidget;
    QPushButton *_rewindButton, *_forwardButton, *_pauseButton, *_backButton;
    QSlider *_slider;
    QLineEdit *_currentTimeLineEdit;
    QComboBox *_playSpeedCombo;
    QAction *_openLayerAction, *_forwardAction, *_backAction, *_rewindAction,
            *_pauseAction;
};

#endif