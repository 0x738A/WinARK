#include "stdafx.h"
#include "BypassDlg.h"
#include <DriverHelper.h>

LRESULT CBypassDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_BypassKDBG.Attach(GetDlgItem(IDC_BYPASS_KDBG));
	return TRUE;
}

LRESULT CBypassDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	int checkCode = m_BypassKDBG.GetCheck();
	DWORD flag = 0;
	if (checkCode == BST_CHECKED) {
		flag |= BYPASS_KERNEL_DEBUGGER;
	}
	if (m_enable) {
		DriverHelper::Unbypass(flag);
	}
	return TRUE;
}

LRESULT CBypassDlg::OnBypass(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int checkCode = m_BypassKDBG.GetCheck();
	DWORD flag = 0;
	if (checkCode == BST_CHECKED) {
		flag |= BYPASS_KERNEL_DEBUGGER;
	}

	if (flag == 0) {
		AtlMessageBox(m_hWnd, L"δѡ���κ��ƹ���", L"��������", MB_ICONERROR);
		return FALSE;
	}
	if (m_enable) {
		bool ok = DriverHelper::Unbypass(flag);
		if (ok) {
			SetDlgItemText(IDC_BYPASS, L"���ü���ƹ�");
			m_enable = false;
		}
		else {
			AtlMessageBox(m_hWnd, L"����ʧ��");
		}
	}
	else {
		bool ok = DriverHelper::Bypass(flag);
		if (ok) {
			SetDlgItemText(IDC_BYPASS, L"���ü���ƹ�");
			m_enable = true;
		}
		else {
			AtlMessageBox(m_hWnd, L"����ʧ��");
		}
	}

	return TRUE;
}