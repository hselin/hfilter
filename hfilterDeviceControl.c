#include "hfilter.h"


#define IOCTL_HFILTER_SET_SUB_DEVICE_SSD        CTL_CODE(FILE_DEVICE_DISK, 0x901, METHOD_BUFFERED,     FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_HFILTER_SET_SUB_DEVICE_HDD        CTL_CODE(FILE_DEVICE_DISK, 0x902, METHOD_BUFFERED,     FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_HFILTER_SET_SUB_DEVICE_LBA_RANGE  CTL_CODE(FILE_DEVICE_DISK, 0x903, METHOD_BUFFERED,     FILE_READ_DATA | FILE_WRITE_DATA)

extern void printMDL(PMDL Mdl, char *title);

int hfilterSendCommand(hfilterCommandContextStruct *commandContext);
void hfilterHandleCommandIrpFailed(SCSI_REQUEST_BLOCK *srb, IRP *irp, hfilterCommandContextStruct *commandContext);
NTSTATUS hfilterCommandIrpCompletionCallback(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

extern hfilterConfigurationStruct *hfilterConfig;


int hfilterLoadSubDeviceConfig(PDEVICE_OBJECT devObj);
int hfilterSaveSubDeviceConfig(PDEVICE_OBJECT devObj);


NTSTATUS hfilterHandleDeviceControlIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    PIO_STACK_LOCATION currentIrpStack = NULL;
    int status = HFILTER_FORWARD_REQUEST;
    NTSTATUS returnStatus = STATUS_SUCCESS;
    UINT32 returnInformation = 0;

    int tempStatus = HFILTER_OK;

    //hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp\n");

    if((DeviceObject==NULL) || (Irp==NULL))
	{
        hfilterDebugPrint("ERROR: hfilterHandleDeviceControlIrp - param check 1\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleDeviceControlIrp_end;
	}

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) DeviceObject->DeviceExtension;

    if(hfilterDeviceExtension==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterHandleDeviceControlIrp - param check 2\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleDeviceControlIrp_end;
	}

    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);

    if(currentIrpStack==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterHandleDeviceControlIrp - currentIrpStack==NULL\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleDeviceControlIrp_end;
    }    

    switch(currentIrpStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_HFILTER_SET_SUB_DEVICE_SSD:
            hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - IOCTL_HFILTER_SET_SUB_DEVICE_SSD\n");

            hfilterDebugPrint("INFO: rfilterHandleSCSIIrp - KeGetCurrentIrql %d\n", KeGetCurrentIrql());

            //set device as subdevice SSD
            tempStatus = hfilterSetSubDeviceSSD(hfilterDeviceExtension);

            if(tempStatus==HFILTER_ERROR)
            {
                returnStatus = STATUS_INTERNAL_ERROR;
                returnInformation = 0;
            }
            else
            {
                hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - device %p set as SSD\n", DeviceObject);
                
                
                //load config
                tempStatus = hfilterLoadSubDeviceConfig(DeviceObject);

                if(tempStatus==HFILTER_ERROR)
                {
                    hfilterDebugPrint("ERROR: hfilterHandleDeviceControlIrp - hfilterLoadSubDeviceConfig\n");
                    returnStatus = STATUS_SUCCESS;
                    returnInformation = 0;
                }
                else
                {
                    returnStatus = STATUS_SUCCESS;
                    returnInformation = 0;
                }
            }

            status = HFILTER_COMPLETE_REQUEST;
            goto hfilterHandleDeviceControlIrp_end;
            break;

        case IOCTL_HFILTER_SET_SUB_DEVICE_HDD:
            hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - IOCTL_HFILTER_SET_SUB_DEVICE_HDD\n");
            
            //set device as subdevice HDD
            tempStatus = hfilterSetSubDeviceHDD(hfilterDeviceExtension);

            if(tempStatus==HFILTER_ERROR)
            {
                returnStatus = STATUS_INTERNAL_ERROR;
                returnInformation = 0;
            }
            else
            {
                hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - device %p set as HDD\n", DeviceObject);

                returnStatus = STATUS_SUCCESS;
                returnInformation = 0;
            }

            status = HFILTER_COMPLETE_REQUEST;
            goto hfilterHandleDeviceControlIrp_end;
            break;

        case IOCTL_HFILTER_SET_SUB_DEVICE_LBA_RANGE:
            hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - IOCTL_HFILTER_SET_SUB_DEVICE_LBA_RANGE\n");

            if(Irp->AssociatedIrp.SystemBuffer)
            {
                tempStatus = hfilterSetSubDeviceInfo((hfilterSubDeviceInfoStruct *)Irp->AssociatedIrp.SystemBuffer, DeviceObject);
            }
            else
            {
                hfilterDebugPrint("ERROR: hfilterHandleDeviceControlIrp - IOCTL_HFILTER_SET_SUB_DEVICE_LBA_RANGE Irp->AssociatedIrp.SystemBuffer is NULL\n");
                tempStatus = HFILTER_ERROR;
            }

            if(tempStatus==HFILTER_ERROR)
            {
                returnStatus = STATUS_INTERNAL_ERROR;
                returnInformation = 0;
            } else {
                hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - set sub device LBA range good\n");

                //save config
                tempStatus = hfilterSaveSubDeviceConfig(DeviceObject);

                if(tempStatus==HFILTER_ERROR)
                {
                    returnStatus = STATUS_INTERNAL_ERROR;
                    returnInformation = 0;
                }
                else
                {
                    returnStatus = STATUS_SUCCESS;
                    returnInformation = 0;
                }
            }

            status = HFILTER_COMPLETE_REQUEST;
            goto hfilterHandleDeviceControlIrp_end;
            return status;
            break;
        default:
            status = HFILTER_FORWARD_REQUEST;
            goto hfilterHandleDeviceControlIrp_end;
            break;
    }


hfilterHandleDeviceControlIrp_end:

    if(status==HFILTER_COMPLETE_REQUEST)
    {
        hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - HFILTER_COMPLETE_REQUEST\n");

        Irp->IoStatus.Status = returnStatus;
        Irp->IoStatus.Information = (ULONG_PTR) returnInformation;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return returnStatus;
    }

    if(status==HFILTER_QUEUE_REQUEST)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_QUEUE_REQUEST\n");
        return STATUS_PENDING;
    }


    ASSERT(status==HFILTER_FORWARD_REQUEST);

    return hfilterSendIrpToNextDriver(DeviceObject, Irp);
}

#define HFILTER_CONFIG_INFO_LBA                                                   (0x0)

int hfilterSaveSubDeviceConfig(PDEVICE_OBJECT devObj)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    int status = HFILTER_OK;
    hfilterCommandContextStruct *commandContext = NULL;
    UINT8 *dataBuffer = NULL;
    hfilterSubDeviceInfoStruct *subDeviceInfo = NULL;
    CDB *pCdb = NULL;
    UINT32 configInfoLBA = HFILTER_CONFIG_INFO_LBA;
    UINT16 configInfoSizeInBlocks = 1;
    int index = 0;
    hfilterLBARangeStruct *outputLBARange = NULL;
    hfilterLBARangeStruct *subDeviceLBARange = NULL;
    int returnStatus = HFILTER_OK;

    hfilterDebugPrint("INFO: hfilterSaveSubDeviceConfig\n");

    if(devObj==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - devObj==NULL\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSaveSubDeviceConfig_end;
	}

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) devObj->DeviceExtension;

    if(hfilterDeviceExtension==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterDeviceExtension is NULL\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSaveSubDeviceConfig_end;
	}
 
    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_SSD_SUB_DEVICE_INDEX)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - not ssd subdevice\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSaveSubDeviceConfig_end;
    }

    commandContext = hfilterMemoryAlloc(NonPagedPool, sizeof(hfilterCommandContextStruct));

    if(commandContext==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterMemoryAlloc(%d) commandContext\n", sizeof(hfilterCommandContextStruct));
        returnStatus = HFILTER_ERROR;
        goto hfilterSaveSubDeviceConfig_end;
    }

    memset(commandContext, 0, sizeof(hfilterCommandContextStruct));

    dataBuffer = hfilterMemoryAlloc(NonPagedPool, SUB_DEVICE_BLOCK_SIZE);
       
    if(dataBuffer==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterMemoryAlloc(%d) dataBuffer\n", sizeof(SUB_DEVICE_BLOCK_SIZE));
        returnStatus = HFILTER_ERROR;
        goto hfilterSaveSubDeviceConfig_end;
    }    
    
    memset(dataBuffer, 0, SUB_DEVICE_BLOCK_SIZE);


    ASSERT(SUB_DEVICE_BLOCK_SIZE >= sizeof(hfilterSubDeviceInfoStruct));


    subDeviceInfo = (hfilterSubDeviceInfoStruct *) dataBuffer;

    for(index=0;index<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES;index++)
    {
        outputLBARange = &subDeviceInfo->lbaRangeList[index];
        subDeviceLBARange = &hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[index];

        outputLBARange->inuse = subDeviceLBARange->inuse;
        outputLBARange->startLBA = subDeviceLBARange->startLBA;
        outputLBARange->endLBA = subDeviceLBARange->endLBA;
        outputLBARange->mappedStartLBA = subDeviceLBARange->mappedStartLBA;
        outputLBARange->mappedEndLBA = subDeviceLBARange->mappedEndLBA;


        hfilterDebugPrint("index [%d]\n", index);
        hfilterDebugPrint("inuse:          0x%llx\n", outputLBARange->inuse);
        hfilterDebugPrint("startLBA:       0x%llx\n", outputLBARange->startLBA);
        hfilterDebugPrint("endLBA:         0x%llx\n", outputLBARange->endLBA);
        hfilterDebugPrint("\n"); 
        hfilterDebugPrint("mappedStartLBA: 0x%llx\n", outputLBARange->mappedStartLBA);
        hfilterDebugPrint("mappedEndLBA    0x%llx\n", outputLBARange->mappedEndLBA);
        hfilterDebugPrint("\n"); 
    }

    subDeviceInfo->magicNumber = HFILTER_SUB_DEVICE_INFO_MAGIC_NUMBER;


    commandContext->targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
    

    pCdb = (CDB *) commandContext->cdb;
    
    pCdb->CDB10.OperationCode = SCSIOP_WRITE;

    pCdb->CDB10.LogicalBlockByte0 = GET_BYTE(configInfoLBA, 3); //(MSB)
    pCdb->CDB10.LogicalBlockByte1 = GET_BYTE(configInfoLBA, 2);
    pCdb->CDB10.LogicalBlockByte2 = GET_BYTE(configInfoLBA, 1);
    pCdb->CDB10.LogicalBlockByte3 = GET_BYTE(configInfoLBA, 0); //LSB

    pCdb->CDB10.TransferBlocksMsb = GET_BYTE(configInfoSizeInBlocks, 1); //(MSB)
    pCdb->CDB10.TransferBlocksLsb = GET_BYTE(configInfoSizeInBlocks, 0); //LSB


    commandContext->cdbLength = 10;

    commandContext->commandType = HFILTER_DATA_OUT;


    commandContext->dataTransferBuffer = dataBuffer;
    commandContext->dataTransferBufferSize = SUB_DEVICE_BLOCK_SIZE;


    commandContext->synchronous = 1;

    status = hfilterSendCommand(commandContext);

    if(status!=HFILTER_OK)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterSendCommand\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSaveSubDeviceConfig_end;
    }


    //wait and free stuff here...
    
    
hfilterSaveSubDeviceConfig_end:    
    if(commandContext)
        hfilterMemoryFree(commandContext);

    if(dataBuffer)
        hfilterMemoryFree(dataBuffer);
}





#if 1
int hfilterLoadSubDeviceConfig(PDEVICE_OBJECT devObj)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    int status = HFILTER_OK;
    hfilterCommandContextStruct *commandContext = NULL;
    UINT8 *dataBuffer = NULL;
    hfilterSubDeviceInfoStruct *subDeviceInfo = NULL;
    CDB *pCdb = NULL;
    UINT32 configInfoLBA = HFILTER_CONFIG_INFO_LBA;
    UINT16 configInfoSizeInBlocks = 1;
    int index = 0;
    hfilterLBARangeStruct *inputLBARange = NULL;
    hfilterLBARangeStruct *subDeviceLBARange = NULL;
    int returnStatus = HFILTER_OK;

    hfilterDebugPrint("INFO: hfilterLoadSubDeviceConfig\n");

    if(devObj==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterLoadSubDeviceConfig - devObj==NULL\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterLoadSubDeviceConfig_end;
	}

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) devObj->DeviceExtension;

    if(hfilterDeviceExtension==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterLoadSubDeviceConfig - hfilterDeviceExtension is NULL\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterLoadSubDeviceConfig_end;
	}
 
    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_SSD_SUB_DEVICE_INDEX)
    {
        hfilterDebugPrint("ERROR: hfilterLoadSubDeviceConfig - not ssd subdevice\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterLoadSubDeviceConfig_end;
    }

    commandContext = hfilterMemoryAlloc(NonPagedPool, sizeof(hfilterCommandContextStruct));

    if(commandContext==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterLoadSubDeviceConfig - hfilterMemoryAlloc(%d) commandContext\n", sizeof(hfilterCommandContextStruct));
        returnStatus = HFILTER_ERROR;
        goto hfilterLoadSubDeviceConfig_end;
    }

    memset(commandContext, 0, sizeof(hfilterCommandContextStruct));

    dataBuffer = hfilterMemoryAlloc(NonPagedPool, SUB_DEVICE_BLOCK_SIZE);
       
    if(dataBuffer==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterLoadSubDeviceConfig - hfilterMemoryAlloc(%d) dataBuffer\n", sizeof(SUB_DEVICE_BLOCK_SIZE));
        returnStatus = HFILTER_ERROR;
        goto hfilterLoadSubDeviceConfig_end;
    }    
    
    memset(dataBuffer, 0, SUB_DEVICE_BLOCK_SIZE);


    ASSERT(SUB_DEVICE_BLOCK_SIZE >= sizeof(hfilterSubDeviceInfoStruct));


    commandContext->targetDeviceObject = hfilterDeviceExtension->targetDeviceObject;
    

    pCdb = (CDB *) commandContext->cdb;
    
    pCdb->CDB10.OperationCode = SCSIOP_READ;

    pCdb->CDB10.LogicalBlockByte0 = GET_BYTE(configInfoLBA, 3); //(MSB)
    pCdb->CDB10.LogicalBlockByte1 = GET_BYTE(configInfoLBA, 2);
    pCdb->CDB10.LogicalBlockByte2 = GET_BYTE(configInfoLBA, 1);
    pCdb->CDB10.LogicalBlockByte3 = GET_BYTE(configInfoLBA, 0); //LSB

    pCdb->CDB10.TransferBlocksMsb = GET_BYTE(configInfoSizeInBlocks, 1); //(MSB)
    pCdb->CDB10.TransferBlocksLsb = GET_BYTE(configInfoSizeInBlocks, 0); //LSB


    commandContext->cdbLength = 10;

    commandContext->commandType = HFILTER_DATA_IN;


    commandContext->dataTransferBuffer = dataBuffer;
    commandContext->dataTransferBufferSize = SUB_DEVICE_BLOCK_SIZE;


    commandContext->synchronous = 1;

    status = hfilterSendCommand(commandContext);

    if(status!=HFILTER_OK)
    {
        hfilterDebugPrint("ERROR: hfilterLoadSubDeviceConfig - hfilterSendCommand\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterLoadSubDeviceConfig_end;
    }




    subDeviceInfo = (hfilterSubDeviceInfoStruct *) dataBuffer;


    if(subDeviceInfo->magicNumber != HFILTER_SUB_DEVICE_INFO_MAGIC_NUMBER)
    {
        hfilterDebugPrint("ERROR: hfilterLoadSubDeviceConfig - subDeviceInfo->magicNumber!0x%llx\n", HFILTER_SUB_DEVICE_INFO_MAGIC_NUMBER);
        returnStatus = HFILTER_ERROR;
        goto hfilterLoadSubDeviceConfig_end;
    }


    for(index=0;index<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES;index++)
    {
        inputLBARange = &subDeviceInfo->lbaRangeList[index];
        subDeviceLBARange = &hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[index];

        subDeviceLBARange->inuse          = inputLBARange->inuse;
        subDeviceLBARange->startLBA       = inputLBARange->startLBA;
        subDeviceLBARange->endLBA         = inputLBARange->endLBA;
        subDeviceLBARange->mappedStartLBA = inputLBARange->mappedStartLBA;
        subDeviceLBARange->mappedEndLBA   = inputLBARange->mappedEndLBA;



        hfilterDebugPrint("index [%d]\n", index);
        hfilterDebugPrint("inuse:          0x%llx\n", subDeviceLBARange->inuse);
        hfilterDebugPrint("startLBA:       0x%llx\n", subDeviceLBARange->startLBA);
        hfilterDebugPrint("endLBA:         0x%llx\n", subDeviceLBARange->endLBA);
        hfilterDebugPrint("\n"); 
        hfilterDebugPrint("mappedStartLBA: 0x%llx\n", subDeviceLBARange->mappedStartLBA);
        hfilterDebugPrint("mappedEndLBA    0x%llx\n", subDeviceLBARange->mappedEndLBA);
        hfilterDebugPrint("\n"); 
    }
    
    
hfilterLoadSubDeviceConfig_end:    
    if(commandContext)
        hfilterMemoryFree(commandContext);

    if(dataBuffer)
        hfilterMemoryFree(dataBuffer);

    return returnStatus;
}
#endif






int hfilterBuildSRB(SCSI_REQUEST_BLOCK *srb, IRP *irp, hfilterCommandContextStruct *commandContext)
{
    hfilterDebugPrint("INFO: hfilterBuildSRB\n");

    if((srb==NULL) || (irp==NULL) || (commandContext==NULL))
    {
        return HFILTER_ERROR;
    }
    
    if(commandContext->cdbLength > 16)
    {
        return HFILTER_ERROR;
    }

    memset(srb, 0, sizeof(SCSI_REQUEST_BLOCK));

    srb->Length = sizeof(SCSI_REQUEST_BLOCK);
    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;

    memcpy(srb->Cdb, commandContext->cdb, MIN(sizeof(srb->Cdb), commandContext->cdbLength));

    srb->CdbLength = commandContext->cdbLength;

    if(commandContext->commandType&HFILTER_DATA_OUT)
    {
        srb->SrbFlags = SRB_FLAGS_DATA_OUT;
    }

    if(commandContext->commandType&HFILTER_DATA_IN)
    {
        srb->SrbFlags = SRB_FLAGS_DATA_IN;
    }

    srb->SrbFlags |= (SRB_FLAGS_NO_QUEUE_FREEZE | SRB_FLAGS_ADAPTER_CACHE_ENABLE);

    srb->DataTransferLength = (ULONG) commandContext->dataTransferBufferSize;
    srb->DataBuffer = commandContext->dataTransferBuffer;
    
    srb->SenseInfoBufferLength = (UCHAR) commandContext->senseDataBufferSize;
    srb->SenseInfoBuffer = commandContext->senseDataBuffer;

    srb->TimeOutValue = HFILTER_DEVICE_COMMAND_TIMEOUT_VALUE;
    
    srb->OriginalRequest = irp;

    srb->QueueSortKey = (ULONG) 0;

    
    hfilterPrintBuffer("device command", srb->Cdb, srb->CdbLength, 1);
    
    return HFILTER_OK;
}






int hfilterBuildIRP(IRP *irp, SCSI_REQUEST_BLOCK *srb,  hfilterCommandContextStruct *commandContext)
{
    PIO_STACK_LOCATION irpStack = NULL;

    hfilterDebugPrint("INFO: hfilterBuildIRP\n");

    if((irp==NULL) || (srb==NULL) || (commandContext==NULL))
    {
        return HFILTER_ERROR;
    }

    if((srb->DataBuffer) && (srb->DataTransferLength == 0))
    {
        return HFILTER_ERROR;
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    if(irpStack==NULL)
    {
        return HFILTER_ERROR;
    }

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->Parameters.Scsi.Srb = srb;


    if(srb->DataTransferLength)
    {
        irp->MdlAddress = IoAllocateMdl(srb->DataBuffer, srb->DataTransferLength, FALSE, FALSE, irp);
        MmBuildMdlForNonPagedPool(irp->MdlAddress);

        printMDL(irp->MdlAddress, "irp");

        hfilterDebugPrint("INFO: hfilterBuildIRP - MmGetMdlByteCount: 0x%x\n", MmGetMdlByteCount(irp->MdlAddress)); 
    }
    

    IoSetCompletionRoutine(irp, (PIO_COMPLETION_ROUTINE)hfilterCommandIrpCompletionCallback, commandContext, TRUE, TRUE, TRUE);

    return HFILTER_OK;
}


int hfilterSendCommand(hfilterCommandContextStruct *commandContext)
{
    int returnStatus = HFILTER_OK;

    PIRP irp = NULL;
    PSCSI_REQUEST_BLOCK srb = NULL;
    
    NTSTATUS driverCallStatus = STATUS_SUCCESS;
    int status = HFILTER_OK;

    if(commandContext==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSendCommand - commandContext == NULL\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendCommand_end;
    }

    if(commandContext->targetDeviceObject == NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSendCommand - targetDeviceObject == NULL\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendCommand_end;
    }

    //alloc irp
    irp = IoAllocateIrp(commandContext->targetDeviceObject->StackSize, FALSE);

    if(irp == NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSendCommand - IoAllocateIrp()\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendCommand_end;
    }

    commandContext->senseDataBufferSize = SENSE_BUFFER_SIZE;

    //alloc sense data buffer
    if(commandContext->senseDataBufferSize)
    {
        commandContext->senseDataBuffer = hfilterMemoryAlloc(NonPagedPool, commandContext->senseDataBufferSize);
        if(commandContext->senseDataBuffer==NULL)
        {
            returnStatus = HFILTER_ERROR;
            goto hfilterSendCommand_end;
        }
        memset(commandContext->senseDataBuffer, 0, commandContext->senseDataBufferSize);
    } else {
        commandContext->senseDataBufferSize = 0;
        commandContext->senseDataBuffer = NULL;
    }

    srb = &commandContext->srb;

    //setup SRB
    status = hfilterBuildSRB(srb, irp, commandContext);
    
    if(status!=HFILTER_OK)
    {
        hfilterDebugPrint("ERROR: hfilterSendCommand - hfilterBuildSRB\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendCommand_end;
    }

    //setup IRP
    status = hfilterBuildIRP(irp, srb, commandContext);
    
    if(status!=HFILTER_OK)
    {
        hfilterDebugPrint("ERROR: hfilterSendCommand - hfilterBuildIRP\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendCommand_end;
    }

    commandContext->irp = irp;

    if(commandContext->synchronous)
        KeInitializeEvent(&commandContext->waitEvent, SynchronizationEvent, FALSE);

    //send the irp on its way...
    driverCallStatus = IoCallDriver(commandContext->targetDeviceObject, irp);

    if(driverCallStatus == STATUS_PENDING)
    {
        hfilterDebugPrint("INFO: hfilterSendCommand - STATUS_PENDING\n");
        
        if(commandContext->synchronous)
        {
            hfilterDebugPrint("INFO: hfilterSendCommand - WAIT\n");
            KeWaitForSingleObject(&commandContext->waitEvent, Executive, KernelMode, FALSE, NULL);
            hfilterDebugPrint("INFO: hfilterSendCommand - WAIT DONE\n");
        }
    } else {
        hfilterDebugPrint("INFO: hfilterSendCommand !STATUS_PENDING 0x%x\n", driverCallStatus);
    }

hfilterSendCommand_end:
    if(returnStatus==HFILTER_ERROR)
    {

        if(commandContext->senseDataBuffer)
            hfilterMemoryFree(commandContext->senseDataBuffer);

        if(irp)
            IoFreeIrp(irp);
    }

    return returnStatus;
}

NTSTATUS hfilterCommandIrpCompletionCallback(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    hfilterCommandContextStruct *commandContext = NULL;
    SCSI_REQUEST_BLOCK *srb = NULL;

    hfilterDebugPrint("INFO: hfilterCommandIrpCompletionCallback\n");

    if((Irp==NULL) || (Context==NULL))
    {
  	    goto hfilterCommandIrpCompletionCallback_end;
    }

    commandContext = (hfilterCommandContextStruct *) Context;

    srb = &commandContext->srb;

    if(SRB_STATUS(srb->SrbStatus)==SRB_STATUS_SUCCESS)
    {
        hfilterDebugPrint("INFO: hfilterCommandIrpCompletionCallback - SRB_STATUS_SUCCESS\n");
    } else {
        hfilterHandleCommandIrpFailed(srb, Irp, commandContext);
    }

    //hfilterDumpSrb(srb, "irp");

   
hfilterCommandIrpCompletionCallback_end:

    if(commandContext)
    {
        if(commandContext->synchronous==0)
        {
            if(commandContext->senseDataBuffer)
                hfilterMemoryFree(commandContext->senseDataBuffer);

            if(Irp)
            {
                if(Irp->MdlAddress)
                    IoFreeMdl(Irp->MdlAddress);
    
                IoFreeIrp(Irp);
            }
        } else {
             KeSetEvent(&commandContext->waitEvent, IO_NO_INCREMENT, FALSE);
        }
    } else
    {
        if(Irp)
        {
            if(Irp->MdlAddress)
                IoFreeMdl(Irp->MdlAddress);
    
            IoFreeIrp(Irp);
        }
    }

  	//always return STATUS_MORE_PROCESSING_REQUIRED to terminate the completion processing of the IRP.
  	return STATUS_MORE_PROCESSING_REQUIRED;
}

void hfilterHandleCommandIrpFailed(SCSI_REQUEST_BLOCK *srb, IRP *irp, hfilterCommandContextStruct *commandContext)
{
    hfilterDebugPrint("INFO: hfilterHandleCommandIrpFailed\n");
    ASSERT(FALSE);
}