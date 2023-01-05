#pragma once

#include <string>
#include <windows.h>

class PeParser
{
public:
	static bool getCVDataAndPdbSig70(HMODULE module, std::string& pdbname, std::string& pdbsig70);
};
