#include "asynctaskqueue.h"



AsyncTaskQueue::AsyncTaskQueue(QObject *parent)
    : QObject(parent)
    , _poolSize(10) {
}

AsyncTaskQueue::~AsyncTaskQueue() {

}

void AsyncTaskQueue::setPoolSize(quint32 size) {
    QMutexLocker locker(&_taskLock);
    _poolSize = size;

    while ( _pendindTasks.size() > 0 && _runningTasks.size() < _poolSize ){
        quint64 nextId = _pendindTasks.dequeue();
        _runningTasks.append(nextId);
        _allTasks[nextId]->startAsyncTask();
    }
}

void AsyncTaskQueue::addTask(AsyncTask *t) {
    if (!t){
        return;
    }
    qDebug()<< QDateTime::currentMSecsSinceEpoch() << " add new async task:"<<t->id();
    QMutexLocker locker(&_taskLock);
    if (_runningTasks.contains(t->id())){
        return;
    }
    if (_pendindTasks.contains(t->id())){
        return;
    }
    _allTasks[t->id()] = t;
    if (_runningTasks.size() < _poolSize){
        _runningTasks.append(t->id());
        t->startAsyncTask();
    } else {
        _pendindTasks.enqueue(t->id());
    }
}

void AsyncTaskQueue::finishTask(quint64 id) {
    QMutexLocker locker(&_taskLock);
    if (_runningTasks.contains(id)){
        _runningTasks.removeAll(id);
    }
    if (_pendindTasks.contains(id)){
        _pendindTasks.removeAll(id);
    }

    while ( _pendindTasks.size() > 0 && _runningTasks.size() < _poolSize ){
        quint64 nextId = _pendindTasks.dequeue();
        _runningTasks.append(nextId);
        _allTasks[nextId]->startAsyncTask();
    }
}

void AsyncTaskQueue::cancelTask(quint64 id) {
    QMutexLocker locker(&_taskLock);
    if (_runningTasks.contains(id)){
        _runningTasks.removeAll(id);
    }
    if (_pendindTasks.contains(id)){
        _pendindTasks.removeAll(id);
    }

    while ( _pendindTasks.size() > 0 && _runningTasks.size() < _poolSize ){
        quint64 nextId = _pendindTasks.dequeue();
        _runningTasks.append(nextId);
        _allTasks[nextId]->startAsyncTask();
    }
}

