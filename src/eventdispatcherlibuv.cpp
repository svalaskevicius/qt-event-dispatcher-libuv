#include "eventdispatcherlibuv.h"

#include <QCoreApplication>
#include <QSocketNotifier>
#ifdef Q_OS_WIN
#include <cassert>
#include <QWinEventNotifier>
#endif

#include "uv.h"
#include <QDebug>

#include "eventdispatcherlibuv_p.h"

#include <QtGui/qpa/qwindowsysteminterface.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformintegration.h>

extern uint qGlobalPostedEventsCount(); // from qapplication.cpp

namespace
{

void forgetCurrentUvHandles()
{
    uv_walk(uv_default_loop(), [](uv_handle_t* handle, void* arg){
        if (uv_has_ref(handle)) {
            uv_unref(handle);
        }
    }, 0);
}

}

namespace qtjs {


EventDispatcherLibUv::EventDispatcherLibUv(QObject *parent) :
    QAbstractEventDispatcher(parent),
    socketNotifier(new EventDispatcherLibUvSocketNotifier()),
    timerNotifier(new EventDispatcherLibUvTimerNotifier()),
    timerTracker(new EventDispatcherLibUvTimerTracker()),
    asyncChannel(new EventDispatcherLibUvAsyncChannel()),
    finalise(false),
    osEventDispatcher(nullptr)
{
}

EventDispatcherLibUv::~EventDispatcherLibUv(void)
{
    socketNotifier.reset();
    timerNotifier.reset();
    timerTracker.reset();
    asyncChannel.reset();
    uv_run(uv_default_loop(), UV_RUN_NOWAIT);
    if (osEventDispatcher) {
        delete osEventDispatcher;
    }
}

void EventDispatcherLibUv::wakeUp(void)
{
    if (osEventDispatcher) {
        osEventDispatcher->wakeUp();
    }
    asyncChannel->send();
}

void EventDispatcherLibUv::interrupt(void)
{
    if (osEventDispatcher) {
        osEventDispatcher->interrupt();
    }
    asyncChannel->send();
}

void EventDispatcherLibUv::flush(void)
{
    if (osEventDispatcher) {
        osEventDispatcher->flush();
    }
}

bool EventDispatcherLibUv::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    if (osEventDispatcher) {
        osEventDispatcher->processEvents(flags & ~QEventLoop::WaitForMoreEvents & ~QEventLoop::EventLoopExec);
    } else {
        emit awake();
        QWindowSystemInterface::sendWindowSystemEvents(flags);
    }
    QCoreApplication::sendPostedEvents();
    emit aboutToBlock();

    int leftHandles = uv_run(uv_default_loop(), UV_RUN_ONCE);
#ifdef Q_OS_WIN
    activateEventNotifiers();
#endif
    if (!leftHandles) {
        if (osEventDispatcher) {
            osEventDispatcher->processEvents(flags & ~QEventLoop::EventLoopExec | QEventLoop::WaitForMoreEvents);
        } else if (finalise) {
            qApp->exit(0);
        }
    }
    return leftHandles;
}

bool EventDispatcherLibUv::hasPendingEvents(void)
{
    if (osEventDispatcher) {
        return osEventDispatcher->hasPendingEvents() || qGlobalPostedEventsCount();
    }
    return qGlobalPostedEventsCount();
}

void EventDispatcherLibUv::registerSocketNotifier(QSocketNotifier* notifier)
{
    socketNotifier->registerSocketNotifier(notifier->socket(), notifier->type(), [notifier]{
        QEvent event(QEvent::SockAct);
        QCoreApplication::sendEvent(notifier, &event);
    });
}
void EventDispatcherLibUv::unregisterSocketNotifier(QSocketNotifier* notifier)
{
    socketNotifier->unregisterSocketNotifier(notifier->socket(), notifier->type());
}

void EventDispatcherLibUv::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject* object)
{
    timerNotifier->registerTimer(timerId, interval, [timerId, object, this] {
        timerTracker->fireTimer(timerId);
        QTimerEvent e(timerId);
        QCoreApplication::sendEvent(object, &e);
    });
    timerTracker->registerTimer(timerId, interval, timerType, object);
}

bool EventDispatcherLibUv::unregisterTimer(int timerId)
{
    bool ret = timerNotifier->unregisterTimer(timerId);
    if (ret) {
        timerTracker->unregisterTimer(timerId);
    }
    return ret;
}

bool EventDispatcherLibUv::unregisterTimers(QObject* object)
{
    bool ret = true;
    for (auto info : registeredTimers(object)) {
        ret &= unregisterTimer(info.timerId);
    }
    return ret;
}

QList<QAbstractEventDispatcher::TimerInfo> EventDispatcherLibUv::registeredTimers(QObject* object) const
{
    return timerTracker->getTimerInfo(object);
}

int EventDispatcherLibUv::remainingTime(int timerId)
{
    return timerTracker->remainingTime(timerId);
}

#ifdef Q_OS_WIN
void EventDispatcherLibUv::activateEventNotifiers() {
    std::vector<WinEventNotifierInfo*> queue;
    {
        std::lock_guard<std::mutex> lock(winEventActQueueMutex);
        winEventActQueue.swap(queue);
    }
    for (auto* weni : queue) {
        QEvent event(QEvent::WinEventAct);
        QCoreApplication::sendEvent(weni->notifier, &event);
    }
}

void EventDispatcherLibUv::queueEventNotifierActivation(WinEventNotifierInfo* weni)
{
    {
        std::lock_guard<std::mutex> lock(winEventActQueueMutex);
        winEventActQueue.push_back(weni);
    }
    asyncChannel->send();
}

void EventDispatcherLibUv::queueEventNotifierActivation(PVOID context, BOOLEAN timedOut) {
    assert(!timedOut);
    auto* weni = static_cast<WinEventNotifierInfo*>(context);
    weni->dispatcher->queueEventNotifierActivation(weni);
}

bool EventDispatcherLibUv::registerEventNotifier(QWinEventNotifier *notifier)
{
    if (!notifier) {
        qWarning("EventDispatcherLibUv: Null event notifier");
        return false;
    }
    if (notifier->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("EventDispatcherLibUv: Event notifiers cannot be enabled from another thread");
        return false;
    }

    for (const auto& weni : winEventNotifierList) {
        if (weni->notifier == notifier)
            return true;
    }

    auto weni = std::unique_ptr<WinEventNotifierInfo>(new WinEventNotifierInfo(this, notifier));
    if (!RegisterWaitForSingleObject(&weni->waitHandle, notifier->handle(), queueEventNotifierActivation, weni.get(), INFINITE, WT_EXECUTEINWAITTHREAD)) {
        qWarning("EventDispatcherLibUv: RegisterWaitForSingleObject failed");
        return false;
    }
    if (winEventNotifierList.empty())
        asyncChannel->ref();
    winEventNotifierList.push_back(std::move(weni));
    return true;
}

void EventDispatcherLibUv::unregisterEventNotifier(QWinEventNotifier *notifier)
{
    if (!notifier) {
        qWarning("EventDispatcherLibUv: Null event notifier");
        return;
    }
    if (notifier->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("EventDispatcherLibUv: Event notifiers cannot be disabled from another thread");
        return;
    }

    for (auto I = winEventNotifierList.begin(), E = winEventNotifierList.end(); I != E; ++I) {
        const auto& weni = *I;
        if (weni->notifier == notifier) {
            BOOL res = UnregisterWaitEx(weni->waitHandle, INVALID_HANDLE_VALUE);
            assert(res); (void)res;
            // Remove any pending activations for the notifier being unregistered.
            {
                std::lock_guard<std::mutex> lock(winEventActQueueMutex);
                winEventActQueue.erase(std::remove(winEventActQueue.begin(), winEventActQueue.end(), weni.get()), winEventActQueue.end());
            }
            winEventNotifierList.erase(I);
            if (winEventNotifierList.empty())
                asyncChannel->unref();
            return;
        }
    }
}
#endif

void EventDispatcherLibUv::startingUp() {
    auto pi = QGuiApplicationPrivate::platformIntegration();
    if (pi) {
        osEventDispatcher = pi->createEventDispatcher();
        if (osEventDispatcher) {
            osEventDispatcher->startingUp();
        }
    }
}

void EventDispatcherLibUv::closingDown() {
    if (osEventDispatcher) {
        osEventDispatcher->closingDown();
    }
}

void EventDispatcherLibUv::setFinalise()
{
    forgetCurrentUvHandles();
    finalise = true;
}


}
