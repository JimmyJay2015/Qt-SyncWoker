#ifndef WORKER_H
#define WORKER_H

#include <QObject>

#include <QtCore>


// 抽象任务
class Task {
public:
    Task() {
        static quint64 sequence = 0;
        _id = ++sequence;
    }
    ~Task(){  qDebug()<<"~Task() "<<_id;  }

    inline quint64 id() { return _id; }

    virtual void process() = 0;
    virtual void finish() = 0;

private:
    // 只作为 标记，不做为 唯一 ID
    quint64 _id;
};

// worker
class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);

public: // api
    quint64 addTask(Task *t);
    void cancelTask(quint64 id);

signals:
    void startWork();
    void finishWork(Task *t, bool hasLeft);

public slots:
    void onTaskAdded();

private:
    QMutex _taskMutex;
    QQueue<Task *> _taskList;
};



class Thread : public QThread{

    Q_OBJECT
public:
    Thread(QObject *parent = nullptr) : QThread(parent){}
    ~Thread(){
        requestInterruption();
        quit();
        wait();
        qDebug()<<"~Thread()";
    }
};


class TaskQueue : public QObject{
    Q_OBJECT
public:
    explicit TaskQueue(QObject *parent = nullptr);
    ~TaskQueue();

    quint64 addTask(Task *t);
    void cancelTask(quint64 id);

public slots:
    void onTaskFinished(Task *t, bool hasLeft);
    void onThreadInIdle();

private:
    QMutex _threadIdleMutex;
    QTimer _threadIdleTimer;
    Thread *_thread;
    Worker *_worker;

};



#endif // WORKER_H
