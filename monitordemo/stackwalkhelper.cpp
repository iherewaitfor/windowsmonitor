#include "stackwalkhelper.h"
#include <DbgHelp.h>
#include <set>
#include <process.h>
#include "fillstackresult.h"
#include <iostream>
using namespace std;
StackWalkHelper* StackWalkHelper::Instance()
{
    static StackWalkHelper instance;
    return &instance;
}

StackWalkHelper::StackWalkHelper()
{
    _init = 0;
    _exitNow = 0;
    _stackWalkTimeout = 0;
    _checkThread = NULL;
    _wakeupEvent = NULL;
    _mainThreadSuspend = 0;
    _mainThreadSuspendTimePoint = 0;
    _mainThread = NULL;
}

StackWalkHelper::~StackWalkHelper()
{
    _exitNow = 1;
    if (_checkThread != NULL)
    {
        SetEvent(_wakeupEvent);
        WaitForSingleObject(_checkThread, INFINITE);
        ::CloseHandle(_checkThread);
        _checkThread = NULL;
    }
    if (_wakeupEvent != NULL)
    {
        ::CloseHandle(_wakeupEvent);
        _wakeupEvent = NULL;
    }
    if (_init == 1)
    {
        _SymCleanup(::GetCurrentProcess());
        _init = 0;
    }
    if (_hDbgHelp)
    {
        FreeLibrary(_hDbgHelp);
        _hDbgHelp = NULL;
    }
}

int StackWalkHelper::init()
{
    _wakeupEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (_wakeupEvent == NULL)
    {
        cout << "create event failed" << endl;
        return -1;
    }
    _checkThread = (HANDLE)_beginthreadex(NULL, 0, &StackWalkHelper::checkThreadRoutine, (void*)this, 0, NULL);
    if (_checkThread == NULL)
    {
        cout << "create thread failed" << endl;
        return -1;
    }
    _hDbgHelp = ::LoadLibraryW(L"dbghelp.dll");
    if (_hDbgHelp == NULL)
    {
        cout << "loadlibrary dbghelp.dll failed: " << ::GetLastError() << endl;
        return -1;
    }

    _SymInitialize = (func_SymInitialize)GetProcAddress(_hDbgHelp, "SymInitialize");
    _SymCleanup = (func_SymCleanup)GetProcAddress(_hDbgHelp, "SymCleanup");
    _StackWalk64 = (func_StackWalk64)GetProcAddress(_hDbgHelp, "StackWalk64");
    _SymSetOptions = (func_SymSetOptions)GetProcAddress(_hDbgHelp, "SymSetOptions");
    _SymFunctionTableAccess64 = (func_SymFunctionTableAccess64)GetProcAddress(_hDbgHelp, "SymFunctionTableAccess64");
    _SymGetModuleBase64 = (func_SymGetModuleBase64)GetProcAddress(_hDbgHelp, "SymGetModuleBase64");
    _SymGetModuleInfo64 = (func_SymGetModuleInfo64)GetProcAddress(_hDbgHelp, "SymGetModuleInfo64");

    if (_SymInitialize == NULL ||
        _SymCleanup == NULL ||
        _StackWalk64 == NULL ||
        _SymSetOptions == NULL ||
        _SymFunctionTableAccess64 == NULL ||
        _SymGetModuleInfo64 == NULL ||
        _SymGetModuleBase64 == NULL
        )
    {
        cout << "GetProcAddress failed" << endl;
        return -1;
    }

    BOOL res = _SymInitialize(::GetCurrentProcess(), "./", TRUE);
    if (res == FALSE)
    {
        cout <<  "SymInitializeW failed" << endl;
        return -1;
    }
    _SymSetOptions(NULL);
    cout << "SymInitizlize success" << endl;
    return 1;
}

BOOL __stdcall myReadProcMem(HANDLE hProcess, DWORD64 qwBaseAddress, PVOID lpBuffer, DWORD nSize,
                             LPDWORD lpNumberOfBytesRead)
{
    SIZE_T st;
    BOOL bRet = ReadProcessMemory(hProcess, (LPVOID)qwBaseAddress, lpBuffer, nSize, &st);
    *lpNumberOfBytesRead = (DWORD)st;
    return bRet;
}

std::string getPathLast(char* path)
{
    std::string p = path;
    size_t index = p.rfind('\\');
    if (index == p.npos)
        return p;
    return p.substr(index + 1);
}

bool StackWalkHelper::stackWalkOtherThread(HANDLE hThread, unsigned int entryTimePoint1, volatile int& entryTimePoint2,callstack::StackWalkResult* output)
{
    cout <<  "_init=" << _init  << endl;
    if (_init != 1)
    {
        if (_init == -1)
        {
            //ReportFreqCtrl::Instance()->setForceNowAllow();
            return false;
        }
        _init = init();
        if (_init != 1)
        {
            //ReportFreqCtrl::Instance()->setForceNowAllow();
            return false;
        }
    }

    if (_stackWalkTimeout)
    {
        cout << "stackwalk timeout" << endl;
        //ReportFreqCtrl::Instance()->setForceNowAllow();
        return false;
    }

    if (entryTimePoint1 != entryTimePoint2)
    {
        cout << "timepoint not equal" << endl;
        return false;
    }
    std::vector<DWORD64> offset = stackwalk64(hThread);
    if (offset.empty())
    {
        cout << "offset empty" << endl;
        return false;
    }

    fillContent(offset, output);
    //FillStackResult::fill(offset, output);
    cout << "fillContent complete" << endl;

    if (isPdbSig70Unknow(output))
    {
        cout << "pdbsig70 unknow" << endl;
        return false;
    }

    return true;
}

std::string toUpperCase(std::string str)
{
    for (size_t i = 0; i < str.size(); ++i)
        str[i] = toupper(str[i]);
    return str;
}
bool StackWalkHelper::isSystemDll(const std::string& imageName)
{
    if (_systemDll.find(imageName) == _systemDll.end())
    {
        _systemDll[imageName] = false;
        size_t pos = imageName.find_last_of('.');
        if (pos != imageName.npos && pos > 0)
        {
            std::string dll = imageName.substr(0, pos);
            HMODULE m = ::GetModuleHandleA(dll.c_str());
            std::wstring buf;
            buf.resize(1024);
            DWORD length = ::GetModuleFileNameW(m, (wchar_t*)buf.c_str(), buf.size() - 1);
            if (length > 0 && length < buf.size())
            {
                if (buf.find(L"C:\\Windows\\") != std::string::npos || buf.find(L"C:/Windows/") != std::string::npos ||
                    buf.find(L"c:\\windows\\") != std::string::npos || buf.find(L"w:/Windows/") != std::string::npos
                    )
                    _systemDll[imageName] = true;
            }
        }
    }

    return _systemDll[imageName];
}

bool startsWith(const std::string& str, const std::string prefix) {
    return (str.rfind(prefix, 0) == 0);
}

bool endsWith(const std::string& str, const std::string suffix) {
    if (suffix.length() > str.length()) { return false; }

    return (str.rfind(suffix) == (str.length() - suffix.length()));
}
std::string StackWalkHelper::readPdbSig70(const std::string& imageName, ULONGLONG base, ULONGLONG end)
{
#define CHECKADDR(addr, size) if((ULONGLONG)addr<base || (ULONGLONG)addr+size>end) {\
cout<<"ADDR check error: addr="<<addr<<", base="<<base<<", addr+size="<<(addr+size)<<", end="<<end;\
    break;\
    }

    do
    {
        std::string dll;
        if (endsWith(imageName,".exe")&& endsWith(imageName, ".EXE") && endsWith(imageName, ".dll") && endsWith(imageName,".DLL"))
        {
            size_t pos = imageName.find_last_of('.');
            if (pos == imageName.npos || pos <= 0)
            {
                cout << "imageName error:" << imageName;
                break;
            }
            dll = imageName.substr(0, pos);
        }
        else
        {
            dll = imageName;
        }
        uintptr_t base_pointer = (uintptr_t)GetModuleHandleA(dll.c_str());
        if (base_pointer == NULL)
        {
            cout << "Getmodulehandle null:" << imageName;
            break;
        }
        IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)base_pointer;
        IMAGE_FILE_HEADER* file_header = (IMAGE_FILE_HEADER*)(base_pointer + dos_header->e_lfanew + 4);
        CHECKADDR(file_header, sizeof(IMAGE_FILE_HEADER));
        IMAGE_OPTIONAL_HEADER* opt_header = (IMAGE_OPTIONAL_HEADER*)(((char*)file_header) + sizeof(IMAGE_FILE_HEADER));
        CHECKADDR(opt_header, sizeof(IMAGE_OPTIONAL_HEADER));
        IMAGE_DATA_DIRECTORY* dir = &opt_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        CHECKADDR(dir, sizeof(IMAGE_DATA_DIRECTORY));
        IMAGE_DEBUG_DIRECTORY* dbg_dir = (IMAGE_DEBUG_DIRECTORY*)(base_pointer + dir->VirtualAddress);
        CHECKADDR(dbg_dir, sizeof(IMAGE_DEBUG_DIRECTORY));
        if (IMAGE_DEBUG_TYPE_CODEVIEW != dbg_dir->Type)
        {
            cout << "type error: " << imageName << ", type=" << dbg_dir->Type;
            break;
        }
        PdbInfo* pdb_info = (PdbInfo*)(base_pointer + dbg_dir->AddressOfRawData);
        if (0 != memcmp(&pdb_info->Signature, "RSDS", 4))
        {
            cout << "RSDS error:" << imageName;
            break;
        }
        return callstack::Utils::toString(pdb_info->Guid, pdb_info->Age);
    }while (false);
    return "UNKNOW";
}

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

unsigned int __stdcall StackWalkHelper::checkThreadRoutine(LPVOID param)
{
    StackWalkHelper* helper = (StackWalkHelper*)param;
    helper->checkThreadRun();
    return 0;
}

void StackWalkHelper::checkThreadRun()
{
    while (!_exitNow)
    {
        if (_mainThreadSuspend)
        {
            DWORD now = ::GetTickCount();
            DWORD pre = _mainThreadSuspendTimePoint;
            if (now - pre >= 1000)
            {
                _stackWalkTimeout = 1;
                DWORD mainThread = _mainThread;
                ResumeThread((HANDLE)mainThread);
                _mainThreadSuspend = 0;
                cout << "main thread suspend > 2s, resume it";
            }
            WaitForSingleObject(_wakeupEvent, 100);
        }
        else
        {
            WaitForSingleObject(_wakeupEvent, 2000);
        }
    }
}

bool StackWalkHelper::isPdbSig70Unknow(callstack::StackWalkResult* output)
{
    if (output->allImage.empty())
        return true;
    for (std::map<std::string, callstack::ImageInfo>::const_iterator pos = output->allImage.begin();
        pos != output->allImage.end(); ++pos)
    {
        if (pos->second.PdbSig70 == "UNKNOW")
            return true;
    }

    return false;
}
