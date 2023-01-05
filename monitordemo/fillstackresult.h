#pragma once

#include <windows.h>
#include "callstackdef.h"

struct PdbInfo
{
    DWORD     Signature;
    GUID      Guid;
    DWORD     Age;
    char      PdbFileName[1];
};

class FillStackResult
{
public:
    static void fill(const std::vector<DWORD64>& offset, callstack::StackWalkResult* result);
};
