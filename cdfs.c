
#include "precomp.h"
#include "fs_rec.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, UdfsRecFsControl)
#pragma alloc_text (PAGE, CdfsRecFsControl)

#endif

NTSTATUS CdfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;

	PAGED_CODE();

	return	Status;
}

NTSTATUS UdfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;

	PAGED_CODE();

	return	Status;
}

