

#include "precomp.h"
#include "fs_rec.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text (PAGE, ExFatRecFsControl)
#pragma alloc_text (PAGE, IsExFatVolume)

#endif


#pragma pack(push,1)

typedef struct _EXFAT_DBR
{
	unsigned char BS_jmpBoot[3];		//跳转指令 offset: 0

	unsigned char BS_OEMName[8];		// offset: 3

	unsigned char BPB_BytesPerSec[2];	//每扇区字节数 offset:11

	unsigned char BPB_SecPerClus[1];	//每簇扇区数 offset:13
	
	unsigned char BPB_RsvdSecCnt[2];	//保留扇区数目 offset:14
	
	unsigned char BPB_NumFATs[1];		//此卷中FAT表数 offset:16
	
	unsigned char BPB_RootEntCnt[2];	//FAT32为0 offset:17
	
	unsigned char BPB_TotSec16[2];		//FAT32为0 offset:19
	
	unsigned char BPB_Media[1];			//存储介质 offset:21
	
	unsigned char BPB_FATSz16[2];		//FAT32为0 offset:22
	
	unsigned char BPB_SecPerTrk[2];		//磁道扇区数 offset:24
	
	unsigned char BPB_NumHeads[2];		//磁头数 offset:26
	
	unsigned char BPB_HiddSec[4];		//FAT区前隐扇区数 offset:28 －－
	
	unsigned char BPB_TotSec32[4];		//该卷总扇区数 offset:32
	
	unsigned char BPB_FATSz32[4];		//一个FAT表扇区数 offset:36
	
	unsigned char BPB_ExtFlags[2];		//FAT32特有 offset:40
	
	unsigned char BPB_FSVer[2];			//FAT32特有 offset:42
	
	unsigned char BPB_RootClus[4];		//根目录簇号 offset:44
	
	unsigned char FSInfo[2];			//保留扇区FSINFO扇区数offset:48
	
	unsigned char BPB_BkBootSec[2];		//通常为6 offset:50
	
	unsigned char BPB_Reserved[12];		//扩展用 offset:52
	
	unsigned char BS_DrvNum[1];			// offset:64
	
	unsigned char BS_Reserved1[1];		// offset:65
	
	unsigned char BS_BootSig[1];		// offset:66
	
	unsigned char BS_VolID[4];			// offset:67
	
	union
	{
		struct  
		{
			unsigned char BS_FilSysType[11];	// offset:71
			unsigned char BS_FilSysType1[8];	//"FAT32 " offset:82
			unsigned char BS_Data[420];			// 90 - 510
		} FAT32;

		struct
		{
			UCHAR				Unk_71;				// 71
			LARGE_INTEGER		Unk_72;				// 72	
			ULONG				Unk_80;				// 80
			ULONG				Unk_84;				// 84
			ULONG				Unk_88;				// 88
			ULONG				Unk_92;				// 92
			unsigned char		BS_Data[414];		// 96 - 510
		} ExFat;

	} FileSysType;

	unsigned short BS_EndSign;			// 510

} EXFAT_DBR,*PEXFAT_DBR;


#pragma pack(pop)

#define	ExFatOemName     "EXFAT   "
#define END_SIGN		 0xAA55

BOOLEAN IsExFatVolume(PUCHAR SectorData)
{
	BOOLEAN		bExFatVolume;
	UCHAR		Empty[53];
	PEXFAT_DBR	Dbr;

	PAGED_CODE();

	Dbr = (PEXFAT_DBR)SectorData;

	bExFatVolume = FALSE;
	memset(Empty,0,53);
	
	if(	Dbr->BS_jmpBoot[0] == 0xEB &&
		Dbr->BS_jmpBoot[1] == 0x76 &&
		Dbr->BS_jmpBoot[2] == 0x90 )
	{
		if(	memcmp(Dbr->BS_OEMName,ExFatOemName,8)	== 0		&&
			memcmp(Dbr->BPB_BytesPerSec,Empty,53)	== 0		&&
			Dbr->BS_EndSign	== END_SIGN							&&
			Dbr->FileSysType.ExFat.BS_Data[12] >= 7		&&			// 108
			Dbr->FileSysType.ExFat.BS_Data[12] < 0xC		&&			// 108
			Dbr->FileSysType.ExFat.BS_Data[13] <= 0x10	&&			// 109
			Dbr->FileSysType.ExFat.Unk_80 >= 0x18			&&			// 80
			Dbr->FileSysType.ExFat.BS_Data[14]			&&			// 110
			Dbr->FileSysType.ExFat.BS_Data[14] <= 2		&&			// 110
			(Dbr->FileSysType.ExFat.Unk_80 + Dbr->FileSysType.ExFat.Unk_84 * Dbr->FileSysType.ExFat.BS_Data[14] ) <= \
						Dbr->FileSysType.ExFat.Unk_88 &&
			Dbr->FileSysType.ExFat.Unk_92  >= 0x10	
			)
		{
			



			bExFatVolume = TRUE;
		}
	}

	return bExFatVolume;
}

NTSTATUS ExFatRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp)
{
	NTSTATUS			Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	IrpSp;
	PDEVICE_EXTENSION	DevExt;
	ULONG				SectorSize;
	LARGE_INTEGER		StartingOffset;
	BOOLEAN				BlockReaded = FALSE;
	UCHAR*				BlockData;

	PAGED_CODE();

	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	if(IrpSp->MinorFunction == IRP_MN_MOUNT_VOLUME)
	{
		Status = STATUS_UNRECOGNIZED_VOLUME;

		if(FsRecGetDeviceSectorSize(IrpSp->Parameters.MountVolume.DeviceObject,&SectorSize,0))
		{
			StartingOffset.QuadPart = 0;
			BlockData = NULL;

			if (FsRecReadBlock(IrpSp->Parameters.MountVolume.DeviceObject, &StartingOffset, 512, SectorSize, &BlockData, &BlockReaded) )
			{
				if ( IsExFatVolume(BlockData) )
					Status = STATUS_FS_DRIVER_REQUIRED;
			}

			if(BlockData) 
				ExFreePoolWithTag(BlockData,0);
		}
		else
		{
			BlockReaded = TRUE;
		}
	}
	else if(IrpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM)
	{
		Status = FsRecLoadFileSystem(DeviceObject,L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\ExFat");
	}
	else
	{
		Status = STATUS_INVALID_DEVICE_REQUEST;
	}

	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp,0);
	return	Status;
}
