#include "synctaskqueue.h"

#include <windows.h>


Worker::Worker(QObject *parent) : QObject(parent) {
    connect(this, SIGNAL(startWork()), this, SLOT(onTaskAdded()));
}

void Worker::onTaskAdded() {
    QMutexLocker locker(&_taskMutex);
    if (_taskList.empty()){
        return;
    }
    Task *t = _taskList.dequeue();
    if (!t){
        return;
    }
    locker.unlock();
    if (!t){
        return;
    }
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" Worker process task" << t->id() <<" in thread id:" << QThread::currentThreadId();
    t->process();

    locker.relock();
    bool hasLeft = !_taskList.isEmpty();
    locker.unlock();
    emit finishWork(t, hasLeft);
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" Worker process task" << t->id() <<", has left:"<< hasLeft <<" end in thread id:" << QThread::currentThreadId();
}

quint64 Worker::addTask(Task *t) {
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" Worker add Task "<< t->id() <<" in thread id:" << QThread::currentThreadId();
    QMutexLocker locker(&_taskMutex);
    _taskList.enqueue(t);
    locker.unlock();

    emit startWork();
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" Worker add Task "<< t->id() <<" end in thread id:" << QThread::currentThreadId();
    return t->id();
}

void Worker::cancelTask(quint64 id) {
     QMutexLocker locker(&_taskMutex);
     for (qint32 i = 0, l = _taskList.size(); i < l; ++i) {
         if (_taskList.at(i)->id() == id) {
             _taskList.removeAt(i);
             break;
         }
     }
}


TaskQueue::TaskQueue(QObject *parent) : QObject(parent)
, _thread(nullptr)
, _worker(nullptr)
{
    _threadIdleTimer.setInterval(3000);
    _threadIdleTimer.setSingleShot(true);

    connect(&_threadIdleTimer, SIGNAL(timeout()), this, SLOT(onThreadInIdle()));
}

TaskQueue::~TaskQueue() {
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" ~TaskQueue()";
}

quint64 TaskQueue::addTask(Task *t) {
    if (!t){
        return 0;
    }
    QMutexLocker locaker(&_threadIdleMutex);
    if (!_thread) {
        _thread = new Thread(this);
        _worker = new Worker();
        _worker->moveToThread(_thread);
        qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" main thread id:" << QThread::currentThreadId();
        qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" _worker thread id:" << _worker->thread();
        qDebug()<< QDateTime::currentMSecsSinceEpoch()<<"  ============= ";

        connect(_worker, SIGNAL(finishWork(Task *, bool)), this, SLOT(onTaskFinished(Task *, bool)));

        // 启动线程的 event loop
        _thread->start();
    }

    // 给 worker 加一个任务
    _worker->addTask(t);
    _threadIdleTimer.stop();

    return t->id();
}

void TaskQueue::cancelTask(quint64 id) {
    _worker->cancelTask(id);
}

void TaskQueue::onTaskFinished(Task *t, bool hasLeft) {
    if (!t) {
        return;
    }
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" TaskQueue finish task" << t->id() <<" in thread id:" << QThread::currentThreadId();
    t->finish();
    if (!hasLeft){
        _threadIdleTimer.start();
    }
}

void TaskQueue::onThreadInIdle() {
    qDebug()<<"TaskQueue::onThreadInIdle()";
    QMutexLocker locaker(&_threadIdleMutex);
    if (_thread) {
        _thread->requestInterruption();
        _thread->quit();
        _thread->wait();
        _thread->deleteLater();
        _worker->deleteLater();
        _thread = 0;
        _worker = 0;
    }
}
