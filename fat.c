

#include "precomp.h"
#include "fs_rec.h"


#pragma pack(push,1)

typedef struct _BIOS_PARAMETER_BLOCK 
{
	USHORT		BytesPerSector;							// offset = 0x000  0
	UCHAR		SectorsPerCluster;                       // offset = 0x002  2
	USHORT		ReservedSectors;                      // offset = 0x003  3
	UCHAR		Fats;                                 // offset = 0x005  5
	USHORT		RootEntries;                          // offset = 0x006  6
	USHORT		Sectors;                              // offset = 0x008  8
	UCHAR		Media;									// offset = 0x00A 10
	USHORT		SectorsPerFat;                        // offset = 0x00B 11
	USHORT		SectorsPerTrack;                      // offset = 0x00D 13
	USHORT		Heads;                                // offset = 0x00F 15
	ULONG		HiddenSectors;                        // offset = 0x011 17
	ULONG		LargeSectors;                            // offset = 0x015 21
} BIOS_PARAMETER_BLOCK;								// sizeof = 0x019 25

typedef struct _PACKED_BOOT_SECTOR 
{
	UCHAR Jump[3];                                  // offset = 0x000   0
	UCHAR Oem[8];                                   // offset = 0x003   3
	BIOS_PARAMETER_BLOCK	PackedBpb;				// offset = 0x00B  11
	UCHAR PhysicalDriveNumber;                      // offset = 0x024  36
	UCHAR CurrentHead;                              // offset = 0x025  37
	UCHAR Signature;                                // offset = 0x026  38
	UCHAR Id[4];                                    // offset = 0x027  39
	UCHAR VolumeLabel[11];                          // offset = 0x02B  43
	UCHAR SystemId[8];                              // offset = 0x036  54
} PACKED_BOOT_SECTOR,*PPACKED_BOOT_SECTOR;          // sizeof = 0x03E  62

#pragma pack(pop)

VOID UnpackBiosParameterBlock(IN BIOS_PARAMETER_BLOCK* bpb,OUT BIOS_PARAMETER_BLOCK* bios);

#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, FatRecFsControl)
#pragma alloc_text (PAGE, IsFatVolume)
#pragma alloc_text (PAGE, UnpackBiosParameterBlock)

#endif

BOOLEAN
IsFatVolume(
    IN PUCHAR BlockData
    )


/*++



Routine Description:



    This routine looks at the buffer passed in which contains the FAT boot

    sector and determines whether or not it represents an actual FAT boot

    sector.



Arguments:



    Buffer - Pointer to buffer containing potential boot block.



Return Value:



    The function returns TRUE if the buffer contains a recognizable FAT boot

    sector, otherwise it returns FALSE.

--*/

{
	PPACKED_BOOT_SECTOR  Buffer;
    BIOS_PARAMETER_BLOCK bios;

    BOOLEAN result;

    PAGED_CODE();

	Buffer = (PPACKED_BOOT_SECTOR)BlockData;

    //

    // Begin by unpacking the Bios Parameter Block that is packed in the boot

    // sector so that it can be examined without incurring alignment faults.

    //

    UnpackBiosParameterBlock( &Buffer->PackedBpb, &bios );

    //

    // Assume that the sector represents a FAT boot block and then determine

    // whether or not it really does.

    //

    result = TRUE;


    if (bios.Sectors) 
	{
        bios.LargeSectors = 0;
    }



    // FMR Jul.11.1994 NaokiM - Fujitsu -

    // FMR boot sector has 'IPL1' string at the beginnig.



    if (Buffer->Jump[0] != 0x49 && /* FMR */
        Buffer->Jump[0] != 0xe9 &&
        Buffer->Jump[0] != 0xeb) 
	{

        result = FALSE;

    // FMR Jul.11.1994 NaokiM - Fujitsu -

    // Sector size of FMR partition is 2048.



    }
	else if (bios.BytesPerSector !=  128 &&

               bios.BytesPerSector !=  256 &&

               bios.BytesPerSector !=  512 &&

               bios.BytesPerSector != 1024 &&

               bios.BytesPerSector != 2048 && /* FMR */

               bios.BytesPerSector != 4096)
	{
        result = FALSE;
    }
	else if (bios.SectorsPerCluster !=  1 &&

               bios.SectorsPerCluster !=  2 &&

               bios.SectorsPerCluster !=  4 &&

               bios.SectorsPerCluster !=  8 &&

               bios.SectorsPerCluster != 16 &&

               bios.SectorsPerCluster != 32 &&

               bios.SectorsPerCluster != 64 &&

               bios.SectorsPerCluster != 128)
	{

        result = FALSE;

    } 
	else if (!bios.ReservedSectors) 
	{

        result = FALSE;

    } 
	else if (!bios.Fats) 
	{



        result = FALSE;



    //

    // Prior to DOS 3.2 might contains value in both of Sectors and

    // Sectors Large.

    //

    } 
	else if (!bios.Sectors && !bios.LargeSectors) 
	{

        result = FALSE;



    // FMR Jul.11.1994 NaokiM - Fujitsu -

    // 1. Media descriptor of FMR partitions is 0xfa.

    // 2. Media descriptor of partitions formated by FMR OS/2 is 0x00.

    // 3. Media descriptor of floppy disks formated by FMR DOS is 0x01.



    } 
	else if (bios.Media != 0x00 && /* FMR */

               bios.Media != 0x01 && /* FMR */

               bios.Media != 0xf0 &&

               bios.Media != 0xf8 &&

               bios.Media != 0xf9 &&

               bios.Media != 0xfa && /* FMR */

               bios.Media != 0xfb &&

               bios.Media != 0xfc &&

               bios.Media != 0xfd &&

               bios.Media != 0xfe &&

               bios.Media != 0xff) 
	{

        result = FALSE;

    } 
	else if (bios.SectorsPerFat != 0 && bios.RootEntries == 0) 
	{

        result = FALSE;

    }

    return result;

}

VOID UnpackBiosParameterBlock(IN BIOS_PARAMETER_BLOCK* bpb,OUT BIOS_PARAMETER_BLOCK* bios)
{



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
	return Status;
}

