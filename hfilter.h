#pragma once
#ifndef HFILTER_H
#define HFILTER_H

#include "Ntddk.h"
#include "ntstatus.h"
#include "ntddstor.h"
#include "initguid.h"
#include "ntddstor.h"
#include "ntintsafe.h"
#include "srb.h"
#include "scsi.h"
//#include "ntifs.h"

#define HFILTER_OK                 (0)

#define HFILTER_COMPLETE_REQUEST   (1)
#define HFILTER_FORWARD_REQUEST    (2)
#define HFILTER_SEND_REQUEST       (4)
#define HFILTER_QUEUE_REQUEST      (8)

#define HFILTER_ERROR              (-1)

DEFINE_GUID( FILE_TYPE_NOTIFICATION_GUID_PAGE_FILE,         0x0d0a64a1, 0x38fc, 0x4db8, 0x9f, 0xe7, 0x3f, 0x43, 0x52, 0xcd, 0x7c, 0x5c );
DEFINE_GUID( FILE_TYPE_NOTIFICATION_GUID_HIBERNATION_FILE,  0xb7624d64, 0xb9a3, 0x4cf8, 0x80, 0x11, 0x5b, 0x86, 0xc9, 0x40, 0xe7, 0xb7 );
DEFINE_GUID( FILE_TYPE_NOTIFICATION_GUID_CRASHDUMP_FILE,    0x9d453eb7, 0xd2a6, 0x4dbd, 0xa2, 0xe3, 0xfb, 0xd0, 0xed, 0x91, 0x09, 0xa9 );

#define GET_BYTE(val, offset)                                   ((val >> (8 * offset)) & (0xFF))
#define SET_BYTE(val, offset)                                   ((val & 0xFF) << (8 * offset))

#define BLOCK_TO_BYTES_SHIFT                                  (9)
#define SUB_DEVICE_BLOCK_SIZE                                 (512)
#define hfilterDebugPrint(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, __VA_ARGS__)

#define HFILTER_MEMORY_POOL_ALLOC_TAG_1                       '1_FH'
#define HFILTER_DEVICE_INQUIRY_COMMAND_TIMEOUT_VALUE          (60 * 1)
#define HFILTER_DEVICE_COMMAND_TIMEOUT_VALUE                  (60 * 1)
#define HFILTER_SUB_DEVICE_READ_WRITE_COMMAND_TIMEOUT_VALUE   (60 * 1)

#define HFILTER_MAX_NUMBER_OF_SUB_DEVICES                     (2)

#define HFILTER_MAX_NUMBER_OF_ADD_DEVICES                     (8)

#define HFILTER_INVALID_SUB_DEVICE_INDEX					  (-1)
#define HFILTER_SSD_SUB_DEVICE_INDEX						  (0)
#define HFILTER_HDD_SUB_DEVICE_INDEX						  (1)
#define HFILTER_VHD_SUB_DEVICE_INDEX                          (2)
#define HFILTER_USB_SUB_DEVICE_INDEX                          (3)

#define HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES			  (8)

#define HFILTER_SUB_DEVICE_MAX_NUMBER_OF_SCSI_OPS_PER_IRP     (3+((HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES-1)*2))

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

#define HFILTER_DATA_OUT                                       (0x1)
#define HFILTER_DATA_IN                                        (0x2)



#pragma pack(1)
typedef struct
{
	UINT64 inuse;
    UINT64 startLBA;
    UINT64 endLBA;

    UINT64 mappedStartLBA;
    UINT64 mappedEndLBA;
} hfilterLBARangeStruct;
#pragma pack()

typedef struct
{
    UINT64 inuse;
    UINT8  opcode;
    UINT64 startLBA;
    UINT64 mappedStartLBA;
    UINT64 endLBA;
    UINT64 sectorCount;
    INT32  hfilterSubDeviceIndex;

    IRP *irp;
    SCSI_REQUEST_BLOCK srb;
    UINT64 opByteOffset;
    UINT8 *dataTransferBuffer;
    UINT32 dataTransferBufferSize;
    UINT8 *senseDataBuffer;
    UINT32 senseDataBufferSize;
} hfilterSCSIOPStruct;

typedef struct
{
  PDEVICE_OBJECT targetDeviceObject;  //next in line...
  hfilterLBARangeStruct lbaRangeList[HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES];
} hfilterSubDeviceStruct;

/*
typedef struct
{
    UINT32 inuse;
    PDEVICE_OBJECT physicalDeviceObject;
} hfilterAddDeviceStruct;
*/

#define HFILTER_SUB_DEVICE_INFO_MAGIC_NUMBER                0xAAAAAAAAAAAACAFE

#pragma pack(1)
typedef struct
{
    UINT64 magicNumber;
    hfilterLBARangeStruct lbaRangeList[HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES];
} hfilterSubDeviceInfoStruct;
#pragma pack()


typedef struct
{
    //hfilterAddDeviceStruct addDeviceList[HFILTER_MAX_NUMBER_OF_ADD_DEVICES];
    hfilterSubDeviceStruct subDeviceList[HFILTER_MAX_NUMBER_OF_SUB_DEVICES];
} hfilterConfigurationStruct;


typedef struct
{
    PDEVICE_OBJECT PDO;  //my pdo
    PDEVICE_OBJECT targetDeviceObject;  //next in line...
    PDEVICE_OBJECT hfilterDeviceObject; //my obj (DO)
    PSTORAGE_DEVICE_DESCRIPTOR deviceDescriptor;
    INT32 subDeviceIndex;
} hfilterDeviceExtensionStruct;

typedef struct{
    PDEVICE_OBJECT hfilterDeviceObject;
    IRP *superIrp;
    SCSI_REQUEST_BLOCK *superSrb;
    //UINT8 *superIRPMdlVirtualAddress;
    UINT8 *superSrbDataTransferBuffer;
    UINT64 superSrbDataTransferSize;
    UINT64 superCDBStartLBA;
    UINT64 superCDBEndLBA;
    UINT32 superCDBSectorCount;
    UINT32 scsiOpCount;
    volatile UINT32 scsiOpCompletionCount;
    hfilterSCSIOPStruct scsiOpList[HFILTER_SUB_DEVICE_MAX_NUMBER_OF_SCSI_OPS_PER_IRP];
} hfilterReadWriteSubIrpCommandContextStruct;

typedef struct{
    IRP                 *irp;
    SCSI_REQUEST_BLOCK  srb;

    DEVICE_OBJECT       *targetDeviceObject;

    KEVENT              waitEvent;
    int                 synchronous;

    UINT8               cdb[16];
    UCHAR               cdbLength;

    UINT32              commandType;

    UINT8               *dataTransferBuffer;
    UINT32              dataTransferBufferSize;

    UINT8               *senseDataBuffer;
    UINT32              senseDataBufferSize;
} hfilterCommandContextStruct;

typedef struct{
    UCHAR opcode;
    UCHAR EVPD                      : 1;
    UCHAR obsolete                  : 1;
    UCHAR reserved                  : 6;
    UCHAR pageCode;
    UCHAR allocationLength[2];
    UCHAR control;
} hfilterSCSIInquiry6CDBStruct;


void hfilterPrintBuffer(char *title, unsigned char *buf, unsigned int length, int printHeader);
void *hfilterMemoryAlloc(POOL_TYPE PoolType, SIZE_T NumberOfBytes);
void hfilterMemoryFree(void *P);

NTSTATUS hfilterSendIrpToNextDriver(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS hfilterSendIrpToDriver(IN PDEVICE_OBJECT targetDeviceObject, IN PIRP Irp);


NTSTATUS hfilterSendIrpToNextDriverSynchronously(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

int hfilterQueueWorkItem(DEVICE_OBJECT *deviceObj, PIO_WORKITEM_ROUTINE callback, void *context);


int hfilterAddSubDevice(hfilterDeviceExtensionStruct *hfilterDeviceExtension);
int hfilterSetSubDeviceSSD(hfilterDeviceExtensionStruct *hfilterDeviceExtension);
int hfilterSetSubDeviceHDD(hfilterDeviceExtensionStruct *hfilterDeviceExtension);

int hfilterSetSubDeviceInfo(hfilterSubDeviceInfoStruct *subDeviceInfo, DEVICE_OBJECT *devObj);
//int hfilterHandleDSMIOCTL(PDEVICE_OBJECT DeviceObject, PIRP Irp, DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa);

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL) DRIVER_DISPATCH hfilterHandleDeviceControlIrp;
__drv_dispatchType(IRP_MJ_SCSI) DRIVER_DISPATCH hfilterHandleSCSIIrp;
__drv_dispatchType(IRP_MJ_PNP) DRIVER_DISPATCH hfilterHandlePnpIrp;


#endif