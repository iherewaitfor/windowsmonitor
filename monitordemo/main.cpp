#include <Windows.h>
#include<commctrl.h>
#include <iostream>
#include <thread>
 
#include "detours.h"
#include "threadmonitor.h" 

HWND hwnd;
 
int timecount = 0;
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    TCHAR winTitle[MAX_PATH] = { 0 };
    switch (message) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        // MessageBox(hwnd, TEXT("关闭程序!"), TEXT("结束"), MB_OK | MB_ICONINFORMATION);
        PostQuitMessage(0);
        break;
    case WM_TIMER:
        timecount++;

        if (timecount == 10) {
            Sleep(10000); //制造主线程卡顿。
        }
        wsprintf(winTitle, TEXT("测试窗口。计时%d"), timecount);
        ::SetWindowText(hwnd, winTitle);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
 
int main(void) {
    static TCHAR szAppName[] = TEXT("TextWindow");
    MSG msg;
    WNDCLASS wndclass;
    // wndclass.cbSize = sizeof(WNDCLASSEX);
    wndclass.style = CS_HREDRAW | CS_VREDRAW;                         //窗口样式
    wndclass.lpszClassName = szAppName;                               //窗口类名
    wndclass.lpszMenuName = NULL;                                     //窗口菜单:无
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);    //窗口背景颜色
    wndclass.lpfnWndProc = WndProc;                                   //窗口处理函数
    wndclass.cbWndExtra = 0;                                          //窗口实例扩展:无
    wndclass.cbClsExtra = 0;                                          //窗口类扩展:无
    wndclass.hInstance = GetModuleHandle(0);                                   //窗口实例句柄
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);               //窗口最小化图标:使用缺省图标
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);                 //窗口采用箭头光标
    UnregisterClass(wndclass.lpszClassName, GetModuleHandle(0));
    if (!RegisterClass(&wndclass)) {    
        MessageBox(NULL, TEXT("窗口注册失败"), TEXT("错误"), MB_OK | MB_ICONERROR);
        return 0;
    }
 
    hwnd = CreateWindow(szAppName, TEXT("测试窗口"), WS_OVERLAPPEDWINDOW,
        0, 0, 500, 400, NULL, NULL, GetModuleHandle(0), NULL);
 
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetTimer(hwnd, 123, 1000, NULL); //定时器。
    ThreadMonitor::Instance()->init();
    std::vector<unsigned int> msgv;
    msgv.push_back(WM_NCLBUTTONDOWN);
    ThreadMonitor::Instance()->monitorMsg(msgv);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    std::cout << "Finish" << std::endl;
    getchar();
    return 0;
}