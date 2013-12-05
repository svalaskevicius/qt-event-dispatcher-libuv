#include "test_setup.h"

#include "../src/runner/eventdispatcherlibuv_p.h"

#include <QSocketNotifier>


MOCK_BASE_CLASS( MockedLibuvApi, qtjs::LibuvApi ) {
    MOCK_METHOD(uv_poll_init, 3)
    MOCK_METHOD(uv_poll_start, 3)
    MOCK_METHOD(uv_poll_stop, 1)

    MOCK_METHOD(uv_timer_init, 2)
    MOCK_METHOD(uv_timer_start, 4)
    MOCK_METHOD(uv_timer_stop, 1)

    MOCK_METHOD(uv_hrtime, 0)

    MOCK_METHOD(uv_close, 2)

    MOCK_METHOD(uv_async_init, 3)
    MOCK_METHOD(uv_async_send, 1)

    MOCK_METHOD(uv_unref, 1)
};

namespace {

struct PollMocker {
    uv_poll_t *registeredHandle, *startedHandle, *stoppedHandle;
    uv_handle_t *closedHandle;
    bool checkStart, checkStop, checkClose;
    MockedLibuvApi *api;

    PollMocker(MockedLibuvApi *api);
    void mockInit(int fd);
    void mockStart(int type);
    void mockInitAndExecute(int fd, int type);
    void mockImplicitStopClose();
    void mockStop();
    void mockClose();
    void checkHandles();
    void verifyAndReset();
};

struct AsyncMocker {
    uv_handle_t *closedHandle, *unreffedHandle;
    uv_async_t *registeredHandle, *asyncHandleSend;
    bool checkClose, checkAsync;
    MockedLibuvApi *api;

    AsyncMocker(MockedLibuvApi *api);
    void mockInit();
    void mockClose();
    void mockAsyncSend();
    void checkHandles();
};

struct TimerMocker {
    uv_timer_t *registeredHandle, *startedHandle, *stoppedHandle;
    uv_handle_t *closedHandle;
    bool checkStart, checkStop, checkClose;
    MockedLibuvApi *api;

    TimerMocker(MockedLibuvApi *api);
    void mockInit();
    void mockStart(uint64_t timeout);
    void mockImplicitStopClose();
    void mockStop();
    void mockClose();
    void checkHandles();
    void verifyAndReset();
};

}

TEST_CASE("EventDispatcherLibUv supports QSocketNotifier registration")
{
    SECTION("registerSocketNotifier initialises libuv handle for the given read fd")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(19, UV_READABLE);

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(19, QSocketNotifier::Read, []{});

        mocker.checkHandles();
    }

    SECTION("registerSocketNotifier initialises libuv handle for the given write fd")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(20, UV_WRITABLE);

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(20, QSocketNotifier::Write, []{});

        mocker.checkHandles();
    }

    SECTION("unregisterSocketNotifier stops uv poller for a registered handle")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(20, UV_WRITABLE);
        mocker.mockStop();

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(20, QSocketNotifier::Write, []{});
        dispatcher.unregisterSocketNotifier(20, QSocketNotifier::Write);

        mocker.checkHandles();
    }

    SECTION("it implicitly stops uv poller for a registered handle on destruction")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(20, UV_WRITABLE);
        mocker.mockStop();

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(20, QSocketNotifier::Write, []{});
    }

    SECTION("unregisterSocketNotifier does not call libuv if the socket was not registered")
    {
        MockedLibuvApi *api = new MockedLibuvApi();

        MOCK_EXPECT( api->uv_poll_stop ).never();

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.unregisterSocketNotifier(20, QSocketNotifier::Write);
    }

    SECTION("unregisterSocketNotifier stops uv poller just once for a registered handle")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(20, UV_WRITABLE);
        mocker.mockStop();

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(20, QSocketNotifier::Write, []{});
        dispatcher.unregisterSocketNotifier(20, QSocketNotifier::Write);
        dispatcher.unregisterSocketNotifier(20, QSocketNotifier::Write);

        mocker.checkHandles();
    }

    SECTION("socket watcher invokes read callback")
    {
        int callbackInvoked = 0;
        qtjs::SocketCallbacks callbacks = {
            UV_READABLE,
            [&callbackInvoked]{ callbackInvoked++; },
            []{ FAIL("unexpected call"); }
        };

        uv_poll_t request;
        request.data = &callbacks;

        qtjs::uv_socket_watcher(&request, 0, UV_READABLE);

        REQUIRE( callbackInvoked == 1 );
    }

    SECTION("socket watcher invokes write callback")
    {
        int callbackInvoked = 0;
        qtjs::SocketCallbacks callbacks = {
            UV_WRITABLE,
            []{ FAIL("unexpected call"); },
            [&callbackInvoked]{ callbackInvoked++; }
        };

        uv_poll_t request;
        request.data = &callbacks;

        qtjs::uv_socket_watcher(&request, 0, UV_WRITABLE);

        REQUIRE( callbackInvoked == 1 );
    }


    SECTION("registerSocketNotifier initialises libuv handle read callback")
    {
        int callbackInvoked = 0;
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(19, UV_READABLE);

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(19, QSocketNotifier::Read, [&callbackInvoked]{ callbackInvoked++; });

        mocker.checkHandles();
        REQUIRE( mocker.startedHandle->data );
        ((qtjs::SocketCallbacks *)mocker.startedHandle->data)->readAvailable();
        REQUIRE_THROWS( ((qtjs::SocketCallbacks *)mocker.startedHandle->data)->writeAvailable() );
        REQUIRE( callbackInvoked == 1 );
    }

    SECTION("registerSocketNotifier initialises libuv handle write callback")
    {
        int callbackInvoked = 0;
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(19, UV_WRITABLE);

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(19, QSocketNotifier::Write, [&callbackInvoked]{ callbackInvoked++; });

        mocker.checkHandles();
        REQUIRE( mocker.startedHandle->data );
        REQUIRE_THROWS( ((qtjs::SocketCallbacks *)mocker.startedHandle->data)->readAvailable() );
        ((qtjs::SocketCallbacks *)mocker.startedHandle->data)->writeAvailable();
        REQUIRE( callbackInvoked == 1 );
    }

    SECTION("registerSocketNotifier combines libuv handle read&write callbacks")
    {
        int callbackInvoked = 0;
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(19, UV_READABLE);
        mocker.mockStart(UV_WRITABLE);

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(19, QSocketNotifier::Read, [&callbackInvoked]{ callbackInvoked++; });
        dispatcher.registerSocketNotifier(19, QSocketNotifier::Write, [&callbackInvoked]{ callbackInvoked++; });

        mocker.checkHandles();
        REQUIRE( mocker.startedHandle->data );

        ((qtjs::SocketCallbacks *)mocker.startedHandle->data)->readAvailable();
        ((qtjs::SocketCallbacks *)mocker.startedHandle->data)->writeAvailable();

        REQUIRE( callbackInvoked == 2 );
    }

    SECTION("unregisterSocketNotifier only unregisters the requested events type")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(20, UV_WRITABLE);
        mocker.mockStart(UV_READABLE);

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(20, QSocketNotifier::Write, []{});
        dispatcher.registerSocketNotifier(20, QSocketNotifier::Read, []{});

        mocker.checkHandles();
        mocker.verifyAndReset();

        mocker.mockStop();
        mocker.mockStart(UV_READABLE);

        dispatcher.unregisterSocketNotifier(20, QSocketNotifier::Write);

        mocker.checkHandles();
        mocker.verifyAndReset();
    }

    SECTION("unregisterSocketNotifier calls uv_close before deallocation")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        PollMocker mocker(api);
        mocker.mockInitAndExecute(20, UV_WRITABLE);
        mocker.mockClose();

        qtjs::EventDispatcherLibUvSocketNotifier dispatcher(api);
        dispatcher.registerSocketNotifier(20, QSocketNotifier::Write, []{});
        dispatcher.unregisterSocketNotifier(20, QSocketNotifier::Write);

        mocker.checkHandles();
    }
}

TEST_CASE("EventDispatcherLibUv async wakeups")
{
    SECTION("it manages async handles lifetime")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        AsyncMocker mocker(api);
        mocker.mockInit();
        mocker.mockClose();

        {
            qtjs::EventDispatcherLibUvAsyncChannel channel(api);
        }

        mocker.checkHandles();
    }

    SECTION("it uses async handle to wake up")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        AsyncMocker mocker(api);
        mocker.mockInit();
        mocker.mockAsyncSend();

        {
            qtjs::EventDispatcherLibUvAsyncChannel channel(api);
            channel.send();
        }

        mocker.checkHandles();
    }
}

TEST_CASE("EventDispatcherLibUv supports QTimer registration")
{
    SECTION("it does not unregister a non-existing timer")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        qtjs::EventDispatcherLibUvTimerNotifier dispatcher(api);

        REQUIRE( dispatcher.unregisterTimer(83) == false );
    }

    SECTION("it unregisters a registered timer once")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        qtjs::EventDispatcherLibUvTimerNotifier dispatcher(api);

        TimerMocker mocker(api);
        mocker.mockInit();
        mocker.mockStart(30);
        mocker.mockStop();

        dispatcher.registerTimer(83, 30, []{});
        REQUIRE( dispatcher.unregisterTimer(83) == true );
        REQUIRE( dispatcher.unregisterTimer(83) == false );

        mocker.checkHandles();
    }

    SECTION("it unregisters a registered timer on destruction")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        qtjs::EventDispatcherLibUvTimerNotifier dispatcher(api);

        TimerMocker mocker(api);
        mocker.mockInit();
        mocker.mockStart(30);
        mocker.mockStop();

        dispatcher.registerTimer(83, 30, []{});
    }

    SECTION("timer watcher invokes timeout callback")
    {
        int callbackInvoked = 0;
        qtjs::TimerData data = {
            [&callbackInvoked]{ callbackInvoked++; }
        };

        uv_timer_t request;
        request.data = &data;

        qtjs::uv_timer_watcher(&request, 0);

        REQUIRE( callbackInvoked == 1 );
    }

    SECTION("registerTimer initialises timeout callback")
    {
        int callbackInvoked = 0;
        MockedLibuvApi *api = new MockedLibuvApi();
        TimerMocker mocker(api);
        mocker.mockInit();
        mocker.mockStart(30);

        qtjs::EventDispatcherLibUvTimerNotifier dispatcher(api);
        dispatcher.registerTimer(83, 30, [&callbackInvoked]{ callbackInvoked++; });

        mocker.checkHandles();
        REQUIRE( mocker.startedHandle->data );
        ((qtjs::TimerData *)mocker.startedHandle->data)->timeout();
        REQUIRE( callbackInvoked == 1 );
    }

    SECTION("calls uv_close before deallocating timer handle")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        TimerMocker mocker(api);
        mocker.mockInit();
        mocker.mockStart(30);
        mocker.mockStop();
        mocker.mockClose();

        qtjs::EventDispatcherLibUvTimerNotifier dispatcher(api);
        dispatcher.registerTimer(83, 30, []{});
        dispatcher.unregisterTimer(83);

        mocker.checkHandles();
    }

}

TEST_CASE("EventDispatcherLibUv tracks timer execution")
{
    SECTION("TimerWatcher returns empty list when there are no timers registered")
    {
        qtjs::EventDispatcherLibUvTimerTracker watcher;
        REQUIRE( watcher.getTimerInfo(nullptr).empty() );
    }

    SECTION("TimerWatcher returns a list of registered timers for object")
    {
        qtjs::EventDispatcherLibUvTimerTracker watcher;
        watcher.registerTimer(12, 101, Qt::CoarseTimer, (QObject *)919192);
        auto list = watcher.getTimerInfo((QObject *)919192);
        REQUIRE( list.count() == 1 );
        REQUIRE( list.back().timerId == 12 );
        REQUIRE( list.back().interval == 101 );
        REQUIRE( list.back().timerType == Qt::CoarseTimer );
    }

    SECTION("TimerWatcher returns time left until next firing at the start")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        qtjs::EventDispatcherLibUvTimerTracker watcher(api);

        uint64_t returnedValues[] = {1000000, 3000000},
                 *pReturnedValues = returnedValues;

        MOCK_EXPECT( api->uv_hrtime ).exactly(2)
            .calls([&pReturnedValues]() { return *pReturnedValues++; });

        watcher.registerTimer(12, 5, Qt::CoarseTimer, (QObject *)919192);

        REQUIRE( watcher.remainingTime(12) == 3 );
    }

    SECTION("TimerWatcher returns time left until next firing after firing")
    {
        MockedLibuvApi *api = new MockedLibuvApi();
        qtjs::EventDispatcherLibUvTimerTracker watcher(api);

        uint64_t returnedValues[] = {1000000, 3000000, 4000000},
                 *pReturnedValues = returnedValues;

        MOCK_EXPECT( api->uv_hrtime ).exactly(3)
            .calls([&pReturnedValues]() { return *pReturnedValues++; });

        watcher.registerTimer(12, 5, Qt::CoarseTimer, (QObject *)919192);

        watcher.fireTimer(12);

        REQUIRE( watcher.remainingTime(12) == 4 );
    }

    SECTION("TimerWatcher unregisters timerinfo")
    {
        qtjs::EventDispatcherLibUvTimerTracker watcher;
        watcher.registerTimer(12, 101, Qt::CoarseTimer, (QObject *)919192);
        watcher.unregisterTimer(12);
        REQUIRE( watcher.getTimerInfo((QObject *)919192).empty() );
    }


}





namespace {

PollMocker::PollMocker(MockedLibuvApi *api)
    : checkStart(false), checkStop(false), checkClose(false), api(api)
{
}

void PollMocker::mockInit(int fd)
{
    MOCK_EXPECT( api->uv_poll_init ).once()
            .with( mock::equal(uv_default_loop()), mock::retrieve(registeredHandle), mock::equal(fd) )
            .returns(0);
}

void PollMocker::mockStart(int type)
{
    MOCK_EXPECT( api->uv_poll_start ).once()
            .with( mock::retrieve(startedHandle), mock::equal(type), mock::equal(&qtjs::uv_socket_watcher) )
            .returns(0);
    checkStart = true;
}

void PollMocker::mockInitAndExecute(int fd, int type)
{
    mockInit(fd);
    mockStart(type);
    mockImplicitStopClose();
}

void PollMocker::mockStop()
{
    MOCK_RESET(api->uv_poll_stop);
    MOCK_EXPECT( api->uv_poll_stop ).once()
            .with( mock::retrieve(stoppedHandle) )
            .returns(0);
    checkStop = true;
}
void PollMocker::mockClose()
{
    MOCK_RESET(api->uv_close);
    MOCK_EXPECT( api->uv_close ).once()
        .with( mock::retrieve(closedHandle), mock::equal(&qtjs::uv_close_pollHandle));
    checkClose = true;
}

void PollMocker::mockImplicitStopClose()
{
    MOCK_EXPECT( api->uv_poll_stop ).returns(0);
    MOCK_EXPECT( api->uv_close );
}

void PollMocker::checkHandles()
{
    REQUIRE( registeredHandle );
    if (checkStart) {
        REQUIRE( registeredHandle == startedHandle );
    }
    if (checkStop) {
        REQUIRE( registeredHandle == stoppedHandle );
    }
    if (checkClose) {
        REQUIRE( (uv_handle_t *)registeredHandle == closedHandle );
    }
}

void PollMocker::verifyAndReset()
{
    MOCK_VERIFY(api->uv_poll_init);
    MOCK_RESET(api->uv_poll_init);
    if (checkStart) {
        MOCK_VERIFY(api->uv_poll_start);
        MOCK_RESET(api->uv_poll_start);
        checkStart = false;
    }
    if (checkStop) {
        MOCK_VERIFY(api->uv_poll_stop);
        MOCK_RESET(api->uv_poll_stop);
        checkStop = false;
        mockImplicitStopClose();
    }
}



AsyncMocker::AsyncMocker(MockedLibuvApi *api) : checkClose(false), checkAsync(false), api(api)
{
}
void AsyncMocker::mockInit()
{
    MOCK_EXPECT(api->uv_async_init).once()
        .with( mock::equal(uv_default_loop()), mock::retrieve(registeredHandle), mock::equal(nullptr))
        .returns(0);
    MOCK_EXPECT(api->uv_unref).once()
        .with( mock::retrieve(unreffedHandle));
    MOCK_EXPECT(api->uv_close);
}
void AsyncMocker::mockClose()
{
    MOCK_RESET(api->uv_close);
    MOCK_EXPECT( api->uv_close ).once()
        .with( mock::retrieve(closedHandle), mock::equal(&qtjs::uv_close_asyncHandle));
    checkClose = true;
}

void AsyncMocker::mockAsyncSend()
{
    MOCK_EXPECT(api->uv_async_send).once()
        .with( mock::retrieve(asyncHandleSend))
        .returns(0);
    checkAsync = true;
}

void AsyncMocker::checkHandles()
{
    REQUIRE( registeredHandle );
    REQUIRE( (uv_handle_t *)registeredHandle == unreffedHandle );
    if (checkClose) {
        REQUIRE( (uv_handle_t *)registeredHandle == closedHandle );
    }
    if (checkAsync) {
        REQUIRE(registeredHandle == asyncHandleSend);
    }
}





TimerMocker::TimerMocker(MockedLibuvApi *api) : checkStart(false), checkStop(false), checkClose(false), api(api)
{
}
void TimerMocker::mockInit()
{
    MOCK_EXPECT( api->uv_timer_init ).once()
        .with( mock::equal(uv_default_loop()), mock::retrieve(registeredHandle) )
        .returns(0);
}
void TimerMocker::mockStart(uint64_t timeout)
{
    MOCK_EXPECT( api->uv_timer_start ).once()
        .with( mock::retrieve(startedHandle),  mock::equal(&qtjs::uv_timer_watcher), mock::equal(timeout), mock::equal(timeout) )
        .returns(0);
    checkStart = true;
    mockImplicitStopClose();
}

void TimerMocker::mockImplicitStopClose()
{
    MOCK_EXPECT( api->uv_timer_stop ).returns(0);
    MOCK_EXPECT( api->uv_close );
}

void TimerMocker::mockStop()
{
    MOCK_RESET(api->uv_timer_stop);
    MOCK_EXPECT( api->uv_timer_stop ).once()
        .with( mock::retrieve(stoppedHandle))
        .returns(0);
    checkStop = true;
}
void TimerMocker::mockClose()
{
    MOCK_RESET(api->uv_close);
    MOCK_EXPECT( api->uv_close ).once()
            .with( mock::retrieve(closedHandle), mock::equal(&qtjs::uv_close_timerHandle));
    checkClose = true;
}
void TimerMocker::checkHandles()
{
    REQUIRE( registeredHandle );
    if (checkStart) {
        REQUIRE( registeredHandle == startedHandle );
    }
    if (checkStop) {
        REQUIRE( registeredHandle == stoppedHandle );
    }
    if (checkClose) {
        REQUIRE( (uv_handle_t *)registeredHandle == closedHandle );
    }
}
void TimerMocker::verifyAndReset()
{
    MOCK_VERIFY(api->uv_timer_init);
    MOCK_RESET(api->uv_timer_init);
    if (checkStart) {
        MOCK_VERIFY(api->uv_timer_start);
        MOCK_RESET(api->uv_timer_start);
        checkStart = false;
    }
    if (checkStop) {
        MOCK_VERIFY(api->uv_timer_stop);
        MOCK_RESET(api->uv_timer_stop);
        checkStop = false;
        mockImplicitStopClose();
    }
}

}

