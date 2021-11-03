#include "stdafx.h"
#include "resource.h"
#include "KernelHookView.h"

LRESULT CKernelHookView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	CRect r(0, 0, 400, 25);
	CTabCtrl tabCtrl;
	auto hTabCtrl = tabCtrl.Create(m_hWnd, &r, nullptr, WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS
		| TCS_HOTTRACK | TCS_SINGLELINE | TCS_RIGHTJUSTIFY | TCS_TABS,
		WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY,TabId);
	m_TabCtrl.SubclassWindow(hTabCtrl);
	
	//m_TabCtrl.SetFont()

	struct {
		PCWSTR Name;
	}columns[] = {
		L"SSDT",
		L"Shadow SSDT",
		L"Object Callback"
	};

	int i = 0;
	for (auto& col : columns) {
		m_TabCtrl.InsertItem(i++, col.Name);
	}

	InitSSDTHookTable();

	return 0;
}

LRESULT CKernelHookView::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	CRect rc;
	GetClientRect(&rc);
	int width = rc.Width();
	int clientHeight = rc.Height();
	m_TabCtrl.GetClientRect(&rc);
	int height = rc.Height();
	::MoveWindow(m_TabCtrl.m_hWnd, rc.left, rc.top, width, height, true);

	int iX = rc.left;
	int iY = rc.top + height;
	clientHeight -= height;
	if(_index==0)
		::MoveWindow(m_hwndArray[_index], iX, iY, width, clientHeight, true);

	bHandled = false;
	return 0;
}

void CKernelHookView::InitSSDTHookTable() {
	BarDesc bars[] = {
		{19,"�����",0},
		{25,"��������",0},
		{20,"ԭʼ������ַ",0},
		{10,"Hook ����",0},
		{20,"��ǰ������ַ",0},
		{260,"��ǰ������ַ����ģ��",0}
	};

	TableInfo table = {
		1,1,TABLE_SORTMENU | TABLE_COPYMENU | TABLE_APPMENU,9,0,0,0
	};

	BarInfo info;
	info.nbar = _countof(bars);
	info.font = 9;
	for (int i = 0; i < info.nbar; i++) {
		info.bar[i].defdx = bars[i].defdx;
		info.bar[i].mode = bars[i].mode;
		info.bar[i].name = bars[i].name;
	}

	m_SSDTHookTable = new CSSDTHookTable(info, table);
	RECT rect;
	::GetClientRect(m_TabCtrl.m_hWnd, &rect);
	int height = rect.bottom - rect.top;
	GetClientRect(&rect);
	rect.top += height;
	rect.bottom -= height;
	m_SSDTHookTable->Create(m_hWnd, rect, nullptr, WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_BORDER | WS_EX_LAYERED);
	m_hwndArray[static_cast<int>(TabColumn::SSDT)] = m_SSDTHookTable->m_hWnd;
	m_SSDTHookTable->ShowWindow(SW_SHOW);
}

LRESULT CKernelHookView::OnTcnSelChange(int, LPNMHDR hdr, BOOL&) {
	int index = 0;
	
	index = m_TabCtrl.GetCurSel();
	m_SSDTHookTable->ShowWindow(SW_HIDE);

	switch (static_cast<TabColumn>(index)) {
		case TabColumn::SSDT:
			m_SSDTHookTable->ShowWindow(SW_SHOW);
			break;
	}
	::PostMessage(m_hWnd, WM_SIZE, 0, 0);
	_index = index;
	return 0;
}