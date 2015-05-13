#include "hfilter.h"

extern hfilterConfigurationStruct *hfilterConfig;

int hfilterSendSubDeviceReadWriteSubIrpCommand(hfilterReadWriteSubIrpCommandContextStruct *commandContext, UINT32 scsiOpIndex);



int hfilterBuildSubDeviceReadWriteSRB(SCSI_REQUEST_BLOCK *srb, IRP *irp, SCSI_REQUEST_BLOCK *superSrb, hfilterSCSIOPStruct *scsiOP);

void zfilterBuildSubDeviceReadCDB(UINT32 startingLBA, UINT16 sectorCount, UCHAR *cdb, UCHAR *cdbLength);
void zfilterBuildSubDeviceWriteCDB(UINT32 startingLBA, UINT16 sectorCount, UCHAR *cdb, UCHAR *cdbLength);
void remapReadWriteLBA(UCHAR *cdb, UINT32 mappedStartingLBA);
int hfilterBuildSubDeviceReadWriteIRP(IRP *irp, SCSI_REQUEST_BLOCK *srb, hfilterSCSIOPStruct *scsiOP, hfilterReadWriteSubIrpCommandContextStruct *commandContext);

NTSTATUS hfilterSubDeviceReadWriteCommandSubIrpCompletionCallback(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

int hfilterQueueWorkItem(DEVICE_OBJECT *deviceObj, PIO_WORKITEM_ROUTINE callback, void *context);

IO_WORKITEM_ROUTINE_EX hfilterSubDeviceReadWriteCommandSuperIrpCompletionCallback;



void hfilterHandleSubDeviceReadWriteCommandSubIrpFailed(SCSI_REQUEST_BLOCK *srb, IRP *irp, hfilterReadWriteSubIrpCommandContextStruct *commandContext);

hfilterLBARangeStruct *hfilterGetContainingSubDeviceLBARange(UINT64 lba);
hfilterLBARangeStruct *hfilterGetNextSubDeviceLBARange(UINT64 lba);


void hfilterDumpSrb(SCSI_REQUEST_BLOCK *srb, char *title);
void printMDL(PMDL Mdl, char *title);

int printCounter = 0;

#if 1
NTSTATUS hfilterHandleSCSIIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
	PIO_STACK_LOCATION irpStack = NULL;
	SCSI_REQUEST_BLOCK *srb = NULL;
    int status = HFILTER_FORWARD_REQUEST;
    PDEVICE_OBJECT targetDeviceObject = NULL;

    //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp\n");


    if((DeviceObject==NULL) || (Irp==NULL))
	{
        hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - param check 1\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) DeviceObject->DeviceExtension;

    if(hfilterDeviceExtension==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - param check 2\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

#if 0
    //if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_INVALID_SUB_DEVICE_INDEX)
        //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - subDeviceIndex %d\n", hfilterDeviceExtension->subDeviceIndex);

   /*
    if(hfilterDeviceExtension->subDeviceIndex==HFILTER_SSD_SUB_DEVICE_INDEX)
	{
        irpStack = IoGetCurrentIrpStackLocation(Irp);

        if(irpStack!=NULL)
        {
            srb = irpStack->Parameters.Scsi.Srb;

            if(srb!=NULL)
            {
                switch(srb->Function)
	                {
                            case SRB_FUNCTION_EXECUTE_SCSI:
                                //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - SSD SRB_FUNCTION_EXECUTE_SCSI\n");
                                //hfilterPrintBuffer("ssd CDB", (unsigned char *)srb->Cdb, srb->CdbLength, 1);
                                if(srb->Cdb[0]==SCSIOP_WRITE)
                                {
                                    //ASSERT(FALSE);
                                }
				                break;

			                default:
                                //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - SSD !SRB_FUNCTION_EXECUTE_SCSI\n");
				                break;
                    }
            }
        }
	}
    */
    

    {

        irpStack = IoGetCurrentIrpStackLocation(Irp);

        if(irpStack==NULL)
        {
            hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - irpStack==NULL\n");
        } else {
            hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - [0x%x] [0x%x] [0x%x]\n", irpStack->MajorFunction, irpStack->MinorFunction, Irp->Flags);
        }
    }
#endif
    
    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_HDD_SUB_DEVICE_INDEX)
	{
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

#if 0
    if(hfilterDeviceExtension->subDeviceIndex==HFILTER_HDD_SUB_DEVICE_INDEX)
	{
        //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - redirect HDD traffic\n");
		//status = HFILTER_FORWARD_REQUEST;
        targetDeviceObject = hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].targetDeviceObject;
        status = HFILTER_SEND_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}
#endif

    if(hfilterConfig->subDeviceList[HFILTER_HDD_SUB_DEVICE_INDEX].targetDeviceObject==NULL)
	{
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_HDD_SUB_DEVICE_INDEX targetDeviceObject is NULL\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

    if(hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].targetDeviceObject==NULL)
	{
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_SSD_SUB_DEVICE_INDEX targetDeviceObject is NULL\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}


    irpStack = IoGetCurrentIrpStackLocation(Irp);

    if(irpStack==NULL)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - irpStack==NULL\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
    }

    if(irpStack->MinorFunction!=0)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - MinorFunction!=0\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
    }

    srb = irpStack->Parameters.Scsi.Srb;

    if(srb==NULL)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - srb==NULL\n");
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
    }


	switch(srb->Function)
	{
            case SRB_FUNCTION_EXECUTE_SCSI:
                //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - SRB_FUNCTION_EXECUTE_SCSI\n");
				break;

			default:
                //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - !SRB_FUNCTION_EXECUTE_SCSI\n");
				status = HFILTER_FORWARD_REQUEST;
				goto hfilterHandleSCSIIrp_end;
				break;
    }


	switch(srb->Cdb[0]){
		case SCSIOP_READ:
		case SCSIOP_WRITE:
                {
                    PCDB pCdb = NULL;
                    UINT64 opStartLBA = 0;
	                UINT64 startLBA = 0;
                    UINT32 opSectorCount = 0;
                    UINT32 totalSubOpSectorCount = 0;
                    UINT64 opEndLBA, endLBA = 0;
                    int subCommandStatus = HFILTER_OK;
                    int i = 0;


                    hfilterLBARangeStruct *subDeviceLBARange = NULL;
                    hfilterLBARangeStruct *nextSubDeviceLBARange = NULL;
                    hfilterReadWriteSubIrpCommandContextStruct *commandContext = NULL;
                    hfilterSCSIOPStruct *scsiOP = NULL;

                    //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - CDB READ/WRITE\n");

                    commandContext = hfilterMemoryAlloc(NonPagedPool, sizeof(hfilterReadWriteSubIrpCommandContextStruct));

                    if(commandContext==NULL)
                    {
                        hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - hfilterMemoryAlloc %d\n", sizeof(hfilterReadWriteSubIrpCommandContextStruct));
				        status = HFILTER_FORWARD_REQUEST;
				        goto hfilterHandleSCSIIrp_end;
                    }

                    memset(commandContext, 0, sizeof(hfilterReadWriteSubIrpCommandContextStruct));

                    commandContext->hfilterDeviceObject = DeviceObject;
                    commandContext->superIrp = Irp;
                    commandContext->superSrb = srb;


    
                    pCdb = (PCDB) srb->Cdb;

                    opStartLBA = SET_BYTE(pCdb->CDB10.LogicalBlockByte0, 3);
                    opStartLBA |= SET_BYTE(pCdb->CDB10.LogicalBlockByte1, 2);
                    opStartLBA |= SET_BYTE(pCdb->CDB10.LogicalBlockByte2, 1);
                    opStartLBA |= SET_BYTE(pCdb->CDB10.LogicalBlockByte3, 0);

                    opSectorCount = SET_BYTE(pCdb->CDB10.TransferBlocksMsb, 1);
                    opSectorCount |= SET_BYTE(pCdb->CDB10.TransferBlocksLsb, 0);

                    opEndLBA = opStartLBA + opSectorCount - 1;

                    startLBA = opStartLBA;
                    endLBA = opEndLBA;

                    commandContext->superCDBStartLBA = opStartLBA;
                    commandContext->superCDBEndLBA = opEndLBA;
                    commandContext->superCDBSectorCount = opSectorCount;

                    commandContext->scsiOpCount = 0;
                    commandContext->scsiOpCompletionCount = 0;



                    //hfilterPrintBuffer("super CDB", (unsigned char *)srb->Cdb, srb->CdbLength, 1);

                    //hfilterDebugPrint("INFO: super startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%x\n", commandContext->superCDBStartLBA, commandContext->superCDBEndLBA, commandContext->superCDBSectorCount);

                   
                    while(1)
                    {                        
                        //hfilterDebugPrint("INFO: startLBA: 0x%llx opEndLBA: 0x%llx totalSubOpSectorCount: 0x%x\n", startLBA, opEndLBA, totalSubOpSectorCount);

                        if(startLBA > opEndLBA)
                        {
                            break;
                        }

                        commandContext->scsiOpCount = commandContext->scsiOpCount + 1;

                        if((commandContext->scsiOpCount-1)>=HFILTER_SUB_DEVICE_MAX_NUMBER_OF_SCSI_OPS_PER_IRP)
                        {
                            hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp commandContext->scsiOpCount-1 >= HFILTER_SUB_DEVICE_MAX_NUMBER_OF_SCSI_OPS_PER_IRP\n", commandContext->scsiOpCount-1, HFILTER_SUB_DEVICE_MAX_NUMBER_OF_SCSI_OPS_PER_IRP);
                            ASSERT(FALSE);
                        }

                        scsiOP = &commandContext->scsiOpList[commandContext->scsiOpCount-1];

                        memset(scsiOP, 0, sizeof(hfilterSCSIOPStruct));
                        scsiOP->inuse = 0;
                        scsiOP->opcode = srb->Cdb[0];
                        scsiOP->startLBA = startLBA;
                        scsiOP->mappedStartLBA = startLBA;

                        subDeviceLBARange = hfilterGetContainingSubDeviceLBARange(startLBA);

                        if(subDeviceLBARange==NULL)
                        {
                            //not found within a containing LBA range...
                            scsiOP->inuse = 1;

                            nextSubDeviceLBARange = hfilterGetNextSubDeviceLBARange(startLBA);

                            if(nextSubDeviceLBARange==NULL)
                                scsiOP->endLBA = endLBA;

                            if(nextSubDeviceLBARange)
                            {
                                scsiOP->endLBA = MIN(endLBA, (nextSubDeviceLBARange->startLBA-1));
                            }

                            
                            scsiOP->sectorCount = (UINT64)(scsiOP->endLBA - scsiOP->startLBA + 1);
                            scsiOP->hfilterSubDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;
                            //hfilterDebugPrint("INFO: op startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%llx device: %d [outside]\n", scsiOP->startLBA, scsiOP->endLBA, scsiOP->sectorCount, scsiOP->hfilterSubDeviceIndex);
                        }

                        if(subDeviceLBARange)
                        {
                            //found a containing LBA range...
                            scsiOP->inuse = 1;                            
                            scsiOP->endLBA = MIN(endLBA, subDeviceLBARange->endLBA);
                            scsiOP->sectorCount = (UINT64)(scsiOP->endLBA - scsiOP->startLBA + 1);
                            scsiOP->hfilterSubDeviceIndex = HFILTER_SSD_SUB_DEVICE_INDEX;
                            //hfilterDebugPrint("INFO: op startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%llx device: %d [inside]\n", scsiOP->startLBA, scsiOP->endLBA, scsiOP->sectorCount, scsiOP->hfilterSubDeviceIndex);


                            //remap LBA
                            {
                                UINT64 startLBAOffset = 0;

                                ASSERT(scsiOP->startLBA >= subDeviceLBARange->startLBA);

                                startLBAOffset = scsiOP->startLBA - subDeviceLBARange->startLBA;

                                scsiOP->mappedStartLBA = subDeviceLBARange->mappedStartLBA + startLBAOffset;


                                if(printCounter<200)
                                {                                    
                                    hfilterDebugPrint("INFO: map [0x%llx : 0x%llx]\n", scsiOP->startLBA, scsiOP->mappedStartLBA);
                                    printCounter++;
                                }
                            }
                        }

                        startLBA = scsiOP->endLBA + 1;
                        totalSubOpSectorCount += (UINT32) scsiOP->sectorCount;
                    } //while(1)








                    //process scsiops
                    {
                        UINT32 scsiOpIndex = 0;

                        //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - commandContext->scsiOpCount %d\n", commandContext->scsiOpCount);

                        //check for error condition
                        if(commandContext->scsiOpCount==0)
                        {
                            hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp commandContext->scsiOpCount = 0\n");
                            
                            if(commandContext)
                                hfilterMemoryFree(commandContext);
                            
                            commandContext = NULL;
                            
                            status = HFILTER_FORWARD_REQUEST;
                            goto hfilterHandleSCSIIrp_end;
                        }

                        //check length
                        if(totalSubOpSectorCount!=commandContext->superCDBSectorCount)
                        {
                            hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp totalSubOpSectorCount!=commandContext->superCDBSectorCount 0x%x 0x%x\n", totalSubOpSectorCount, commandContext->superCDBSectorCount);

                            if(commandContext)
                                hfilterMemoryFree(commandContext);
                            
                            commandContext = NULL;

                            status = HFILTER_FORWARD_REQUEST;
                            goto hfilterHandleSCSIIrp_end;
                        }

                        //check for straight re-direct
                        if(commandContext->scsiOpCount==1)
                        {
                            scsiOP = &commandContext->scsiOpList[0];

                            if((scsiOP->startLBA != opStartLBA) || (scsiOP->endLBA != opEndLBA))
                            {
                                hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp commandContext->scsiOpCount = 1 with LBA range mismatch\n");
                                ASSERT(FALSE);
                            }

                            //done with whole transaction
                            //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - redirect to %d\n", scsiOP->hfilterSubDeviceIndex);
                            targetDeviceObject = hfilterConfig->subDeviceList[scsiOP->hfilterSubDeviceIndex].targetDeviceObject;

                            if(commandContext)
                                hfilterMemoryFree(commandContext);
                            

                            remapReadWriteLBA(srb->Cdb, (UINT32) scsiOP->mappedStartLBA);

                            commandContext = NULL;
                            scsiOP = NULL;


                            status = HFILTER_SEND_REQUEST;
                            goto hfilterHandleSCSIIrp_end;
                        }


                        //ready for multiple scsiops...

                        commandContext->superSrbDataTransferSize = (UINT64) commandContext->superSrb->DataTransferLength;

                        if(commandContext->superSrbDataTransferSize)
                        {
                            /*
                            if(commandContext->superIrp->MdlAddress)
                            {
                                try{
                                    if(srb->Cdb[0]==SCSIOP_READ)
                                        MmProbeAndLockPages(commandContext->superIrp->MdlAddress, KernelMode, IoWriteAccess);

                                    if(srb->Cdb[0]==SCSIOP_WRITE)
                                        MmProbeAndLockPages(commandContext->superIrp->MdlAddress, KernelMode, IoReadAccess);
                                }
                       
                                except(EXCEPTION_EXECUTE_HANDLER)
                                {
                                    hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - MmProbeAndLockPages\n");

                                    if(commandContext)
                                        hfilterMemoryFree(commandContext);
                            
                                    commandContext = NULL;

				                    status = HFILTER_FORWARD_REQUEST;
				                    goto hfilterHandleSCSIIrp_end;
                                }
                            }
                            */

                            /*
                            commandContext->superIRPMdlVirtualAddress = MmGetMdlVirtualAddress(commandContext->superIrp->MdlAddress);

                            if(commandContext->superIRPMdlVirtualAddress == NULL)
                            {
                                hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - MmGetMdlVirtualAddress\n");
                                hfilterMemoryFree(commandContext);

				                status = HFILTER_FORWARD_REQUEST;
				                goto hfilterHandleSCSIIrp_end;
                            }
                            */

                            commandContext->superSrbDataTransferBuffer = commandContext->superSrb->DataBuffer;

                            if(commandContext->superSrbDataTransferBuffer==NULL)
                            {
                                hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - commandContext->superSrbDataTransferBuffer==NULL @@@@@@@@@@@@@@@@@@@@@\n");

                                commandContext->superSrbDataTransferBuffer = NULL;
                                
                                if(commandContext->superIrp->MdlAddress==NULL)
                                {
                                    hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - null Mdl\n");
                                    ASSERT(FALSE);
                                } else
                                {
                                    hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - MmGetMdlByteCount: 0x%x superSrbDataTransferSize: 0x%llx\n", MmGetMdlByteCount(commandContext->superIrp->MdlAddress), commandContext->superSrbDataTransferSize); 
                                    printMDL(commandContext->superIrp->MdlAddress, "original IRP");
                                }

                                //MmProbeAndLockPages(commandContext->superIrp->MdlAddress, KernelMode, IoWriteAccess);

                                //commandContext->superSrbDataTransferBuffer = MmGetMdlVirtualAddress(commandContext->superIrp->MdlAddress);
                                

                                /*
                                if(commandContext)
                                    hfilterMemoryFree(commandContext);
                            
                                commandContext = NULL;

				                status = HFILTER_FORWARD_REQUEST;
				                goto hfilterHandleSCSIIrp_end;
                                */
                            }

                        } //if(commandContext->superSrbDataTransferSize)

                        IoMarkIrpPending(Irp);

                        //issue scsi command
                        for(scsiOpIndex=0; scsiOpIndex < commandContext->scsiOpCount; scsiOpIndex++)
                        {
                            subCommandStatus = hfilterSendSubDeviceReadWriteSubIrpCommand(commandContext, scsiOpIndex);

                            if(subCommandStatus!=HFILTER_OK)
                            {
                                hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp subCommandStatus\n");
                                ASSERT(FALSE);
                            }
                        }

                        status = HFILTER_QUEUE_REQUEST;
                    } //process scsiops
                    
                } //case read + write

				break;

		default:
                hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - CDB !READ/WRITE\n");
				status = HFILTER_FORWARD_REQUEST;
				goto hfilterHandleSCSIIrp_end;
				break;
	}

hfilterHandleSCSIIrp_end:
    if(status==HFILTER_COMPLETE_REQUEST)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_COMPLETE_REQUESTn");

        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    if(status==HFILTER_QUEUE_REQUEST)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_QUEUE_REQUEST\n");

        //IoMarkIrpPending(Irp);
        return STATUS_PENDING;
    }

    if(status==HFILTER_SEND_REQUEST)
    {
        //hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_SEND_REQUEST\n");

        if(targetDeviceObject)
        {
            return hfilterSendIrpToDriver(targetDeviceObject, Irp);
        } else {
            hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_SEND_REQUEST targetDeviceObject==NULL\n");
            return hfilterSendIrpToNextDriver(DeviceObject, Irp);
        }
    }


    ASSERT(status==HFILTER_FORWARD_REQUEST);

#if 0
    {
        PIO_STACK_LOCATION irpStackFwd = NULL;
	    SCSI_REQUEST_BLOCK *srbFwd = NULL;

        irpStackFwd = IoGetCurrentIrpStackLocation(Irp);

        if(irpStackFwd!=NULL)
        {

            if(irpStackFwd->MinorFunction!=0)
            {
                hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - fwd MinorFunction 0x%x\n", irpStackFwd->MinorFunction);
            }

            srbFwd = irpStackFwd->Parameters.Scsi.Srb;

            if(srbFwd!=NULL)
            {
                if((irpStackFwd->MinorFunction==1)&&(srbFwd->Function==SRB_FUNCTION_IO_CONTROL))
                {
                    hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - fwd request synchronously\n");

 	                //fwd request
	                return hfilterSendIrpToNextDriverSynchronously(DeviceObject, Irp);
                }
                
                /*
                switch(srbFwd->Function)
	            {
                    case SRB_FUNCTION_EXECUTE_SCSI:
				        break;

			        default:
                        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - fwd srb function 0x%x\n", srbFwd->Function);
				        break;
                }
                */
            }

        }
    }
#endif

    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_INVALID_SUB_DEVICE_INDEX)
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_FORWARD_REQUEST\n");

 	//fwd request
	return hfilterSendIrpToNextDriver(DeviceObject, Irp);
}
#endif



UINT64 hfilterByteToBlock(UINT64 val, UINT64 blockLength)
{
    return (UINT64) (val/blockLength);
}

UINT64 hfilterBlockToByte(UINT64 val, UINT64 blockLength)
{
    return (UINT64) (val * blockLength);
}


int hfilterSendSubDeviceReadWriteSubIrpCommand(hfilterReadWriteSubIrpCommandContextStruct *commandContext, UINT32 scsiOpIndex)
{
    int returnStatus = HFILTER_OK;
    PDEVICE_OBJECT targetDeviceObject = NULL;
    hfilterSCSIOPStruct *scsiOP = NULL;

    PIRP irp = NULL;
    PSCSI_REQUEST_BLOCK srb = NULL;
    
    NTSTATUS driverCallStatus = STATUS_SUCCESS;
    int status = HFILTER_OK;

    if(commandContext == NULL)
    {
        returnStatus = HFILTER_ERROR;
        goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
    }

    if((commandContext->superIrp == NULL) || (commandContext->superSrb == NULL) || (commandContext->scsiOpCount == 0))
    {
        hfilterDebugPrint("ERROR: hfilterSendSubDeviceReadWriteSubIrpCommand - param check 2\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
    }
    
    scsiOP = &commandContext->scsiOpList[scsiOpIndex];

    if((scsiOP == NULL) || (scsiOP->inuse==0))
    {
        hfilterDebugPrint("ERROR: hfilterSendSubDeviceReadWriteSubIrpCommand - param check 3\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
    }

    targetDeviceObject = hfilterConfig->subDeviceList[scsiOP->hfilterSubDeviceIndex].targetDeviceObject;

    if(targetDeviceObject==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSendSubDeviceReadWriteSubIrpCommand - param check 4\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
    }

    //alloc command context and irp
    irp = IoAllocateIrp(targetDeviceObject->StackSize, FALSE);

    if(irp == NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSendSubDeviceReadWriteSubIrpCommand - param check 5\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
    }

    //alloc data transfer buffer
    if(scsiOP->sectorCount)
    {
        UINT64 opBlockOffset = 0;
        UINT64 opByteOffset = 0;

        opBlockOffset = scsiOP->startLBA - commandContext->superCDBStartLBA;
        opByteOffset = hfilterBlockToByte(opBlockOffset, SUB_DEVICE_BLOCK_SIZE);

        
        scsiOP->dataTransferBufferSize = hfilterBlockToByte(scsiOP->sectorCount, SUB_DEVICE_BLOCK_SIZE);
        scsiOP->opByteOffset = opByteOffset;
        
        
        if(commandContext->superSrbDataTransferBuffer)
            scsiOP->dataTransferBuffer = commandContext->superSrbDataTransferBuffer + opByteOffset;
        else scsiOP->dataTransferBuffer = NULL;
        
        hfilterDebugPrint("INFO: hfilterSendSubDeviceReadWriteSubIrpCommand - commandContext->superCDBStartLBA: 0x%llx scsiOP->startLBA: 0x%llx scsiOP->sectorCount: 0x%llx\n", commandContext->superCDBStartLBA, scsiOP->startLBA, scsiOP->sectorCount);
        hfilterDebugPrint("INFO: hfilterSendSubDeviceReadWriteSubIrpCommand - superSrbDataTransferBuffer: %p opByteOffset: 0x%llx dataTransferBuffer: %p dataTransferBufferSize 0x%x\n", commandContext->superSrbDataTransferBuffer, scsiOP->opByteOffset, scsiOP->dataTransferBuffer, scsiOP->dataTransferBufferSize);
#if 0

        //scsiOP->dataTransferBufferSize = scsiOP->sectorCount << (BLOCK_TO_BYTES_SHIFT);
        scsiOP->dataTransferBufferSize = hfilterBlockToByte(scsiOP->sectorCount, SUB_DEVICE_BLOCK_SIZE);
        scsiOP->dataTransferBuffer = hfilterMemoryAlloc(NonPagedPool, scsiOP->dataTransferBufferSize);
        if(scsiOP->dataTransferBuffer==NULL)
        {
            hfilterDebugPrint("ERROR: hfilterSendSubDeviceReadWriteSubIrpCommand - hfilterMemoryAlloc %d\n", scsiOP->dataTransferBufferSize);
            returnStatus = HFILTER_ERROR;
            goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
        }
        memset(scsiOP->dataTransferBuffer, 0, scsiOP->dataTransferBufferSize);
#endif

    } else {
        scsiOP->dataTransferBufferSize = 0;
        scsiOP->dataTransferBuffer = NULL;
    }

    scsiOP->senseDataBufferSize = SENSE_BUFFER_SIZE;

    //alloc sense data buffer
    if(scsiOP->senseDataBufferSize)
    {
        //scsiOP->senseDataBufferSize = SENSE_BUFFER_SIZE;
        scsiOP->senseDataBuffer = hfilterMemoryAlloc(NonPagedPool, scsiOP->senseDataBufferSize);
        if(scsiOP->senseDataBuffer==NULL)
        {
            returnStatus = HFILTER_ERROR;
            goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
        }
        memset(scsiOP->senseDataBuffer, 0, scsiOP->senseDataBufferSize);
    } else {
        scsiOP->senseDataBufferSize = 0;
        scsiOP->senseDataBuffer = NULL;
    }

    srb = &scsiOP->srb;

    //setup SRB
    status = hfilterBuildSubDeviceReadWriteSRB(srb, irp, commandContext->superSrb, scsiOP);
    
    if(status!=HFILTER_OK)
    {
        hfilterDebugPrint("ERROR: hfilterSendSubDeviceReadWriteSubIrpCommand - hfilterBuildSubDeviceReadWriteSRB\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
    }

    //setup IRP
    status = hfilterBuildSubDeviceReadWriteIRP(irp, srb, scsiOP, commandContext);
    
    if(status!=HFILTER_OK)
    {
        hfilterDebugPrint("ERROR: hfilterSendSubDeviceReadWriteSubIrpCommand - hfilterBuildSubDeviceReadWriteIRP\n");
        returnStatus = HFILTER_ERROR;
        goto hfilterSendSubDeviceReadWriteSubIrpCommand_end;
    }

    scsiOP->irp = irp;

    //send the irp on its way...
    driverCallStatus = IoCallDriver(targetDeviceObject, irp);

    if(driverCallStatus == STATUS_PENDING)
    {
        hfilterDebugPrint("INFO: hfilterSendSubDeviceReadWriteSubIrpCommand - STATUS_PENDING\n");
    } else {
        hfilterDebugPrint("INFO: hfilterSendSubDeviceReadWriteSubIrpCommand !STATUS_PENDING 0x%x\n", driverCallStatus);
    }

hfilterSendSubDeviceReadWriteSubIrpCommand_end:
    if(returnStatus==HFILTER_ERROR)
    {
        if(scsiOP)
        {
            if(scsiOP->senseDataBuffer)
                hfilterMemoryFree(scsiOP->senseDataBuffer);
        }

        if(irp)
            IoFreeIrp(irp);
    }

    return returnStatus;
}

int hfilterBuildSubDeviceReadWriteSRB(SCSI_REQUEST_BLOCK *srb, IRP *irp, SCSI_REQUEST_BLOCK *superSrb, hfilterSCSIOPStruct *scsiOP)
{
    UINT32 startingLBA = 0;
    UINT16 sectorCount = 0;

    if((srb==NULL) || (irp==NULL) || (superSrb==NULL) || (scsiOP==NULL))
    {
        return HFILTER_ERROR;
    }

    memset(srb, 0, sizeof(SCSI_REQUEST_BLOCK));

    srb->Length = sizeof(SCSI_REQUEST_BLOCK);
    srb->Function = SRB_FUNCTION_EXECUTE_SCSI;

    srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;

    //startingLBA =  (UINT32) scsiOP->startLBA;
    startingLBA =  (UINT32) scsiOP->mappedStartLBA;
    sectorCount =  (UINT16) scsiOP->sectorCount;    

    if(scsiOP->opcode == SCSIOP_READ)
    {
        zfilterBuildSubDeviceReadCDB(startingLBA, sectorCount, srb->Cdb, &srb->CdbLength);
        //srb->SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
        srb->SrbFlags = (superSrb->SrbFlags & ~SRB_FLAGS_PORT_DRIVER_ALLOCSENSE);
    }
    
    if(scsiOP->opcode == SCSIOP_WRITE)
    {
        zfilterBuildSubDeviceWriteCDB(startingLBA, sectorCount, srb->Cdb, &srb->CdbLength);
        //srb->SrbFlags = SRB_FLAGS_DATA_OUT | SRB_FLAGS_DISABLE_SYNCH_TRANSFER;
        srb->SrbFlags = (superSrb->SrbFlags & ~SRB_FLAGS_PORT_DRIVER_ALLOCSENSE);
    }

    srb->DataTransferLength = (ULONG) scsiOP->dataTransferBufferSize;
    srb->DataBuffer = scsiOP->dataTransferBuffer;
    
    srb->SenseInfoBufferLength = (UCHAR) scsiOP->senseDataBufferSize;
    srb->SenseInfoBuffer = scsiOP->senseDataBuffer;

    srb->TimeOutValue = HFILTER_SUB_DEVICE_READ_WRITE_COMMAND_TIMEOUT_VALUE;
    
    srb->OriginalRequest = irp;

    srb->QueueSortKey = (ULONG) startingLBA;

    
    hfilterPrintBuffer("sub CDB", srb->Cdb, srb->CdbLength, 1);
    
    return HFILTER_OK;
}

void zfilterBuildSubDeviceReadCDB(UINT32 startingLBA, UINT16 sectorCount, UCHAR *cdb, UCHAR *cdbLength)
{
    CDB *pCdb = NULL;

    pCdb = (CDB *) cdb;
    
    pCdb->CDB10.OperationCode = SCSIOP_READ;

    pCdb->CDB10.LogicalBlockByte0 = GET_BYTE(startingLBA, 3); //(MSB)
    pCdb->CDB10.LogicalBlockByte1 = GET_BYTE(startingLBA, 2);
    pCdb->CDB10.LogicalBlockByte2 = GET_BYTE(startingLBA, 1);
    pCdb->CDB10.LogicalBlockByte3 = GET_BYTE(startingLBA, 0); //LSB

    pCdb->CDB10.TransferBlocksMsb = GET_BYTE(sectorCount, 1); //(MSB)
    pCdb->CDB10.TransferBlocksLsb = GET_BYTE(sectorCount, 0); //LSB

    *cdbLength = 10;
}

void zfilterBuildSubDeviceWriteCDB(UINT32 startingLBA, UINT16 sectorCount, UCHAR *cdb, UCHAR *cdbLength)
{
    CDB *pCdb = NULL;

    pCdb = (CDB *) cdb;
    
    pCdb->CDB10.OperationCode = SCSIOP_WRITE;

    pCdb->CDB10.LogicalBlockByte0 = GET_BYTE(startingLBA, 3); //(MSB)
    pCdb->CDB10.LogicalBlockByte1 = GET_BYTE(startingLBA, 2);
    pCdb->CDB10.LogicalBlockByte2 = GET_BYTE(startingLBA, 1);
    pCdb->CDB10.LogicalBlockByte3 = GET_BYTE(startingLBA, 0); //LSB

    pCdb->CDB10.TransferBlocksMsb = GET_BYTE(sectorCount, 1); //(MSB)
    pCdb->CDB10.TransferBlocksLsb = GET_BYTE(sectorCount, 0); //LSB

    *cdbLength = 10;
}


void remapReadWriteLBA(UCHAR *cdb, UINT32 mappedStartingLBA)
{
    CDB *pCdb = NULL;

    pCdb = (CDB *) cdb;

    pCdb->CDB10.LogicalBlockByte0 = GET_BYTE(mappedStartingLBA, 3); //(MSB)
    pCdb->CDB10.LogicalBlockByte1 = GET_BYTE(mappedStartingLBA, 2);
    pCdb->CDB10.LogicalBlockByte2 = GET_BYTE(mappedStartingLBA, 1);
    pCdb->CDB10.LogicalBlockByte3 = GET_BYTE(mappedStartingLBA, 0); //LSB
}


int hfilterBuildSubDeviceReadWriteIRP(IRP *irp, SCSI_REQUEST_BLOCK *srb, hfilterSCSIOPStruct *scsiOP, hfilterReadWriteSubIrpCommandContextStruct *commandContext)
{
    PIO_STACK_LOCATION irpStack = NULL;

    if((irp==NULL) || (srb==NULL) || (scsiOP==NULL) || (commandContext==NULL))
    {
        return HFILTER_ERROR;
    }

    /*
    if((srb->DataTransferLength) && (srb->DataBuffer == NULL))
    {
        return HFILTER_ERROR;
    }
    */

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

    //irp->MdlAddress = commandContext->superIrp->MdlAddress;

    if(srb->DataTransferLength)
    {
        if(srb->DataBuffer)
        {
            irp->MdlAddress = IoAllocateMdl(srb->DataBuffer, srb->DataTransferLength, FALSE, FALSE, irp);
            IoBuildPartialMdl(commandContext->superIrp->MdlAddress, irp->MdlAddress, srb->DataBuffer, srb->DataTransferLength);        
    #if 0
            IoAllocateMdl(srb->DataBuffer, srb->DataTransferLength, FALSE, FALSE, irp);
            MmBuildMdlForNonPagedPool(irp->MdlAddress);
    #endif
        } else {
            UINT8 *virtualAddress = NULL;
            UINT8 *systemAddress = NULL;
            UINT8 *temp = NULL;
            UINT8 *temp2 = NULL;
#if 0
            try
            {
                if(srb->Cdb[0]==SCSIOP_READ)
                {
                    hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - write probe\n");
                    MmProbeAndLockPages(commandContext->superIrp->MdlAddress, KernelMode, IoWriteAccess);
                }

                if(srb->Cdb[0]==SCSIOP_WRITE)
                {
                    hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - read probe\n");
                    MmProbeAndLockPages(commandContext->superIrp->MdlAddress, KernelMode, IoReadAccess);
                }
            }
                       
            except(EXCEPTION_EXECUTE_HANDLER)
            {
                hfilterDebugPrint("ERROR: hfilterBuildSubDeviceReadWriteIRP - MmProbeAndLockPages\n");

                ASSERT(FALSE);
            }
#endif




            //virtualAddress = commandContext->superSrbDataTransferBuffer + scsiOP->opByteOffset;
            temp = (UINT8 *)MmGetMdlVirtualAddress(commandContext->superIrp->MdlAddress);
            //temp = (UINT8 *)MmGetSystemAddressForMdlSafe(commandContext->superIrp->MdlAddress, NormalPagePriority);

            virtualAddress = temp + scsiOP->opByteOffset;

            hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - virtualAddress: %p superSrbDataTransferBuffer: %p opByteOffset: 0x%llx\n", virtualAddress, commandContext->superSrbDataTransferBuffer, scsiOP->opByteOffset);

            irp->MdlAddress = IoAllocateMdl(virtualAddress, srb->DataTransferLength, FALSE, FALSE, irp);
            IoBuildPartialMdl(commandContext->superIrp->MdlAddress, irp->MdlAddress, virtualAddress, srb->DataTransferLength);

            printMDL(irp->MdlAddress, "sub irp");
            
            srb->DataBuffer = virtualAddress;

            hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - setting srb->DataBuffer to %p\n", srb->DataBuffer);
#if 0
            temp2 = MmGetSystemAddressForMdlSafe(commandContext->superIrp->MdlAddress, NormalPagePriority);

            if(temp2==NULL)
            {
                hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - MmGetSystemAddressForMdlSafe ERROR\n");
                ASSERT(FALSE);
            }

            hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - MmGetSystemAddressForMdlSafe OK\n");

            *temp2 = 0xa;
            temp2++;
            *temp2 = 0xb;
#endif
/*

            hfilterDebugPrint("INFO: remap\n"); 

            systemAddress = MmGetSystemAddressForMdlSafe(commandContext->superIrp->MdlAddress, NormalPagePriority);

            if(systemAddress==NULL)
            {
                hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - MmGetSystemAddressForMdlSafe ERROR\n"); 
                ASSERT(FALSE);
            }

            systemAddress = systemAddress + scsiOP->opByteOffset;

            irp->MdlAddress = IoAllocateMdl(systemAddress, srb->DataTransferLength, FALSE, FALSE, irp);

            MmBuildMdlForNonPagedPool(irp->MdlAddress);
*/

            hfilterDebugPrint("INFO: hfilterBuildSubDeviceReadWriteIRP - MmGetMdlByteCount: 0x%x\n", MmGetMdlByteCount(irp->MdlAddress)); 
        }
    }
    

    IoSetCompletionRoutine(irp, (PIO_COMPLETION_ROUTINE)hfilterSubDeviceReadWriteCommandSubIrpCompletionCallback, commandContext, TRUE, TRUE, TRUE);

    return HFILTER_OK;
}

NTSTATUS hfilterSubDeviceReadWriteCommandSubIrpCompletionCallback(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    hfilterReadWriteSubIrpCommandContextStruct *commandContext = NULL;
    hfilterSCSIOPStruct *scsiOP = NULL;
    SCSI_REQUEST_BLOCK *srb = NULL;
    UINT32 scsiOpIndex = 0;
    volatile UINT32 scsiOpCompletionCount = 0;

    hfilterDebugPrint("INFO: hfilterSubDeviceReadWriteCommandSubIrpCompletionCallback\n");

    if((Irp==NULL) || (Context==NULL))
    {
  	    //always return STATUS_MORE_PROCESSING_REQUIRED to terminate the completion processing of the IRP.
  	    return STATUS_MORE_PROCESSING_REQUIRED;
    }

    commandContext = (hfilterReadWriteSubIrpCommandContextStruct *) Context;


    //find the scsiop
    for(scsiOpIndex=0; scsiOpIndex < commandContext->scsiOpCount; scsiOpIndex++)
    {
        scsiOP = &commandContext->scsiOpList[scsiOpIndex];

        if((scsiOP==NULL)||(scsiOP->inuse==0))
            continue;

        if(scsiOP->irp == Irp)
            srb = &scsiOP->srb;
    }

    if(srb==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSubDeviceReadWriteCommandSubIrpCompletionCallback - srb==NULL\n");
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    if(SRB_STATUS(srb->SrbStatus)==SRB_STATUS_SUCCESS)
    {
        hfilterDebugPrint("INFO: hfilterSubDeviceReadWriteCommandSubIrpCompletionCallback - SRB_STATUS_SUCCESS\n");
    } else {
        hfilterHandleSubDeviceReadWriteCommandSubIrpFailed(srb, Irp, commandContext);
    }

    hfilterDumpSrb(srb, "subIrp");

    if(Irp->MdlAddress)
        IoFreeMdl(Irp->MdlAddress);

    IoFreeIrp(Irp);


    scsiOpCompletionCount = InterlockedIncrement(&commandContext->scsiOpCompletionCount);

    if(scsiOpCompletionCount==commandContext->scsiOpCount)
    {
        //we are done with this command text... queue a work item to complete this commandContext
        //hfilterQueueWorkItem(commandContext->hfilterDeviceObject, hfilterSubDeviceReadWriteCommandSuperIrpCompletionCallback, commandContext);
        hfilterDebugPrint("INFO: hfilterSubDeviceReadWriteCommandSubIrpCompletionCallback - direct cb\n");
        hfilterSubDeviceReadWriteCommandSuperIrpCompletionCallback(NULL, commandContext, NULL);
    }


  	//always return STATUS_MORE_PROCESSING_REQUIRED to terminate the completion processing of the IRP.
  	return STATUS_MORE_PROCESSING_REQUIRED;
}

int hfilterQueueWorkItem(DEVICE_OBJECT *deviceObj, PIO_WORKITEM_ROUTINE callback, void *context)
{
    PIO_WORKITEM workItem = NULL;

    workItem = IoAllocateWorkItem(deviceObj);

    if(workItem==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterQueueWorkItem - IoAllocateWorkItem\n");
        return HFILTER_ERROR;
    }

    IoQueueWorkItemEx(workItem, callback, DelayedWorkQueue, context);

    return HFILTER_OK;
}



VOID hfilterSubDeviceReadWriteCommandSuperIrpCompletionCallback(PVOID IoObject, PVOID Context, PIO_WORKITEM IoWorkItem)
{
    hfilterReadWriteSubIrpCommandContextStruct *commandContext = NULL;
    hfilterSCSIOPStruct *scsiOP = NULL;
    UINT32 scsiOpIndex = 0;
    IRP *superIrp = NULL;
    SCSI_REQUEST_BLOCK *superSrb = NULL;
    UINT64 xferLength = 0;
    UINT64 superCDBStartLBA = 0;

    //if((IoObject==NULL) || (Context==NULL) || (IoWorkItem==NULL))
    if(Context==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSubDeviceReadWriteCommandSuperIrpCompletionCallback - parameter error\n");
  	    return;
    }

    commandContext = (hfilterReadWriteSubIrpCommandContextStruct *) Context;

    for(scsiOpIndex=0; scsiOpIndex < HFILTER_SUB_DEVICE_MAX_NUMBER_OF_SCSI_OPS_PER_IRP; scsiOpIndex++)
    {
        scsiOP = &commandContext->scsiOpList[scsiOpIndex];

        if((scsiOP==NULL)||(scsiOP->inuse==0))
            continue;

        if(scsiOP->senseDataBuffer)
            hfilterMemoryFree(scsiOP->senseDataBuffer);
    }

    superIrp = commandContext->superIrp;
    superSrb = commandContext->superSrb;
    xferLength = commandContext->superSrbDataTransferSize;
    superCDBStartLBA = commandContext->superCDBStartLBA;

    hfilterMemoryFree(commandContext);
    commandContext = NULL;

    /*
    if(superIrp->MdlAddress)
    {
        MmUnlockPages(superIrp->MdlAddress);
    }
    */

    if(IoWorkItem)
        IoFreeWorkItem(IoWorkItem);

    if(superSrb)
    {
        hfilterDebugPrint("INFO: hfilterSubDeviceReadWriteCommandSuperIrpCompletionCallback - SRB_STATUS_SUCCESS\n");
        superSrb->SrbStatus = SRB_STATUS_SUCCESS;
        superSrb->ScsiStatus = 0;
        superSrb->InternalStatus = superCDBStartLBA;
        superSrb->QueueSortKey = superCDBStartLBA;

        hfilterDumpSrb(superSrb, "superSrb");
    }
                            
    if(superIrp)
    {
        hfilterDebugPrint("INFO: hfilterSubDeviceReadWriteCommandSuperIrpCompletionCallback - STATUS_SUCCESS\n");

        superIrp->IoStatus.Status = STATUS_SUCCESS;
        superIrp->IoStatus.Information = (ULONG_PTR) xferLength;
        //IoMarkIrpPending(superIrp);
        IoCompleteRequest(superIrp, IO_NO_INCREMENT);
    }
}


void hfilterHandleSubDeviceReadWriteCommandSubIrpFailed(SCSI_REQUEST_BLOCK *srb, IRP *irp, hfilterReadWriteSubIrpCommandContextStruct *commandContext)
{
    hfilterDebugPrint("INFO: hfilterHandleSubDeviceReadWriteCommandSubIrpFailed\n");
    ASSERT(FALSE);
}


hfilterLBARangeStruct *hfilterGetContainingSubDeviceLBARange(UINT64 lba)
{
    hfilterLBARangeStruct *subDeviceLBARange = NULL;
    int i = 0;

    for(i=0; i<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES; i++)
    {
        subDeviceLBARange = &hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[i];
                        
        if((subDeviceLBARange==NULL) || (subDeviceLBARange->inuse==0))
            continue;

        if((lba>=subDeviceLBARange->startLBA)&&(lba<=subDeviceLBARange->endLBA))
        {
            return subDeviceLBARange;
        }
    }

    return NULL;
}

hfilterLBARangeStruct *hfilterGetNextSubDeviceLBARange(UINT64 lba)
{
    hfilterLBARangeStruct *subDeviceLBARange = NULL;
    hfilterLBARangeStruct *nextLBARange = NULL;
    int i = 0;

    for(i=0; i<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES; i++)
    {
        subDeviceLBARange = &hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[i];
                        
        if((subDeviceLBARange==NULL) || (subDeviceLBARange->inuse==0))
            continue;

        if(subDeviceLBARange->startLBA>=lba)
        {
            if(nextLBARange==NULL)
            {
                nextLBARange = subDeviceLBARange;
            } else
            {
                if(subDeviceLBARange->startLBA < nextLBARange->startLBA)
                    nextLBARange = subDeviceLBARange;
            }
        }
    }

    return nextLBARange;
}

void hfilterDumpSrb(SCSI_REQUEST_BLOCK *srb, char *title)
{
    if(srb==NULL)
        return;

    hfilterDebugPrint("INFO: hfilterDumpSrb %s\n", (title)?title:"");
    hfilterDebugPrint("-------------------------\n");
    hfilterDebugPrint("Function:         0x%x\n", srb->Function);
    hfilterDebugPrint("SrbStatus:        0x%x\n", srb->SrbStatus);
    hfilterDebugPrint("ScsiStatus:       0x%x\n", srb->ScsiStatus);
    hfilterDebugPrint("PathId:           0x%x\n", srb->PathId);
    hfilterDebugPrint("TargetId:         0x%x\n", srb->TargetId);
    hfilterDebugPrint("Lun:              0x%x\n", srb->Lun);
    hfilterDebugPrint("QueueTag:         0x%x\n", srb->QueueTag);
    hfilterDebugPrint("QueueAction:      0x%x\n", srb->QueueAction);
    hfilterDebugPrint("SrbFlags:         0x%x\n", srb->SrbFlags);
    hfilterDebugPrint("TimeOutValue:     0x%x\n", srb->TimeOutValue);
    hfilterDebugPrint("InternalStatus:   0x%x\n", srb->InternalStatus);
    hfilterDebugPrint("QueueSortKey:     0x%x\n", srb->QueueSortKey);
    hfilterDebugPrint("LinkTimeoutValue: 0x%x\n", srb->LinkTimeoutValue);    
    hfilterDebugPrint("SenseInfoBufferLength:     0x%x\n", srb->SenseInfoBufferLength);
    hfilterDebugPrint("SenseInfoBuffer:           %p\n", srb->SenseInfoBuffer);
    hfilterDebugPrint("DataBuffer:                0x%x\n", srb->DataBuffer);
    hfilterDebugPrint("DataTransferLength:        %p\n", srb->DataTransferLength);
    hfilterDebugPrint("-------------------------\n");
}

void printMDL(PMDL Mdl, char *title)
{
    PMDL currentMdl, nextMdl;

    hfilterDebugPrint("=======[%s]=======\n", title);

    for (currentMdl = Mdl; currentMdl != NULL; currentMdl = nextMdl) 
    {
        nextMdl = currentMdl->Next;

        hfilterDebugPrint("-------------------------\n");
        hfilterDebugPrint("MdlFlags:         0x%x\n", currentMdl->MdlFlags);

        hfilterDebugPrint("MappedSystemVa:   %p\n", currentMdl->MappedSystemVa);
        hfilterDebugPrint("StartVa:          %p\n", currentMdl->StartVa);

        hfilterDebugPrint("ByteCount:        0x%x\n", currentMdl->ByteCount);
        hfilterDebugPrint("ByteOffset:       0x%x\n", currentMdl->ByteOffset);

        hfilterDebugPrint("Process:          %p\n", currentMdl->Process);
        
        hfilterDebugPrint("-------------------------\n");
    }
}

VOID MyFreeMdl(PMDL Mdl)
{
    PMDL currentMdl, nextMdl;

    for (currentMdl = Mdl; currentMdl != NULL; currentMdl = nextMdl) 
    {
        nextMdl = currentMdl->Next;
        if (currentMdl->MdlFlags & MDL_PAGES_LOCKED) 
        {
            MmUnlockPages(currentMdl);
        }
        IoFreeMdl(currentMdl);
    }
} 
