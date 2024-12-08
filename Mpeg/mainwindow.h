#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QProgressBar>
#include <QTextEdit>
#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRunButtonClicked();
    void onPlayButtonClicked();

private:
    Ui::MainWindow *ui;
    QPushButton *runButton;
    QPushButton *playButton;
    QProgressBar *progressBar;
    QTextEdit *outputTextEdit;
    QWidget *videoWidget;

    // Multimedia components
    QMediaPlayer *mediaPlayer;
    QVideoWidget *videoDisplay;
};

#endif // MAINWINDOW_H
