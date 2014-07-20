#include "eventdispatcherlibuv.h"
#include <QCoreApplication>
#include <QSocketNotifier>


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
    delete osEventDispatcher;
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
    osEventDispatcher->processEvents(flags & ~QEventLoop::WaitForMoreEvents & ~QEventLoop::EventLoopExec);
    QCoreApplication::sendPostedEvents();
    emit aboutToBlock();

    int leftHandles = uv_run(uv_default_loop(), UV_RUN_ONCE);
    if (!leftHandles) {
        osEventDispatcher->processEvents(flags & ~QEventLoop::EventLoopExec | QEventLoop::WaitForMoreEvents);
    }
    return leftHandles;
}

bool EventDispatcherLibUv::hasPendingEvents(void)
{
    return osEventDispatcher->hasPendingEvents() || qGlobalPostedEventsCount();
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

void EventDispatcherLibUv::startingUp() {
    osEventDispatcher = QGuiApplicationPrivate::platformIntegration()->createEventDispatcher();
    osEventDispatcher->startingUp();
}

void EventDispatcherLibUv::closingDown() {
    osEventDispatcher->closingDown();
}

void EventDispatcherLibUv::setFinalise()
{
    forgetCurrentUvHandles();
    finalise = true;
}


}
