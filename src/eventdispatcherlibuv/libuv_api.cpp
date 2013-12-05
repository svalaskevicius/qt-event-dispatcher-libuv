#include "../eventdispatcherlibuv_p.h"


namespace qtjs {

int LibuvApi::uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd)
{
    return ::uv_poll_init(loop, handle, fd);
}

int LibuvApi::uv_poll_start(uv_poll_t* handle, int events, uv_poll_cb cb)
{
    return ::uv_poll_start(handle, events, cb);
}

int LibuvApi::uv_poll_stop(uv_poll_t* handle)
{
    return ::uv_poll_stop(handle);
}

int LibuvApi::uv_timer_init(uv_loop_t* loop, uv_timer_t* handle)
{
    return ::uv_timer_init(loop, handle);
}

int LibuvApi::uv_timer_start(uv_timer_t* handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat)
{
    return ::uv_timer_start(handle, cb, timeout, repeat);
}

int LibuvApi::uv_timer_stop(uv_timer_t* handle)
{
    return ::uv_timer_stop(handle);
}

uint64_t LibuvApi::uv_hrtime()
{
    return ::uv_hrtime();
}

void LibuvApi::uv_close(uv_handle_t* handle, uv_close_cb close_cb)
{
    return ::uv_close(handle, close_cb);
}

int LibuvApi::uv_async_init(uv_loop_t* loop, uv_async_t* async, uv_async_cb async_cb)
{
    return ::uv_async_init(loop, async, async_cb);
}
int LibuvApi::uv_async_send(uv_async_t* async)
{
    return ::uv_async_send(async);
}

void LibuvApi::uv_unref(uv_handle_t* handle)
{
    ::uv_unref(handle);
}


}
