#include <ntifs.h>
#include <intrin.h>
// get errors for deprecated functions
#include <dontuse.h>
#include <TraceLoggingProvider.h>
#include <evntrace.h>
#include"util.h"
#include"AntiRootkit.h"
#include"RegManager.h"
#include"ProcManager.h"
#include "..\KernelLibrary\khook.h"
#include "..\KernelLibrary\SysMon.h"
#include "..\KernelLibrary\Logging.h"



// define a tag (because of little endianess, viewed in PoolMon as 'arkv'
#define DRIVER_TAG 'vkra'


// {86F32D72-5D88-49D2-A44D-F632FF240C06}
TRACELOGGING_DEFINE_PROVIDER(g_Provider, "AntiRootkit",\
	(0x86f32d72, 0x5d88, 0x49d2, 0xa4, 0x4d, 0xf6, 0x32, 0xff, 0x24, 0xc, 0x6));

// PiUpdateDriverDBCache RtlInsertElementGenericTableAvl ����ȷ���ýṹ��Ĵ�С
// �˽ṹx86 x64ͨ��
typedef struct _PiDDBCacheEntry
{
	LIST_ENTRY		List;
	UNICODE_STRING	DriverName;
	ULONG			TimeDateStamp;
	NTSTATUS		LoadStatus;
	char			_0x0028[16]; // data from the shim engine, or uninitialized memory for custom drivers
} PiDDBCacheEntry, * PPiDDBCacheEntry;

typedef struct _UNLOADED_DRIVERS {
	UNICODE_STRING Name;
	PVOID StartAddress;
	PVOID EndAddress;
	LARGE_INTEGER CurrentTime;
}UNLOADED_DRIVER, * PUNLOADED_DRIVER;


// ��������
#ifdef _WIN64
	// 64λ����
#else
	// 32λ����
#endif

UNICODE_STRING g_RegisterPath;
PDEVICE_OBJECT g_DeviceObject;

DRIVER_UNLOAD AntiRootkitUnload;
DRIVER_DISPATCH AntiRootkitDeviceControl, AntiRootkitCreateClose;
DRIVER_DISPATCH AntiRootkitRead, AntiRootkitWrite, AntiRootkitShutdown;


extern "C" NTSTATUS NTAPI ZwQueryInformationProcess(
	_In_ HANDLE ProcessHandle,
	_In_ PROCESSINFOCLASS ProcessInformationClass,
	_Out_writes_bytes_(ProcessInformationLength) PVOID ProcessInformation,
	_In_ ULONG ProcessInformationLength,
	_Out_opt_ PULONG ReturnLength
);

NTSYSCALLAPI
NTSTATUS
NTAPI
NtReadVirtualMemory(
	_In_ HANDLE ProcessHandle,
	_In_opt_ PVOID BaseAddress,
	_Out_writes_bytes_(BufferSize) PVOID Buffer,
	_In_ SIZE_T BufferSize,
	_Out_opt_ PSIZE_T NumberOfBytesRead
);

NTKERNELAPI
NTSTATUS
NTAPI
ObReferenceObjectByName(
	_In_ PUNICODE_STRING ObjectName,
	_In_ ULONG Attributes,
	_In_opt_ PACCESS_STATE AccessState,
	_In_ POBJECT_TYPE ObjectType,
	_In_ KPROCESSOR_MODE AccessMode,
	_Inout_opt_ PVOID ParseContext,
	_Out_ PVOID *Object
);

// �����߼�����С����
// ��ʶ��Щ�߼����ڹؼ��߼����ǹؼ��߼�
// ����ģ�����ʱ��������һЩ����ʧ�ܵ�������紴���߳�ʧ�ܣ�д���ļ�ʧ�ܵȡ��������һ��С����
// ����Ϊȫ��ִ��ʧ�ܣ���ô�ǲ�����ġ�
// ϵͳ���ڵ���Դ���ж�״̬�£�Ӧ��֤�������ڶ��ӻ����£����̶ȵ�����߼�����
extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	// �������Լ����ˣ������û��ʵ������������Ӧ
	if (DriverObject == nullptr) {
		return STATUS_UNSUCCESSFUL;
	}

	TraceLoggingRegister(g_Provider);

	TraceLoggingWrite(g_Provider, "DriverEntry started",
		TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
		TraceLoggingValue("Ark Driver", "DriverName"),
		TraceLoggingUnicodeString(RegistryPath, "RegistryPath")
	);

	Log(LogLevel::Information, "DriverEntry called.Registry Path: %wZ\n", RegistryPath);

	RTL_OSVERSIONINFOW info;
	NTSTATUS status = RtlGetVersion(&info);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to get the version information (0x%08X)\n", status));
		return status;
	}

	// Set an Unload routine
	DriverObject->DriverUnload = AntiRootkitUnload;
	// Set dispatch routine the driver supports
	// ��ͼ����ʱ
	DriverObject->MajorFunction[IRP_MJ_CREATE] = AntiRootkitCreateClose;
	// ��������ʱ
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = AntiRootkitCreateClose;
	// �豸��������
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = AntiRootkitDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_READ] = AntiRootkitRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = AntiRootkitWrite;
	DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] = AntiRootkitShutdown;

	// Create a device object
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\AntiRootkit");
	// �������ʱ��ʼ����Щ����
	PDEVICE_OBJECT DeviceObject = nullptr;

	status = IoCreateDevice(
		DriverObject,		// our driver object,
		0,					// no need for extra bytes
		&devName,			// the device name
		FILE_DEVICE_UNKNOWN,// device type
		0,					// characteristics flags,
		FALSE,				// not exclusive
		&DeviceObject		// the resulting pointer
	);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to create device object (0x%08X)\n", status));
		return status;
	}

	// get I/O Manager's help
	// Large buffers may be expensive to copy
	//DeviceObject->Flags |= DO_BUFFERED_IO;

	// Large buffers
	//DeviceObject->Flags |= DO_DIRECT_IO;
	
	status = IoRegisterShutdownNotification(DeviceObject);
	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to register shutdown notify (0x%08X)\n", status));
		IoDeleteDevice(DeviceObject); // important!
		return status;
	}
	
	// Create a symbolic link to the device object
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\AntiRootkit");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {
		TraceLoggingWrite(g_Provider,
			"Error",
			TraceLoggingLevel(TRACE_LEVEL_ERROR),
			TraceLoggingValue("Symbolic link creation failed", "Message"),
			TraceLoggingNTStatus(status, "Status", "Returned status"));

		KdPrint(("Failed to create symbolic link (0x%08X)\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	g_DeviceObject = DeviceObject;

	// ���е���Դ���룬�뿼��ʧ�ܵ�����£�������ʲô����
	g_RegisterPath.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool,
		RegistryPath->Length, DRIVER_TAG);
	if (g_RegisterPath.Buffer == nullptr) {
		KdPrint(("Failed to allocate memory\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	g_RegisterPath.MaximumLength = RegistryPath->Length;
	RtlCopyUnicodeString(&g_RegisterPath, (PUNICODE_STRING)RegistryPath);

	//KdPrint(("Copied registry path: %wZ\n", &g_RegisterPath));

	//test();
	// More generally, if DriverEntry returns any failure status,the Unload routine is not called.
	return STATUS_SUCCESS;
}

void AntiRootkitUnload(_In_ PDRIVER_OBJECT DriverObject) {
	ExFreePool(g_RegisterPath.Buffer);
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\AntiRootkit");
	// delete symbolic link
	IoDeleteSymbolicLink(&symLink);
	IoUnregisterShutdownNotification(g_DeviceObject);
	// delete device object
	IoDeleteDevice(DriverObject->DeviceObject);
	KdPrint(("Driver Unload called\n"));
	TraceLoggingWrite(g_Provider, "Unload",
		TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
		TraceLoggingString("Driver unloading", "Message"));
	TraceLoggingUnregister(g_Provider);
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, 0);
	return status;
}
/*
	In more complex cases,these could be separate functions,
	where in the Create case the driver can (for instance) check to see who the caller is
	and only let approved callers succeed with opening a device.
*/
_Use_decl_annotations_
NTSTATUS AntiRootkitCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);

	Log(LogLevel::Verbose, "Create/Close called\n");

	auto status = STATUS_SUCCESS;
	// ��ȡ����ĵ�ǰջ�ռ�
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	// �ж����󷢸�˭���Ƿ���֮ǰ���ɵĿ����豸
	if (DeviceObject != g_DeviceObject) {
		CompleteIrp(Irp);
		return status;
	}

	TraceLoggingWrite(g_Provider, "Create/Close",
		TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
		TraceLoggingValue(stack->MajorFunction == IRP_MJ_CREATE ? "Create" : "Close", "Operation"));

	if (stack->MajorFunction == IRP_MJ_CREATE) {
		// verify it's WinArk client (very simple at the moment)
		HANDLE hProcess;
		status = ObOpenObjectByPointer(PsGetCurrentProcess(), OBJ_KERNEL_HANDLE, nullptr, 0, *PsProcessType, KernelMode, &hProcess);
		if (NT_SUCCESS(status)) {
			UCHAR buffer[280] = { 0 };
			status = ZwQueryInformationProcess(hProcess, ProcessImageFileName, buffer, sizeof(buffer) - sizeof(WCHAR), nullptr);
			if (NT_SUCCESS(status)) {
				auto path = (UNICODE_STRING*)buffer;
				auto bs = wcsrchr(path->Buffer, L'\\');
				NT_ASSERT(bs);
				if (bs == nullptr || 0 != _wcsicmp(bs, L"\\WinArk.exe"))
					status = STATUS_ACCESS_DENIED;
			}
			ZwClose(hProcess);
		}
	}
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;	// a polymorphic member, meaning different things in different request.
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

// �ڴ���������ʱ���ں��е��ڴ������޵ģ�Ҫע���ֹ���������
//		1.���ƻ������ĳ���
_Use_decl_annotations_
NTSTATUS AntiRootkitDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	// get our IO_STACK_LOCATION
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	const auto& dic = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl;
	ULONG len = 0;

	switch (dic.IoControlCode) {
		case IOCTL_ARK_GET_SHADOW_SERVICE_TABLE:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(void*)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			void* p = *(PULONG*)Irp->AssociatedIrp.SystemBuffer;
			SystemServiceTable* pSystemServiceTable = (SystemServiceTable*)p;
			// �������������ĳ���
			if (dic.OutputBufferLength < sizeof(PULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			pSystemServiceTable += 1;
			khook::_win32kTable = pSystemServiceTable;
			*(PULONG*)Irp->AssociatedIrp.SystemBuffer = pSystemServiceTable->ServiceTableBase;
			len = sizeof(PULONG);
			status = STATUS_SUCCESS;
			break;
		}

		case IOCTL_ARK_GET_SSDT_API_ADDR:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			// �������������ĳ���
			if (dic.OutputBufferLength < sizeof(void*)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			PVOID address;
			ULONG number = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
			bool success = khook::GetApiAddress(number,&address);
			if (success) {
				*(PVOID*)Irp->AssociatedIrp.SystemBuffer = address;
				len = sizeof(address);
				status = STATUS_SUCCESS;
			}
			break;
		}

		case IOCTL_ARK_GET_SHADOW_SSDT_API_ADDR:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			// �������������ĳ���
			if (dic.OutputBufferLength < sizeof(void*)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			PVOID address;
			ULONG number = *(ULONG*)Irp->AssociatedIrp.SystemBuffer;
			bool success = khook::GetShadowApiAddress(number, &address);
			if (success) {
				*(PVOID*)Irp->AssociatedIrp.SystemBuffer = address;
				len = sizeof(address);
				status = STATUS_SUCCESS;
			}
			break;
		}
		
		case IOCTL_ARK_OPEN_PROCESS:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			// ��ȡ���������������ĳ���
			if (dic.InputBufferLength < sizeof(OpenProcessThreadData) || dic.OutputBufferLength < sizeof(HANDLE)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			// ��û�����
			auto data = (OpenProcessThreadData*)Irp->AssociatedIrp.SystemBuffer;
			OBJECT_ATTRIBUTES attr = RTL_CONSTANT_OBJECT_ATTRIBUTES(nullptr, 0);
			CLIENT_ID id{};
			id.UniqueProcess = UlongToHandle(data->Id);
			// Ƕ�����壬ZwOpenProcess�ڲ����ܻ�ִ�е������Ļص�������
			// �ص���������ʱ����������ͬһ���̣߳��ҹ���һ���߳�ջ��������ܴ��ڡ�ջ������Ŀ���
			// 1.���ڵ��ô��ڻص�������api����������ʹ��ջ�ռ䣬�����ڴ棬���������ڴ�
			// 2.��������ִ���ڹ���������ص������еĴ��룬��������ʹ��ջ�ռ�
			// 3.����ִ���ڹ���������ص������еĴ��룬����Ҫ����Ƕ�׵�api,���������̣߳�
			// ͨ���̼߳�ͨ�Ű�ϵͳapi���õĽ�����ظ�����̡߳�
			// 4.�����ڴ�����ʹ�õݹ飬�����Ҫʹ�ã�ע��ݹ����

			status = ZwOpenProcess((HANDLE*)data, data->AccessMask, &attr, &id);
			len = NT_SUCCESS(status) ? sizeof(HANDLE) : 0;
			break;
		}

		case IOCTL_ARK_GET_VERSION:
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			// �������������ĳ���
			if (dic.OutputBufferLength < sizeof(USHORT)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			*(USHORT*)Irp->AssociatedIrp.SystemBuffer = DRIVER_CURRENT_VERSION;
			len = sizeof(USHORT);
			status = STATUS_SUCCESS;
			break;

		case IOCTL_ARK_SET_PRIORITY:
		{
			len = dic.InputBufferLength;
			if (len < sizeof(ThreadData)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			// ��û�����
			auto data = (ThreadData*)dic.Type3InputBuffer;
			if (data == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			__try {
				if (data->Priority < 1 || data->Priority>31) {
					status = STATUS_INVALID_PARAMETER;
					break;
				}

				PETHREAD Thread;
				status = PsLookupThreadByThreadId(UlongToHandle(data->ThreadId),&Thread);
				if (!NT_SUCCESS(status))
					break;

				KeSetPriorityThread(Thread, data->Priority);
				ObDereferenceObject(Thread);

				KdPrint(("Thread Priority change for %d to %d succeeded!\n",
					data->ThreadId, data->Priority));
			} __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION
				? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
				status = STATUS_ACCESS_VIOLATION;
			}
			break;
		}

		case IOCTL_ARK_DUP_HANDLE:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(DupHandleData)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			if (dic.OutputBufferLength < sizeof(HANDLE)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			const auto data = static_cast<DupHandleData*>(Irp->AssociatedIrp.SystemBuffer);
			HANDLE hProcess;
			OBJECT_ATTRIBUTES procAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(nullptr, OBJ_KERNEL_HANDLE);
			CLIENT_ID pid{};
			pid.UniqueProcess = UlongToHandle(data->SourcePid);
			status = ZwOpenProcess(&hProcess, PROCESS_DUP_HANDLE, &procAttributes, &pid);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Failed to open process %d (0x%08X)\n", data->SourcePid, status));
				break;
			}

			HANDLE hTarget;
			status = ZwDuplicateObject(hProcess, ULongToHandle(data->Handle), NtCurrentProcess(),
				&hTarget, data->AccessMask, 0, data->Flags);
			ZwClose(hProcess);
			if (!NT_SUCCESS(status)) {
				KdPrint(("Failed to duplicate handle (0x%8X)\n", status));
				break;
			}

			*(HANDLE*)Irp->AssociatedIrp.SystemBuffer = hTarget;
			len = sizeof(HANDLE);
			break;
		}

		case IOCTL_ARK_OPEN_THREAD:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(OpenProcessThreadData) || dic.OutputBufferLength < sizeof(HANDLE)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto data = (OpenProcessThreadData*)Irp->AssociatedIrp.SystemBuffer;
			OBJECT_ATTRIBUTES attr = RTL_CONSTANT_OBJECT_ATTRIBUTES(nullptr, 0);
			CLIENT_ID id{};
			id.UniqueThread = UlongToHandle(data->Id);
			status = ZwOpenThread((HANDLE*)data, data->AccessMask, &attr, &id);
			len = NT_SUCCESS(status) ? sizeof(HANDLE) : 0;
			break;
		}

		case IOCTL_ARK_OPEN_KEY:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			auto data = static_cast<KeyData*>(Irp->AssociatedIrp.SystemBuffer);
			if (dic.InputBufferLength < sizeof(KeyData) + ULONG((data->Length - 1) * 2)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			if (dic.OutputBufferLength < sizeof(HANDLE)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			if (data->Length > 2048) {
				status = STATUS_BUFFER_OVERFLOW;
				break;
			}

			UNICODE_STRING keyName;
			keyName.Buffer = data->Name;
			keyName.Length = keyName.MaximumLength = (USHORT)data->Length * sizeof(WCHAR);
			OBJECT_ATTRIBUTES keyAttr;
			InitializeObjectAttributes(&keyAttr, &keyName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
			HANDLE hKey{ nullptr };
			status = ZwOpenKey(&hKey, data->Access, &keyAttr);
			if (NT_SUCCESS(status)) {
				*(HANDLE*)data = hKey;
				len = sizeof(HANDLE);
			}
			break;
		}
		case IOCTL_ARK_INIT_NT_SERVICE_TABLE:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(PULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			void* address = *(PULONG*)Irp->AssociatedIrp.SystemBuffer;
			SystemServiceTable* pServiceTable = (SystemServiceTable*)address;
			khook::_ntTable = pServiceTable;
			status = STATUS_SUCCESS;
			break;
		}

		case IOCTL_ARK_GET_SERVICE_LIMIT:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(PULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			if (dic.OutputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			
			void* address = *(PULONG*)Irp->AssociatedIrp.SystemBuffer;
			SystemServiceTable* pServiceTable = (SystemServiceTable*)address;
			*(ULONG*)Irp->AssociatedIrp.SystemBuffer = pServiceTable->NumberOfServices;
			len = sizeof(ULONG);
			status = STATUS_SUCCESS;
			break;
		}

		case IOCTL_ARK_GET_PROCESS_NOTIFY_COUNT:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(ProcessNotifyCountData)) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			if (dic.OutputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto info = (ProcessNotifyCountData*)Irp->AssociatedIrp.SystemBuffer;
			ULONG count = 0;
			if (info->pCount) {
				count = *info->pCount;
			}
			if (info->pExCount) {
				count += *info->pExCount;
				*(ULONG*)Irp->AssociatedIrp.SystemBuffer = count;
				status = STATUS_SUCCESS;
				len = sizeof(count);
			}
			
			break;
		}
		case IOCTL_ARK_ENUM_IMAGELOAD_NOTIFY:
		case IOCTL_ARK_ENUM_THREAD_NOTIFY:
		case IOCTL_ARK_ENUM_PROCESS_NOTIFY:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			if (dic.InputBufferLength < sizeof(NotifyInfo)) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			auto info = (NotifyInfo*)Irp->AssociatedIrp.SystemBuffer;
			if (dic.OutputBufferLength < sizeof(void*) * info->Count) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			
			EnumProcessNotify((PEX_CALLBACK)info->pRoutine, info->Count,(KernelCallbackInfo*)Irp->AssociatedIrp.SystemBuffer);
			status = STATUS_SUCCESS;
			len = sizeof(void*) * info->Count + sizeof(ULONG);
			break;
		}

		case IOCTL_ARK_GET_THREAD_NOTIFY_COUNT:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(ThreadNotifyCountData)) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			if (dic.OutputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto info = (ThreadNotifyCountData*)Irp->AssociatedIrp.SystemBuffer;
			ULONG count = 0;
			if (info->pCount) {
				count = *info->pCount;
			}
			if (info->pNonSystemCount) {
				count += *info->pNonSystemCount;
				*(ULONG*)Irp->AssociatedIrp.SystemBuffer = count;
				status = STATUS_SUCCESS;
				len = sizeof(count);
			}
			break;
		}

		case IOCTL_ARK_GET_UNLOADED_DRIVERS_COUNT:
		case IOCTL_ARK_GET_IMAGE_NOTIFY_COUNT:
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(PULONG)) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			if (dic.OutputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			auto pCount = *(PULONG*)Irp->AssociatedIrp.SystemBuffer;
			ULONG count = 0;
			if (pCount) {
				count = *pCount;
				*(ULONG*)Irp->AssociatedIrp.SystemBuffer = count;
				status = STATUS_SUCCESS;
				len = sizeof(count);
			}
			break;
		}

		case IOCTL_ARK_ENUM_PIDDBCACHE_TABLE:
		{
			// RtlLookupElementGenericTableAvl()
			// RtlDeleteElementGenericTableAvl()
			// EtwpFreeKeyNameList
			// PiDDBCacheTable
			len = dic.InputBufferLength;
			if (len < sizeof(ULONG_PTR)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			if (dic.Type3InputBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			PRTL_AVL_TABLE PiDDBCacheTable = nullptr;

			__try {
				// ������뻺����
				ULONG_PTR PiDDBCacheTableAddress = *(ULONG_PTR*)dic.Type3InputBuffer;
				// ������������
				auto pData = (PiDDBCacheData*)Irp->UserBuffer;
				if (pData == nullptr) {
					status = STATUS_INVALID_PARAMETER;
					break;
				}
				ULONG size = dic.OutputBufferLength;
				LogInfo("PiDDBCacheTableAddress: %p\n", PiDDBCacheTableAddress);
				PiDDBCacheTable = (PRTL_AVL_TABLE)PiDDBCacheTableAddress;
				if (PiDDBCacheTable != nullptr) {
					for (auto p = RtlEnumerateGenericTableAvl(PiDDBCacheTable, TRUE);
						p != nullptr;
						) {
						// Process the element pointed to by p
						PiDDBCacheEntry* entry = (PiDDBCacheEntry*)p;
						LogInfo("%wZ,%d,%d\n", entry->DriverName, entry->LoadStatus, entry->TimeDateStamp);
						// ���ȵ÷ŵ��½ṹ��
						if (size < sizeof(PiDDBCacheData)) {
							status = STATUS_INFO_LENGTH_MISMATCH;
							break;
						}
						// ��Сsize part1
						size -= sizeof(PiDDBCacheData);
						// �����Ϣ
						pData->LoadStatus = entry->LoadStatus;
						pData->StringLen = entry->DriverName.Length;
						pData->TimeDateStamp = entry->TimeDateStamp;
						if (size < entry->DriverName.Length + sizeof(WCHAR)) {
							status = STATUS_INFO_LENGTH_MISMATCH;
							break;
						}
						// �ַ�����������ṹ�����
						pData->StringOffset = sizeof(PiDDBCacheData);
						auto pString = (WCHAR*)((PUCHAR)pData + pData->StringOffset);
						memcpy(pString, entry->DriverName.Buffer, entry->DriverName.Length);
						// ��Сsize part2
						size -= entry->DriverName.Length;
						// ��Сsize part3
						size -= sizeof(WCHAR);
						pString[entry->DriverName.Length] = L'\0';
						// ��һ���λ��
						pData->NextEntryOffset = sizeof(PiDDBCacheData) + pData->StringLen + sizeof(WCHAR);

						p = RtlEnumerateGenericTableAvl(PiDDBCacheTable, FALSE);
						// ����ָ��
						if (p != nullptr)
							pData = (PiDDBCacheData*)((PUCHAR)pData + pData->NextEntryOffset);
					};

					pData->NextEntryOffset = 0;

					len = dic.OutputBufferLength;
					status = STATUS_SUCCESS;
				}
			}
			__except (GetExceptionCode() == STATUS_ACCESS_VIOLATION
				? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
				status = STATUS_ACCESS_VIOLATION;
			}
			
			break;
		}

		case IOCTL_ARK_ENUM_UNLOADED_DRIVERS:
		{
			if (dic.Type3InputBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(UnloadedDriversInfo)) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			__try {
				// ������뻺����
				auto info = (UnloadedDriversInfo*)dic.Type3InputBuffer;
				// ������������
				auto pData = (UnloadedDriverData*)Irp->UserBuffer;
				if (pData == nullptr) {
					status = STATUS_INVALID_PARAMETER;
					break;
				}
				ULONG size = dic.OutputBufferLength;
				// MmUnloadedDrivers MmLastUnloadedDriver
				PUNLOADED_DRIVER MmUnloadDrivers = nullptr;
				MmUnloadDrivers = *(PUNLOADED_DRIVER*)info->pMmUnloadedDrivers;
				LogInfo("MmUnloadDriversAddress %p\n", MmUnloadDrivers);
				for (ULONG i = 0; i < info->Count; ) {
					LogInfo("%wZ,%p,%p,%X\n", MmUnloadDrivers[i].Name, MmUnloadDrivers[i].StartAddress,
						MmUnloadDrivers[i].EndAddress,
						MmUnloadDrivers[i].CurrentTime);
					// ���ȵ÷ŵ��½ṹ��
					if (size < sizeof(UnloadedDriverData)) {
						status = STATUS_INFO_LENGTH_MISMATCH;
						break;
					}
					// ��Сsize part1
					size -= sizeof(UnloadedDriverData);
					// �������
					pData->CurrentTime = MmUnloadDrivers[i].CurrentTime;
					pData->EndAddress = MmUnloadDrivers[i].EndAddress;
					pData->StartAddress = MmUnloadDrivers[i].StartAddress;
					pData->StringLen = MmUnloadDrivers[i].Name.Length;
					// ����Ƿ��ܴ���ַ���
					if (size < MmUnloadDrivers[i].Name.Length + sizeof(WCHAR)) {
						status = STATUS_INFO_LENGTH_MISMATCH;
						break;
					}
					// �洢�ַ���
					pData->StringOffset = sizeof(UnloadedDriverData);
					auto pString = (WCHAR*)((PUCHAR)pData + pData->StringOffset);
					memcpy(pString, MmUnloadDrivers[i].Name.Buffer, pData->StringLen);
					// ��Сsize part2
					size -= pData->StringLen;
					// ��Сsize part3
					size -= sizeof(WCHAR);
					pString[pData->StringLen] = L'\0';

					// ��һ���λ��
					pData->NextEntryOffset = sizeof(UnloadedDriverData) + pData->StringLen + sizeof(WCHAR);

					i++;
					// ����Ƿ���Ҫ����ָ��
					if (i < info->Count)
						pData = (UnloadedDriverData*)((PUCHAR)pData + pData->NextEntryOffset);
				}
				pData->NextEntryOffset = 0;

				len = dic.OutputBufferLength;
				status = STATUS_SUCCESS;
			}
			__except (GetExceptionCode() == STATUS_ACCESS_VIOLATION
				? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
				status = STATUS_ACCESS_VIOLATION;
			}
			
			break;
		}

		case IOCTL_ARK_GET_PIDDBCACHE_DATA_SIZE: 
		{
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(ULONG_PTR)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			if (dic.OutputBufferLength < sizeof(ULONG)) {
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			ULONG size = 0;
			PRTL_AVL_TABLE PiDDBCacheTable = nullptr;
			ULONG_PTR PiDDBCacheTableAddress = *(ULONG_PTR*)Irp->AssociatedIrp.SystemBuffer;
			PiDDBCacheTable = (PRTL_AVL_TABLE)PiDDBCacheTableAddress;
			ULONG count = RtlNumberGenericTableElementsAvl(PiDDBCacheTable);
			if (PiDDBCacheTable != nullptr) {
				for (auto p = RtlEnumerateGenericTableAvl(PiDDBCacheTable, TRUE);
					p != nullptr;
					p = RtlEnumerateGenericTableAvl(PiDDBCacheTable, FALSE)) {
					// Process the element pointed to by p
					PiDDBCacheEntry* entry = (PiDDBCacheEntry*)p;
					//LogInfo("%wZ,%d,%d\n", entry->DriverName, entry->LoadStatus, entry->TimeDateStamp);
					// part1 �ṹ���С
					size += sizeof(PiDDBCacheData);
					// part2 �ַ�������+'\0'
					size += entry->DriverName.Length + sizeof(WCHAR);
				};
			}

			*(ULONG*)Irp->AssociatedIrp.SystemBuffer = size;
			status = STATUS_SUCCESS;
			len = sizeof(ULONG);
			break;
		}

		case IOCTL_ARK_GET_UNLOADED_DRIVERS_DATA_SIZE:
		{
			ULONG size = 0;
			if (Irp->AssociatedIrp.SystemBuffer == nullptr) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (dic.InputBufferLength < sizeof(UnloadedDriversInfo)) {
				status = STATUS_INVALID_BUFFER_SIZE;
				break;
			}
			auto info = (UnloadedDriversInfo*)Irp->AssociatedIrp.SystemBuffer;
			// MmUnloadedDrivers MmLastUnloadedDriver
			PUNLOADED_DRIVER MmUnloadDrivers = nullptr;
			MmUnloadDrivers = *(PUNLOADED_DRIVER*)info->pMmUnloadedDrivers;
			for (ULONG i = 0; i < info->Count; i++) {
				// part1 �ṹ���С
				size += sizeof(UnloadedDriverData);
				// part2 �ַ�������+L'\0'
				size += MmUnloadDrivers[i].Name.Length + sizeof(WCHAR);
			}
			*(ULONG*)Irp->AssociatedIrp.SystemBuffer = size;
			status = STATUS_SUCCESS;
			len = sizeof(ULONG);
			break;
		}
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

NTSTATUS AntiRootkitShutdown(PDEVICE_OBJECT, PIRP Irp) {
	return CompleteIrp(Irp, STATUS_SUCCESS);
}

NTSTATUS AntiRootkitRead(PDEVICE_OBJECT, PIRP Irp) {
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Read.Length;
	if (len == 0)
		return CompleteIrp(Irp, STATUS_INVALID_PARAMETER);

	// DO_DIRECT_IO ��������
	//auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	//if (!buffer)
	//	return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);

	return CompleteIrp(Irp, STATUS_SUCCESS, len);
}


NTSTATUS AntiRootkitWrite(PDEVICE_OBJECT, PIRP Irp) {
	NTSTATUS status = STATUS_SUCCESS;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto len = stack->Parameters.Write.Length;

	do
	{
		if (len < sizeof(ThreadData)) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		auto data = static_cast<ThreadData*>(Irp->UserBuffer);
		if (data == nullptr) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		__try {
			if (data->Priority < 1 || data->Priority>31) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			PETHREAD thread;
			status = PsLookupThreadByThreadId(UlongToHandle(data->ThreadId), &thread);
			if (!NT_SUCCESS(status)) {
				break;
			}
			auto oldPriority = KeSetPriorityThread(thread, data->Priority);
			ObReferenceObject(thread);

			KdPrint(("Priority change for thread %d from %d to %d succeeded!\n", data->ThreadId,
				oldPriority, data->Priority));

			TraceLoggingWrite(g_Provider, "Boosting",
				TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
				TraceLoggingUInt32(data->ThreadId, "ThreadId"),
				TraceLoggingInt32(oldPriority, "OldPriority"),
				TraceLoggingInt32(data->Priority, "NewPriority"));

			len = sizeof(ThreadData);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			status = STATUS_ACCESS_VIOLATION;
		}
		
	} while (false);

	return CompleteIrp(Irp, status, len);
}

KEVENT kEvent;

KSTART_ROUTINE MyThreadFunc;
void MyThreadFunc(IN PVOID context) {
	PUNICODE_STRING str = (PUNICODE_STRING)context;
	KdPrint(("Kernel thread running: %wZ\n", str));
	MyGetCurrentTime();
	KdPrint(("Wait 3s!\n"));
	MySleep(3000);
	MyGetCurrentTime();
	KdPrint(("Kernel thread exit!\n"));
	KeSetEvent(&kEvent, 0, true);
	PsTerminateSystemThread(STATUS_SUCCESS);
}

void CreateThreadTest() {
	HANDLE hThread;
	UNICODE_STRING ustrTest = RTL_CONSTANT_STRING(L"This is a string for test!");
	NTSTATUS status;

	KeInitializeEvent(&kEvent, 
		SynchronizationEvent,	// when this event is set, 
		// it releases at most one thread (auto reset)
		FALSE);

	status = PsCreateSystemThread(&hThread, 0, NULL, NULL, NULL, MyThreadFunc, (PVOID)&ustrTest);
	if (!NT_SUCCESS(status)) {
		KdPrint(("PsCreateSystemThread failed!"));
		return;
	}
	ZwClose(hThread);
	KeWaitForSingleObject(&kEvent, Executive, KernelMode, FALSE, NULL);
	KdPrint(("CreateThreadTest over!\n"));
}

void test() {
	//ULONG ul = 1234, ul0 = 0;
	//PKEY_VALUE_PARTIAL_INFORMATION pkvi;
	//RegCreateKey(L"\\Registry\\Machine\\Software\\AppDataLow\\Tencent\\{61B942F7-A946-4585-B624-B2C0228FFE8C}");
	//RegSetValueKey(L"\\Registry\\Machine\\Software\\AppDataLow\\Tencent\\{61B942F7-A946-4585-B624-B2C0228FFE8C}", L"key", REG_DWORD, &ul, sizeof(ul));
	//RegQueryValueKey(L"\\Registry\\Machine\\Software\\AppDataLow\\Tencent\\{61B942F7-A946-4585-B624-B2C0228FFE8C}", L"key", &pkvi);
	EnumSubKeyTest();
	/*memcpy(&ul0, pkvi->Data, pkvi->DataLength);
	KdPrint(("key: %d\n", ul0));*/
	/*ExFreePool(pkvi);*/
}




