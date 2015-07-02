

#include "precomp.h"
#include "fs_rec.h"


#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, NtfsRecFsControl)
#pragma alloc_text (PAGE, IsNtfsVolume)

#endif

#pragma pack(push,1)

typedef struct _BIOS_PARAMETER_BLOCK 
{
	UCHAR  BytesPerSector[2];                       // offset = 0x000  0
	UCHAR  SectorsPerCluster[1];                    // offset = 0x002  2
	UCHAR  ReservedSectors[2];                      // offset = 0x003  3
	UCHAR  Fats[1];                                 // offset = 0x005  5
	UCHAR  RootEntries[2];                          // offset = 0x006  6
	UCHAR  Sectors[2];                              // offset = 0x008  8
	UCHAR  Media[1];                                // offset = 0x00A 10
	UCHAR  SectorsPerFat[2];                        // offset = 0x00B 11
	UCHAR  SectorsPerTrack[2];                      // offset = 0x00D 13
	UCHAR  Heads[2];                                // offset = 0x00F 15
	UCHAR  HiddenSectors[4];                        // offset = 0x011 17
	UCHAR  LargeSectors[4];                         // offset = 0x015 21
} BIOS_PARAMETER_BLOCK;								// sizeof = 0x019 25

typedef struct _PACKED_BOOT_SECTOR
{
	UCHAR					Jump[3];				//	0 跳转指令 offset: 0
	UCHAR					Oem[8];					//	3 offset: 3
	BIOS_PARAMETER_BLOCK	PackedBpb;
	ULONG					BS_Sign;			//  24 - 27, 总为 80 00 80 00
	LARGE_INTEGER			NumberSectors;		//  28 - 2F, 扇区总数，即分区大小
	LARGE_INTEGER			MftStartLcn;		//  30 - 37, $MFT 开始簇号
	LARGE_INTEGER			Mft2StartLcn;		//  38 - 3F, $MFTmirr 开始簇号
	LONG					ClustersPerFileRecordSegment;		//  40 - 43, 每个MFT的簇数
	LONG					DefaultClustersPerIndexAllocationBuffer;	//  44 - 47, 每Index的簇数
	UCHAR					SerialNumber[8];	//  48 - 4F, 分区逻辑序列号

} PACKED_BOOT_SECTOR,*PPACKED_BOOT_SECTOR;

#pragma pack(pop)


BOOLEAN
IsNtfsVolume(
    IN PUCHAR				BlockData,
    IN ULONG				BytesPerSector,
    IN PLARGE_INTEGER		NumberOfSectors
    )

/*++

Routine Description:

    This routine looks at the buffer passed in which contains the NTFS boot
    sector and determines whether or not it represents an NTFS volume.

Arguments:

    BootSector - Pointer to buffer containing a potential NTFS boot sector.

    BytesPerSector - Supplies the number of bytes per sector for the drive.

    NumberOfSectors - Supplies the number of sectors on the partition.

Return Value:

    The function returns TRUE if the buffer contains a recognizable NTFS boot
    sector, otherwise it returns FALSE.

--*/

{
	PPACKED_BOOT_SECTOR	BootSector;

    PAGED_CODE();

	BootSector = (PPACKED_BOOT_SECTOR)BlockData;

    //
    // Now perform all the checks, starting with the Name and Checksum.
    // The remaining checks should be obvious, including some fields which
    // must be 0 and other fields which must be a small power of 2.
    //

    if (BootSector->Oem[0] == 'N' &&
        BootSector->Oem[1] == 'T' &&
        BootSector->Oem[2] == 'F' &&
        BootSector->Oem[3] == 'S' &&
        BootSector->Oem[4] == ' ' &&
        BootSector->Oem[5] == ' ' &&
        BootSector->Oem[6] == ' ' &&
        BootSector->Oem[7] == ' '
		
		&&
		
        //
        // Check number of bytes per sector.  The low order byte of this
        // number must be zero (smallest sector size = 0x100) and the
        // high order byte shifted must equal the bytes per sector gotten
        // from the device and stored in the Vcb.  And just to be sure,
        // sector size must be less than page size.
        //
		
        BootSector->PackedBpb.BytesPerSector[0] == 0
		
		&&
		
        ((ULONG) (BootSector->PackedBpb.BytesPerSector[1] << 8) == BytesPerSector)
		
		&&
		
        BootSector->PackedBpb.BytesPerSector[1] << 8 <= PAGE_SIZE
		
		&&
		
        //
        //  Sectors per cluster must be a power of 2.
        //
		
        (BootSector->PackedBpb.SectorsPerCluster[0] == 0x1 ||
		BootSector->PackedBpb.SectorsPerCluster[0] == 0x2 ||
		BootSector->PackedBpb.SectorsPerCluster[0] == 0x4 ||
		BootSector->PackedBpb.SectorsPerCluster[0] == 0x8 ||
		BootSector->PackedBpb.SectorsPerCluster[0] == 0x10 ||
		BootSector->PackedBpb.SectorsPerCluster[0] == 0x20 ||
		BootSector->PackedBpb.SectorsPerCluster[0] == 0x40 ||
		BootSector->PackedBpb.SectorsPerCluster[0] == 0x80)
		
		&&
		
        //
        //  These fields must all be zero.  For both Fat and HPFS, some of
        //  these fields must be nonzero.
        //
		
        BootSector->PackedBpb.ReservedSectors[0] == 0 &&
        BootSector->PackedBpb.ReservedSectors[1] == 0 &&
        BootSector->PackedBpb.Fats[0] == 0 &&
        BootSector->PackedBpb.RootEntries[0] == 0 &&
        BootSector->PackedBpb.RootEntries[1] == 0 &&
        BootSector->PackedBpb.Sectors[0] == 0 &&
        BootSector->PackedBpb.Sectors[1] == 0 &&
        BootSector->PackedBpb.SectorsPerFat[0] == 0 &&
        BootSector->PackedBpb.SectorsPerFat[1] == 0 &&
        BootSector->PackedBpb.LargeSectors[0] == 0 &&
        BootSector->PackedBpb.LargeSectors[1] == 0 &&
        BootSector->PackedBpb.LargeSectors[2] == 0 &&
        BootSector->PackedBpb.LargeSectors[3] == 0
		
		&&
		
        //
        //  Number of Sectors cannot be greater than the number of sectors
        //  on the partition.
        //
		
        !( BootSector->NumberSectors.QuadPart > NumberOfSectors->QuadPart )
		
		&&
		
        //
        //  Check that both Lcn values are for sectors within the partition.
        //
		
        !( BootSector->MftStartLcn.QuadPart * BootSector->PackedBpb.SectorsPerCluster[0] > NumberOfSectors->QuadPart )
		
		&&
		
        !( BootSector->Mft2StartLcn.QuadPart * BootSector->PackedBpb.SectorsPerCluster[0] > NumberOfSectors->QuadPart )
		
		&&
		
        //
        //  Clusters per file record segment and default clusters for Index
        //  Allocation Buffers must be a power of 2.  A negative number indicates
        //  a shift value to get the actual size of the structure.
        //
		
        ((BootSector->ClustersPerFileRecordSegment >= -31 &&
		BootSector->ClustersPerFileRecordSegment <= -9) ||
		BootSector->ClustersPerFileRecordSegment == 0x1 ||
		BootSector->ClustersPerFileRecordSegment == 0x2 ||
		BootSector->ClustersPerFileRecordSegment == 0x4 ||
		BootSector->ClustersPerFileRecordSegment == 0x8 ||
		BootSector->ClustersPerFileRecordSegment == 0x10 ||
		BootSector->ClustersPerFileRecordSegment == 0x20 ||
		BootSector->ClustersPerFileRecordSegment == 0x40)
		
		&&
		
        ((BootSector->DefaultClustersPerIndexAllocationBuffer >= -31 &&
		BootSector->DefaultClustersPerIndexAllocationBuffer <= -9) ||
		BootSector->DefaultClustersPerIndexAllocationBuffer == 0x1 ||
		BootSector->DefaultClustersPerIndexAllocationBuffer == 0x2 ||
		BootSector->DefaultClustersPerIndexAllocationBuffer == 0x4 ||
		BootSector->DefaultClustersPerIndexAllocationBuffer == 0x8 ||
		BootSector->DefaultClustersPerIndexAllocationBuffer == 0x10 ||
		BootSector->DefaultClustersPerIndexAllocationBuffer == 0x20 ||
		BootSector->DefaultClustersPerIndexAllocationBuffer == 0x40)) 
	{
			
		return TRUE;
			
	} 
	else 
	{
			
		//
		// This does not appear to be an NTFS volume.
		//
			
		return FALSE;
	}
} 

NTSTATUS NtfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	IrpSp;
	LARGE_INTEGER		StartingOffset;
	LARGE_INTEGER		BackupOffset;
	LARGE_INTEGER		EndOffset;
	ULONG				SectorSize;
	LARGE_INTEGER		SectorCount;
	PUCHAR				BlockData;

	PAGED_CODE();

	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	do 
	{
		if(IrpSp->MinorFunction != IRP_MN_MOUNT_VOLUME)
		{
			if(IrpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM)
			{
				Status = FsRecLoadFileSystem(DeviceObject,
							L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Ntfs");
			}
			else
			{
				Status = STATUS_INVALID_DEVICE_REQUEST; //0xC0000010;
			}
			break;
		}
		
		Status = STATUS_UNRECOGNIZED_VOLUME; //0xC000014F;

		if( FsRecGetDeviceSectorSize(IrpSp->Parameters.MountVolume.DeviceObject, &SectorSize, 0)		&&
			FsRecGetDeviceSectors(IrpSp->Parameters.MountVolume.DeviceObject, SectorSize, &SectorCount)	)
		{
			StartingOffset.QuadPart = 0;

			BlockData = NULL;

			// 
			BackupOffset.QuadPart = (SectorCount.QuadPart >> 1) * SectorSize;
			EndOffset.QuadPart = (SectorCount.QuadPart - 1)  * SectorSize;

			do 
			{

				if ( FsRecReadBlock(DeviceObject, &StartingOffset, 512, SectorSize, &BlockData, 0) )
				{
					if(IsNtfsVolume(BlockData, SectorSize, &SectorCount))
					{
						Status = STATUS_FS_DRIVER_REQUIRED; //0xC000019C;
						break;
					}
				}

				if ( FsRecReadBlock(DeviceObject, &BackupOffset, 512, SectorSize, &BlockData, 0) )
				{
					if(IsNtfsVolume(BlockData, SectorSize, &SectorCount))
					{
						Status = STATUS_FS_DRIVER_REQUIRED; //0xC000019C;
						break;
					}
				}

				if ( FsRecReadBlock(DeviceObject, &EndOffset, 512, SectorSize, &BlockData, 0) )
				{
					if ( IsNtfsVolume(BlockData, SectorSize, &SectorCount) )
					{
						Status = STATUS_FS_DRIVER_REQUIRED; //0xC000019C;
						break;
					}
				}

			} while (FALSE);

			if ( BlockData )
				ExFreePoolWithTag(BlockData, 0);

		}

	} while (FALSE);

	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp,0);
	return Status;
}

