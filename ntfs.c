

#include "precomp.h"
#include "fs_rec.h"


#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, NtfsRecFsControl)
#pragma alloc_text (PAGE, IsNtfsVolume)

#endif

#pragma pack(push,1)

typedef struct _DBR_NTFS
{
	UCHAR	BS_jmpBoot[3];		//	0 跳转指令 offset: 0
	UCHAR	BS_OEMName[8];		//	3 offset: 3
	UCHAR	BS_BytesPerSector[2];	//	B 一般为 00 02 / 512 < 00 10 / 4096 
	UCHAR	BS_SecPerClus;		//	D 每簇扇区数 offset:13
	UCHAR	BS_Reserved[22];	//  E - 0x23
	/*
	USHORT	BS_ReserveClus;		//  E - F
	UCHAR	BS_Zero[3];			//  10
	UCHAR	BS_Unused;			//  13
	USHORT	BS_MediaType;		//  14 - 15 , 硬盘 F8
	USHORT	BS_Unk;				//  16 - 17, 总为 0
	USHORT	BS_SectorPerHead;	//  18 - 19, 每磁头扇区数
	USHORT	BS_HeadPerTrack;	//  1A - 1B, 每柱面磁头数
	ULONG	BS_OffsetSectors;	//  1C - 1F, 从 MBR - DBR 的扇区总数
	ULONG	BS_NotUsed;			//  20 - 23
	*/
	ULONG			BS_Sign;					//  24 - 27, 总为 80 00 80 00
	LARGE_INTEGER	BS_SectorTotal;		//  28 - 2F, 扇区总数，即分区大小
	LARGE_INTEGER	BS_MFTStartOffset;	//  30 - 37, $MFT 开始簇号
	LARGE_INTEGER	BS_MFTmirrOffset;	//  38 - 3F, $MFTmirr 开始簇号
	ULONG			BS_ClusPerMFT;		//  40 - 43, 每个MFT的簇数
	ULONG			BS_ClusPerIndex;	//  44 - 47, 每Index的簇数
	UCHAR			BS_SerialNumber[8]; //  48 - 4F, 分区逻辑序列号

} DBR_NTFS,*PDBR_NTFS;

#pragma pack(pop)


BOOLEAN IsNtfsVolume(PUCHAR SectorData,ULONG BytesPerSector,PLARGE_INTEGER SectorCount)
{
	PDBR_NTFS Dbr;

	PAGED_CODE();

	Dbr = (PDBR_NTFS)SectorData;

	if( Dbr->BS_OEMName[0] == 'N'  &&
		Dbr->BS_OEMName[1] == 'T'  &&
		Dbr->BS_OEMName[2] == 'F'  &&
		Dbr->BS_OEMName[3] == ' '  &&
		Dbr->BS_OEMName[4] == ' '  &&
		Dbr->BS_OEMName[5] == ' '  &&
		Dbr->BS_OEMName[6] == ' '  &&
		Dbr->BS_OEMName[7] == ' '  &&
		Dbr->BS_BytesPerSector[0] != 0		&&
		Dbr->BS_BytesPerSector[1] <= 0x10	&&
		(Dbr->BS_SecPerClus == 1  ||
		 Dbr->BS_SecPerClus == 2  ||
		 Dbr->BS_SecPerClus == 4  ||
		 Dbr->BS_SecPerClus == 8  ||
		 Dbr->BS_SecPerClus == 0x10 ||
		 Dbr->BS_SecPerClus == 0x20 ||
		 Dbr->BS_SecPerClus == 0x40 ||
		 Dbr->BS_SecPerClus == 0x80 ) &&
		 Dbr->BS_Reserved[0] == 0 &&
		 Dbr->BS_Reserved[1] == 0 &&
		 Dbr->BS_Reserved[2] == 0 &&
		 Dbr->BS_Reserved[3] == 0 &&
		 Dbr->BS_Reserved[4] == 0 &&
		 Dbr->BS_Reserved[5] == 0 &&
		 Dbr->BS_Reserved[6] == 0 &&
		 Dbr->BS_Reserved[7] == 0 &&
		 Dbr->BS_Reserved[8] == 0 &&
		 Dbr->BS_Reserved[9] == 0 &&
		 Dbr->BS_Reserved[10] == 0 &&
		 Dbr->BS_Reserved[11] == 0 &&
		 Dbr->BS_Reserved[12] == 0 &&
		 Dbr->BS_Reserved[13] == 0 &&
		 Dbr->BS_Reserved[14] == 0 &&
		 Dbr->BS_Reserved[15] == 0 &&
		 Dbr->BS_Reserved[16] == 0 &&
		 Dbr->BS_Reserved[17] == 0 &&
		 Dbr->BS_Reserved[18] == 0 &&
		 Dbr->BS_Reserved[19] == 0 &&
		 Dbr->BS_Reserved[20] == 0 &&
		 Dbr->BS_Reserved[21] == 0 &&
		 )
	{



		return TRUE;
	}
	return FALSE;
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
	return	Status;
}

