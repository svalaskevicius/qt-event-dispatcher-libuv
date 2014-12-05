#include "../eventdispatcherlibuv_p.h"

namespace qtjs {

EventDispatcherLibUvTimerNotifier::EventDispatcherLibUvTimerNotifier(LibuvApi *api) : api(api)
{
    if (!this->api) {
        this->api.reset(new LibuvApi());
    }
}

EventDispatcherLibUvTimerNotifier::~EventDispatcherLibUvTimerNotifier()
{
    for (auto it : timers) {
        unregisterTimerWatcher(it.second);
    }
    timers.clear();
}


void EventDispatcherLibUvTimerNotifier::registerTimer(int timerId, int interval, std::function<void()> callback)
{
    auto it = timers.find(timerId);
    if (timers.end() == it) {
        timers.insert(std::make_pair(timerId, new uv_timer_t()));
        it = timers.find(timerId);
        Q_ASSERT(timers.end() != it);
        it->second->data = new TimerData();
        api->uv_timer_init(uv_default_loop(), it->second);
    }
    uv_timer_t *timer = it->second;
    ((TimerData *)timer->data)->timeout = callback;
    api->uv_timer_start(timer, &uv_timer_watcher, interval, interval);
}

bool EventDispatcherLibUvTimerNotifier::unregisterTimer(int timerId) {
    auto it = timers.find(timerId);
    if (it == timers.end()) {
        return false;
    }
    unregisterTimerWatcher(it->second);
    timers.erase(it);
    return true;
}

void EventDispatcherLibUvTimerNotifier::unregisterTimerWatcher(uv_timer_t *watcher)
{
    api->uv_timer_stop(watcher);
    api->uv_close((uv_handle_t *)watcher, &uv_close_timerHandle);
}


void uv_timer_watcher(uv_timer_t* handle)
{
    TimerData *data = (TimerData *) handle->data;
    if (data) {
        data->timeout();
    }
}

void uv_close_timerHandle(uv_handle_t* handle)
{
    uv_timer_t *timer = (uv_timer_t *)handle;
    delete ((TimerData *)timer->data);
    delete timer;
}

}
