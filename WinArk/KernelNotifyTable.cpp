#include "stdafx.h"
#include "KernelNotifyTable.h"
#include "DriverHelper.h"
#include <PdbParser.h>
#include "Helpers.h"
#include <filesystem>

CKernelNotifyTable::CKernelNotifyTable(BarInfo& bars, TableInfo& table)
	:CTable(bars, table) {
	SetTableWindowInfo(bars.nbar);
	Refresh();
}

LRESULT CKernelNotifyTable::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return 0;
}

LRESULT CKernelNotifyTable::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lparam, BOOL& /*bHandled*/) {
	
	return 0;
}
LRESULT CKernelNotifyTable::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	PaintTable(m_hWnd);
	return 0;
}

LRESULT CKernelNotifyTable::OnHScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	Tablefunction(m_hWnd, uMsg, wParam, lParam);
	return 0;
}

LRESULT CKernelNotifyTable::OnVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	Tablefunction(m_hWnd, uMsg, wParam, lParam);
	return 0;
}

LRESULT CKernelNotifyTable::OnUserVabs(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	Tablefunction(m_hWnd, uMsg, wParam, lParam);
	return 0;
}

LRESULT CKernelNotifyTable::OnUserVrel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	Tablefunction(m_hWnd, uMsg, wParam, lParam);
	return 0;
}

LRESULT CKernelNotifyTable::OnUserChgs(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	Tablefunction(m_hWnd, uMsg, wParam, lParam);
	return 0;
}

LRESULT CKernelNotifyTable::OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
	return Tablefunction(m_hWnd, WM_VSCROLL, zDelta >= 0 ? 0 : 1, wParam);
}
LRESULT CKernelNotifyTable::OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}
LRESULT CKernelNotifyTable::OnLBtnDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}
LRESULT CKernelNotifyTable::OnLBtnUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}
LRESULT CKernelNotifyTable::OnRBtnDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}
LRESULT CKernelNotifyTable::OnUserSts(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}
LRESULT CKernelNotifyTable::OnWindowPosChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}
LRESULT CKernelNotifyTable::OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}
LRESULT CKernelNotifyTable::OnSysKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/) {
	return Tablefunction(m_hWnd, uMsg, wParam, lParam);
}

int CKernelNotifyTable::ParseTableEntry(CString& s, char& mask, int& select, CallbackInfo& info, int column) {
	// �ص�������ַ �ص����� ����ģ�� �ļ�����
	switch (column)
	{
		case 0:
			s.Format(L"0x%p", info.Routine);
			break;
		case 1:
		{
			switch (info.Type)
			{		
				case CallbackType::CreateProcessNotify:
					s = L"CreateProcess";
					break;
				case CallbackType::CreateThreadNotify:
					s = L"CreateThread";
					break;

				case CallbackType::LoadImageNotify:
					s = L"LoadImage";
					break;
			}
			break;
		}
			
		case 2:
			s = std::wstring(info.Module.begin(), info.Module.end()).c_str();
			break;
		case 3:
			s = info.Company.c_str();
			break;
	}

	return s.GetLength();
}

bool CKernelNotifyTable::CompareItems(const CallbackInfo& s1, const CallbackInfo& s2, int col, bool asc) {

	return false;
}

void CKernelNotifyTable::Refresh() {
	SymbolHandler handler(::GetCurrentProcess(), false);
	void* kernelBase = Helpers::GetKernelBase();
	CHAR path[MAX_PATH];
	::GetCurrentDirectoryA(MAX_PATH, path);
	std::string pdbPath = "\\Symbols";
	std::string name;
	pdbPath = path + pdbPath;

	for (auto& iter : std::filesystem::directory_iterator(pdbPath)) {
		auto filename = iter.path().filename().string();
		if (filename.find("ntk") != std::string::npos) {
			name = filename;
			break;
		}
	}
	std::string pdbFile = pdbPath + "\\" + name;
	handler.LoadSymbolsForModule(pdbFile, (DWORD64)kernelBase);

	ULONG count;
	ProcessNotifyCountData data;
	data.pCount = (PULONG)handler.GetSymbolAddressFromName("PspCreateProcessNotifyRoutineCount");
	data.pExCount = (PULONG)handler.GetSymbolAddressFromName("PspCreateProcessNotifyRoutineExCount");
	count = DriverHelper::GetProcessNotifyCount(&data);
	NotifyInfo info;
	info.Count = count;
	info.pRoutine = (void*)handler.GetSymbolAddressFromName("PspCreateProcessNotifyRoutine");
	
	wil::unique_virtualalloc_ptr<> buffer(::VirtualAlloc(nullptr, count * sizeof(void*), MEM_COMMIT, PAGE_READWRITE));
	
	KernelCallbackInfo* p = (KernelCallbackInfo*)buffer.get();
	p->Count = count;
	DriverHelper::EnumProcessNotify(&info, p);

	m_Table.data.info.clear();
	m_Table.data.n = 0;
	for (int i = 0; i < count; i++) {
		CallbackInfo info;
		info.Routine = p->Address[i];
		info.Type = CallbackType::CreateProcessNotify;
		info.Module = Helpers::GetModuleByAddress((ULONG_PTR)info.Routine);
		std::wstring path(info.Module.begin(), info.Module.end());
		info.Company = GetCompanyName(path);
		m_Table.data.info.push_back(std::move(info));
	}
	m_Table.data.n = m_Table.data.info.size();
}

std::wstring CKernelNotifyTable::GetCompanyName(std::wstring path) { 
	BYTE buffer[1 << 12];
	WCHAR* companyName = nullptr;
	if (::GetFileVersionInfo(path.c_str(), 0, sizeof(buffer), buffer)) {
		WORD* langAndCodePage;
		UINT len;
		if (::VerQueryValue(buffer, L"\\VarFileInfo\\Translation", (void**)&langAndCodePage, &len)) {
			CString text;
			text.Format(L"\\StringFileInfo\\%04x%04x\\CompanyName", langAndCodePage[0], langAndCodePage[1]);
			
			if (::VerQueryValue(buffer, text, (void**)&companyName, &len))
				return companyName;
		}
	}
	return companyName;
}

