#include "../eventdispatcherlibuv_p.h"


namespace qtjs {


EventDispatcherLibUvTimerTracker::EventDispatcherLibUvTimerTracker(LibuvApi *api) : api(api)
{
    if (!this->api) {
        this->api.reset(new LibuvApi());
    }
}

void EventDispatcherLibUvTimerTracker::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *object)
{
    timers[object].append(QAbstractEventDispatcher::TimerInfo(timerId, interval, timerType));
    timerInfos[timerId] = {api->uv_hrtime() / 1000000, interval, object};
}

void EventDispatcherLibUvTimerTracker::unregisterTimer(int timerId)
{
    void *object = timerInfos[timerId].object;
    timerInfos.erase(timerId);
    cleanTimerFromObject(timerId, object);
    if (timers[object].empty()) {
        timers.erase(object);
    }
}

void EventDispatcherLibUvTimerTracker::cleanTimerFromObject(int timerId, void *object)
{
    QMutableListIterator<QAbstractEventDispatcher::TimerInfo> it(timers[object]);
    while (it.hasNext()) {
        if (it.next().timerId == timerId) {
            it.remove();
            return;
        }
    }
}


QList<QAbstractEventDispatcher::TimerInfo> EventDispatcherLibUvTimerTracker::getTimerInfo(QObject *object)
{
    auto it = timers.find(object);
    if (timers.end() == it) {
        return QList<QAbstractEventDispatcher::TimerInfo>();
    }
    return it->second;
}

void EventDispatcherLibUvTimerTracker::fireTimer(int timerId)
{
    timerInfos[timerId].lastFired = api->uv_hrtime() / 1000000;
}

int EventDispatcherLibUvTimerTracker::remainingTime(int timerId)
{
    const TimerInfo &timerInfo = timerInfos[timerId];
    return timerInfo.interval
            + timerInfo.lastFired
            - api->uv_hrtime() / 1000000;
}


}
