
#include "precomp.h"
#include "fs_rec.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,PUNICODE_STRING Registry);
NTSTATUS FsRecCleanupClose(PDEVICE_OBJECT DevObj,PIRP Irp);
NTSTATUS FsRecFsControl(PDEVICE_OBJECT DevObj,PIRP Irp);
NTSTATUS FsRecCreate(PDEVICE_OBJECT DevObj,PIRP Irp);
NTSTATUS FsRecFsControl(PDEVICE_OBJECT DevObj,PIRP Irp);
VOID	 FsRecUnload(PDRIVER_OBJECT DriverObject);


#ifdef ALLOC_PRAGMA

#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (INIT, FsRecCreateAndRegisterDO)
#pragma alloc_text (PAGE, FsRecCleanupClose)
#pragma alloc_text (PAGE, FsRecFsControl)
#pragma alloc_text (PAGE, FsRecCreate)
#pragma alloc_text (PAGE, FsRecUnload)
#pragma alloc_text (PAGE, FsRecFsControl)

#pragma alloc_text (PAGE, FsRecLoadFileSystem)
#pragma alloc_text (PAGE, FsRecReadBlock)
#pragma alloc_text (PAGE, FsRecGetDeviceSectorSize)
#pragma alloc_text (PAGE, FsRecGetDeviceSectors)

#endif

PKEVENT	FsRecLoadSync = NULL;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject,PUNICODE_STRING Registry)
{
	int				Index = 0;
	PDEVICE_OBJECT	DeviceObject;

	MmPageEntireDriver(DriverEntry);

	DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = FsRecFsControl;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = FsRecCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = 
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = FsRecCleanupClose;
	
	DriverObject->DriverUnload = FsRecUnload;

	FsRecLoadSync = (PKEVENT)ExAllocatePoolWithTag(sizeof(KEVENT),NonPagedPool,POOL_TAG);
	if(FsRecLoadSync == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	KeInitializeEvent(FsRecLoadSync,SynchronizationEvent,TRUE);
	
	if(NT_SUCCESS(FsRecCreateAndRegisterDO(DriverObject,NULL,NULL,
		L"\\Cdfs",
		L"\\FileSystem\\CdfsRecognizer",
		CDFS_TYPE,
		FILE_DEVICE_CD_ROM_FILE_SYSTEM,
		TRUE)))
	{
		Index ++;
	}

	if (NT_SUCCESS( FsRecCreateAndRegisterDO(
		DriverObject,
		NULL,
		&DeviceObject,
		L"\\UdfsCdRom",
		L"\\FileSystem\\UdfsCdRomRecognizer",
		UDFS_TYPE,
		FILE_DEVICE_CD_ROM_FILE_SYSTEM,
		FALSE) ))
	{
		Index++;
	}

	if (NT_SUCCESS( FsRecCreateAndRegisterDO(
		DriverObject,
		NULL,
		&DeviceObject,
		L"\\Fat",
		L"\\FileSystem\\FatDiskRecognizer",
		FATCD_TYPE,
		FILE_DEVICE_DISK_FILE_SYSTEM,
		FALSE) ))
	{
		Index++;
	}
	
    if (NT_SUCCESS( FsRecCreateAndRegisterDO(
		DriverObject,
		DeviceObject,
		NULL,
		L"\\FatCdrom",
		L"\\FileSystem\\FatCdRomRecognizer",
		FATCD_TYPE,
		FILE_DEVICE_CD_ROM_FILE_SYSTEM,
		FALSE) ))
	{
		Index++;
	}
	
    if (NT_SUCCESS( FsRecCreateAndRegisterDO(DriverObject, NULL, NULL,
			L"\\Ntfs",
			L"\\FileSystem\\NtfsRecognizer", 
			NTFS_TYPE, 
			FILE_DEVICE_DISK_FILE_SYSTEM, 
			FALSE) ))
	{
		++Index;
	}

    if (NT_SUCCESS( FsRecCreateAndRegisterDO(DriverObject, 
		NULL, 
		NULL,
		L"\\ExFat", 
		L"\\FileSystem\\ExFatRecognizer",
		EXFAT_TYPE, 
		FILE_DEVICE_DISK_FILE_SYSTEM, 
		FALSE) ))
	{
		++Index;
	}

	return Index != 0 ? STATUS_SUCCESS : STATUS_IMAGE_ALREADY_LOADED;

}

NTSTATUS FsRecCreateAndRegisterDO(PDRIVER_OBJECT	DriverObject,
								  PDEVICE_OBJECT	ParentDev,
								  PDEVICE_OBJECT*	CreatedDev,
								  LPWSTR			DeviceName,
								  LPWSTR			Recognizer,
								  FILE_SYSTEM_TYPE  FileSystemType,
								  ULONG				DeviceType,
								  BOOLEAN			bLowPrority)
{
	NTSTATUS					Status;
	IO_STATUS_BLOCK				IoStatusBlock;
	OBJECT_ATTRIBUTES			ObjectAttributes;
	UNICODE_STRING				DestinationString;
	HANDLE						Handle;
	PDEVICE_OBJECT				DeviceObject;
	PDEVICE_EXTENSION			DevExt;
	PFN_IoRegisterFileSystem	pfn_IoRegisterFileSystem = NULL;

	if(CreatedDev)  CreatedDev = NULL;
	RtlInitUnicodeString(&DestinationString, DeviceName);
	InitializeObjectAttributes(&ObjectAttributes,&DestinationString,
								OBJ_CASE_INSENSITIVE,
								NULL,
								NULL);

	Status = ZwCreateFile(&Handle,
							SYNCHRONIZE, // 0x100000,
							&ObjectAttributes, 
							&IoStatusBlock, 
							NULL,
							0,
							FILE_SHARE_READ | FILE_SHARE_WRITE, 
							1,
							0,
							NULL,
							0);

	if(NT_SUCCESS(Status))
	{
		ZwClose(Handle);
	}

	if(Status != STATUS_OBJECT_NAME_NOT_FOUND)
	{
		Status = STATUS_SUCCESS;
	}

	if(!NT_SUCCESS(Status))
	{
		RtlInitUnicodeString(&DestinationString, Recognizer);

		Status = IoCreateDevice(DriverObject, 
					sizeof(DEVICE_EXTENSION),
					&DestinationString, 
					DeviceType, 
					0,
					FALSE,
					&DeviceObject);

		if(NT_SUCCESS(Status))
		{
			DevExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

			DevExt->FileSystemType = FileSystemType;

			DevExt->LoadStatus = DRIVER_NOTLOAD;

			if(ParentDev)
			{
				DevExt->Device = ((PDEVICE_EXTENSION)ParentDev->DeviceExtension)->Device;
				DevExt = (PDEVICE_EXTENSION)ParentDev->DeviceExtension;
			}

			DevExt->Device = DeviceObject;

			if(CreatedDev)
				CreatedDev[0] = DeviceObject;

			if(bLowPrority)
			{
				DeviceObject->Flags |= DO_LOW_PRIORITY_FILESYSTEM;
			}

			RtlInitUnicodeString(&DestinationString, L"IoRegisterFileSystem");

			pfn_IoRegisterFileSystem =
				(PFN_IoRegisterFileSystem)MmGetSystemRoutineAddress(&DestinationString);

			if(pfn_IoRegisterFileSystem)
				pfn_IoRegisterFileSystem(DeviceObject);

			Status = STATUS_SUCCESS;
		}
	}
	else
	{
		Status = STATUS_IMAGE_ALREADY_LOADED;
	}
	
	return Status;
}

NTSTATUS FsRecCreate(PDEVICE_OBJECT DevObj,PIRP Irp)
{
	NTSTATUS	Status = STATUS_OBJECT_PATH_NOT_FOUND;
	PIO_STACK_LOCATION	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	if(IrpSp->FileObject->FileName.Length)
	{
		Status = STATUS_SUCCESS;
	}
	Irp->IoStatus.Status		= Status;
	Irp->IoStatus.Information	= 1;
	IofCompleteRequest(Irp,0);
	return Status;
}

NTSTATUS FsRecCleanupClose(PDEVICE_OBJECT DevObj,PIRP Irp)
{
	IoCompleteRequest(Irp,0);
	return STATUS_SUCCESS;
}

VOID FsRecUnload(PDRIVER_OBJECT DriverObject)
{
	PAGED_CODE();

	if(FsRecLoadSync)
	{
		ExFreePoolWithTag(FsRecLoadSync,0);
		FsRecLoadSync = NULL;
	}
}

NTSTATUS FsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status;
	PDEVICE_EXTENSION	DevExt;
	PIO_STACK_LOCATION  IrpSp;

	PAGED_CODE();
	
	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	DevExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	if(DevExt->LoadStatus && IrpSp->MinorFunction == IRP_MN_MOUNT_VOLUME)
	{
		Status = DevExt->LoadStatus != DRIVER_LOADED ? STATUS_FS_DRIVER_REQUIRED : STATUS_UNRECOGNIZED_VOLUME;
		Irp->IoStatus.Status = Status;
		IoCompleteRequest(Irp,0);
		return Status;
	}
	else
	{
		switch(DevExt->FileSystemType)
		{
		case CDFS_TYPE:
			Status = CdfsRecFsControl(DeviceObject,Irp);
			break;

		case FATCD_TYPE:
			Status = FatRecFsControl(DeviceObject,Irp);
			break;

		case NTFS_TYPE:
			Status = NtfsRecFsControl(DeviceObject,Irp);
			break;

		case EXFAT_TYPE:
			Status = ExFatRecFsControl(DeviceObject,Irp);
			break;

		default:
			Status = STATUS_INVALID_DEVICE_REQUEST;
			break;

		}
	}

	return Status;
}

BOOLEAN FsRecReadBlock(PDEVICE_OBJECT DeviceObject,PLARGE_INTEGER StartingOffset,ULONG LengthToRead,ULONG SectorSize,OUT PUCHAR* BlockData,OUT PBOOLEAN BlockReaded)
{
	NTSTATUS		Status;
	IO_STATUS_BLOCK IoStatusBlock;
	KEVENT			Event;
	PIRP			Irp;
	ULONG			SectorAlignLength = 0;

	PAGED_CODE();
	
	if(BlockReaded) BlockReaded[0] = FALSE;

	KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

	if ( LengthToRead > SectorSize )
		SectorAlignLength = SectorSize * (LengthToRead + SectorSize - 1) / SectorSize;
	else
		SectorAlignLength = SectorSize;

	if(!BlockData)
	{
		BlockData[0] = ExAllocatePoolWithTag(PagedPool,(SectorAlignLength + PAGE_SIZE - 1) & 0xFFFFF000,POOL_TAG);
	}

	if(BlockData[0])
	{
		Irp = IoBuildSynchronousFsdRequest(IRP_MJ_READ,
			DeviceObject,
			BlockData[0],
			SectorAlignLength,
			StartingOffset,
			&Event,
			&IoStatusBlock);
		if(Irp)
		{
			(IoGetNextIrpStackLocation(Irp))->Flags |= IRP_PAGING_IO;
			
			Status = IoCallDriver(DeviceObject,Irp);
			if(Status == STATUS_PENDING)
			{
				KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,NULL);
				Status = IoStatusBlock.Status;
			}
			
			if(!NT_SUCCESS(Status))
			{
				if(BlockReaded)
					BlockReaded[0] = TRUE;
				
				return FALSE;
			}
			
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN FsRecGetDeviceSectorSize(PDEVICE_OBJECT DeviceObject,PULONG SectorSize,OUT NTSTATUS* OutStatus)
{
	NTSTATUS			Status;
	KEVENT				Event;
	IO_STATUS_BLOCK		IoStatus;
	PIRP				Irp;
	LARGE_INTEGER		DiskLength;
	PIO_STACK_LOCATION	IrpSp;
	ULONG				IoCtlCode;
	DISK_GEOMETRY		DiskGeometry;
	
	PAGED_CODE();

	KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

	Irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_LENGTH_INFO,
			DeviceObject,
			NULL,
			0,
			(PVOID)&DiskLength,
			sizeof(LARGE_INTEGER),
			FALSE,
			&Event,
			&IoStatus);

	if(Irp)
	{
		(IoGetNextIrpStackLocation(Irp))->Flags |= IRP_PAGING_IO; // 系统自动释放
	
		Status = IoCallDriver(DeviceObject, Irp);
	
		if(Status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,NULL);
			Status = IoStatus.Status;
		}

		if(DeviceObject->DeviceType == FILE_DEVICE_CD_ROM)
		{
			IoCtlCode = IOCTL_CDROM_GET_DRIVE_GEOMETRY;
		}
		else if(DeviceObject->DeviceType == FILE_DEVICE_DISK)
		{
			IoCtlCode = IOCTL_DISK_GET_DRIVE_GEOMETRY;
		}
		else
		{
			if(OutStatus) OutStatus[0] = Status;
			return FALSE;
		}

		KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

		Irp = IoBuildDeviceIoControlRequest(IoCtlCode,
			DeviceObject,
			NULL, 
			0, 
			&DiskGeometry, 
			sizeof(DISK_GEOMETRY),
			FALSE,
			&Event,
			&IoStatus);

		if(Irp)
		{
			(IoGetNextIrpStackLocation(Irp))->Flags |= IRP_PAGING_IO; // 系统自动释放
			
			Status = IoCallDriver(DeviceObject, Irp);
			
			if(Status == STATUS_PENDING)
			{
				KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,NULL);
				Status = IoStatus.Status;
			}

			if(OutStatus) OutStatus[0] = Status;

			if(NT_SUCCESS(Status) && DiskGeometry.BytesPerSector)
			{
				SectorSize[0] = DiskGeometry.BytesPerSector;
				return TRUE;
			}
		}
		else
		{
			if(OutStatus) OutStatus[0] = STATUS_INSUFFICIENT_RESOURCES;
			return FALSE;
		}

		return FALSE;
	}

	if(OutStatus)
		OutStatus[0] = STATUS_INSUFFICIENT_RESOURCES; // 0xC000009A;
	
	return FALSE;
}


NTSTATUS FsRecLoadFileSystem(PDEVICE_OBJECT DeviceObject,LPWSTR DriverRegistryPath)
{
	NTSTATUS			Status = STATUS_IMAGE_ALREADY_LOADED;
	PDEVICE_EXTENSION	DevExt,Ext;
	UNICODE_STRING		DriverName;
	UNICODE_STRING		FunName;
	PFN_ZwLoadDriver	pfn_ZwLoadDriver;
	PFN_IoUnregisterFileSystem	pfn_IoUnregisterFileSystem;

	PAGED_CODE();

	RtlInitUnicodeString(&FunName,L"ZwLoadDriver");
	pfn_ZwLoadDriver = (PFN_ZwLoadDriver)MmGetSystemRoutineAddress(&FunName);

	RtlInitUnicodeString(&FunName,L"IoUnregisterFileSystem");
	pfn_IoUnregisterFileSystem = (PFN_IoUnregisterFileSystem)MmGetSystemRoutineAddress(&FunName);

	if(!pfn_ZwLoadDriver || pfn_IoUnregisterFileSystem) return Status;

	DevExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	if(DevExt->LoadStatus != DRIVER_LOADED)
	{
		KeEnterCriticalRegion();
		KeWaitForSingleObject(FsRecLoadSync,Executive,KernelMode,FALSE,NULL);
		if(DevExt->LoadStatus == DRIVER_NOTLOAD)
		{
			RtlInitUnicodeString(&DriverName,DriverRegistryPath);
			Status = pfn_ZwLoadDriver(&DriverName);
			
			Ext = DevExt;
			while ( DevExt->LoadStatus != DRIVER_LOADING )
			{
				Ext->LoadStatus = DRIVER_LOADING;
				Ext = (PDEVICE_EXTENSION)Ext->Device->DeviceExtension;
			}
		}
		
		Ext = DevExt;
		while ( Ext->LoadStatus != DRIVER_LOADED )
		{
			pfn_IoUnregisterFileSystem(DeviceObject);
			DeviceObject = DevExt->Device;
			DevExt->LoadStatus = DRIVER_LOADED;
			Ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
		}

		KeSetEvent(FsRecLoadSync,0,FALSE);
		KeLeaveCriticalRegion();
	}

	return	Status;
}

// The length of the disk, volume, or partition, in bytes

BOOLEAN FsRecGetDeviceSectors(PDEVICE_OBJECT DeviceObject,ULONG BytesPerSector,OUT PLARGE_INTEGER SectorCount)
{
	KEVENT			Event;
	IO_STATUS_BLOCK	IoStatus;
	LARGE_INTEGER	Length;
	PIRP			Irp;
	NTSTATUS		Status;
	LARGE_INTEGER	Divide;
	ULONG			Remainder;

	if( DeviceObject->DeviceType == FILE_DEVICE_DISK)
	{
		KeInitializeEvent(&Event,SynchronizationEvent,FALSE);

		Irp = IoBuildDeviceIoControlRequest(
					IOCTL_DISK_GET_LENGTH_INFO,	// 7405C
					DeviceObject,
					NULL,
					0,
					(PVOID)&Length,
					sizeof(LARGE_INTEGER),
					FALSE,
					&Event,
					&IoStatus);
		if(Irp)
		{
			(IoGetNextIrpStackLocation(Irp))->Flags |= IRP_PAGING_IO;
		
			Status = IoCallDriver(DeviceObject,Irp);
			
			if(Status == STATUS_PENDING)
			{
				KeWaitForSingleObject(&Event,Executive,KernelMode,FALSE,NULL);
				Status = IoStatus.Status;
			}
			
			if(NT_SUCCESS(Status))
			{
				Divide = RtlExtendedLargeIntegerDivide(Length,BytesPerSector,&Remainder);
				SectorCount->QuadPart = Divide.QuadPart;
				return TRUE;
			}
		}
	}

	return FALSE;
}

