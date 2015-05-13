#include "hfilter.h"
/*
typedef struct{
    PDEVICE_OBJECT DeviceObject;
    SCSI_REQUEST_BLOCK srb;
    UINT8 *dataTransferBuffer;
    UINT32 dataTransferBufferSize;
    UINT8 *senseDataBuffer;
    UINT32 senseDataBufferSize;
} zfilterDeviceDSMCommandContextStruct;

int zfilterHandleDSMTrim(PDEVICE_OBJECT DeviceObject, zfilterDeviceExtensionStruct *zfilterDeviceExtension, PIRP Irp, DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa);
int zfilterHandleDSMNotification(PDEVICE_OBJECT DeviceObject, PIRP Irp, DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa);

int zfilterSendDeviceDSMCommand(PDEVICE_OBJECT DeviceObject, zfilterDeviceExtensionStruct *zfilterDeviceExtension, PIRP DSMIOCTLIrp, LONGLONG byteOffset, ULONGLONG byteLength);
void zfilterBuildDeviceDSMCDB(UINT64 startingLBA, UINT32 sectorCount, UCHAR *cdb, UCHAR *cdbLength);

int zfilterBuildDeviceDSMSRB(SCSI_REQUEST_BLOCK *srb, IRP *irp, zfilterDeviceDSMCommandContextStruct *commandContext, zfilterDeviceExtensionStruct *zfilterDeviceExtension, LONGLONG byteOffset, ULONGLONG byteLength);
int zfilterBuildDeviceDSMIRP(IRP *irp, SCSI_REQUEST_BLOCK *srb, zfilterDeviceDSMCommandContextStruct *commandContext);

NTSTATUS zfilterDeviceDSMCommandIrpCompletionCallback(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

void zfilterPrintDSMIrp(PIRP Irp);
void zfilterGetDeviceDSMSupport(PDEVICE_OBJECT DeviceObject, zfilterDeviceDSMSupportStruct *deviceDSMSupportPtr);

//top level - returns fwd or complete
int zfilterHandleDSMIOCTL(PDEVICE_OBJECT DeviceObject, PIRP Irp, DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa)
{
    zfilterDeviceExtensionStruct *zfilterDeviceExtension = NULL;
    int status = ZFILTER_FORWARD_REQUEST;

    if((DeviceObject==NULL) || (Irp==NULL) || (dsa==NULL))
        return ZFILTER_FORWARD_REQUEST;

    zfilterDeviceExtension = (zfilterDeviceExtensionStruct *) DeviceObject->DeviceExtension;

    if(zfilterDeviceExtension==NULL)
        return ZFILTER_FORWARD_REQUEST;

    //zfilterPrintDSMIrp(Irp);

    //get device dsm support info
    zfilterGetDeviceDSMSupport(DeviceObject, &zfilterDeviceExtension->deviceDSMSupport);

    zfilterDebugPrint("INFO: zfilterHandleDSMIOCTL - action: 0x%x\n", dsa->Action);

    if(dsa->Action&DeviceDsmAction_Trim)
        status = zfilterHandleDSMTrim(DeviceObject, zfilterDeviceExtension, Irp, dsa);

    if(dsa->Action&DeviceDsmAction_Notification)
        status = zfilterHandleDSMNotification(DeviceObject, Irp, dsa);

    return status;
}

//2nd level - returns fwd or complete
int zfilterHandleDSMTrim(PDEVICE_OBJECT DeviceObject, zfilterDeviceExtensionStruct *zfilterDeviceExtension, PIRP Irp, DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa)
{
    int status = ZFILTER_ERROR;
    int returnStatus = ZFILTER_COMPLETE_REQUEST;
    UCHAR *tempCharPtr = NULL;
    
    if((DeviceObject==NULL) || (zfilterDeviceExtension==NULL) || (Irp==NULL) || (dsa==NULL))
        return ZFILTER_FORWARD_REQUEST;


    tempCharPtr = (UCHAR *)dsa;

    if((dsa->DataSetRangesOffset==0) || (dsa->DataSetRangesLength==0))
    {
        //data set ranges does not exist
    } else {
        DWORD dataSetRangeIndex = 0;
        DWORD numDataSetRanges = 0;
        DEVICE_DATA_SET_RANGE *dsr = NULL;

        dsr = (DEVICE_DATA_SET_RANGE *)(tempCharPtr + dsa->DataSetRangesOffset);
        numDataSetRanges = dsa->DataSetRangesLength / sizeof(DEVICE_DATA_SET_RANGE);

        zfilterDebugPrint("INFO: zfilterHandleDSMTrim - numDataSetRanges: %d [%d]\n", numDataSetRanges, sizeof(DEVICE_DATA_SET_RANGE));

        for(dataSetRangeIndex = 0; dataSetRangeIndex < numDataSetRanges; dataSetRangeIndex++)
        {
            zfilterDebugPrint("INFO: zfilterHandleDSMTrim - StartingOffset [0x%llx 0x%llx]\n", dsr->StartingOffset, dsr->LengthInBytes);

            status = zfilterSendDeviceDSMCommand(DeviceObject, zfilterDeviceExtension, Irp, dsr->StartingOffset, dsr->LengthInBytes);

            zfilterDebugPrint("INFO: zfilterHandleDSMTrim - zfilterSendDeviceDSMCommand: %d\n", status);

            if(status!=ZFILTER_OK)
            {
                returnStatus = ZFILTER_FORWARD_REQUEST;
            }

            dsr++;
        }
    }

    return returnStatus;
}






//3rd level - returns ok or error
int zfilterSendDeviceDSMCommand(PDEVICE_OBJECT DeviceObject, zfilterDeviceExtensionStruct *zfilterDeviceExtension, PIRP DSMIOCTLIrp, LONGLONG byteOffset, ULONGLONG byteLength)
{
    zfilterDeviceDSMCommandContextStruct *commandContext = NULL;
    PIRP irp = NULL;
    PSCSI_REQUEST_BLOCK srb = NULL;
    NTSTATUS driverCallStatus = STATUS_SUCCESS;
    int status = ZFILTER_OK;

    int returnStatus = ZFILTER_OK;


    if((DeviceObject==NULL) || (zfilterDeviceExtension==NULL) || (DSMIOCTLIrp==NULL))
    {
        returnStatus = ZFILTER_ERROR;
        goto zfilterSendDeviceDSMCommand_exit;
    }

    //alloc command context and irp
    commandContext = zfilterMemoryAlloc(NonPagedPool, sizeof(zfilterDeviceDSMCommandContextStruct));
    irp = IoAllocateIrp(zfilterDeviceExtension->targetDeviceObject->StackSize, FALSE);

    if((commandContext==NULL) || (irp==NULL))
    {
        returnStatus = ZFILTER_ERROR;
        goto zfilterSendDeviceDSMCommand_exit;
    }

    memset(commandContext, 0, sizeof(zfilterDeviceDSMCommandContextStruct));

    //alloc data transfer buffer
    if(zfilterDeviceExtension->deviceDSMSupport.dataTransferBufferSize)
    {
        commandContext->dataTransferBufferSize = zfilterDeviceExtension->deviceDSMSupport.dataTransferBufferSize;
        commandContext->dataTransferBuffer = zfilterMemoryAlloc(NonPagedPool, commandContext->dataTransferBufferSize);
        if(commandContext->dataTransferBuffer==NULL)
        {
            returnStatus = ZFILTER_ERROR;
            goto zfilterSendDeviceDSMCommand_exit;
        }
        memset(commandContext->dataTransferBuffer, 0, commandContext->dataTransferBufferSize);
    } else {
        commandContext->dataTransferBufferSize = 0;
        commandContext->dataTransferBuffer = NULL;
    }

    //alloc sense data buffer
    if(zfilterDeviceExtension->deviceDSMSupport.senseDataBufferSize)
    {
        commandContext->senseDataBufferSize = zfilterDeviceExtension->deviceDSMSupport.senseDataBufferSize;
        commandContext->senseDataBuffer = zfilterMemoryAlloc(NonPagedPool, commandContext->senseDataBufferSize);
        if(commandContext->senseDataBuffer==NULL)
        {
            returnStatus = ZFILTER_ERROR;
            goto zfilterSendDeviceDSMCommand_exit;
        }
        memset(commandContext->senseDataBuffer, 0, commandContext->senseDataBufferSize);
    } else {
        commandContext->senseDataBufferSize = 0;
        commandContext->senseDataBuffer = NULL;
    }

    srb = &commandContext->srb;

    //setup SRB
    status = zfilterBuildDeviceDSMSRB(srb, irp, commandContext, zfilterDeviceExtension, byteOffset, byteLength);
    
    if(status!=ZFILTER_OK)
    {
        returnStatus = ZFILTER_ERROR;
        goto zfilterSendDeviceDSMCommand_exit;
    }

    //setup IRP
    status = zfilterBuildDeviceDSMIRP(irp, srb, commandContext);
    
    if(status!=ZFILTER_OK)
    {
        returnStatus = ZFILTER_ERROR;
        goto zfilterSendDeviceDSMCommand_exit;
    }


    //send the irp on its way...
    driverCallStatus = IoCallDriver(zfilterDeviceExtension->targetDeviceObject, irp);

zfilterSendDeviceDSMCommand_exit:
    if(returnStatus==ZFILTER_ERROR)
    {
        if(commandContext)
        {
            if(commandContext->dataTransferBuffer)
                zfilterMemoryFree(commandContext->dataTransferBuffer);

            if(commandContext->senseDataBuffer)
                zfilterMemoryFree(commandContext->senseDataBuffer);

            zfilterMemoryFree(commandContext);
        }

        if(irp)
            IoFreeIrp(irp);
    }

    return returnStatus;
}

UINT64 zfilterByteToBlock(UINT64 val, UINT64 blockLength)
{
    return (UINT64) (val/blockLength);
}

//4th level - returns ok or error
int zfilterBuildDeviceDSMSRB(SCSI_REQUEST_BLOCK *srb, IRP *irp, zfilterDeviceDSMCommandContextStruct *commandContext, zfilterDeviceExtensionStruct *zfilterDeviceExtension, LONGLONG byteOffset, ULONGLONG byteLength)
{
    UINT64 startingLBA = 0;
    UINT32 sectorCount = 0;

    if((srb==NULL) || (irp==NULL) || (commandContext==NULL) || (zfilterDeviceExtension==NULL))
    {
        return ZFILTER_ERROR;
    }

    memset(srb, 0, sizeof(SCSI_REQUEST_BLOCK));

    srb->Length = sizeof(SCSI_REQUEST_BLOCK);
    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    //Srb->QueueTag = SP_UNTAGGED;
    srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;

    startingLBA =  zfilterByteToBlock((UINT64) byteOffset, zfilterDeviceExtension->deviceDSMSupport.deviceBlockLength);
    sectorCount =  (UINT32) zfilterByteToBlock((UINT64) byteLength, zfilterDeviceExtension->deviceDSMSupport.deviceBlockLength);

    zfilterBuildDeviceDSMCDB(startingLBA, sectorCount, srb->Cdb, &srb->CdbLength);

    srb->SrbFlags = SRB_FLAGS_DATA_OUT | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
    
    srb->DataTransferLength = commandContext->dataTransferBufferSize;
    srb->DataBuffer = commandContext->dataTransferBuffer;
    
    srb->SenseInfoBufferLength = commandContext->senseDataBufferSize;
    srb->SenseInfoBuffer = commandContext->senseDataBuffer;

    srb->TimeOutValue = ZFILTER_DEVICE_DSM_COMMAND_TIMEOUT_VALUE;
    
    srb->OriginalRequest = irp;

    srb->QueueSortKey = (ULONG) startingLBA;

    
    zfilterPrintBuffer("CDB", srb->Cdb, srb->CdbLength, 1);
    
    return ZFILTER_OK;
}

void zfilterBuildDeviceDSMCDB(UINT64 startingLBA, UINT32 sectorCount, UCHAR *cdb, UCHAR *cdbLength)
{
    zfilterSCSIWriteSame16CDBStruct *writeSame16CDB = NULL;

    writeSame16CDB = (zfilterSCSIWriteSame16CDBStruct *) cdb;
    
    writeSame16CDB->opcode = SCSIOP_WRITE_SAME16;

    writeSame16CDB->lbdata = 0;
    writeSame16CDB->pbdata = 0;

    writeSame16CDB->unmap = 1;

    writeSame16CDB->lba[0] = GET_BYTE(startingLBA, 7); //(MSB)
    writeSame16CDB->lba[1] = GET_BYTE(startingLBA, 6);
    writeSame16CDB->lba[2] = GET_BYTE(startingLBA, 5);
    writeSame16CDB->lba[3] = GET_BYTE(startingLBA, 4);
    writeSame16CDB->lba[4] = GET_BYTE(startingLBA, 3);
    writeSame16CDB->lba[5] = GET_BYTE(startingLBA, 2);
    writeSame16CDB->lba[6] = GET_BYTE(startingLBA, 1);
    writeSame16CDB->lba[7] = GET_BYTE(startingLBA, 0); //LSB

    writeSame16CDB->transferBlockCount[0] = GET_BYTE(sectorCount, 3); //(MSB)
    writeSame16CDB->transferBlockCount[1] = GET_BYTE(sectorCount, 2);
    writeSame16CDB->transferBlockCount[2] = GET_BYTE(sectorCount, 1);
    writeSame16CDB->transferBlockCount[3] = GET_BYTE(sectorCount, 0); //LSB

    *cdbLength = 16;
}

//4th level - returns ok or error
int zfilterBuildDeviceDSMIRP(IRP *irp, SCSI_REQUEST_BLOCK *srb, zfilterDeviceDSMCommandContextStruct *commandContext)
{
    PIO_STACK_LOCATION irpStack = NULL;

    if((irp==NULL) || (srb==NULL) || (commandContext==NULL))
    {
        return ZFILTER_ERROR;
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    if(irpStack==NULL)
    {
        return ZFILTER_ERROR;
    }

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->Parameters.Scsi.Srb = srb;

    IoSetCompletionRoutine(irp, (PIO_COMPLETION_ROUTINE)zfilterDeviceDSMCommandIrpCompletionCallback, commandContext, TRUE, TRUE, TRUE);

    return ZFILTER_OK;
}

//2nd level - return fwd or complete
int zfilterHandleDSMNotification(PDEVICE_OBJECT DeviceObject, PIRP Irp, DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa)
{
    UCHAR *tempCharPtr = NULL;
    tempCharPtr = (UCHAR *)dsa;

    //debug
        return ZFILTER_FORWARD_REQUEST;

    if((DeviceObject==NULL) || (Irp==NULL) || (dsa==NULL))
        return ZFILTER_FORWARD_REQUEST;

    if((dsa->ParameterBlockOffset==0) || (dsa->ParameterBlockLength==0))
    {
        //parameter block does not exist
    } else {
        ULONG fileTypeIndex = 0;
        DEVICE_DSM_NOTIFICATION_PARAMETERS *notificationParameters = NULL;

        notificationParameters = (DEVICE_DSM_NOTIFICATION_PARAMETERS *)(tempCharPtr + dsa->ParameterBlockOffset);

        zfilterDebugPrint("INFO: zfilterHandleDSMNotification - Size: %d\n", notificationParameters->Size);

        switch(notificationParameters->Flags)
        {
            case DEVICE_DSM_NOTIFY_FLAG_BEGIN:
                                                zfilterDebugPrint("INFO: zfilterHandleDSMNotification - DEVICE_DSM_NOTIFY_FLAG_BEGIN\n");
                                                break;
            case DEVICE_DSM_NOTIFY_FLAG_END:
                                                zfilterDebugPrint("INFO: zfilterHandleDSMNotification - DEVICE_DSM_NOTIFY_FLAG_END\n");
                                                break;
            default:
                                                zfilterDebugPrint("Error: zfilterHandleDSMNotification - unknown flags\n");
                                                break;
        }

        zfilterDebugPrint("INFO: zfilterHandleDSMNotification - NumFileTypeIDs: %d\n", notificationParameters->NumFileTypeIDs);

        for(fileTypeIndex=0; fileTypeIndex<notificationParameters->NumFileTypeIDs; fileTypeIndex++)
        {
            if(IsEqualGUID(&FILE_TYPE_NOTIFICATION_GUID_PAGE_FILE, &notificationParameters->FileTypeID[fileTypeIndex]))
            {
                zfilterDebugPrint("INFO: zfilterHandleDSMNotification - FILE_TYPE_NOTIFICATION_GUID_PAGE_FILE [%d]\n", fileTypeIndex);
            }
            
            if(IsEqualGUID(&FILE_TYPE_NOTIFICATION_GUID_HIBERNATION_FILE, &notificationParameters->FileTypeID[fileTypeIndex]))
            {
                zfilterDebugPrint("INFO: zfilterHandleDSMNotification - FILE_TYPE_NOTIFICATION_GUID_HIBERNATION_FILE [%d]\n", fileTypeIndex);
            }
            
            if(IsEqualGUID(&FILE_TYPE_NOTIFICATION_GUID_CRASHDUMP_FILE, &notificationParameters->FileTypeID[fileTypeIndex]))
            {
                zfilterDebugPrint("INFO: zfilterHandleDSMNotification - FILE_TYPE_NOTIFICATION_GUID_CRASHDUMP_FILE [%d]\n", fileTypeIndex);
            }
        }
    }


    if((dsa->DataSetRangesOffset==0) || (dsa->DataSetRangesLength==0))
    {
        //data set ranges does not exist
    } else {
        DWORD dataSetRangeIndex = 0;
        DWORD numDataSetRanges = 0;
        DEVICE_DATA_SET_RANGE *dsr = NULL;

        dsr = (DEVICE_DATA_SET_RANGE *)(tempCharPtr + dsa->DataSetRangesOffset);
        numDataSetRanges = dsa->DataSetRangesLength / sizeof(DEVICE_DATA_SET_RANGE);

        zfilterDebugPrint("INFO: zfilterHandleDSMNotification - numDataSetRanges: %d [%d]\n", numDataSetRanges, sizeof(DEVICE_DATA_SET_RANGE));

        for(dataSetRangeIndex = 0; dataSetRangeIndex < numDataSetRanges; dataSetRangeIndex++)
        {
            zfilterDebugPrint("INFO: zfilterHandleDSMNotification - StartingOffset [0x%llx 0x%llx]\n", dsr->StartingOffset, dsr->LengthInBytes);

            dsr++;
        }
    }


    return ZFILTER_FORWARD_REQUEST;
    //return ZFILTER_COMPLETE_REQUEST;
}


void zfilterHandleDeviceDSMCommandFailed(SCSI_REQUEST_BLOCK *srb, IRP *irp, zfilterDeviceDSMCommandContextStruct *commandContext)
{
    if((srb==NULL) || (irp==NULL) || (commandContext==NULL))
    {
        return;
    }

    zfilterDebugPrint("ERROR: zfilterHandleDeviceDSMCommandFailed - 0x%x 0x%x\n", srb->SrbStatus, srb->ScsiStatus);

    if((srb->SrbStatus&SRB_STATUS_AUTOSENSE_VALID) && (srb->SenseInfoBufferLength) && (srb->SenseInfoBuffer))
    {       
        zfilterPrintBuffer("sense buffer", srb->SenseInfoBuffer, srb->SenseInfoBufferLength, 1);
    }
}

NTSTATUS zfilterDeviceDSMCommandIrpCompletionCallback(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    zfilterDeviceDSMCommandContextStruct *commandContext = NULL;
    SCSI_REQUEST_BLOCK *srb = NULL;

    if((Irp==NULL) || (Context==NULL))
    {
  	    //always return STATUS_MORE_PROCESSING_REQUIRED to terminate the completion processing of the IRP.
  	    return STATUS_MORE_PROCESSING_REQUIRED;
    }

    commandContext = (zfilterDeviceDSMCommandContextStruct *) Context;
    srb = &commandContext->srb;


    if(SRB_STATUS(srb->SrbStatus)==SRB_STATUS_SUCCESS)
    {
        zfilterDebugPrint("INFO: zfilterDeviceDSMCommandIrpCompletionCallback - SRB_STATUS_SUCCESS\n");
    } else {
        zfilterHandleDeviceDSMCommandFailed(srb, Irp, commandContext);
    }

    if(commandContext->dataTransferBuffer)
        zfilterMemoryFree(commandContext->dataTransferBuffer);

    if(commandContext->senseDataBuffer)
        zfilterMemoryFree(commandContext->senseDataBuffer);

    zfilterMemoryFree(commandContext);

    IoFreeIrp(Irp);

  	//always return STATUS_MORE_PROCESSING_REQUIRED to terminate the completion processing of the IRP.
  	return STATUS_MORE_PROCESSING_REQUIRED;
}

void zfilterPrintDSMIrp(PIRP Irp)
{
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
    DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa = NULL;
    dsa = (DEVICE_MANAGE_DATA_SET_ATTRIBUTES *) Irp->AssociatedIrp.SystemBuffer;

    zfilterDebugPrint("INFO: zfilterHandleDeviceControlIrp - IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES IoControlCode: %x\n", currentIrpStack->Parameters.DeviceIoControl.IoControlCode);
    zfilterDebugPrint("INFO: OutputBufferLength: %x\n", currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength);
    zfilterDebugPrint("INFO: InputBufferLength:  %x\n", currentIrpStack->Parameters.DeviceIoControl.InputBufferLength);
    zfilterDebugPrint("INFO: RequestorMode:      %x\n", Irp->RequestorMode);

    zfilterDebugPrint("INFO: dsa->Size:                 %x %x\n", dsa->Size, sizeof(DEVICE_MANAGE_DATA_SET_ATTRIBUTES));
    zfilterDebugPrint("INFO: dsa->Action:               0x%x\n",  dsa->Action);
    zfilterDebugPrint("INFO: dsa->Flags:                0x%x\n",  dsa->Flags);
    zfilterDebugPrint("INFO: dsa->ParameterBlockOffset: 0x%x\n",  dsa->ParameterBlockOffset);
    zfilterDebugPrint("INFO: dsa->ParameterBlockLength: 0x%x\n",  dsa->ParameterBlockLength);
    zfilterDebugPrint("INFO: dsa->DataSetRangesOffset:  0x%x\n",  dsa->DataSetRangesOffset);
    zfilterDebugPrint("INFO: dsa->DataSetRangesLength:  0x%x\n",  dsa->DataSetRangesLength);

    zfilterPrintBuffer(NULL, dsa, currentIrpStack->Parameters.DeviceIoControl.InputBufferLength, 1);
}


void zfilterGetDeviceDSMSupport(PDEVICE_OBJECT DeviceObject, zfilterDeviceDSMSupportStruct *deviceDSMSupportPtr)
{
    memset(deviceDSMSupportPtr, 0, sizeof(zfilterDeviceDSMSupportStruct));

    deviceDSMSupportPtr->dataTransferBufferSize = 512;
    deviceDSMSupportPtr->senseDataBufferSize = SENSE_BUFFER_SIZE;
    deviceDSMSupportPtr->deviceBlockLength = 512;
}
*/