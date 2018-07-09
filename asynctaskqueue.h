#ifndef ASYNCTASKQUEUE_H
#define ASYNCTASKQUEUE_H

#include <QObject>

#include <QtCore>

class AsyncTask : public QObject {
    Q_OBJECT

public:
    explicit AsyncTask(QObject *parent) : QObject(parent) {
        static quint64 sequence = 0;
        _id = ++sequence;
    }

    virtual void startAsyncTask() = 0;

    inline quint64 id() { return _id; }

signals:
    void finished(quint64 id);

private:
    quint64 _id;
};

class AsyncTaskQueue : public QObject {
    Q_OBJECT
public:
    explicit AsyncTaskQueue(QObject *parent = nullptr);
    ~AsyncTaskQueue();

    void setPoolSize(quint32 size);

    void addTask(AsyncTask *t);
    void finishTask(quint64 id);
    // 正在运行、stop 掉，没运行，移除出 map
    void cancelTask(quint64 id);

private:
    QMutex _taskLock;
    QHash<quint64, AsyncTask *> _allTasks;
    QList<quint64> _runningTasks;
    QQueue<quint64> _pendindTasks;

    int _poolSize;
};





#endif // ASYNCTASKQUEUE_H
