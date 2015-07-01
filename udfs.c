

#include "precomp.h"
#include "fs_rec.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, UdfsRecFsControl)
#pragma alloc_text (PAGE, IsUdfsVolume)

#endif

BOOLEAN IsUdfsVolume(PDEVICE_OBJECT DeviceObject, ULONG SectorSize)
{

	return FALSE;
}

NTSTATUS UdfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	IrpSp;
	LARGE_INTEGER		StartingOffset;
	ULONG				SectorSize;
	NTSTATUS			OutStatus;
	BOOLEAN				BlockReaded = FALSE;
	UCHAR*				BlockData;
	
	PAGED_CODE();
	
	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	
	if(IrpSp->MinorFunction == IRP_MN_MOUNT_VOLUME)
	{
		Status = STATUS_UNRECOGNIZED_VOLUME;

		if ( FsRecGetDeviceSectorSize(IrpSp->Parameters.MountVolume.DeviceObject, &SectorSize, 0) )
		{
			if ( IsUdfsVolume(IrpSp->Parameters.MountVolume.DeviceObject, SectorSize) )
				Status = STATUS_FS_DRIVER_REQUIRED;
		}
	}
	else if(IrpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM)
	{
		Status = FsRecLoadFileSystem(DeviceObject,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Udfs");
	}
	else
	{
		Status = STATUS_INVALID_DEVICE_REQUEST;
	}
	
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp,0);
	return	Status;
}

