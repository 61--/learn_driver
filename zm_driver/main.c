#include "ntddk.h"
#include "ntstrsafe.h"




NTSTATUS DriverEntry(
	PDRIVER_OBJECT driver, PUNICODE_STRING reg_path);
#pragma alloc_text(INIT, DriverEntry)

VOID DriverUnload(PDRIVER_OBJECT driver)
{
	DbgPrint("driver unload success\r\n");
}
void AttachAllComs(PDRIVER_OBJECT driver);
NTSTATUS MyDispatch(PDEVICE_OBJECT device, PIRP irp);

#define  MAX_COM_ID 32
static PDEVICE_OBJECT s_fltobj[MAX_COM_ID] = { 0 };
static PDEVICE_OBJECT s_nextobj[MAX_COM_ID] = { 0 };


NTSTATUS DriverEntry(
	PDRIVER_OBJECT driver,PUNICODE_STRING reg_path) 
{ 
	size_t i;
	
	DbgPrint("DriverEntry\r\n");

	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++){
		driver->MajorFunction[i] = MyDispatch;
	}

	driver->DriverUnload = DriverUnload;
	AttachAllComs(driver);
	return STATUS_SUCCESS; 
}

PDEVICE_OBJECT OpenCom(ULONG id, NTSTATUS* status)
{
	UNICODE_STRING name_str;
	static WCHAR name[32] = { 0 };
	PFILE_OBJECT fileobj = NULL;
	PDEVICE_OBJECT devobj = NULL;

	memset(name, 0, sizeof(WCHAR)* 32);
	RtlStringCchPrintfW(
		name, 32,
		L"\\Device\\Serial%d", id);
	RtlInitUnicodeString(&name_str, name);
	*status = IoGetDeviceObjectPointer(
		&name_str, FILE_ALL_ACCESS,
		&fileobj, &devobj);
	if (*status == STATUS_SUCCESS)
		ObDereferenceObject(fileobj);

	return devobj;
}

NTSTATUS AttachDevice(
	PDRIVER_OBJECT driver,
	PDEVICE_OBJECT oldObj,
	PDEVICE_OBJECT *fltobj,
	PDEVICE_OBJECT *next)
{
	NTSTATUS status;
	PDEVICE_OBJECT topdev = NULL;
	status = IoCreateDevice(
		driver, 0, NULL, oldObj->DeviceType, 0,
		FALSE, fltobj);
	if (status != STATUS_SUCCESS)
		return status;
	if (oldObj->Flags & DO_BUFFERED_IO)
		(*fltobj)->Flags |= DO_BUFFERED_IO;
	if (oldObj->Flags & DO_DIRECT_IO)
		(*fltobj)->Flags |= DO_DIRECT_IO;
	if (oldObj->Characteristics & FILE_DEVICE_SECURE_OPEN)
		(*fltobj)->Characteristics |= FILE_DEVICE_SECURE_OPEN;
	(*fltobj)->Flags |= DO_POWER_PAGABLE;

	topdev = IoAttachDeviceToDeviceStack(*fltobj, oldObj);
	if (topdev == NULL){
		IoDeleteDevice(*fltobj);
		*fltobj = NULL;
		status = STATUS_UNSUCCESSFUL;
		return status;
	}
	*next = topdev;
	(*fltobj)->Flags = (*fltobj)->Flags & ~DO_DEVICE_INITIALIZING;
	return STATUS_SUCCESS;
}



void AttachAllComs(PDRIVER_OBJECT driver)
{
	ULONG i;
	PDEVICE_OBJECT com_ob;
	NTSTATUS status;
	for (i = 0; i < MAX_COM_ID;i++)
	{
		com_ob = OpenCom(i, &status);
		if (com_ob == NULL)
			continue;
		AttachDevice(
			driver, com_ob,
			&s_fltobj[i],
			&s_nextobj[i]);

	}
}

NTSTATUS MyDispatch(PDEVICE_OBJECT device, PIRP irp)
{
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
	ULONG i, j;
	for (i = 0; i < MAX_COM_ID; i++){
		if (s_fltobj[i] == device){
			if (irpsp->MajorFunction == IRP_MJ_PNP_POWER){
				PoStartNextPowerIrp(irp);
				IoSkipCurrentIrpStackLocation(irp);
				return PoCallDriver(s_nextobj[i], irp);
			}
			if (irpsp->MajorFunction == IRP_MJ_WRITE)
			{
				ULONG len = irpsp->Parameters.Write.Length;
				PUCHAR buf = NULL;
				if (irp->MdlAddress != NULL)
					buf = (PUCHAR)
					MmGetSystemAddressForMdlSafe(
					irp->MdlAddress, NormalPagePriority);
				else
					buf = (PUCHAR)irp->UserBuffer;
				if (buf == NULL)
					buf = (PUCHAR)irp->AssociatedIrp.SystemBuffer;
				for (j = 0; j < len; ++j){
					DbgPrint("send data: %2x\r\n", buf[j]);
				}
			}
			IoSkipCurrentIrpStackLocation(irp);
			return IoCallDriver(s_nextobj[i], irp);
		}
	}
	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}