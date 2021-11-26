#pragma once

#include "SymbolHandler.h"

// ����
class SymbolManager {
public:
	static SymbolManager& Get();
	SymbolHandler* GetCommon();
	SymbolHandler* GetForProcess(DWORD pid);

	std::unique_ptr<SymbolInfo> GetSymbolFromAddress(DWORD pid, DWORD64 address, PDWORD64 offset = nullptr);

private:

	SymbolManager();

	SymbolHandler _commonSymbols;
	std::unordered_map<DWORD, std::unique_ptr<SymbolHandler>> _procSymbols;
};