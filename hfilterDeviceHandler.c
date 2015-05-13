#include "hfilter.h"

VOID
ClassSendDeviceIoControlSynchronous(
    __in ULONG IoControlCode,
    __in PDEVICE_OBJECT TargetDeviceObject,
    __inout_xcount_opt(max(InputBufferLength,OutputBufferLength)) PVOID Buffer,
    __in ULONG InputBufferLength,
    __in ULONG OutputBufferLength,
    __in BOOLEAN InternalDeviceIoControl,
    __out PIO_STATUS_BLOCK IoStatus
    );


extern hfilterConfigurationStruct *hfilterConfig;











__drv_maxIRQL(PASSIVE_LEVEL)
NTSTATUS
ClassGetDescriptor(
    __in PDEVICE_OBJECT DeviceObject,
    __in PSTORAGE_PROPERTY_ID PropertyId,
    __deref_out PSTORAGE_DESCRIPTOR_HEADER *Descriptor
    )
{
    STORAGE_PROPERTY_QUERY query = {0};
    IO_STATUS_BLOCK ioStatus;

    PSTORAGE_DESCRIPTOR_HEADER descriptor = NULL;
    ULONG length;

    UCHAR pass = 0;

    PAGED_CODE();

    //
    // Set the passed-in descriptor pointer to NULL as default
    //

    *Descriptor = NULL;

    query.PropertyId = *PropertyId;
    query.QueryType = PropertyStandardQuery;

    //
    // On the first pass we just want to get the first few
    // bytes of the descriptor so we can read it's size
    //

    descriptor = (PVOID)&query;

    ASSERT(sizeof(STORAGE_PROPERTY_QUERY) >= (sizeof(ULONG)*2));

    ClassSendDeviceIoControlSynchronous(
        IOCTL_STORAGE_QUERY_PROPERTY,
        DeviceObject,
        &query,
        sizeof(STORAGE_PROPERTY_QUERY),
        sizeof(ULONG) * 2,
        FALSE,
        &ioStatus
        );

    if(!NT_SUCCESS(ioStatus.Status)) {

        hfilterDebugPrint("ClassGetDescriptor: error %lx trying to query properties #1\n", ioStatus.Status);
        return ioStatus.Status;
    }

    if (descriptor->Size == 0) {

        //
        // This DebugPrint is to help third-party driver writers
        //

        hfilterDebugPrint("ClassGetDescriptor: size returned was zero?! status %x\n", ioStatus.Status);
        return STATUS_UNSUCCESSFUL;

    }

    //
    // This time we know how much data there is so we can
    // allocate a buffer of the correct size
    //

    length = descriptor->Size;
    ASSERT(length >= sizeof(STORAGE_PROPERTY_QUERY));
    length = max(length, sizeof(STORAGE_PROPERTY_QUERY));

    descriptor = hfilterMemoryAlloc(NonPagedPool, length);

    if(descriptor == NULL) {

        hfilterDebugPrint("ClassGetDescriptor: unable to memory for descriptor (%d bytes)\n", length);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // setup the query again, as it was overwritten above
    //

    RtlZeroMemory(&query, sizeof(STORAGE_PROPERTY_QUERY));
    query.PropertyId = *PropertyId;
    query.QueryType = PropertyStandardQuery;

    //
    // copy the input to the new outputbuffer
    //

    RtlCopyMemory(descriptor,
                  &query,
                  sizeof(STORAGE_PROPERTY_QUERY)
                  );

    ClassSendDeviceIoControlSynchronous(
        IOCTL_STORAGE_QUERY_PROPERTY,
        DeviceObject,
        descriptor,
        sizeof(STORAGE_PROPERTY_QUERY),
        length,
        FALSE,
        &ioStatus
        );

    if(!NT_SUCCESS(ioStatus.Status)) {

        hfilterDebugPrint("ClassGetDescriptor: error %lx trying to query properties #1\n", ioStatus.Status);
        hfilterMemoryFree(descriptor);
        return ioStatus.Status;
    }

    //
    // return the memory we've allocated to the caller
    //

    *Descriptor = descriptor;
    return ioStatus.Status;
} // end ClassGetDescriptor()




NTSTATUS
ClassSignalCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    )
{
    KeSetEvent(Event, IO_NO_INCREMENT, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
} // end ClassSignalCompletion()




NTSTATUS
ClassSendIrpSynchronous(
    __in PDEVICE_OBJECT TargetDeviceObject,
    __in PIRP Irp
    )
{
    KEVENT event;
    NTSTATUS status;

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
    ASSERT(TargetDeviceObject != NULL);
    ASSERT(Irp != NULL);
    ASSERT(Irp->StackCount >= TargetDeviceObject->StackSize);

    //
    // ISSUE-2000/02/20-henrygab   What if APCs are disabled?
    //    May need to enter critical section before IoCallDriver()
    //    until the event is hit?
    //

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);
    IoSetCompletionRoutine(Irp, ClassSignalCompletion, &event,
                           TRUE, TRUE, TRUE);

    status = IoCallDriver(TargetDeviceObject, Irp);

    if (status == STATUS_PENDING) {

        #if DBG
            LARGE_INTEGER timeout;

            //timeout.QuadPart = (LONGLONG)(-1 * 10 * 1000 * (LONGLONG)1000 * ClasspnpGlobals.SecondsToWaitForIrps);
            timeout.QuadPart = (LONGLONG)(-1 * 10 * 1000 * (LONGLONG)1000 * 5);

            do {
                status = KeWaitForSingleObject(&event,
                                               Executive,
                                               KernelMode,
                                               FALSE,
                                               &timeout);


                if (status == STATUS_TIMEOUT) {

                    //
                    // This DebugPrint should almost always be investigated by the
                    // party who sent the irp and/or the current owner of the irp.
                    // Synchronous Irps should not take this long (currently 30
                    // seconds) without good reason.  This points to a potentially
                    // serious problem in the underlying device stack.
                    //

                    hfilterDebugPrint("ClassSendIrpSynchronous: (%p) irp %p did not "
                                "complete within %x seconds\n",
                                TargetDeviceObject, Irp,
                                5
                                );

                    if (1 != 0) {
                        ASSERT(!" - Irp failed to complete within 30 seconds - ");
                    }
                }


            } while (status==STATUS_TIMEOUT);
        #else
            (VOID)KeWaitForSingleObject(&event,
                                        Executive,
                                        KernelMode,
                                        FALSE,
                                        NULL);
        #endif

        status = Irp->IoStatus.Status;
    }

    return status;
} // end ClassSendIrpSynchronous()






VOID
ClassSendDeviceIoControlSynchronous(
    __in ULONG IoControlCode,
    __in PDEVICE_OBJECT TargetDeviceObject,
    __inout_xcount_opt(max(InputBufferLength,OutputBufferLength)) PVOID Buffer,
    __in ULONG InputBufferLength,
    __in ULONG OutputBufferLength,
    __in BOOLEAN InternalDeviceIoControl,
    __out PIO_STATUS_BLOCK IoStatus
    )
{
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    ULONG method;

    PAGED_CODE();

    irp = NULL;
    method = IoControlCode & 3;

    #if DBG // Begin Argument Checking (nop in fre version)

        ASSERT(ARGUMENT_PRESENT(IoStatus));

        if ((InputBufferLength != 0) || (OutputBufferLength != 0)) {
            ASSERT(ARGUMENT_PRESENT(Buffer));
        }
        else {
            ASSERT(!ARGUMENT_PRESENT(Buffer));
        }
    #endif

    //
    // Begin by allocating the IRP for this request.  Do not charge quota to
    // the current process for this IRP.
    //

    irp = IoAllocateIrp(TargetDeviceObject->StackSize, FALSE);
    if (!irp) {
        IoStatus->Information = 0;
        IoStatus->Status = STATUS_INSUFFICIENT_RESOURCES;
        return;
    }

    //
    // Get a pointer to the stack location of the first driver which will be
    // invoked.  This is where the function codes and the parameters are set.
    //

    irpSp = IoGetNextIrpStackLocation(irp);

    //
    // Set the major function code based on the type of device I/O control
    // function the caller has specified.
    //

    if (InternalDeviceIoControl) {
        irpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    } else {
        irpSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
    }

    //
    // Copy the caller's parameters to the service-specific portion of the
    // IRP for those parameters that are the same for all four methods.
    //

    irpSp->Parameters.DeviceIoControl.OutputBufferLength = OutputBufferLength;
    irpSp->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;
    irpSp->Parameters.DeviceIoControl.IoControlCode = IoControlCode;

    //
    // Get the method bits from the I/O control code to determine how the
    // buffers are to be passed to the driver.
    //

    switch (method)
    {
        //
        // case 0
        //
        case METHOD_BUFFERED:
        {
            if ((InputBufferLength != 0) || (OutputBufferLength != 0))
            {
                irp->AssociatedIrp.SystemBuffer = hfilterMemoryAlloc(NonPagedPoolCacheAligned, max(InputBufferLength, OutputBufferLength));
                if (irp->AssociatedIrp.SystemBuffer == NULL)
                {
                    IoFreeIrp(irp);

                    IoStatus->Information = 0;
                    IoStatus->Status = STATUS_INSUFFICIENT_RESOURCES;
                    return;
                }

                if (InputBufferLength != 0)
                {
                    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, Buffer, InputBufferLength);
                }
            }

            irp->UserBuffer = Buffer;

            break;
        }

        //
        // case 1, case 2
        //
        case METHOD_IN_DIRECT:
        case METHOD_OUT_DIRECT:
        {
            if (InputBufferLength != 0)
            {
                irp->AssociatedIrp.SystemBuffer = Buffer;
            }

            if (OutputBufferLength != 0)
            {
                irp->MdlAddress = IoAllocateMdl(Buffer,
                                                OutputBufferLength,
                                                FALSE,
                                                FALSE,
                                                (PIRP) NULL);
                if (irp->MdlAddress == NULL)
                {
                    IoFreeIrp(irp);

                    IoStatus->Information = 0;
                    IoStatus->Status = STATUS_INSUFFICIENT_RESOURCES;
                    return;
                }

                try
                {
                    MmProbeAndLockPages(irp->MdlAddress,
                                        KernelMode,
                                        (method == METHOD_IN_DIRECT) ? IoReadAccess : IoWriteAccess);
                }
                except(EXCEPTION_EXECUTE_HANDLER)
                {
                    IoFreeMdl(irp->MdlAddress);
                    IoFreeIrp(irp);

                    IoStatus->Information = 0;
                    IoStatus->Status = GetExceptionCode();
                    return;
                }
            }

            break;
        }

        //
        // case 3
        //
        case METHOD_NEITHER:
        {
            ASSERT(!"ClassSendDeviceIoControlSynchronous does not support METHOD_NEITHER Ioctls");

            IoFreeIrp(irp);

            IoStatus->Information = 0;
            IoStatus->Status = STATUS_NOT_SUPPORTED;
            return;
        }
    }

    irp->Tail.Overlay.Thread = PsGetCurrentThread();

    //
    // send the irp synchronously
    //

    ClassSendIrpSynchronous(TargetDeviceObject, irp);

    //
    // copy the iostatus block for the caller
    //

    *IoStatus = irp->IoStatus;

    //
    // free any allocated resources
    //

    switch (method) {
        case METHOD_BUFFERED: {

            ASSERT(irp->UserBuffer == Buffer);

            //
            // first copy the buffered result, if any
            // Note that there are no security implications in
            // not checking for success since only drivers can
            // call into this routine anyways...
            //

            if (OutputBufferLength != 0) {
                RtlCopyMemory(Buffer, // irp->UserBuffer
                              irp->AssociatedIrp.SystemBuffer,
                              OutputBufferLength
                              );
            }

            //
            // then free the memory allocated to buffer the io
            //

            if ((InputBufferLength !=0) || (OutputBufferLength != 0)) {
                hfilterMemoryFree(irp->AssociatedIrp.SystemBuffer);
            }
            break;
        }

        case METHOD_IN_DIRECT:
        case METHOD_OUT_DIRECT: {

            //
            // we alloc a mdl if there is an output buffer specified
            // free it here after unlocking the pages
            //

            if (OutputBufferLength != 0) {
                ASSERT(irp->MdlAddress != NULL);
                MmUnlockPages(irp->MdlAddress);
                IoFreeMdl(irp->MdlAddress);
                irp->MdlAddress = (PMDL) NULL;
            }
            break;
        }

        case METHOD_NEITHER: {
            ASSERT(!"Code is out of date");
            break;
        }
    }

    //
    // we always have allocated an irp.  free it here.
    //

    IoFreeIrp(irp);
    irp = (PIRP) NULL;

    //
    // return the io status block's status to the caller
    //

    return;
} // end ClassSendDeviceIoControlSynchronous()



static int vhdIndex = 0;

int hfilterAddSubDevice(hfilterDeviceExtensionStruct *hfilterDeviceExtension)
{
    STORAGE_PROPERTY_ID propertyId = StorageDeviceProperty;
    int returnStatus = HFILTER_OK;
    NTSTATUS status = STATUS_SUCCESS;


    hfilterDebugPrint("INFO: hfilterAddSubDevice\n");

    if(hfilterDeviceExtension==NULL)
        return HFILTER_ERROR;

    hfilterDeviceExtension->subDeviceIndex = HFILTER_INVALID_SUB_DEVICE_INDEX;
    hfilterDeviceExtension->deviceDescriptor = NULL;
    
    status = ClassGetDescriptor(hfilterDeviceExtension->targetDeviceObject, &propertyId, &hfilterDeviceExtension->deviceDescriptor);

    if (NT_SUCCESS(status) && (hfilterDeviceExtension->deviceDescriptor))
    {
        char *serialNumber = "";
        char *vendorID = "";
        char *productID = "";
        char *structStartCharPtr = (char *)hfilterDeviceExtension->deviceDescriptor;

        hfilterDebugPrint("INFO: hfilterAddSubDevice - ClassGetDescriptor good\n");
        //hfilterPrintBuffer("deviceDescriptor", deviceDescriptor, deviceDescriptor->Size, 1);

        if ((hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset != -1))
        {
            serialNumber = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset;
        }

        if ((hfilterDeviceExtension->deviceDescriptor->VendorIdOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->VendorIdOffset != -1))
        {
            vendorID = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->VendorIdOffset;
        }

        if ((hfilterDeviceExtension->deviceDescriptor->ProductIdOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->ProductIdOffset != -1))
        {
            productID = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->ProductIdOffset;
        }

        hfilterDebugPrint("INFO: hfilterAddSubDevice - vendorID: [%s] productID: [%s] serialNumber: [%s] BusType: [0x%x]\n", vendorID, productID, serialNumber, hfilterDeviceExtension->deviceDescriptor->BusType);

        if(hfilterDeviceExtension->deviceDescriptor->RawPropertiesLength)
        {
            hfilterPrintBuffer("raw properties", (unsigned char *)hfilterDeviceExtension->deviceDescriptor->RawDeviceProperties, hfilterDeviceExtension->deviceDescriptor->RawPropertiesLength, 1);
        }



#if 0
        #define SSD_SUB_DEVICE_SERIAL_NUMBER   "3030303030303030303030303030303030303130"
        #define HDD_SUB_DEVICE_SERIAL_NUMBER   "3130303030303030303030303030303030303130"

        if(strncmp(serialNumber, SSD_SUB_DEVICE_SERIAL_NUMBER, strlen(SSD_SUB_DEVICE_SERIAL_NUMBER))==0)
        {
            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
            hfilterDeviceExtension->subDeviceIndex = HFILTER_SSD_SUB_DEVICE_INDEX;
            hfilterDebugPrint("INFO: hfilterAddSubDevice - found SSD subdevice\n");


            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].inuse = 1;
            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].startLBA = 8192;
            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].endLBA = 16383;
        }

        if(strncmp(serialNumber, HDD_SUB_DEVICE_SERIAL_NUMBER, strlen(HDD_SUB_DEVICE_SERIAL_NUMBER))==0)
        {
            hfilterConfig->subDeviceList[HFILTER_HDD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
            hfilterDeviceExtension->subDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;
            hfilterDebugPrint("INFO: hfilterAddSubDevice - found HDD subdevice\n");
        }

        if(hfilterDeviceExtension->deviceDescriptor->BusType==BusTypeUsb)
        {
            hfilterDebugPrint("INFO: hfilterAddSubDevice - found USB subdevice\n");
            hfilterDeviceExtension->subDeviceIndex = HFILTER_USB_SUB_DEVICE_INDEX;
        }

        if(hfilterDeviceExtension->deviceDescriptor->BusType==BusTypeFileBackedVirtual)
        {
            hfilterDebugPrint("INFO: hfilterAddSubDevice - found VHD subdevice\n");
            hfilterDeviceExtension->subDeviceIndex = HFILTER_VHD_SUB_DEVICE_INDEX;
        }
#endif
        
#if 0    
        if(hfilterDeviceExtension->deviceDescriptor->BusType==BusTypeUsb)
        {
            hfilterDebugPrint("INFO: hfilterAddSubDevice - found USB subdevice\n");
            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
            hfilterDeviceExtension->subDeviceIndex = HFILTER_SSD_SUB_DEVICE_INDEX;
            hfilterDebugPrint("INFO: hfilterAddSubDevice - map USB to SSD subdevice\n");
            
            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].inuse = 1;
            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].startLBA = 0;
            hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].endLBA = 0x1836E21;
        }

        if(hfilterDeviceExtension->deviceDescriptor->BusType==BusTypeFileBackedVirtual)
        {
            hfilterDebugPrint("INFO: hfilterAddSubDevice - found VHD subdevice\n");
            hfilterConfig->subDeviceList[HFILTER_HDD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
            hfilterDeviceExtension->subDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;
            hfilterDebugPrint("INFO: hfilterAddSubDevice - map VHD to HDD subdevice\n");
        }
#endif
        if(hfilterDeviceExtension->deviceDescriptor->BusType==BusTypeFileBackedVirtual)
        {
            hfilterDebugPrint("INFO: hfilterAddSubDevice - found VHD subdevice\n");
            
            if(vhdIndex==0)
            {
                hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
                hfilterDeviceExtension->subDeviceIndex = HFILTER_SSD_SUB_DEVICE_INDEX;
                hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].inuse = 1;
                
                //hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].endLBA = 0x2800;  //5MB
                //hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].endLBA = 0x1836E21; //13GB
                //hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].endLBA = 0xFA000; //500MB

                hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].startLBA = 0x32000; //offset @ 100MB
                hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].endLBA =   0xFA000; //end @ 500MB

                hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].mappedStartLBA = 0xFA000; //map start @ 500MB
                hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].mappedEndLBA =   0x1C2000; //end @ 900MB

                hfilterDebugPrint("INFO: hfilterAddSubDevice - SSD range 0x32000 to 0xFA000\n");
                hfilterDebugPrint("INFO: hfilterAddSubDevice - SSD map   0xFA000 to 0x1C2000\n");
                hfilterDebugPrint("INFO: hfilterAddSubDevice - map VHD to SSD subdevice\n");
            }
            if(vhdIndex==1)
            {
                hfilterConfig->subDeviceList[HFILTER_HDD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
                hfilterDeviceExtension->subDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;
                hfilterDebugPrint("INFO: hfilterAddSubDevice - map VHD to HDD subdevice\n");
            }
            vhdIndex++;
        }

        returnStatus = HFILTER_OK;
    } else {
        hfilterDebugPrint("INFO: ClassGetDescriptor !good\n");
        returnStatus = HFILTER_ERROR;
    }

    return returnStatus;
}




































int hfilterSetSubDeviceSSD(hfilterDeviceExtensionStruct *hfilterDeviceExtension)
{
    STORAGE_PROPERTY_ID propertyId = StorageDeviceProperty;
    int returnStatus = HFILTER_OK;
    NTSTATUS status = STATUS_SUCCESS;


    hfilterDebugPrint("INFO: hfilterSetSubDeviceSSD\n");

    if(hfilterDeviceExtension==NULL)
        return HFILTER_ERROR;

    hfilterDeviceExtension->subDeviceIndex = HFILTER_INVALID_SUB_DEVICE_INDEX;
    hfilterDeviceExtension->deviceDescriptor = NULL;
    
    status = ClassGetDescriptor(hfilterDeviceExtension->targetDeviceObject, &propertyId, &hfilterDeviceExtension->deviceDescriptor);

    if (NT_SUCCESS(status) && (hfilterDeviceExtension->deviceDescriptor))
    {
        char *serialNumber = "";
        char *vendorID = "";
        char *productID = "";
        char *structStartCharPtr = (char *)hfilterDeviceExtension->deviceDescriptor;

        hfilterDebugPrint("INFO: hfilterSetSubDeviceSSD - ClassGetDescriptor good\n");

        if ((hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset != -1))
        {
            serialNumber = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset;
        }

        if ((hfilterDeviceExtension->deviceDescriptor->VendorIdOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->VendorIdOffset != -1))
        {
            vendorID = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->VendorIdOffset;
        }

        if ((hfilterDeviceExtension->deviceDescriptor->ProductIdOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->ProductIdOffset != -1))
        {
            productID = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->ProductIdOffset;
        }

        hfilterDebugPrint("INFO: hfilterSetSubDeviceSSD - vendorID: [%s] productID: [%s] serialNumber: [%s] BusType: [0x%x]\n", vendorID, productID, serialNumber, hfilterDeviceExtension->deviceDescriptor->BusType);

        if(hfilterDeviceExtension->deviceDescriptor->RawPropertiesLength)
        {
            hfilterPrintBuffer("raw properties", (unsigned char *)hfilterDeviceExtension->deviceDescriptor->RawDeviceProperties, hfilterDeviceExtension->deviceDescriptor->RawPropertiesLength, 1);
        }
    } else {
        hfilterDebugPrint("INFO: hfilterSetSubDeviceSSD - ClassGetDescriptor !ok\n");
    }

#if 0
    hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].inuse = 1;                

    hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].startLBA = 0x0;
    hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].endLBA =   0x2800; //5MB

    hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].mappedStartLBA = 0x20;
    hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[0].mappedEndLBA =   0x2820;

    hfilterDebugPrint("INFO: hfilterSetSubDeviceSSD - SSD LBA range 0x0  to 0x2800\n");
    hfilterDebugPrint("INFO: hfilterSetSubDeviceSSD - SSD map LBA   0x20 to 0x2820\n");
#endif
    
    hfilterDeviceExtension->subDeviceIndex = HFILTER_SSD_SUB_DEVICE_INDEX;
    hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;



    returnStatus = HFILTER_OK;

    return returnStatus;
}


int hfilterSetSubDeviceHDD(hfilterDeviceExtensionStruct *hfilterDeviceExtension)
{
    STORAGE_PROPERTY_ID propertyId = StorageDeviceProperty;
    int returnStatus = HFILTER_OK;
    NTSTATUS status = STATUS_SUCCESS;


    hfilterDebugPrint("INFO: hfilterSetSubDeviceHDD\n");

    if(hfilterDeviceExtension==NULL)
        return HFILTER_ERROR;

    hfilterDeviceExtension->subDeviceIndex = HFILTER_INVALID_SUB_DEVICE_INDEX;
    hfilterDeviceExtension->deviceDescriptor = NULL;
    
    status = ClassGetDescriptor(hfilterDeviceExtension->targetDeviceObject, &propertyId, &hfilterDeviceExtension->deviceDescriptor);

    if (NT_SUCCESS(status) && (hfilterDeviceExtension->deviceDescriptor))
    {
        char *serialNumber = "";
        char *vendorID = "";
        char *productID = "";
        char *structStartCharPtr = (char *)hfilterDeviceExtension->deviceDescriptor;

        hfilterDebugPrint("INFO: hfilterSetSubDeviceHDD - ClassGetDescriptor good\n");

        if ((hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset != -1))
        {
            serialNumber = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->SerialNumberOffset;
        }

        if ((hfilterDeviceExtension->deviceDescriptor->VendorIdOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->VendorIdOffset != -1))
        {
            vendorID = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->VendorIdOffset;
        }

        if ((hfilterDeviceExtension->deviceDescriptor->ProductIdOffset != 0) && (hfilterDeviceExtension->deviceDescriptor->ProductIdOffset != -1))
        {
            productID = structStartCharPtr + hfilterDeviceExtension->deviceDescriptor->ProductIdOffset;
        }

        hfilterDebugPrint("INFO: hfilterSetSubDeviceHDD - vendorID: [%s] productID: [%s] serialNumber: [%s] BusType: [0x%x]\n", vendorID, productID, serialNumber, hfilterDeviceExtension->deviceDescriptor->BusType);

        if(hfilterDeviceExtension->deviceDescriptor->RawPropertiesLength)
        {
            hfilterPrintBuffer("raw properties", (unsigned char *)hfilterDeviceExtension->deviceDescriptor->RawDeviceProperties, hfilterDeviceExtension->deviceDescriptor->RawPropertiesLength, 1);
        }
    } else {
        hfilterDebugPrint("INFO: hfilterSetSubDeviceHDD - ClassGetDescriptor !ok\n");
    }

    hfilterConfig->subDeviceList[HFILTER_HDD_SUB_DEVICE_INDEX].targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
    hfilterDeviceExtension->subDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;

    returnStatus = HFILTER_OK;

    return returnStatus;
}

int hfilterSetSubDeviceInfo(hfilterSubDeviceInfoStruct *subDeviceInfo, DEVICE_OBJECT *devObj)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    int index = 0;
    hfilterLBARangeStruct *inputLBARange = NULL;
    hfilterLBARangeStruct *subDeviceLBARange = NULL;

    if((subDeviceInfo==NULL)||(devObj==NULL))
    {
        hfilterDebugPrint("ERROR: hfilterSetSubDeviceInfo - param check 1\n");
        return HFILTER_ERROR;
    }
    
    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) devObj->DeviceExtension;

    if(hfilterDeviceExtension==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterSetSubDeviceInfo - param check 2\n");
        return HFILTER_ERROR;
	}

    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_SSD_SUB_DEVICE_INDEX)
    {
        hfilterDebugPrint("ERROR: hfilterSetSubDeviceInfo - not ssd subdevice\n");
        return HFILTER_ERROR;
    }

    if(subDeviceInfo->magicNumber!=HFILTER_SUB_DEVICE_INFO_MAGIC_NUMBER)
    {
        hfilterDebugPrint("ERROR: hfilterSetSubDeviceInfo - magic check\n");
        return HFILTER_ERROR;
    }

    //updating lba ranges...
    for(index=0;index<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES;index++)
    {
        inputLBARange = &subDeviceInfo->lbaRangeList[index];
        subDeviceLBARange = &hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[index];

        subDeviceLBARange->inuse = inputLBARange->inuse;
        subDeviceLBARange->startLBA = inputLBARange->startLBA;
        subDeviceLBARange->endLBA = inputLBARange->endLBA;
        subDeviceLBARange->mappedStartLBA = inputLBARange->mappedStartLBA;
        subDeviceLBARange->mappedEndLBA = inputLBARange->mappedEndLBA;
    }

    {
        for(index=0;index<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES;index++)
        {
            subDeviceLBARange = &hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[index];
            hfilterDebugPrint("index [%d]\n", index);
            hfilterDebugPrint("inuse:          0x%llx\n", subDeviceLBARange->inuse);
            hfilterDebugPrint("startLBA:       0x%llx\n", subDeviceLBARange->startLBA);
            hfilterDebugPrint("endLBA:         0x%llx\n", subDeviceLBARange->endLBA);
            hfilterDebugPrint("\n"); 
            hfilterDebugPrint("mappedStartLBA: 0x%llx\n", subDeviceLBARange->mappedStartLBA);
            hfilterDebugPrint("mappedEndLBA    0x%llx\n", subDeviceLBARange->mappedEndLBA);
            hfilterDebugPrint("\n"); 
        }
    }

    return HFILTER_OK;
}