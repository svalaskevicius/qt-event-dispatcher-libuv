#pragma once

#include <QAbstractEventDispatcher>
#include <QSocketNotifier>

#include "uv.h"

#include <memory>
#include <map>
#include <functional>

namespace qtjs {


struct SocketCallbacks {
    int eventMask;
    std::function<void()> readAvailable;
    std::function<void()> writeAvailable;
};

struct TimerData {
    std::function<void()> timeout;
};



void uv_socket_watcher(uv_poll_t* handle, int status, int events);
void uv_timer_watcher(uv_timer_t* handle);
void uv_close_pollHandle(uv_handle_t* handle);
void uv_close_timerHandle(uv_handle_t* handle);
void uv_close_asyncHandle(uv_handle_t* handle);



struct LibuvApi {
    virtual ~LibuvApi() {}
    virtual int uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd);
    virtual int uv_poll_start(uv_poll_t* handle, int events, uv_poll_cb cb);
    virtual int uv_poll_stop(uv_poll_t* handle);

    virtual int uv_timer_init(uv_loop_t*, uv_timer_t* handle);
    virtual int uv_timer_start(uv_timer_t* handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat);
    virtual int uv_timer_stop(uv_timer_t* handle);

    virtual uint64_t uv_hrtime(void);

    virtual void uv_close(uv_handle_t* handle, uv_close_cb close_cb);

    virtual int uv_async_init(uv_loop_t*, uv_async_t* async, uv_async_cb async_cb);
    virtual int uv_async_send(uv_async_t* async);

    virtual void uv_unref(uv_handle_t* handle);
};




class EventDispatcherLibUvAsyncChannel {
public:
    EventDispatcherLibUvAsyncChannel(LibuvApi *api = nullptr);
    virtual ~EventDispatcherLibUvAsyncChannel();
    void send();
private:
    std::unique_ptr<LibuvApi> api;
    uv_async_t *handle;
};




class EventDispatcherLibUvSocketNotifier {
public:
    EventDispatcherLibUvSocketNotifier(LibuvApi *api = nullptr);
    virtual ~EventDispatcherLibUvSocketNotifier();
    void registerSocketNotifier(int fd, QSocketNotifier::Type type, std::function<void()> callback);
    void unregisterSocketNotifier(int fd, QSocketNotifier::Type type);
    void wakeup(){}
private:
    std::unique_ptr<LibuvApi> api;
    std::map<int, uv_poll_t*> socketWatchers;
    uv_poll_t *findOrCreateWatcher(int fd);
    bool unregisterPollWatcher(uv_poll_t *fdWatcher, unsigned int eventMask);
};




class EventDispatcherLibUvTimerNotifier {
public:
    EventDispatcherLibUvTimerNotifier(LibuvApi *api = nullptr);
    virtual ~EventDispatcherLibUvTimerNotifier();
    void registerTimer(int timerId, int interval, std::function<void()> callback);
    bool unregisterTimer(int timerId);
private:
    std::unique_ptr<LibuvApi> api;
    std::map<int, uv_timer_t*> timers;
    void unregisterTimerWatcher(uv_timer_t *watcher);
};




class EventDispatcherLibUvTimerTracker {
public:
    EventDispatcherLibUvTimerTracker(LibuvApi *api = nullptr);
    void registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *object);
    void unregisterTimer(int timerId);
    QList<QAbstractEventDispatcher::TimerInfo> getTimerInfo(QObject *object);
    void fireTimer(int timerId);
    int remainingTime(int timerId);
private:
    void cleanTimerFromObject(int timerId, void *object);
    struct TimerInfo {
        unsigned long lastFired;
        int interval;
        void *object;
    };
    std::unique_ptr<LibuvApi> api;
    std::map<void *, QList<QAbstractEventDispatcher::TimerInfo>> timers;
    std::map<int, TimerInfo> timerInfos;
};


}
