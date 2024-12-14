#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QProcess>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);

    this->setWindowTitle("Video Encoder");

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

    QProcess runProcess;
    QString runCommand = "/home/can201/output.out"; // Path to the .out file

    // Set the correct GLIBC path
    QStringList env = QProcess::systemEnvironment();
    env.append("LD_LIBRARY_PATH=/home/can201/glibc-2.34/output/lib"); // Update the path to glibc-2.34
    runProcess.setEnvironment(env);

    // Start the process
    runProcess.start(runCommand);

    // Initialize progress bar
    progressBar->setValue(0);
    int progress = 0;

    // Update progress bar while the process is running
    while (runProcess.state() == QProcess::Running) {
        progress += 3; // Increment progress
        if (progress > 90) progress = 90; // Cap progress at 90% until process finishes
        progressBar->setValue(progress);

        QCoreApplication::processEvents(); // Allow UI to refresh during loop
        QThread::msleep(200); // Simulate delay for visible progress
    }

    // Wait for the process to finish completely
    runProcess.waitForFinished();

    // Capture output
    QString output = runProcess.readAllStandardOutput();
    QString errorOutput = runProcess.readAllStandardError();

    // Display process output in the text edit widget
    if (!output.isEmpty()) {
        outputTextEdit->append("Program Output:\n" + output);
    }
    if (!errorOutput.isEmpty()) {
        outputTextEdit->append("Program Error:\n" + errorOutput);
    }

    // Complete the progress bar
    progressBar->setValue(100);
    outputTextEdit->append("Execution completed.");
}



void MainWindow::onPlayButtonClicked() {
    qDebug() << "Play button clicked";

    QString videoPath = "/home/can201/output.mpeg";  // Replace with your actual video file path
    if (!QFile::exists(videoPath)) {
        outputTextEdit->append("Video file does not exist: " + videoPath);
        return; // Exit if the file does not exist
    }

    mediaPlayer->setMedia(QUrl::fromLocalFile(videoPath));
    mediaPlayer->play();

    outputTextEdit->append("Playing: " + videoPath);
}
