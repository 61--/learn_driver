#include "ntddk.h"


VOID DriverUnload(PDRIVER_OBJECT driver)
{
	DbgPrint("driver unload success\r\n");
}


NTSTATUS DriverEntry(
	PDRIVER_OBJECT driver,PUNICODE_STRING reg_path) 
{ 
	DbgPrint("DriverEntry\r\n");
	driver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS; 
}