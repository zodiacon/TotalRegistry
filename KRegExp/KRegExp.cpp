#include <ntifs.h>
#include <wdmsec.h>
#include "KRegExpCommon.h"
#include "KRegExp.h"

#pragma comment(lib, "Wdmsec.lib")

NTSTATUS RegExpDeviceControl(PDEVICE_OBJECT, PIRP Irp);
void RegExpUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS RegExpCreateClose(PDEVICE_OBJECT, PIRP);
NTSTATUS RegExpDeviceControl(PDEVICE_OBJECT, PIRP);
NTSTATUS OnRegistryCallback(PVOID context, PVOID arg1, PVOID arg2);

constexpr int MaxRegExpProcesses = 8;

LARGE_INTEGER RegCookie;
ULONG RegExpProcessIds[MaxRegExpProcesses];
ULONG RegExpProcessCount;
ERESOURCE RegExpProcessIdsLock;

extern "C" NTSTATUS NTAPI ZwQueryInformationProcess(
	_In_ HANDLE ProcessHandle,
	_In_ PROCESSINFOCLASS ProcessInformationClass,
	_Out_writes_bytes_(ProcessInformationLength) PVOID ProcessInformation,
	_In_ ULONG ProcessInformationLength,
	_Out_opt_ PULONG ReturnLength
);

extern "C" NTSTATUS ObOpenObjectByName(
	_In_ POBJECT_ATTRIBUTES ObjectAttributes,
	_In_ POBJECT_TYPE ObjectType,
	_In_ KPROCESSOR_MODE AccessMode,
	_Inout_opt_ PACCESS_STATE AccessState,
	_In_opt_ ACCESS_MASK DesiredAccess,
	_Inout_opt_ PVOID ParseContext,
	_Out_ PHANDLE Handle);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
	PDEVICE_OBJECT DeviceObject{ nullptr };
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\KRegExp");
	auto status = STATUS_SUCCESS;
	auto resourceInit{ false };

	do {
		status = ExInitializeResourceLite(&RegExpProcessIdsLock);
		if (!NT_SUCCESS(status))
			break;

		resourceInit = true;
		status = IoCreateDeviceSecure(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, 
			&SDDL_DEVOBJ_SYS_ALL_ADM_ALL, nullptr, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to create device object (0x%X)\n", status));
			break;
		}

		//UNICODE_STRING alt = RTL_CONSTANT_STRING(L"9999999.9912");
		//status = CmRegisterCallbackEx(OnRegistryCallback, &alt, DriverObject, nullptr, &RegCookie, nullptr);
		//if (!NT_SUCCESS(status)) {
		//	KdPrint((DRIVER_PREFIX "Failed to register callback (0x%X)\n", status));
		//}

		UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\KRegExp");
		status = IoCreateSymbolicLink(&symName, &devName);
		if (!NT_SUCCESS(status)) {
			KdPrint((DRIVER_PREFIX "Failed to create symbolic link (0x%X)\n", status));
			break;
		}
	} while (false);

	if (!NT_SUCCESS(status)) {
		if (resourceInit)
			ExDeleteResourceLite(&RegExpProcessIdsLock);
		if (DeviceObject)
			IoDeleteDevice(DeviceObject);
		if (RegCookie.QuadPart)
			CmUnRegisterCallback(RegCookie);
		return status;
	}

	DriverObject->DriverUnload = RegExpUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = RegExpCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = RegExpDeviceControl;

	KdPrint((DRIVER_PREFIX "DriverEntry successful\n"));
	return status;
}

void RegExpUnload(PDRIVER_OBJECT DriverObject) {
	if (RegCookie.QuadPart)
		CmUnRegisterCallback(RegCookie);
	ExDeleteResourceLite(&RegExpProcessIdsLock);
	UNICODE_STRING symName = RTL_CONSTANT_STRING(L"\\??\\KRegExp");
	IoDeleteSymbolicLink(&symName);
	IoDeleteDevice(DriverObject->DeviceObject);
	KdPrint((DRIVER_PREFIX "Driver unload\n"));
}

NTSTATUS RegExpCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	auto status = STATUS_SUCCESS;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	if (stack->MajorFunction == IRP_MJ_CREATE) {
		// verify it's RegExp client (very simple at the moment)
		UCHAR buffer[270 * sizeof(WCHAR)] = { 0 };
		status = ZwQueryInformationProcess(NtCurrentProcess(), ProcessImageFileName, buffer, sizeof(buffer) - sizeof(WCHAR), nullptr);
		if (NT_SUCCESS(status)) {
			auto path = (UNICODE_STRING*)buffer;
			KdPrint((DRIVER_PREFIX "Process image name: %wZ\n", path));
			auto bs = wcsrchr(path->Buffer, L'\\');
			NT_ASSERT(bs);
			if (bs == nullptr || 0 != _wcsicmp(bs, L"\\RegExp.exe"))
				status = STATUS_ACCESS_DENIED;
		}
		if (NT_SUCCESS(status) && RegCookie.QuadPart) {
			KdPrint((DRIVER_PREFIX "Registering RegExp PID: %u\n", HandleToUlong(PsGetCurrentProcessId())));
			ExEnterCriticalRegionAndAcquireResourceExclusive(&RegExpProcessIdsLock);
			if (RegExpProcessCount < MaxRegExpProcesses) {
				for (auto& pid : RegExpProcessIds)
					if (pid == 0) {
						pid = HandleToUlong(PsGetCurrentProcessId());
						RegExpProcessCount++;
						break;
					}
			}
			ExReleaseResourceAndLeaveCriticalRegion(&RegExpProcessIdsLock);
		}
	}
	else {
		if (RegCookie.QuadPart) {
			ExEnterCriticalRegionAndAcquireResourceExclusive(&RegExpProcessIdsLock);
			for (auto& pid : RegExpProcessIds) {
				if (pid == HandleToUlong(PsGetCurrentProcessId())) {
					pid = 0;
					RegExpProcessCount--;
					break;
				}
			}
			ExReleaseResourceAndLeaveCriticalRegion(&RegExpProcessIdsLock);
		}
	}
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);
	return status;
}

NTSTATUS RegExpDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	const auto& dic = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl;
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG len = 0;

	switch (dic.IoControlCode) {
		case IOCTL_KREGEXP_OPEN_KEY:
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

		case IOCTL_KREGEXP_DUP_HANDLE:
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
			status = ZwDuplicateObject(hProcess, data->Handle, NtCurrentProcess(),
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
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, 0);
	return status;
}

bool IsRegisteredProcess() {
	auto pid = HandleToUlong(PsGetCurrentProcessId());
	ExEnterCriticalRegionAndAcquireResourceShared(&RegExpProcessIdsLock);
	bool success = false;
	for (auto id : RegExpProcessIds) {
		if (id == pid) {
			success = true;
			break;
		}
	}
	ExReleaseResourceAndLeaveCriticalRegion(&RegExpProcessIdsLock);
	return success;
}

NTSTATUS OnRegistryCallback(PVOID /*context*/, PVOID arg1, PVOID arg2) {
	switch ((REG_NOTIFY_CLASS)(ULONG_PTR)arg1) {
		case RegNtPreOpenKeyEx:
		{
			if (IsRegisteredProcess()) {
				auto info = (REG_OPEN_KEY_INFORMATION_V1*)arg2;
				info->CheckAccessMode = KernelMode;
				KdPrint((DRIVER_PREFIX "IsRegisteredProcess %wZ %wZ\n", info->CompleteName, info->RemainingName));
				NTSTATUS status;
				PUNICODE_STRING name = info->CompleteName;
				if (name == nullptr || name->Length == 0 || name->Buffer[0] != L'\\') {
					UCHAR buffer[1024];
					if (info->RootObject) {
						auto nameInfo = (POBJECT_NAME_INFORMATION)buffer;
						ULONG len;
						status = ObQueryNameString(info->RootObject, nameInfo, sizeof(buffer), &len);
						if (NT_SUCCESS(status)) {
							name = &nameInfo->Name;
							nameInfo->Name.MaximumLength = sizeof(buffer) - sizeof(UNICODE_STRING);
							if (info->RemainingName) {
								RtlAppendUnicodeToString(&nameInfo->Name, L"\\");
								status = RtlAppendUnicodeStringToString(&nameInfo->Name, info->RemainingName);
								KdPrint((DRIVER_PREFIX "Appended string: %wZ\n", &nameInfo->Name));
								if (NT_SUCCESS(status))
									name = &nameInfo->Name;
							}
						}
					}
				}
				KdPrint((DRIVER_PREFIX "key name: %wZ\n", name));
				OBJECT_ATTRIBUTES attr = RTL_CONSTANT_OBJECT_ATTRIBUTES(name, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE);
				HANDLE h;
				status = ZwOpenKey(&h, info->DesiredAccess, &attr);
				if (NT_SUCCESS(status)) {
					status = ObReferenceObjectByHandle(h, info->DesiredAccess, *CmKeyObjectType, info->CheckAccessMode, info->ResultObject, nullptr);
					ZwClose(h);
				}
				else {
					KdPrint((DRIVER_PREFIX "Failed in ZwOpenKey: 0x%X\n", status));
				}
				if (NT_SUCCESS(status)) {
					info->GrantedAccess = info->DesiredAccess;
					KdPrint((DRIVER_PREFIX "RegNtPreOpenKeyEx success %wZ\n", name));
					return STATUS_CALLBACK_BYPASS;
				}
			}
			break;
		}

	}
	return STATUS_SUCCESS;
}
