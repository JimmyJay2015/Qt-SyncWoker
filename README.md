1、moveToThread
QObject worker；
worker.moveToThread(_thread);

这个 worker 不能指定parent，否则 moveToThread 会失败。

同样，如果 worker moveToThread 后，不能设置非同一线程下的 parent。
即，对象树下的所有对象、都必须在同一线程里，否则无论是修改对象树、还是修改线程，都会失败。

2、QThread 对象释放掉，但线程未必停止了。正确的停止方式：
// 首先请求中断任务，QThread 在 run时，可能会存有多个待执行的任务，用 while 循环来一个个执行的
// 因此，QThread 对象释放时，先请求中断 requestInterruption，run 中的 while 则判断是否有中断 

···
while(!isInterruptionRequested()) {
    requestInterruption();
    // 然后 发出 quit event
    quit();
    // 等待退出 QThread 的 event loop
    wait();
}
···

3、	telegram 的 taskqueue， 对 task 的执行、包括结束，都是在同一个 worker 工作函数里执行的。
主线程生成 task，加入到 taskqueue，taskqueue 通过信号 taskadded，通知 worker 开始执行任务， worker 从 task 队列取出 task，调用 task->process()，完成后，发出 worker 的 taskFinished 信号，不在同一线程、所以 生成 event 插入到 主线程的 event loop 里，然后主动调用 QCoreApplication::processEvents()，这时候 worker 的 taskFinished 信号会被处理掉，即，一个任务 process 后，立刻 finished。
telegram worker的本意是，主线程添加 task 至 taskqueue ，将 task 加入到 task 列表、然后发出 taskadded 信号就结束了，work 的工作函数会被 taskadded 信号唤醒，工作函数 while 检查线程是否中断，否则从 task 列表里取出一个 task、然后 process、发出 finished 信号，再次检查线程是否中断。
这里的问题是，主线程可能在上个 task 还在执行的时候，如果再添加一个 task 进来、会塞 taskadded 事件到 event loop 里，那么上一个 work 工作函数在 task finish 之后、调用  QCoreApplication::processEvents() 的时候，会有两个 work 工作函数被执行，虽然 加了锁，不会循环执行相同的任务，但其实已经跑飞了。


一个 串行 的任务队列：
--------------------------------------------------------------------------------
利用 QThread run 的 event loop，来实现任务调度。
将待执行的任务放进 FIFO 队列里->检查启动线程->发出执行任务信号->线程收到信号->从队列里读取任务->执行任务->发出任务完成信号->主线程接收任务完成信号->完成任务。

// 任务抽象。任务执行时在子线程里，任务完成在调用线程里。
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

// 工作者 .h
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


// 工作者 .cpp
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

// 自定义线程析构
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

// 串行任务队列 .h
class TaskQueue : public QObject{
    Q_OBJECT
public:
    explicit TaskQueue(QObject *parent = nullptr);
    ~TaskQueue();

    quint64 addTask(Task *t);
    void cancelTask(quint64 id);

public slots:
    void onTaskFinished(Task *t);

private:
    QMutex _threadIdleMutex;
    Thread *_thread;
    Worker *_worker;
};

// 串行任务队列 .cpp
TaskQueue::TaskQueue(QObject *parent) : QObject(parent)
, _thread(nullptr)
, _worker(nullptr)
{
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

    return t->id();
}

void TaskQueue::cancelTask(quint64 id) {
    _worker->cancelTask(id);
}

void TaskQueue::onTaskFinished(Task *t) {
    if (!t) {
        return;
    }
    qDebug()<< QDateTime::currentMSecsSinceEpoch()<<" TaskQueue finish task" << t->id() <<" in thread id:" << QThread::currentThreadId();
    t->finish();
}


5、可以增加一个 10 秒没有任务，则停止 thread，再有任务加进来，再启动 thread
注意：
1、如何判断：10 秒没有任务在执行
2、多线程数据同步

1、添加任务后，停止计时
2、任务完成时，判断队列里还有没有剩余的任务，没有的话、开始计时

代码：
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





并发 10 个网络请求的并行任务队列
--------------------------------------------------------------------------------
本项目需要并发的是网络请求、包括文件的上传和下载。

分析：
QT 中的网络请求，只能是 main thread 且 支持并发的，等于自带 worker 和 multy thread。
任务队列只管调度任务，其他比如进度之类的事件，由上层直接监听任务即可。
因为是异步的，所以无法直接利用 QThread 的 event loop 来实现任务调度，只能自己实现。
并发任务，更多司职的是任务调度，甚至不太关心任务什么时候完成、成功与否。

任务调度：
1、新增任务，将任务保存至 id-》task map里，检查 正在执行任务 列表，如果大于等于 10 个，则放入 待执行队列 里；否则任务 id 记录到 正在执行列表、并启动任务。
2、任务执行完成后，从 待执行队列 里取出一个任务，放入 正在执行列表 里，然后执行任务。

注意点：
1、任务的启动由 task queue 调度完成，但因为是异步任务、所以 任务 完成之后，都需要有回调通知 task queue，然后由 task queue 执行对应的 应用层回调。
2、task queue 只是调度任务的话，那么 task queue 就不应该知道任何 任务细节，任务是否完成、任务自身知道，所以再由上层调用 finish 表示任务结束。
3、综上，task queue 只需要启动任务。

代码：
// async task
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

// async task queue
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


更具普遍意义的并发任务队列
--------------------------------------------------------------------------------
上述依赖 Qt 的网络请求而不需要自己创建维护多线程 和 worker，而且代码逻辑相对简单。
但，更具普遍意义的是多线程的并发任务队列。
任务队列仍然负责任务的启动，不需要知道任务执行的具体行为、以及是否完成，提供 finish 接口供任务完成后调用、形成调度闭环。
支持多线程，并发数量取决于子线程数量。
worker，工作者，QThread + worker 是 Qt 推荐的多线程模型，QThread 负责启停子线程、worker 负责执行任务。


TODO






GitHUb 地址
--------------------------------------------------------------------------------
https://github.com/JimmyJay2015/Qt-SyncWoker.git



















