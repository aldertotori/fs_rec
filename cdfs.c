
#include "precomp.h"
#include "fs_rec.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, CdfsRecFsControl)
#endif

NTSTATUS CdfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	IrpSp;
	ULONG				SectorSize;

	PAGED_CODE();
	
	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	if ( IrpSp->MinorFunction == IRP_MN_MOUNT_VOLUME )
	{
		if ( FsRecGetDeviceSectorSize(IrpSp->Parameters.MountVolume.DeviceObject, &SectorSize, &Status) || Status != STATUS_NO_MEDIA_IN_DEVICE )
			Status = STATUS_FS_DRIVER_REQUIRED;
	}
	else if( IrpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM )
	{
		Status = FsRecLoadFileSystem(DeviceObject, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Cdfs");
	}
	else
	{
		Status = STATUS_INVALID_DEVICE_REQUEST;
	}

	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp,0);
	return Status;
}

