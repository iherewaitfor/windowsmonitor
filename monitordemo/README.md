- [Monitor Demo](#monitor-demo)
- [How to Run](#how-to-run)
- [涉及知识点](#涉及知识点)
  - [函数调用约定](#函数调用约定)
  - [线程启动](#线程启动)
  - [句柄复制](#句柄复制)
  - [detours hook](#detours-hook)
  - [WinodwsHook](#winodwshook)
  - [SetWindowLongPtr](#setwindowlongptr)
- [主线程逻辑](#主线程逻辑)
  - [使用Detour hook掉主线程的DispatchMessage](#使用detour-hook掉主线程的dispatchmessage)
  - [使用SetWindowsHookExA来hook掉GetMessage函数](#使用setwindowshookexa来hook掉getmessage函数)
- [子线程](#子线程)
  - [启动消息循环](#启动消息循环)
  - [检测卡顿](#检测卡顿)
  - [记录卡顿栈](#记录卡顿栈)
- [参考](#参考)


# Monitor Demo
本项目主要展示了如何监控 windows应用卡顿（主要指主线程卡顿）。发现有卡顿时，记录主线程的运行栈。

主要监控逻辑：
1. 使用Detour hook掉主线程的DispatchMessage
  - 在新的函数中命中过滤消息时，记录一个消息时间gLastActiveTime
2. 使用SetWindowsHookExA来hook掉GetMessage
   - 在新GetMessage函数中，每次主线程取消息时更新gLastActiveTime。
3. 启动子线程
   - 子线程中创建窗口，执行消息循环
   - 子线程定时检查gLastActiveTime与 现在的时长，
   - 如果时长超过阀值，子线程向主线程窗口发送WM_MOUSEMOVE消息。
   - 如果gLastActiveTime还是无变化 ，说明主线程已经卡住了。
# How to Run
安装好[cmake](https://cmake.org/download/)（可使用默认安装，选择把cmake自动加入到命令行）
cd build。然后运行以下命令
```
cmake .. -G "Visual Studio 17 2022" -A Win32
```
构建过程如下:
```
D:\srccode\windowsmonitor\monitordemo\build>cmake .. -G "Visual Studio 17 2022" -A Win32
CMake Deprecation Warning at CMakeLists.txt:2 (cmake_minimum_required):
  Compatibility with CMake < 2.8.12 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value or use a ...<max> suffix to tell
  CMake that the project does not need compatibility with older versions.


-- Selecting Windows SDK version 10.0.22621.0 to target Windows 10.0.19045.
-- The C compiler identification is MSVC 19.33.31630.0
-- The CXX compiler identification is MSVC 19.33.31630.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.33.31629/bin/Hostx64/x86/cl.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.33.31629/bin/Hostx64/x86/cl.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Configuring done
-- Generating done
-- Build files have been written to: D:/srccode/windowsmonitor/monitordemo/build

D:\srccode\windowsmonitor\monitordemo\build>
```
然后在build目录中打开生成的sln，把Demo项目设置为启动项，编译运行即可。


# 涉及知识点
## 函数调用约定
[调用约定](https://learn.microsoft.com/zh-cn/cpp/cpp/calling-conventions?view=msvc-170)
。其中[__stdcall](https://learn.microsoft.com/zh-cn/cpp/cpp/stdcall?view=msvc-170)可参见[https://learn.microsoft.com/zh-cn/cpp/cpp/stdcall?view=msvc-170](https://learn.microsoft.com/zh-cn/cpp/cpp/stdcall?view=msvc-170)
## 线程启动
[_beginthreadex](https://learn.microsoft.com/zh-cn/cpp/c-runtime-library/reference/beginthread-beginthreadex?view=msvc-170)
## 句柄复制
[DuplicateHandle](https://learn.microsoft.com/zh-cn/windows/win32/api/handleapi/nf-handleapi-duplicatehandle?redirectedfrom=MSDN)
## detours hook
可以参考[https://github.com/iherewaitfor/detoursdemo](https://github.com/iherewaitfor/detoursdemo)
## WinodwsHook
SetWindowsHookExA

CallNextHookEx

UnhookWindowsHookEx
## SetWindowLongPtr
# 主线程逻辑
主要监控主线程的消息获取与分发是否有正常进行。监控了特定的鼠标消息WM_NCLBUTTONDOWN。
```C++
volatile DWORD gLastActiveTime = 0;
```
每次有分发消息时更新gLastActiveTime。若卡顿了，gLastActiveTime会得不到及时更新，时间会与当前时间越来越大，从而检测到卡顿。

## 使用Detour hook掉主线程的DispatchMessage
```C++
void ThreadMonitor::hookDispatchMessage()
{
    DetourTransactionBegin();
    DetourUpdateThread(::GetCurrentThread());
    DetourAttach(&(PVOID&)gOriginDispatchMessageW, gMyDispatchMessageW);
    DetourTransactionCommit();
}
```

新的函数逻辑
```C++
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
```

有分发对应消息时，更新gLastActiveTime。

## 使用SetWindowsHookExA来hook掉GetMessage函数
```C++
    _getMessageId = SetWindowsHookExA(WH_GETMESSAGE, gMyGetMessage, NULL, gMainThreadId);
```
新的函数
```C++
LRESULT CALLBACK gMyGetMessage(int code, WPARAM wParam, LPARAM lParam)
{
    if (::GetCurrentThreadId() == gMainThreadId)
    {
        gLastActiveTime = GetTickCount();
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}
```
# 子线程

子线程的主要逻辑有
- 启动消息循环
- 定时检查gLastActiveTime
- 发现超时，PostThreadMessageA一条消息到主线程，等待一小会
- 再检查，若还是超时，说明主线程卡顿。

## 启动消息循环
```C++
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
```
使用SetWindowLongPtr指定了窗口的窗口过程。

同时使用SetTimer启动了定时器，定义间隔为DETECT_TIME。
```C++
SetTimer(_hwnd, 1, DETECT_TIME, NULL);
```
## 检测卡顿
```C++
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
```

## 记录卡顿栈
```C++
std::vector<DWORD64> StackWalkHelper::stackwalk64(HANDLE hThread)
{
    SuspendThread(hThread);
    _mainThread = (DWORD)hThread;
    _mainThreadSuspendTimePoint = ::GetTickCount();
    _mainThreadSuspend = 1;
    SetEvent(_wakeupEvent);
    std::vector<DWORD64> offset;
    CONTEXT c;
    memset(&c, 0, sizeof(c));
    c.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(hThread, &c) == FALSE)
    {
        ResumeThread(hThread);
        _mainThreadSuspend = 0;
        cout << "GetThreadContext failed";
        return offset;
    }

    STACKFRAME64 s;
    memset(&s, 0, sizeof(s));
    s.AddrPC.Offset = c.Eip;
    s.AddrPC.Mode = AddrModeFlat;
    s.AddrFrame.Offset = c.Ebp;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrStack.Offset = c.Esp;
    s.AddrStack.Mode = AddrModeFlat;

    int curRecursionCount = 0;
    cout << "start walk" <<endl;
    for (int frameNum = 0; frameNum < 60 && _stackWalkTimeout == 0; frameNum++)
    {
        if (_StackWalk64(IMAGE_FILE_MACHINE_I386, ::GetCurrentProcess(), hThread, &s, &c, myReadProcMem,
            _SymFunctionTableAccess64, _SymGetModuleBase64, NULL)
            != TRUE)
        {
            cout << "stackwalk64 failed";
            break;
        }
        if (s.AddrPC.Offset == s.AddrReturn.Offset)
        {
            if (curRecursionCount > 10)
            {
                cout << "recursion too many times";
                break;
            }
            curRecursionCount++;
        }
        else
        {
            curRecursionCount = 0;
        }
        if (s.AddrPC.Offset != 0)
        {
            offset.push_back(s.AddrPC.Offset);
        }

        if (s.AddrReturn.Offset == 0)
        {
            break;
        }
    }

    if (_stackWalkTimeout)
    {
        cout << "stack walk timeout" << endl;
        offset.clear();
    }
    cout << "walk complete" << endl;
    ResumeThread(hThread);
    _mainThreadSuspend = 0;
    return offset;
}
```
以上主要是通过[StackWalk64](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64)取得栈的每个栈帧的偏移地址的列表。
```C++
BOOL IMAGEAPI StackWalk64(
  [in]           DWORD                            MachineType,
  [in]           HANDLE                           hProcess,
  [in]           HANDLE                           hThread,
  [in, out]      LPSTACKFRAME64                   StackFrame,
  [in, out]      PVOID                            ContextRecord,
  [in, optional] PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
  [in, optional] PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
  [in, optional] PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
  [in, optional] PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress
);
```


每个栈帧的描述结构为STACKFRAME64：
```C++
typedef struct _tagSTACKFRAME64 {
    ADDRESS64   AddrPC;               // program counter
    ADDRESS64   AddrReturn;           // return address
    ADDRESS64   AddrFrame;            // frame pointer
    ADDRESS64   AddrStack;            // stack pointer
    ADDRESS64   AddrBStore;           // backing store pointer
    PVOID       FuncTableEntry;       // pointer to pdata/fpo or NULL
    DWORD64     Params[4];            // possible arguments to the function
    BOOL        Far;                  // WOW far call
    BOOL        Virtual;              // is this a virtual frame?
    DWORD64     Reserved[3];
    KDHELP64    KdHelp;
} STACKFRAME64, *LPSTACKFRAME64;

typedef enum {
    AddrMode1616,
    AddrMode1632,
    AddrModeReal,
    AddrModeFlat
} ADDRESS_MODE;

typedef struct _tagADDRESS64 {
    DWORD64       Offset;
    WORD          Segment;
    ADDRESS_MODE  Mode;
} ADDRESS64, *LPADDRESS64;
```

以下将偏移地址转换为模块和模块偏移地址。
```C++
void StackWalkHelper::fillContent(const std::vector<DWORD64>& offset, callstack::StackWalkResult* output)
{
    std::string chainId;
    for (size_t i = 0; i < offset.size(); ++i)
    {
        IMAGEHLP_MODULE64 module64;
        ZeroMemory(&module64, sizeof(module64));
        module64.SizeOfStruct = sizeof(module64);
        if ((_SymGetModuleInfo64(::GetCurrentProcess(), offset[i], &module64)))
        {
            callstack::StackFrame frame;
            frame.ImageName = getPathLast(module64.ImageName);
            frame.offset = (unsigned int)(offset[i] - module64.BaseOfImage);
            output->allFrame.push_back(frame);
            if (output->allImage.find(frame.ImageName) == output->allImage.end())
            {
                callstack::ImageInfo info;
                info.ImageName = frame.ImageName;
                info.CVData = getPathLast(module64.CVData);
                info.ImageSize = module64.ImageSize;
                if (module64.PdbSig70.Data1 == 0 && module64.PdbSig70.Data2 == 0 && module64.PdbSig70.Data3 == 0)
                {
                    info.PdbSig70 = readPdbSig70(info.ImageName, module64.BaseOfImage, module64.BaseOfImage + module64.ImageSize);
                }
                else
                {
                    info.PdbSig70 = callstack::Utils::toString(module64.PdbSig70, module64.PdbAge);
                }
                output->allImage[info.ImageName] = info;
            }
            if (chainId.empty() && !isSystemDll(frame.ImageName))
            {
                chainId = frame.ImageName + "[0x" + callstack::Utils::n2hexstr(frame.offset) + "]";
            }
        }
    }
    if (chainId.empty())
    {
        output->chainId = "null";
    }
    else
    {
        output->chainId = chainId;
    }
}
```

通过SymGetModuleInfo64，可以从偏移地址上获取到相关模块信息IMAGEHLP_MODULE。

```C++
BOOL IMAGEAPI SymGetModuleInfo64(
  [in]  HANDLE             hProcess,
  [in]  DWORD64            qwAddr,
  [out] PIMAGEHLP_MODULE64 ModuleInfo
);
```

```C++
typedef struct _IMAGEHLP_MODULE64 {
    DWORD    SizeOfStruct;           // set to sizeof(IMAGEHLP_MODULE64)
    DWORD64  BaseOfImage;            // base load address of module
    DWORD    ImageSize;              // virtual size of the loaded module
    DWORD    TimeDateStamp;          // date/time stamp from pe header
    DWORD    CheckSum;               // checksum from the pe header
    DWORD    NumSyms;                // number of symbols in the symbol table
    SYM_TYPE SymType;                // type of symbols loaded
    CHAR     ModuleName[32];         // module name
    CHAR     ImageName[256];         // image name
    CHAR     LoadedImageName[256];   // symbol file name
    // new elements: 07-Jun-2002
    CHAR     LoadedPdbName[256];     // pdb file name
    DWORD    CVSig;                  // Signature of the CV record in the debug directories
    CHAR     CVData[MAX_PATH * 3];   // Contents of the CV record
    DWORD    PdbSig;                 // Signature of PDB
    GUID     PdbSig70;               // Signature of PDB (VC 7 and up)
    DWORD    PdbAge;                 // DBI age of pdb
    BOOL     PdbUnmatched;           // loaded an unmatched pdb
    BOOL     DbgUnmatched;           // loaded an unmatched dbg
    BOOL     LineNumbers;            // we have line number information
    BOOL     GlobalSymbols;          // we have internal symbol information
    BOOL     TypeInfo;               // we have type information
    // new elements: 17-Dec-2003
    BOOL     SourceIndexed;          // pdb supports source server
    BOOL     Publics;                // contains public symbols
    // new element: 15-Jul-2009
    DWORD    MachineType;            // IMAGE_FILE_MACHINE_XXX from ntimage.h and winnt.h
    DWORD    Reserved;               // Padding - don't remove.
} IMAGEHLP_MODULE64, *PIMAGEHLP_MODULE64;
```

计算把偏移地址转换为模块偏移地址
```C++
      callstack::StackFrame frame;
      frame.ImageName = getPathLast(module64.ImageName);
      frame.offset = (unsigned int)(offset[i] - module64.BaseOfImage);
      output->allFrame.push_back(frame);
```


# 参考
[https://github.com/iherewaitfor/detoursdemo](https://github.com/iherewaitfor/detoursdemo)

[https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64)

[https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-stackframe](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-stackframe)

[https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-address](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-address)

[https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-address64](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-address64)

[https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/nf-dbghelp-symgetmoduleinfo64](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/nf-dbghelp-symgetmoduleinfo64)

[https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-imagehlp_module](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/ns-dbghelp-imagehlp_module)

[https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/nf-dbghelp-symfunctiontableaccess64](https://learn.microsoft.com/zh-cn/windows/win32/api/dbghelp/nf-dbghelp-symfunctiontableaccess64)