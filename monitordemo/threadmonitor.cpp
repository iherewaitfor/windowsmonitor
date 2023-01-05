#include <windows.h>
#include "detours.h"
#include <DbgHelp.h>
#include <process.h>
#include "threadmonitor.h"
#include "callstackdef.h"
#include "stackwalkhelper.h"
#include <iostream>
#include <ctime>
#define FREEZE_TIME 5000
#define DETECT_TIME 1000
typedef LRESULT(WINAPI* func_DispatchMessage)(_In_ CONST MSG*);
func_DispatchMessage gOriginDispatchMessageW = ::DispatchMessage;
DWORD gMainThreadId = 0;
HANDLE gMainThread = NULL;
unsigned int gMsg[100];
unsigned int gMsgCount = 0;
volatile DWORD gLastActiveTime = 0;

bool matchMsg(unsigned int msg)
{
    for (unsigned int i = 0; i < gMsgCount; ++i)
    {
        if (gMsg[i] == msg)
            return false;
    }
    return true;
}

LRESULT WINAPI gMyDispatchMessageW(_In_ CONST MSG* lpMsg)
{
    if (matchMsg(lpMsg->message) && ::GetCurrentThreadId() == gMainThreadId)
    {
        gLastActiveTime = GetTickCount();
        ThreadMonitor::Instance()->startMonitor(lpMsg->message);
        BOOL ret = gOriginDispatchMessageW(lpMsg);
        ThreadMonitor::Instance()->stopMonitor();
        return ret;
    }
    return gOriginDispatchMessageW(lpMsg);
}

LRESULT CALLBACK gMyGetMessage(int code, WPARAM wParam, LPARAM lParam)
{
    if (::GetCurrentThreadId() == gMainThreadId)
    {
        gLastActiveTime = GetTickCount();
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

ThreadMonitor* ThreadMonitor::Instance()
{
    static ThreadMonitor instance;
    return &instance;
}

ThreadMonitor::ThreadMonitor()
{
}

ThreadMonitor::~ThreadMonitor()
{
    release();
}

void ThreadMonitor::init()
{
    if (!_init)
    {
        _monitorThread = NULL;
        _hwnd = NULL;
        _entryTimes = 0;
        _entryTimePoint = 0;
        _init = false;
        _lastBlockEntryTimePoint = 0;
        _exitNow = false;

        gMainThreadId = ::GetCurrentThreadId();
        HANDLE mainThread = ::GetCurrentThread();
        if (::DuplicateHandle(::GetCurrentProcess(), mainThread, ::GetCurrentProcess(), &gMainThread, NULL, FALSE,
            DUPLICATE_SAME_ACCESS)
            != TRUE)
        {
            return;
        }
        hookDispatchMessage();
        _monitorThread = (HANDLE)_beginthreadex(NULL, 0, &ThreadMonitor::MonitorThreadRoutine, (void*)this, 0, NULL);
        if (_monitorThread == NULL)
        {
            unHookDispatchMessage();
            ::CloseHandle(gMainThread);
            return;
        }
        _init = true;
    }
}

void ThreadMonitor::release()
{
    if (_init)
    {
        _init = false;
        _exitNow = true;
        if (_monitorThread)
        {
            SetTimer(_hwnd, 2, 100, NULL);
            PostMessageA(_hwnd, WM_QUIT, 0, 0);
            WaitForSingleObject(_monitorThread, INFINITE);
            ::CloseHandle(_monitorThread);
            _hwnd = NULL;
            _monitorThread = NULL;
        }
        unHookDispatchMessage();
        ::CloseHandle(gMainThread);
    }
}


void ThreadMonitor::monitorMsg(const std::vector<unsigned int>& msg)
{
    for (size_t i = 0; i < msg.size() && i < ARRAYSIZE(gMsg); ++i)
    {
        gMsg[i] = msg[i];
    }
    gMsgCount = msg.size();
}

void ThreadMonitor::hookDispatchMessage()
{
    DetourTransactionBegin();
    DetourUpdateThread(::GetCurrentThread());
    DetourAttach(&(PVOID&)gOriginDispatchMessageW, gMyDispatchMessageW);
    DetourTransactionCommit();
    _getMessageId = SetWindowsHookExA(WH_GETMESSAGE, gMyGetMessage, NULL, gMainThreadId);
}

void ThreadMonitor::unHookDispatchMessage()
{
    DetourTransactionBegin();
    DetourUpdateThread(::GetCurrentThread());
    DetourDetach(&(PVOID&)gOriginDispatchMessageW, gMyDispatchMessageW);
    DetourTransactionCommit();
    UnhookWindowsHookEx(_getMessageId);
}

void ThreadMonitor::startMonitor(unsigned int message)
{
    _entryTimes++;
    if (_entryTimes == 1)
    {
        _message = message;
        _entryTimePoint = ::GetTickCount();
    }
}

void ThreadMonitor::stopMonitor()
{
    if (_entryTimes > 0)
    {
        _entryTimes--;
        if (_entryTimes == 0)
        {
            _message = 0;
            _entryTimePoint = 0;
        }
    }
}

unsigned int __stdcall ThreadMonitor::MonitorThreadRoutine(LPVOID param)
{
    ThreadMonitor* monitor = (ThreadMonitor*)param;
    monitor->run();
    return 0;
}

void ThreadMonitor::run()
{
    _hwnd = ::CreateWindowA("STATIC", "ThreadMonitor", WS_POPUP, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    ::SetWindowLongPtr(_hwnd, GWLP_USERDATA, (LONG_PTR)this);
    ::SetWindowLongPtr(_hwnd, GWLP_WNDPROC, (LONG_PTR)ThreadMonitor::WindowRoutine);
    if (_hwnd != NULL)
    {
        MSG msg;
        SetTimer(_hwnd, 1, DETECT_TIME, NULL);
        while (GetMessageA(&msg, _hwnd, 0, 0) && !_exitNow)
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    DestroyWindow(_hwnd);
}

LRESULT CALLBACK ThreadMonitor::WindowRoutine(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ThreadMonitor* pThis = (ThreadMonitor*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    return pThis->process(hwnd, uMsg, wParam, lParam);
}

LRESULT ThreadMonitor::process(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_TIMER:
    {
        unsigned int message = _message;
        unsigned int entryPointTime = _entryTimePoint;
        if (entryPointTime != 0 && message != 0 && wParam == 1)
        {
            DWORD now = ::GetTickCount();
            if (shouldReport(now, entryPointTime))
            {
                std::cout << "发现卡顿" << std::endl;
                //if (true)
                //{
                //    SetTimer(_hwnd, 1, 5000, NULL);
                //    break;
                //}
                //if (IsDebuggerPresent())
                //{ //若是调试械，退出，不监控。
                //    PostMessageA(_hwnd, WM_QUIT, 0, 0);
                //    return 0;
                //}


                callstack::StackWalkResult result;
                time_t nowsecondes = time_t(0);
                DWORD timePoint = nowsecondes;
                if (StackWalkHelper::Instance()->stackWalkOtherThread(gMainThread, entryPointTime, _entryTimePoint, &result))
                {
                    result.timePoint = timePoint;
                    result.blockTime = now - entryPointTime;
                    result.message = message;
                    std::string stackstring;
                    for (int i = 0; i < result.allFrame.size(); i++) {
                        stackstring.append(result.allFrame[i].ImageName);
                        stackstring.append(std::to_string(result.allFrame[i].offset));
                        stackstring.append("\r\n");
                    }
                    std::cout << "the stack size is:" << result.allFrame.size() << " stack is:" << std::endl;
                    std::cout << stackstring;
                }

                SetTimer(_hwnd, 1, DETECT_TIME, NULL);
                break;
            }
            else
            {
                SetTimer(_hwnd, 1, DETECT_TIME, NULL);
            }
        }
        else
        {
            SetTimer(_hwnd, 1, DETECT_TIME, NULL);
        }
    }
    default:
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool ThreadMonitor::shouldReport(DWORD now, DWORD entryTimePoint)
{
    if (now > entryTimePoint && now - entryTimePoint > FREEZE_TIME)
    {
        if (_lastBlockEntryTimePoint != entryTimePoint)
        {
            _lastBlockEntryTimePoint = entryTimePoint;
            if (now > gLastActiveTime && now - gLastActiveTime > FREEZE_TIME)
            {
                PostThreadMessageA(gMainThreadId, WM_MOUSEMOVE, 0, 0);
                Sleep(100);
                if (gLastActiveTime >= now)
                {
                    // 主线程还是正常的，这种情况常见于打开了一个模态窗口
                    return false;
                }
                return true;
            }
            else
            {
                // 主线程还是正常的，这种情况常见于打开了一个模态窗口
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    return false;
}