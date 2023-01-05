#include "fillstackresult.h"
#include <Psapi.h>
#include "peparser.h"

std::vector<HMODULE> enumModules();
bool findFrame(callstack::StackFrame& frame, unsigned long ptr, const std::map<std::string, callstack::ImageInfo>& imageInfos);
std::map<std::string, callstack::ImageInfo> getImageInfo(const std::vector<HMODULE>& modules);
bool isSystemDll(HMODULE module);

void FillStackResult::fill(const std::vector<DWORD64>& offset, callstack::StackWalkResult* output)
{
    std::vector<HMODULE> modules = enumModules();
    std::map<std::string, callstack::ImageInfo> imageInfo = getImageInfo(modules);

    for (size_t i = 0; i < offset.size(); ++i)
    {
        DWORD64 ptr = offset[i];
        callstack::StackFrame frame;
        if (findFrame(frame, (unsigned long)ptr, imageInfo))
        {
            output->allImage[frame.ImageName] = imageInfo[frame.ImageName];
            if (!::isSystemDll(imageInfo[frame.ImageName].module))
            {
                if (output->chainId.empty())
                {
                    output->chainId = frame.ImageName + "[0x" + callstack::Utils::n2hexstr(frame.offset) + "]";
                }
            }
        }
        output->allFrame.push_back(frame);
    }
    if (output->chainId.empty())
    {
        output->chainId = "null";
    }
}


std::vector<HMODULE> enumModules()
{
    std::vector<HMODULE> modules;
    int count = 100;
    bool first = true;
    while (true)
    {
        modules.resize(count);
        DWORD lpcbNeed = 0;
        BOOL v = EnumProcessModules(GetCurrentProcess(), &modules[0], modules.size() * sizeof(HMODULE), &lpcbNeed);
        if (!v)
        {
            modules.resize(0);
            return modules;
        }

        if (lpcbNeed > modules.size() * sizeof(HMODULE))
        {
            if (!first)
            {
                modules.resize(0);
                return modules;
            }
            first = false;
            count = lpcbNeed / sizeof(HMODULE);
            continue;
        }
        modules.resize(lpcbNeed / sizeof(HMODULE));
        break;
    }
    return modules;
}


std::string getModuleBaseName(HMODULE module)
{
    char name[100] = { 0 };
    DWORD len = GetModuleBaseNameA(GetCurrentProcess(), module, name, 100);
    if (len > 0 && len < 100)
    {
        name[len] = 0;
        return std::string(name);
    }

    std::vector<char> buf;
    buf.resize(1024);
    len = GetModuleFileNameA(module, &buf[0], buf.size());
    if (len > 0 && len < buf.size())
    {
        buf[len] = 0;
        std::string path = &buf[0];
        size_t np = path.find_last_of('\\');
        if (np == path.npos)
            np = path.find_last_of('/');
        if (np != path.npos)
        {
            return path.substr(np + 1);
        }
    }
    return "";
}

size_t getModuleSize(HMODULE module)
{
    MODULEINFO info = { 0 };
    if (GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        return info.SizeOfImage;

    return 0;
}


std::string makeGUID(const GUID& PdbSig70, DWORD PdbAge)
{
    std::string res = callstack::Utils::n2hexstr(PdbSig70.Data1) + callstack::Utils::n2hexstr(PdbSig70.Data2) + callstack::Utils::n2hexstr(PdbSig70.Data3);
    for (int i = 0; i < ARRAYSIZE(PdbSig70.Data4); ++i)
    {
        res += callstack::Utils::n2hexstr(PdbSig70.Data4[i]);
    }
    return res += callstack::Utils::n2hexstr(PdbAge, false);
}

bool getRSDSByModule(HMODULE module, callstack::ImageInfo& out)
{
#define CHECKADDR2(addr, size) if((ULONGLONG)addr<(ULONGLONG)module || (ULONGLONG)addr+size>(ULONGLONG)(module)+out.ImageSize) {\
    break;\
    }

    do
    {
        uintptr_t base_pointer = (uintptr_t)module;
        if (base_pointer == NULL)
        {
            break;
        }
        IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)base_pointer;
        IMAGE_FILE_HEADER* file_header = (IMAGE_FILE_HEADER*)(base_pointer + dos_header->e_lfanew + 4);
        CHECKADDR2(file_header, sizeof(IMAGE_FILE_HEADER));
        IMAGE_OPTIONAL_HEADER* opt_header = (IMAGE_OPTIONAL_HEADER*)(((char*)file_header) + sizeof(IMAGE_FILE_HEADER));
        CHECKADDR2(opt_header, sizeof(IMAGE_OPTIONAL_HEADER));
        IMAGE_DATA_DIRECTORY* dir = &opt_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        CHECKADDR2(dir, sizeof(IMAGE_DATA_DIRECTORY));
        IMAGE_DEBUG_DIRECTORY* dbg_dir = (IMAGE_DEBUG_DIRECTORY*)(base_pointer + dir->VirtualAddress);
        CHECKADDR2(dbg_dir, sizeof(IMAGE_DEBUG_DIRECTORY));
        if (IMAGE_DEBUG_TYPE_CODEVIEW != dbg_dir->Type)
        {
            break;
        }
        PdbInfo* pdb_info = (PdbInfo*)(base_pointer + dbg_dir->AddressOfRawData);
        if (0 != memcmp(&pdb_info->Signature, "RSDS", 4))
        {
            break;
        }
        out.PdbSig70 = makeGUID(pdb_info->Guid, pdb_info->Age);
        std::string pdbname = pdb_info->PdbFileName;
        size_t pos = pdbname.find_last_of('/');
        if (pos == pdbname.npos)
            pos = pdbname.find_last_of('\\');
        if (pos == pdbname.npos)
            out.CVData = pdbname;
        else
            out.CVData = pdbname.substr(pos + 1);
        return true;
    } while (false);
    return false;
}

bool getCVDataAndPdbSig70(HMODULE module, callstack::ImageInfo& out)
{
    if (getRSDSByModule(module, out))
        return true;
    return PeParser::getCVDataAndPdbSig70(module, out.CVData, out.PdbSig70);
}

std::map<std::string, callstack::ImageInfo> getImageInfo(const std::vector<HMODULE>& modules)
{
    std::map<std::string, callstack::ImageInfo> infos;
    for (size_t i = 0; i < modules.size(); ++i)
    {
        callstack::ImageInfo info;
        info.module = modules[i];
        info.ImageSize = getModuleSize(modules[i]);
        if (info.ImageSize == 0)
            continue;
        info.ImageName = getModuleBaseName(modules[i]);
        if (info.ImageName.empty())
            continue;
        if (!getCVDataAndPdbSig70(modules[i], info))
            continue;
        infos[info.ImageName] = info;
    }
    return infos;
}

bool isSystemDll(HMODULE module)
{
    std::wstring buf;
    buf.resize(1024);
    DWORD length = ::GetModuleFileNameW(module, (wchar_t*)buf.c_str(), buf.size() - 1);
    if (length > 0 && length < buf.size())
    {
        std::wstring str = L"cC::\\/wWiInNdDoOwWsS\\/";
        std::wstring path = buf;
        if (path.length() < str.length())
            return false;
        for (size_t i = 0; i < str.size() / 2; ++i)
        {
            if (path[i] != str[i * 2] && path[i] != str[i * 2 + 1])
                return false;
        }
        return true;
    }
    return false;
}

bool findFrame(callstack::StackFrame& frame, unsigned long ptr, const std::map<std::string, callstack::ImageInfo>& imageInfos)
{
    for (std::map<std::string, callstack::ImageInfo>::const_iterator pos = imageInfos.begin(); pos != imageInfos.end(); ++pos)
    {
        unsigned long b = (unsigned long)pos->second.module;
        unsigned long e = b + pos->second.ImageSize;
        if (ptr >= b && ptr <= e)
        {
            frame.ImageName = pos->first;
            frame.offset = ptr - b;
            return true;
        }
    }
    frame.ImageName = "UNKNOW";
    frame.offset = ptr;
    return false;
}