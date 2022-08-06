// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "View.h"
#include "DeviceManagerView.h"
#include "CommandManager.h"
#include "ProcessTable.h"
#include "NetworkTable.h"
#include "KernelModuleTable.h"
#include "DriverTable.h"
#include "ServiceTable.h"
#include "WindowsView.h"
#include "KernelHookView.h"
#include "KernelView.h"
#include "SystemConfigDlg.h"
#include "Interfaces.h"
#include "EtwView.h"
#include "TraceManager.h"
#include "QuickFindDlg.h"


// c2061 ��һ���໹ûʵ��ǰ���ͻ��ཻ��ʹ�ã�ǰ���������ܽ��
enum class TabColumn :int {
	Process, KernelModule, 
	Kernel, 
	KernelHook,
	Network,Driver,Registry,Device,Windows,Service,Config,Etw
};

DEFINE_ENUM_FLAG_OPERATORS(TabColumn);

class CMainFrame : 
	public CFrameWindowImpl<CMainFrame>,
	public CAutoUpdateUI<CMainFrame>,
	public CMessageFilter, 
	public CIdleHandler,
	public IEtwFrame,
	public IQuickFind
{
public:
	DECLARE_FRAME_WND_CLASS(nullptr,IDR_MAINFRAME)

	CMainFrame() :m_TabCtrl(this) {};
	// ����������������ͣ�������ڹ��캯���г�ʼ��

	const UINT TabId = 1234;

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();
	// Inherited via IQuickFind
	void DoFind(PCWSTR text, const QuickFindOptions& options) override;
	void WindowClosed() override;

	// Inherited via IEtwFrame
	BOOL TrackPopupMenu(HMENU hMenu, HWND hWnd, POINT* pt = nullptr, UINT flags = 0) override;
	HFONT GetMonoFont() override;
	void ViewDestroyed(void*) override;
	TraceManager& GetTraceManager() override;
	BOOL SetPaneText(int index, PCWSTR text) override;
	BOOL SetPaneIcon(int index, HICON hIcon) override;
	CUpdateUIBase* GetUpdateUI() override;

	void UpdateUI();

	void SetStartKey(const CString& key);
	void SetStatusText(PCWSTR text);

	CUpdateUIBase* GetUIUpdate() {
		return this;
	}

	UINT TrackPopupMenu(CMenuHandle menu, int x, int y);

	bool CanPaste() const;

	LRESULT OnTcnSelChange(int /*idCtrl*/, LPNMHDR hdr, BOOL& /*bHandled*/);

	void CloseDialog(int nVal);
	void OnGetMinMaxInfo(LPMINMAXINFO lpMMI);

	void InitProcessTable();
	void InitNetworkTable();
	void InitKernelModuleTable();
	void InitDriverTable();
	void InitServiceTable();
	void InitDriverInterface();
	void InitRegistryView();
	void InitDeviceView();
	void InitWindowsView();
	void InitKernelHookView();
	void InitKernelView();
	void InitConfigView();
	void InitEtwView();

	BEGIN_MSG_MAP_EX(CMainFrame)
		MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_CLOSE,OnClose)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(ID_MONITOR_START, OnMonitorStart)
		COMMAND_ID_HANDLER(ID_MONITOR_STOP, OnMonitorStop)
		COMMAND_ID_HANDLER(ID_MONITOR_PAUSE, OnMonitorPause)
		COMMAND_ID_HANDLER(ID_SEARCH_QUICKFIND,OnQuickFind)
		NOTIFY_HANDLER(TabId, TCN_SELCHANGE, OnTcnSelChange)
		CHAIN_MSG_MAP(CAutoUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		COMMAND_RANGE_HANDLER(0x8000,0xefff,OnForwardToActiveView)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()
public:
	CCommandBarCtrl m_CmdBar;

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
public:

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnMonitorStop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMonitorPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnMonitorStart(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnForwardToActiveView(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnQuickFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void InitProcessToolBar(CToolBarCtrl& tb);
	void InitEtwToolBar(CToolBarCtrl& tb, int size = 24);
	void ClearToolBarButtons(CToolBarCtrl& tb);
	void InitCommandBar();
	void InitRegToolBar(CToolBarCtrl& tb, int size = 24);

private:
	CContainedWindowT<CTabCtrl> m_TabCtrl;
	// CTabCtrl m_TabCtrl;
	CToolBarCtrl m_tb;
	
	CProcessTable* m_pProcTable{ nullptr };
	CNetwrokTable* m_pNetTable{ nullptr };
	CKernelModuleTable* m_pKernelModuleTable{ nullptr };
	CDriverTable* m_pDriverTable{ nullptr };
	CServiceTable* m_pServiceTable{ nullptr };
	
	CMultiPaneStatusBarCtrl m_StatusBar;

	CRegistryManagerView m_RegView;
	CDeviceManagerView m_DevView;
	CWindowsView m_WinView;
	CKernelHookView m_KernelHookView;
	CKernelView m_KernelView;

	CSystemConfigDlg m_SysConfigView;

	CEtwView* m_pEtwView{ nullptr };
	TraceManager m_tm;
	CIcon m_RunIcon, m_StopIcon, m_PauseIcon;
	CFont m_MonoFont;
	CQuickFindDlg* m_pQuickFindDlg{ nullptr };

	CString m_StatusText;

	// table array
	HWND m_hwndArray[16];
	// current select tab
	int _index = 0;

	CommandManager m_CmdMgr;
	CEdit m_Edit;
	bool m_AllowModify{ true };
};
