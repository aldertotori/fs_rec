

#include "precomp.h"
#include "fs_rec.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, FatRecFsControl)

#endif

NTSTATUS FatRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;

	PAGED_CODE();

	return	Status;
}

