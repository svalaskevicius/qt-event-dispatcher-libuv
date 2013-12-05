#include "eventdispatcherlibuv.h"
#include <QCoreApplication>
#include <QSocketNotifier>


#include "uv.h"
#include <QDebug>

#include "eventdispatcherlibuv_p.h"

#include <5.2.0/QtGui/qpa/qwindowsysteminterface.h>

extern uint qGlobalPostedEventsCount(); // from qapplication.cpp


namespace qtjs {


EventDispatcherLibUv::EventDispatcherLibUv(QObject *parent) :
    QAbstractEventDispatcher(parent),
    socketNotifier(new EventDispatcherLibUvSocketNotifier()),
    timerNotifier(new EventDispatcherLibUvTimerNotifier()),
    timerTracker(new EventDispatcherLibUvTimerTracker()),
    asyncChannel(new EventDispatcherLibUvAsyncChannel())
{
}

EventDispatcherLibUv::~EventDispatcherLibUv(void) {}

void EventDispatcherLibUv::wakeUp(void)
{
    asyncChannel->send();
}

void EventDispatcherLibUv::interrupt(void)
{
    asyncChannel->send();
}

void EventDispatcherLibUv::flush(void)
{
}

bool EventDispatcherLibUv::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    emit awake();
    QCoreApplication::sendPostedEvents();
    QWindowSystemInterface::sendWindowSystemEvents(flags);
    emit aboutToBlock();

    return uv_run(uv_default_loop(), UV_RUN_ONCE);
}

bool EventDispatcherLibUv::hasPendingEvents(void)
{
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

}
