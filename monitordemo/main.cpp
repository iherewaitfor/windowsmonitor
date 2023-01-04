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
        // MessageBox(hwnd, TEXT("�رճ���!"), TEXT("����"), MB_OK | MB_ICONINFORMATION);
        PostQuitMessage(0);
        break;
    case WM_TIMER:
        timecount++;

        if (timecount == 10) {
            Sleep(10000); //�������߳̿��١�
        }
        wsprintf(winTitle, TEXT("���Դ��ڡ���ʱ%d"), timecount);
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
    wndclass.style = CS_HREDRAW | CS_VREDRAW;                         //������ʽ
    wndclass.lpszClassName = szAppName;                               //��������
    wndclass.lpszMenuName = NULL;                                     //���ڲ˵�:��
    wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);    //���ڱ�����ɫ
    wndclass.lpfnWndProc = WndProc;                                   //���ڴ�����
    wndclass.cbWndExtra = 0;                                          //����ʵ����չ:��
    wndclass.cbClsExtra = 0;                                          //��������չ:��
    wndclass.hInstance = GetModuleHandle(0);                                   //����ʵ�����
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);               //������С��ͼ��:ʹ��ȱʡͼ��
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);                 //���ڲ��ü�ͷ���
    UnregisterClass(wndclass.lpszClassName, GetModuleHandle(0));
    if (!RegisterClass(&wndclass)) {    
        MessageBox(NULL, TEXT("����ע��ʧ��"), TEXT("����"), MB_OK | MB_ICONERROR);
        return 0;
    }
 
    hwnd = CreateWindow(szAppName, TEXT("���Դ���"), WS_OVERLAPPEDWINDOW,
        0, 0, 500, 400, NULL, NULL, GetModuleHandle(0), NULL);
 
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetTimer(hwnd, 123, 1000, NULL); //��ʱ����
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