#pragma once

#include <windows.h>
#include <vector>


class ThreadMonitor
{
public:
    static ThreadMonitor* Instance();
    ~ThreadMonitor();
    void monitorMsg(const std::vector<unsigned int>& msg);
    void init();
    void release();

    void startMonitor(unsigned int message);
    void stopMonitor();
private:
    ThreadMonitor();
    void hookDispatchMessage();
    void unHookDispatchMessage();
    static unsigned int __stdcall MonitorThreadRoutine(LPVOID param);
    void run();
    static LRESULT CALLBACK WindowRoutine(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
    LRESULT process(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
    bool shouldReport(DWORD now, DWORD entryTimePoint);

private:
    HANDLE _monitorThread;
    HWND _hwnd;
    int _entryTimes;
    volatile int _message;
    volatile int _entryTimePoint;
    bool _init;
    unsigned long _lastBlockEntryTimePoint;
    volatile int _exitNow;
    HHOOK _getMessageId;
};
