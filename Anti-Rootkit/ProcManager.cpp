#include"ProcManager.h"

PEPROCESS LookupProcess(HANDLE Pid) {
	PEPROCESS eprocess = nullptr;
	if (NT_SUCCESS(PsLookupProcessByProcessId(Pid, &eprocess)))
		return eprocess;
	else
		return NULL;
}

PETHREAD LookupThread(HANDLE Tid) {
	PETHREAD ethread;
	if (NT_SUCCESS(PsLookupThreadByThreadId(Tid, &ethread)))
		return ethread;
	else
		return NULL;
}

void EnumThread(PEPROCESS Process) {
	ULONG i = 0;
	PETHREAD ethrd = nullptr;
	PEPROCESS eproc = nullptr;
	for (i = 4; i < 262144; i = i + 4) {	// �����PID��TID��Χ�е����⣬�Ժ�����
		ethrd = LookupThread((HANDLE)i);
		if (ethrd != NULL) {
			eproc = IoThreadToProcess(ethrd);
			if (eproc == Process) {
				KdPrint(("[THREAD] ETHREAD=%p TID=%ld\n", 
					ethrd, PsGetThreadId(ethrd)));
			}
			ObDereferenceObject(ethrd);
		}
	}
}


void EnumModule(PEPROCESS Process) {
	UNREFERENCED_PARAMETER(Process);
}

//ö�ٽ���
void EnumProcess() {
	ULONG i = 0;
	PEPROCESS eproc = NULL;
	for (i = 4; i < 262144; i+=4) {
		eproc = LookupProcess((HANDLE)i);
		if (eproc != NULL) {
			ObDereferenceObject(eproc);
			KdPrint(("EPROCESS=%p PID=%ld PPID=%ld Name=%s\n",
				eproc, PsGetProcessId(eproc),
				PsGetProcessInheritedFromUniqueProcessId(eproc),
				PsGetProcessImageFileName(eproc)));
			//EnumThread(eproc);
			KdPrint(("\n"));
		}
	}
}

//���淽����������
void ZwKillProcess(HANDLE Pid) {
	HANDLE hProcess = NULL;
	CLIENT_ID ClientId;
	OBJECT_ATTRIBUTES oa = { 0 };
	ClientId.UniqueProcess = Pid;
	ClientId.UniqueThread = 0;
	oa.Length = sizeof(oa);
	ZwOpenProcess(&hProcess, 1, &oa, &ClientId);
	if (hProcess) {
		ZwTerminateProcess(hProcess,0);
		ZwClose(hProcess);
	}
}

//���淽�������߳�
void ZwKillThread(HANDLE Tid) {
	HANDLE hThread = NULL;
	CLIENT_ID ClientId;
	OBJECT_ATTRIBUTES oa = { 0 };
	ClientId.UniqueProcess = 0;
	ClientId.UniqueThread = Tid;
	oa.Length = sizeof(oa);
	ZwOpenThread(&hThread, 1, &oa, &ClientId);
	if (hThread) {
		// δ��������
		//ZwTerminateThread(hThread, 0);
		ZwClose(hThread);
	}
}