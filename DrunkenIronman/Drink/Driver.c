#include <ntddk.h>


NTSTATUS
DriverEntry(
	_In_	PDRIVER_OBJECT	ptDriverObject,
	_In_	PUNICODE_STRING	pusRegistryPath
)
{
	UNREFERENCED_PARAMETER(ptDriverObject);
	UNREFERENCED_PARAMETER(pusRegistryPath);

	return STATUS_UNSUCCESSFUL;
}
