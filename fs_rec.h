


#include <ntdddisk.h>
#undef DEFINE_GUID
#include <ntddcdrm.h>


#define POOL_TAG	'Fsrc'

#pragma pack(push,1)

typedef enum
{
	CDFS_TYPE  = 1,
	FATCD_TYPE = 2,
	NTFS_TYPE  = 4,
	UDFS_TYPE  = 5,
	EXFAT_TYPE = 6
} FILE_SYSTEM_TYPE;

typedef enum
{
	DRIVER_NOTLOAD = 0,
	DRIVER_LOADED  = 1,
	DRIVER_LOADING = 2
} DRIVER_LOAD_STATUS;

// 0xC
typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT		ParentDevice;		// 0
	FILE_SYSTEM_TYPE	FileSystemType;		// 4
	DRIVER_LOAD_STATUS	LoadStatus;			// 8
} DEVICE_EXTENSION,*PDEVICE_EXTENSION;

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
NTSTATUS ExFatRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp);
BOOLEAN IsExFatVolume(PUCHAR Dbr);

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
NTSTATUS NtfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp);
BOOLEAN IsNtfsVolume(PUCHAR SectorData,ULONG BytesPerSector,PLARGE_INTEGER SectorCount);

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
NTSTATUS UdfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp);
NTSTATUS CdfsRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp);

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
NTSTATUS FatRecFsControl(PDEVICE_OBJECT DeviceObject,PIRP Irp);

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
NTSTATUS FsRecLoadFileSystem(PDEVICE_OBJECT DeviceObject,LPWSTR DriverRegistryPath);

NTSTATUS FsRecCreateAndRegisterDO(PDRIVER_OBJECT	DriverObject,
								  PDEVICE_OBJECT	ParentDev,
								  PDEVICE_OBJECT*	CreatedDev,
								  LPWSTR			DeviceName,
								  LPWSTR			Recognizer,
								  FILE_SYSTEM_TYPE  FileSystemType,
								  ULONG				DeviceType,
								  BOOLEAN			bLowPrority);  // LOW_PRIORITY

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
BOOLEAN FsRecReadBlock(PDEVICE_OBJECT DeviceObject,
					   PLARGE_INTEGER StartingOffset,
					   ULONG LengthToRead,
					   ULONG SectorSize,
					   OUT PUCHAR* BlockData,
					   OUT PBOOLEAN BlockReaded);

BOOLEAN FsRecGetDeviceSectorSize(PDEVICE_OBJECT DeviceObject,PULONG SectorSize,OUT NTSTATUS* OutStatus);

BOOLEAN FsRecGetDeviceSectors(PDEVICE_OBJECT DeviceObject,ULONG BytesPerSector,OUT PLARGE_INTEGER SectorCount);

//////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
typedef struct _PARTITION_ENTRY//分区表结构
{
    UCHAR	active;			// 状态（是否被激活)
    UCHAR	StartHead;		// 分区起始磁头号   
    //UCHAR StartSector;	// 分区起始扇区和柱面号,高2位为柱面号的第 9,10 位, 高字节为柱面号的低 8 位  
    //UCHAR StartCylinder;	// 起始磁盘柱面 
    USHORT  StartSecCyli;	// 与63相位与得出的是开始扇区，把它右移6位就是开始柱面
    UCHAR	PartitionType;	// 分区类型   重要 
    UCHAR	EndHead;		// 分区结束磁头号
    //UCHAR EndSector;		// 分区结束扇区   
    //UCHAR EndCylinder;	// 结束柱面号
    USHORT	EndSecCyli;		// 与63相位与得出的就是结束扇区，把它右移6位就是结束柱面
    ULONG	StartLBA;		// 扇区起始逻辑地址   重要
    ULONG	TotalSector;	// 分区大小      重要
} PARTITION_ENTRY, *PPARTITION_ENTRY;

typedef struct _MBR_SECTOR
{
	UCHAR					BootCode[440];	// 0
	ULONG					DiskSignature;	//磁盘签名
	USHORT					NoneDisk;		//二个字节
	PARTITION_ENTRY			Entry[4];		// 446
	USHORT					Signature;		//结束标志2 Byte 55 AA
} MBR_SECTOR,*PMBR_SECTOR;

#pragma pack(pop)


#ifndef IOCTL_DISK_GET_LENGTH_INFO
#define IOCTL_DISK_GET_LENGTH_INFO CTL_CODE(IOCTL_DISK_BASE, 0x17, METHOD_BUFFERED, FILE_READ_ACCESS)
#endif

#ifndef IRP_MN_USER_FS_REQUEST
#define IRP_MN_USER_FS_REQUEST          0x00
#define IRP_MN_MOUNT_VOLUME             0x01
#define IRP_MN_VERIFY_VOLUME            0x02
#define IRP_MN_LOAD_FILE_SYSTEM         0x03
#define IRP_MN_TRACK_LINK               0x04    // To be obsoleted soon
#define IRP_MN_KERNEL_CALL              0x04
#endif

#define NTDDI_WIN2K                         0x05000000
#define NTDDI_WIN2KSP1                      0x05000100
#define NTDDI_WIN2KSP2                      0x05000200
#define NTDDI_WIN2KSP3                      0x05000300
#define NTDDI_WIN2KSP4                      0x05000400

#define NTDDI_WINXP                         0x05010000
#define NTDDI_WINXPSP1                      0x05010100
#define NTDDI_WINXPSP2                      0x05010200
#define NTDDI_WINXPSP3                      0x05010300
#define NTDDI_WINXPSP4                      0x05010400

#define NTDDI_WS03                          0x05020000
#define NTDDI_WS03SP1                       0x05020100
#define NTDDI_WS03SP2                       0x05020200
#define NTDDI_WS03SP3                       0x05020300
#define NTDDI_WS03SP4                       0x05020400

#define NTDDI_WIN6                          0x06000000
#define NTDDI_WIN6SP1                       0x06000100
#define NTDDI_WIN6SP2                       0x06000200
#define NTDDI_WIN6SP3                       0x06000300
#define NTDDI_WIN6SP4                       0x06000400

#define NTDDI_VISTA                         NTDDI_WIN6   
#define NTDDI_VISTASP1                      NTDDI_WIN6SP1
#define NTDDI_VISTASP2                      NTDDI_WIN6SP2
#define NTDDI_VISTASP3                      NTDDI_WIN6SP3
#define NTDDI_VISTASP4                      NTDDI_WIN6SP4

#define NTDDI_LONGHORN  NTDDI_VISTA

#define NTDDI_WS08                          NTDDI_WIN6SP1
#define NTDDI_WS08SP2                       NTDDI_WIN6SP2
#define NTDDI_WS08SP3                       NTDDI_WIN6SP3
#define NTDDI_WS08SP4                       NTDDI_WIN6SP4

#define NTDDI_WIN7                          0x06010000
#define NTDDI_WIN7SP1						0x06010100
#define NTDDI_WIN7SP2						0x06010200
#define NTDDI_WIN7SP3						0x06010300
#define NTDDI_WIN7SP4						0x06010400


NTSTATUS NTAPI IoRegisterFileSystem(PDEVICE_OBJECT);
NTSTATUS NTAPI ZwLoadDriver(PUNICODE_STRING DriverRegistryPath);
NTSTATUS NTAPI IoUnregisterFileSystem(PDEVICE_OBJECT DeviceObject);

typedef NTSTATUS (NTAPI *PFN_IoRegisterFileSystem)(PDEVICE_OBJECT);
typedef NTSTATUS (NTAPI *PFN_IoUnregisterFileSystem)(PDEVICE_OBJECT);
typedef NTSTATUS (NTAPI *PFN_ZwLoadDriver)(PUNICODE_STRING DriverRegistryPath);

#if _MSC_VER <= 1200

#define ExFreePoolWithTag(x,s)	ExFreePool(x)

#else

NTKERNELAPI
VOID
ExFreePoolWithTag(
				  IN PVOID P,
				  IN ULONG Tag
				  );

#endif


__inline unsigned long lb2bb(unsigned char *dat,unsigned char len)  //小端转为大端
{
	unsigned long temp=0;
	unsigned long fact=1;
	unsigned char i=0;
	for(i=0;i<len;i++)
	{
		temp+=dat[i]*fact;	
		fact*=256;
	}
	return temp;
}

