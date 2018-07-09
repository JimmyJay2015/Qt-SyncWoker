#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QtCore>

#include "synctaskqueue.h"
#include "asynctaskqueue.h"

namespace Ui {
class MainWindow;
}

class TaskQueue;
class AsyncTaskQueue;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

    void onAsyncTaskFinished(quint64 id);

private:
    Ui::MainWindow *ui;
    TaskQueue *task;
    AsyncTaskQueue *asynctask;
};


//////////////////////////////////////////////////////////////////////////////////
// 测试任务
class TestTask : public Task {
public:
    TestTask():Task() {}

    virtual void process() Q_DECL_OVERRIDE;
    virtual void finish() Q_DECL_OVERRIDE;

};

class AsyncTestTask : public AsyncTask {
    Q_OBJECT
public:
    explicit AsyncTestTask(QObject *parent = nullptr);

    virtual void startAsyncTask() Q_DECL_OVERRIDE;

public slots:
    void timeout();

private:
    int _process;
    QTimer _timer;
};






////////////////////////////////////


#endif // MAINWINDOW_H
