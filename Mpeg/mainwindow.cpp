#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    // Initialize UI elements
    runButton = ui->runButton;
    playButton = ui->playButton;
    progressBar = ui->progressBar;
    outputTextEdit = ui->outputTextEdit;

    // Initialize video player and widget
    mediaPlayer = new QMediaPlayer(this);
    videoDisplay = new QVideoWidget(this);
    videoDisplay->setGeometry(30, 200, 640, 360); // Adjust dimensions
    mediaPlayer->setVideoOutput(videoDisplay);
    videoDisplay->show();

    // Connect buttons to slots
    connect(runButton, &QPushButton::clicked, this, &MainWindow::onRunButtonClicked);
    connect(playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onRunButtonClicked() {
    qDebug() << "Run button clicked";
    // Simulate progress bar update
    for (int i = 0; i <= 100; ++i) {
        progressBar->setValue(i);
        QCoreApplication::processEvents(); // Allow UI updates
    }
    outputTextEdit->append("Process completed.");
}

void MainWindow::onPlayButtonClicked() {
    qDebug() << "Play button clicked";

    QString videoPath = "/home/can201/testVid.mpeg";  // Replace with your actual video file path
    if (!QFile::exists(videoPath)) {
        outputTextEdit->append("Video file does not exist: " + videoPath);
        return; // Exit if the file does not exist
    }

    mediaPlayer->setMedia(QUrl::fromLocalFile(videoPath));
    mediaPlayer->play();

    outputTextEdit->append("Playing: " + videoPath);
}
