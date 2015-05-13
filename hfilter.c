#include "hfilter.h"



DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD hfilterUnload;

DRIVER_ADD_DEVICE hfilterAddDevice;

DRIVER_DISPATCH hfilterSendIrpToNextDriver;


extern BOOLEAN IoIsOperationSynchronous(__in PIRP Irp);


hfilterConfigurationStruct *hfilterConfig = NULL;



NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
    ULONG ulIndex = 0;
    PDRIVER_DISPATCH *dispatchFuncPtr = NULL;
    NTSTATUS returnStatus = STATUS_SUCCESS;

    hfilterDebugPrint("INFO: hfilter - DriverEntry v22\n");

    hfilterDebugPrint("INFO: hfilter - sizeof(hfilterReadWriteSubIrpCommandContextStruct): %d\n", sizeof(hfilterReadWriteSubIrpCommandContextStruct));
    hfilterDebugPrint("INFO: hfilter - sizeof(hfilterSubDeviceInfoStruct):                 %d\n", sizeof(hfilterSubDeviceInfoStruct));
 
    for (ulIndex = 0, dispatchFuncPtr = DriverObject->MajorFunction; ulIndex <= IRP_MJ_MAXIMUM_FUNCTION; ulIndex++, dispatchFuncPtr++)
    {
        *dispatchFuncPtr = hfilterSendIrpToNextDriver;
    }

    //setup filtering points

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = hfilterHandleDeviceControlIrp;
    //DriverObject->MajorFunction[IRP_MJ_PNP]           = hfilterHandlePnpIrp;
	DriverObject->MajorFunction[IRP_MJ_SCSI]            = hfilterHandleSCSIIrp;


    hfilterConfig = hfilterMemoryAlloc(NonPagedPool, sizeof(hfilterConfigurationStruct));
   
    if(hfilterConfig==NULL)
        returnStatus = STATUS_INSUFFICIENT_RESOURCES;

    memset(hfilterConfig, 0, sizeof(hfilterConfigurationStruct));

    DriverObject->DriverExtension->AddDevice            = hfilterAddDevice;
    DriverObject->DriverUnload                          = hfilterUnload;
    
    return returnStatus;
}



/*
The AddDevice routine is responsible for creating functional device objects (FDO) 
or filter device objects (filter DO) for devices enumerated by the Plug and Play (PnP) manager.
*/
NTSTATUS hfilterAddDevice(IN PDRIVER_OBJECT DriverObject, IN PDEVICE_OBJECT PhysicalDeviceObject)
{
    //int addSubDeviceStatus = HFILTER_OK;
    NTSTATUS status;
    PDEVICE_OBJECT hfilterDeviceObject = NULL;
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    //int i = 0;
    
    //return STATUS_SUCCESS;

    hfilterDebugPrint("INFO: hfilterAddDevice: Driver %X Device %X\n", DriverObject, PhysicalDeviceObject);

#if 0
    for(i=0;i<HFILTER_MAX_NUMBER_OF_ADD_DEVICES;i++)
    {
        hfilterAddDeviceStruct *addDevice = &hfilterConfig->addDeviceList[i];

        if(addDevice->inuse == 0)
            continue;

        if(addDevice->physicalDeviceObject == PhysicalDeviceObject)
        {
            hfilterDebugPrint("INFO: hfilterAddDevice: Driver %X Device %X same PDO\n", DriverObject, PhysicalDeviceObject);
            return STATUS_SUCCESS;
        }
    }


    for(i=0;i<HFILTER_MAX_NUMBER_OF_ADD_DEVICES;i++)
    {
        hfilterAddDeviceStruct *addDevice = &hfilterConfig->addDeviceList[i];

        if(addDevice->inuse == 1)
            continue;

        addDevice->inuse = 1;
        addDevice->physicalDeviceObject = PhysicalDeviceObject;
    }
#endif

    status = IoCreateDevice(DriverObject,
                            sizeof(hfilterDeviceExtensionStruct),
                            NULL,
                            //FILE_DEVICE_DISK,
                            PhysicalDeviceObject->DeviceType,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &hfilterDeviceObject);

    if (!NT_SUCCESS(status)) {
       hfilterDebugPrint("ERROR: hfilterAddDevice - IoCreateDevice\n");
       return status;
    }

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) hfilterDeviceObject->DeviceExtension;

    memset(hfilterDeviceExtension, 0, sizeof(hfilterDeviceExtensionStruct));

    hfilterDeviceExtension->PDO = PhysicalDeviceObject;

    hfilterDeviceExtension->targetDeviceObject = IoAttachDeviceToDeviceStack(hfilterDeviceObject, PhysicalDeviceObject);

    if (hfilterDeviceExtension->targetDeviceObject == NULL) {
        IoDeleteDevice(hfilterDeviceObject);
        hfilterDebugPrint("ERROR: hfilterAddDevice - IoCreateDevice\n");
        return STATUS_NO_SUCH_DEVICE;
    }

    hfilterDeviceExtension->hfilterDeviceObject = hfilterDeviceObject;
    
    /*
    addSubDeviceStatus = hfilterAddSubDevice(hfilterDeviceExtension);
    hfilterDebugPrint("INFO: hfilterAddDevice - hfilterAddSubDevice %d\n", addSubDeviceStatus);
    */


    hfilterDeviceExtension->subDeviceIndex = HFILTER_INVALID_SUB_DEVICE_INDEX;
    hfilterDeviceExtension->deviceDescriptor = NULL;




    hfilterDebugPrint("INFO: hfilterAddDevice - hfilterDeviceObject: %p hfilterDeviceExtension: %p\n", hfilterDeviceObject, hfilterDeviceExtension);

    
    hfilterDeviceObject->Flags = PhysicalDeviceObject->Flags;

    hfilterDeviceObject->Flags &= ~DO_BUS_ENUMERATED_DEVICE;    
    hfilterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}


NTSTATUS hfilterSendIrpToNextDriver(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;

    IoSkipCurrentIrpStackLocation(Irp);

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) DeviceObject->DeviceExtension;

    return IoCallDriver(hfilterDeviceExtension->targetDeviceObject, Irp);
}


NTSTATUS hfilterSendIrpToNextDriverSynchronously(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    
    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) DeviceObject->DeviceExtension;

    if(IoForwardIrpSynchronously(hfilterDeviceExtension->targetDeviceObject, Irp))
    {
        hfilterDebugPrint("INFO: hfilterSendIrpToNextDriverSynchronously - good\n");
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    ASSERT(FALSE);

    return STATUS_INTERNAL_ERROR;
}




NTSTATUS hfilterSendIrpToDriver(IN PDEVICE_OBJECT targetDeviceObject, IN PIRP Irp)
{
    IoSkipCurrentIrpStackLocation(Irp);

    return IoCallDriver(targetDeviceObject, Irp);
}



void *hfilterMemoryAlloc(POOL_TYPE PoolType, SIZE_T NumberOfBytes)
{
    return ExAllocatePoolWithTag(PoolType, NumberOfBytes, HFILTER_MEMORY_POOL_ALLOC_TAG_1);
}

void hfilterMemoryFree(void *P)
{
    ExFreePoolWithTag(P, HFILTER_MEMORY_POOL_ALLOC_TAG_1);
}

void hfilterPrintBuffer(char *title, unsigned char *buf, unsigned int length, int printHeader)
{
    unsigned int i;
    unsigned int x;
    
    if(printHeader)
    {
        if(title)
            hfilterDebugPrint("%s\n", title);
        hfilterDebugPrint("buf:0x%p | length: %d\n", buf, length);
        hfilterDebugPrint("------------------------\n");
    }

    hfilterDebugPrint("      00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F ASCII\n");

    for( i = 0; i < length; i += 16 )
    {
        hfilterDebugPrint("%4x  ", i);

        for( x = 0; x < 16; x++)
        {
            hfilterDebugPrint("%02x ", *((unsigned char *)buf+i+x));
        }

        for( x = 0; x < 16; x++)
        {
            if((*((unsigned char *)buf+i+x) > 32 ) && (*((unsigned char *)buf+i+x) < 127 ))
            //if(*((unsigned char *)buf+i+x)!=0)
                hfilterDebugPrint("%c", *((unsigned char *)buf+i+x));
            else
                hfilterDebugPrint(".");
        }
        hfilterDebugPrint("\n");
    }
}



VOID hfilterUnload(IN PDRIVER_OBJECT DriverObject)
{
    //PAGED_CODE();

    UNREFERENCED_PARAMETER(DriverObject);

    return;
}