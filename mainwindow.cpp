#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDateTime>

#include <windows.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    task = new TaskQueue();
    asynctask = new AsyncTaskQueue();
    asynctask->setPoolSize(3);
}

void MainWindow::on_pushButton_clicked(){

    task->onThreadInIdle();
    return;
}

void MainWindow::on_pushButton_2_clicked() {
    Task *t = new TestTask();
    task->addTask(t);
}

void MainWindow::on_pushButton_4_clicked()
{
    task->cancelTask(3);
}



void MainWindow::on_pushButton_3_clicked()
{
    AsyncTask *t = new AsyncTestTask();
    connect(t, &AsyncTask::finished, this, &MainWindow::onAsyncTaskFinished);
    asynctask->addTask(t);
}

void MainWindow::onAsyncTaskFinished(quint64 id) {
    qDebug()<< QDateTime::currentMSecsSinceEpoch() << " AsyncTask : "<< id <<" finished";
    asynctask->finishTask(id);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void TestTask::process() {
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" Task "<< id() <<" process() in thread:" << QThread::currentThreadId();
    Sleep(3000);
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" Task "<< id() <<" process() finied in thread:" << QThread::currentThreadId();
}
void TestTask::finish() {
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" Task "<< id() <<" finish() in thread:" << QThread::currentThreadId();
    delete this;
}
//////////////////////////////////////////////////////////////////
AsyncTestTask::AsyncTestTask(QObject *parent)
    : AsyncTask(parent)
    , _process(0)
{
    _timer.setSingleShot(false);
    _timer.setInterval(1000);
    connect(&_timer, SIGNAL(timeout()), this, SLOT(timeout()));
}

void AsyncTestTask::startAsyncTask() {
    qDebug()<< QDateTime::currentMSecsSinceEpoch() << " async task "<< id() <<" start";
    _timer.start();
}

void AsyncTestTask::timeout() {
    qDebug()<< QDateTime::currentMSecsSinceEpoch() << " AsyncTestTask : "<< id() <<", _process:" << _process++;
    if (_process > 3){
        _timer.stop();
        emit finished(id());
    }
}




















