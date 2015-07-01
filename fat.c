

#include "precomp.h"
#include "fs_rec.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, FatRecFsControl)
#pragma alloc_text (PAGE, IsFatVolume)

#endif


typedef struct _FAT_BPB 
{
	unsigned char		ignored[3];	/* Boot strap short or near jump */
	char				system_id[8];	/* Name - can be used to special case
					partition manager volumes */
	unsigned __int8		bytes_per_sect[2];	/* bytes per logical sector */
	unsigned __int8		sects_per_clust;/* sectors/cluster */
	unsigned __int8		reserved_sects[2];	/* reserved sectors */
	unsigned __int8		num_fats;	/* number of FATs */
	unsigned __int8		dir_entries[2];	/* root directory entries */
	unsigned __int8		short_sectors[2];	/* number of sectors */
	unsigned __int8		media;		/* media code (unused) */
	unsigned __int16	fat_length;	/* sectors/FAT */
	unsigned __int16	secs_track;	/* sectors per track */
	unsigned __int16	heads;		/* number of heads */
	unsigned int		hidden;		/* hidden sectors (unused) */
	unsigned int		long_sectors;	/* number of sectors (if short_sectors == 0) */

	/* The following fields are only used by FAT32 */
	unsigned int		fat32_length;	/* sectors/FAT */
	unsigned __int16	flags;		/* bit 8: fat mirroring, low 4: active fat */
	unsigned __int8		version[2];	/* major, minor filesystem version */
	unsigned int		root_cluster;	/* first cluster in root directory */
	unsigned __int16	info_sector;	/* filesystem info sector */
	unsigned __int16	backup_boot;	/* backup boot sector */
	unsigned __int16	reserved2[6];	/* Unused */
} FAT_BPB,*PFAT_BPB;

BOOLEAN IsFatVolume(PUCHAR BlockData)
{
	PFAT_BPB Dbr;

	PAGED_CODE();

	Dbr = (PFAT_BPB)BlockData;



	return FALSE;
}

NTSTATUS FatRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	IrpSp;
	LARGE_INTEGER		StartingOffset;
	ULONG				Length;
	NTSTATUS			OutStatus;
	BOOLEAN				BlockReaded = FALSE;
	UCHAR*				BlockData;

	PAGED_CODE();

	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	if(IrpSp->MinorFunction == IRP_MN_MOUNT_VOLUME)
	{
		Status = STATUS_UNRECOGNIZED_VOLUME;

		if ( FsRecGetDeviceSectorSize(IrpSp->Parameters.MountVolume.DeviceObject, &Length, &OutStatus) )
		{
			StartingOffset.u.LowPart  = 0;
			StartingOffset.u.HighPart = 0;
			BlockData = NULL;

			if ( FsRecReadBlock(IrpSp->Parameters.MountVolume.DeviceObject, &StartingOffset, 512, Length, &BlockData, &BlockReaded) )
			{
				if ( IsFatVolume(BlockData) )
					Status = STATUS_FS_DRIVER_REQUIRED;
			}

			if ( BlockData )
				ExFreePoolWithTag(BlockData, 0);
		}
		else if( OutStatus == STATUS_NO_MEDIA_IN_DEVICE)
		{
			Status = OutStatus;
		}
		else
		{
			BlockReaded = TRUE;
		}
		
		if(BlockReaded)
		{
			if ( IrpSp->Parameters.MountVolume.DeviceObject->Characteristics & FILE_FLOPPY_DISKETTE )
				Status = STATUS_FS_DRIVER_REQUIRED;
		}
	}
	else if(IrpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM)
	{
		Status = FsRecLoadFileSystem(DeviceObject,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Fastfat");
	}
	else
	{
		Status = STATUS_INVALID_DEVICE_REQUEST;
	}
	
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp,0);
	return	Status;
}

