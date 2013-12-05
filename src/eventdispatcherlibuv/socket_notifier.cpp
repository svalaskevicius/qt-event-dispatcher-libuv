#include "../eventdispatcherlibuv_p.h"

#include <QDebug>

namespace {

inline int translateQSocketNotifierTypeToUv(QSocketNotifier::Type type) {
    switch (type) {
        case QSocketNotifier::Read: return UV_READABLE;
        case QSocketNotifier::Write: return UV_WRITABLE;
        default: return -1;
    }
}

}

namespace qtjs {


EventDispatcherLibUvSocketNotifier::EventDispatcherLibUvSocketNotifier(LibuvApi *api) : api(api)
{
    if (!this->api) {
        this->api.reset(new LibuvApi());
    }
}

EventDispatcherLibUvSocketNotifier::~EventDispatcherLibUvSocketNotifier()
{
    for (auto it : socketWatchers) {
        unregisterPollWatcher(it.second, UV_READABLE | UV_WRITABLE);
    }
    socketWatchers.clear();
}

void EventDispatcherLibUvSocketNotifier::registerSocketNotifier(int fd, QSocketNotifier::Type type, std::function<void()> callback)
{
    int uvType = translateQSocketNotifierTypeToUv(type);
    if (uvType < 0) {
        qWarning() << "unsupported notifier type" << type;
        return;
    }
    uv_poll_t *fdWatcher = findOrCreateWatcher(fd);

    SocketCallbacks *callbacks = ((SocketCallbacks *)fdWatcher->data);
    callbacks->eventMask |= uvType;
    if (uvType == UV_READABLE) {
        callbacks->readAvailable = callback;
    }
    if (uvType == UV_WRITABLE) {
        callbacks->writeAvailable = callback;
    }
    api->uv_poll_start(fdWatcher, uvType, &qtjs::uv_socket_watcher);
}

uv_poll_t *EventDispatcherLibUvSocketNotifier::findOrCreateWatcher(int fd)
{
    auto it = socketWatchers.find(fd);
    if (socketWatchers.end() == it) {
        socketWatchers.insert(std::make_pair(fd, new uv_poll_t()));
        it = socketWatchers.find(fd);
        Q_ASSERT(socketWatchers.end() != it);
        it->second->data = new SocketCallbacks();
        api->uv_poll_init(uv_default_loop(), it->second, fd);
    }
    return it->second;
}

void EventDispatcherLibUvSocketNotifier::unregisterSocketNotifier(int fd, QSocketNotifier::Type type)
{
    int uvType = translateQSocketNotifierTypeToUv(type);
    if (uvType < 0) {
        qWarning() << "unsupported notifier type" << type;
        return;
    }
    auto it = socketWatchers.find(fd);
    if (socketWatchers.end() != it) {
        uv_poll_t *fdWatcher = it->second;
        if (unregisterPollWatcher(fdWatcher, uvType)) {
            socketWatchers.erase(it);
        }
    }
}

bool EventDispatcherLibUvSocketNotifier::unregisterPollWatcher(uv_poll_t *fdWatcher, unsigned int eventMask)
{
    api->uv_poll_stop(fdWatcher);
    SocketCallbacks *callbacks = (SocketCallbacks *)fdWatcher->data;
    callbacks->eventMask &= ~eventMask;
    if (!callbacks->eventMask) {
        api->uv_close((uv_handle_t *) fdWatcher, uv_close_pollHandle);
        return true;
    }
    api->uv_poll_start(fdWatcher, callbacks->eventMask, &qtjs::uv_socket_watcher);
    return false;
}



void uv_socket_watcher(uv_poll_t* req, int /* status */, int events)
{
    SocketCallbacks *callbacks = (SocketCallbacks *) req->data;
    if (callbacks) {
        if (events & UV_READABLE) {
            callbacks->readAvailable();
        }
        if (events & UV_WRITABLE) {
            callbacks->writeAvailable();
        }
    }
}


void uv_close_pollHandle(uv_handle_t* handle)
{
    uv_poll_t *fdWatcher = (uv_poll_t *)handle;
    delete (SocketCallbacks *)fdWatcher->data;
    delete fdWatcher;
}


}
