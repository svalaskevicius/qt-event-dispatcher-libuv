#include "../eventdispatcherlibuv_p.h"

namespace qtjs {


EventDispatcherLibUvAsyncChannel::EventDispatcherLibUvAsyncChannel(LibuvApi *api) : api(api), handle(nullptr)
{
    if (!this->api) {
        this->api.reset(new LibuvApi());
    }
    handle = new uv_async_t();
    this->api->uv_async_init(uv_default_loop(), handle, nullptr);
    this->api->uv_unref((uv_handle_t*)handle);
}

EventDispatcherLibUvAsyncChannel::~EventDispatcherLibUvAsyncChannel()
{
    api->uv_close((uv_handle_t *)handle, &uv_close_asyncHandle);
}
void EventDispatcherLibUvAsyncChannel::send()
{
    api->uv_async_send(handle);
}



void uv_close_asyncHandle(uv_handle_t* handle)
{
    delete (uv_async_t *)handle;
}


}
