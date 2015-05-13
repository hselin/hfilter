    if(status==HFILTER_COMPLETE_REQUEST)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_COMPLETE_REQUESTn");
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

    /*
void hfilterLBAIntersection(hfilterSubDeviceStruct subDevice, UINT64 opStartLBA, UINT32 opSectorCount, lbaIntersectionStruct *lbaIntersection)
{
	UINT64 opEndLBA = opStartLBA + opSectorCount - 1;

	for(i=0;i<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES;i++)
	{
		if(opStartLBA>=subDevice.lbaRangeList[i].startLBA){

		}
	}
}
*/


    #if 0
	/*if((hfilterDeviceExtension->subDeviceIndex==HFILTER_INVALID_SUB_DEVICE_INDEX) || 
		(hfilterDeviceExtension->subDeviceIndex==HFILTER_SSD_SUB_DEVICE_INDEX))*/
	if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_HDD_SUB_DEVICE_INDEX)
	{
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

	if(hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].targetDeviceObject==NULL)
	{
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

	//irpStack = IoGetNextIrpStackLocation(irp);

    if(irpStack==NULL)
    {
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
    }

    srb = irpStack->Parameters.Scsi.Srb;

    if(srb==NULL)
    {
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
    }


	switch(srb->Function)
	{
            case SRB_FUNCTION_EXECUTE_SCSI:
				break;
			default:
				status = HFILTER_FORWARD_REQUEST;
				goto hfilterHandleSCSIIrp_end;
				break;
    }


	switch(srb->Cdb[0]){
		case SCSIOP_:
		case SCSIOP_:
				//parse LBA
				startingLBA = 0;
				sectorCount = 0;
				//endingLBA = startingLBA + sectorCount - 1;
				break;
		default:
				status = HFILTER_FORWARD_REQUEST;
				goto hfilterHandleSCSIIrp_end;
				break;
	}

	hfilterLBAIntersection(&hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX], startingLBA, sectorCount, &lbaIntersection);

	//handle special case




	/*
	while(sectorCount)
	{
		for(i=0;i<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES;i++)
		{
			hfilterLBAIntersection(&hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[i], startingLBA, sectorCount, &lbaIntersection);
		
			
		}
	}
	*/
	




hfilterHandleSCSIIrp_end:

    if(status==HFILTER_COMPLETE_REQUEST)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - complete IRP\n");
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }

 	//fwd request
	return hfilterSendIrpToNextDriver(DeviceObject, Irp);
#endif




/*
NTSTATUS zfilterHandleDeviceControlIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    zfilterDeviceExtensionStruct *zfilterDeviceExtension = NULL;
    PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
 
    if(currentIrpStack->Parameters.DeviceIoControl.IoControlCode==IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES)
    {
        DEVICE_MANAGE_DATA_SET_ATTRIBUTES *dsa = NULL;
        int status = 0;

        dsa = (DEVICE_MANAGE_DATA_SET_ATTRIBUTES *) Irp->AssociatedIrp.SystemBuffer;

        status = zfilterHandleDSMIOCTL(DeviceObject, Irp, dsa);

        if(status==ZFILTER_COMPLETE_REQUEST)
        {
            zfilterDebugPrint("INFO: zfilterHandleDeviceControlIrp - complete IRP\n");
            Irp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return STATUS_SUCCESS;
        }
        zfilterDebugPrint("INFO: zfilterHandleDeviceControlIrp - fwd IRP\n");
    }


    return zfilterSendIrpToNextDriver(DeviceObject, Irp);
}

NTSTATUS zfilterHandlePowerIrp(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    return zfilterSendIrpToNextDriver(DeviceObject, Irp);
}

*/

		//case SCSIOP_READ16:
		//case SCSIOP_WRITE16:


                                hfilterMemoryFree(commandContext);
                                            //if(scsiOP->dataTransferBuffer)
                //hfilterMemoryFree(scsiOP->dataTransferBuffer);














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
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) DeviceObject->DeviceExtension;

    if(hfilterDeviceExtension==NULL)
	{
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_INVALID_SUB_DEVICE_INDEX)
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - subDeviceIndex %d\n", hfilterDeviceExtension->subDeviceIndex);

    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_HDD_SUB_DEVICE_INDEX)
	{
		status = HFILTER_FORWARD_REQUEST;
		goto hfilterHandleSCSIIrp_end;
	}

#if 0
    if(hfilterDeviceExtension->subDeviceIndex==HFILTER_HDD_SUB_DEVICE_INDEX)
	{
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - redirect HDD traffic\n");
		status = HFILTER_FORWARD_REQUEST;
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
                hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - SRB_FUNCTION_EXECUTE_SCSI\n");
				break;

			default:
                hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - !SRB_FUNCTION_EXECUTE_SCSI\n");
				status = HFILTER_FORWARD_REQUEST;
				goto hfilterHandleSCSIIrp_end;
				break;
    }


	switch(srb->Cdb[0]){
		case SCSIOP_READ:
		case SCSIOP_WRITE:
                {
                    PCDB pCdb = NULL;
                    UINT64 opStartLBA, startLBA = 0;
	                UINT32 opSectorCount, totalSubOpSectorCount = 0;
                    UINT64 opEndLBA, endLBA = 0;
                    int subCommandStatus = HFILTER_OK;
                    int i = 0;


                    hfilterLBARangeStruct *subDeviceLBARange = NULL;
                    hfilterReadWriteSubIrpCommandContextStruct *commandContext = NULL;
                    hfilterSCSIOPStruct *scsiOP = NULL;

                    hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - CDB READ/WRITE\n");

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



                    hfilterPrintBuffer("CDB", (unsigned char *)srb->Cdb, srb->CdbLength, 1);

                    hfilterDebugPrint("INFO: super startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%x\n", commandContext->superCDBStartLBA, commandContext->superCDBEndLBA, commandContext->superCDBSectorCount);

                    while(startLBA <= opEndLBA)
                    {
                        hfilterDebugPrint("INFO: startLBA: 0x%llx opEndLBA: 0x%llx totalSubOpSectorCount: 0x%x\n", startLBA, opEndLBA, totalSubOpSectorCount);

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
                        
                        for(i=0; i<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES; i++)
                        {
                            /*assume sorted list*/

                            subDeviceLBARange = &hfilterConfig->subDeviceList[HFILTER_SSD_SUB_DEVICE_INDEX].lbaRangeList[i];
                        
                            if((subDeviceLBARange==NULL) || (subDeviceLBARange->inuse==0))
                                continue;

                             //completely outside (left side)
                            if(subDeviceLBARange->endLBA < startLBA)
                            {
                                //skip this lba range
                                continue;
                            }

                            //completely inside...
                            if((subDeviceLBARange->startLBA <= startLBA) && (subDeviceLBARange->endLBA >= endLBA))
                            {
                                //everything is on ssd
                                scsiOP->inuse = 1;
                                scsiOP->startLBA = startLBA;
                                scsiOP->endLBA = endLBA;
                                scsiOP->sectorCount = (UINT64)(scsiOP->endLBA - scsiOP->startLBA + 1);
                                scsiOP->hfilterSubDeviceIndex = HFILTER_SSD_SUB_DEVICE_INDEX;
                                hfilterDebugPrint("INFO: op startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%llx device: %d [inside]\n", scsiOP->startLBA, scsiOP->endLBA, scsiOP->sectorCount, scsiOP->hfilterSubDeviceIndex);
                            }

                            //completely outside (right side)
                            if(subDeviceLBARange->startLBA > endLBA)
                            {
                                //everything is on hdd
                                scsiOP->inuse = 1;
                                scsiOP->startLBA = startLBA;
                                scsiOP->endLBA = endLBA;
                                scsiOP->sectorCount = (UINT64)(scsiOP->endLBA - scsiOP->startLBA + 1);
                                scsiOP->hfilterSubDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;
                                hfilterDebugPrint("%d %d\n", HFILTER_HDD_SUB_DEVICE_INDEX, scsiOP->hfilterSubDeviceIndex);
                                hfilterDebugPrint("INFO: op startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%llx device: %d [right]\n", scsiOP->startLBA, scsiOP->endLBA, scsiOP->sectorCount, scsiOP->hfilterSubDeviceIndex);
                            }


                            /*find intersection...*/


                            if(subDeviceLBARange->startLBA <= startLBA)
                            {
                                //segment start on ssd
                                scsiOP->inuse = 1;
                                scsiOP->startLBA = startLBA;
                                scsiOP->endLBA = subDeviceLBARange->endLBA;
                                scsiOP->sectorCount = (UINT64)(scsiOP->endLBA - scsiOP->startLBA + 1);
                                scsiOP->hfilterSubDeviceIndex = HFILTER_SSD_SUB_DEVICE_INDEX;
                                hfilterDebugPrint("INFO: op startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%llx device: %d [start ssd]\n", scsiOP->startLBA, scsiOP->endLBA, scsiOP->sectorCount, scsiOP->hfilterSubDeviceIndex);
                            }

                            if(subDeviceLBARange->startLBA > startLBA)
                            {
                                //segment start on hdd
                                scsiOP->inuse = 1;
                                scsiOP->startLBA = startLBA;
                                scsiOP->endLBA = subDeviceLBARange->startLBA - 1;
                                scsiOP->sectorCount = (UINT64)(scsiOP->endLBA - scsiOP->startLBA + 1);
                                scsiOP->hfilterSubDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;
                                hfilterDebugPrint("INFO: op startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%llx device: %d [start hdd]\n", scsiOP->startLBA, scsiOP->endLBA, scsiOP->sectorCount, scsiOP->hfilterSubDeviceIndex);
                            }

                            if(scsiOP->inuse == 0)
                            {
                                hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - scsiOP->inuse == 0\n");
                                ASSERT(FALSE);
                            }

                            break;
                        } //for(i=0; i<HFILTER_SUB_DEVICE_MAX_NUMBER_OF_LBA_RANGES; i++)


                        /*
                            If:
                            1. no subdevice lba range
                            2. subdevice lba ranges are on the left side
                        */
                        if(scsiOP->inuse == 0)
                        {
                            scsiOP->inuse = 1;
                            scsiOP->startLBA = startLBA;
                            scsiOP->endLBA = endLBA;
                            scsiOP->sectorCount = (UINT64)(scsiOP->endLBA - scsiOP->startLBA + 1);
                            scsiOP->hfilterSubDeviceIndex = HFILTER_HDD_SUB_DEVICE_INDEX;   
                            hfilterDebugPrint("INFO: op startLBA: 0x%llx endLBA: 0x%llx sectorCnt: 0x%llx device: %d [left]\n", scsiOP->startLBA, scsiOP->endLBA, scsiOP->sectorCount, scsiOP->hfilterSubDeviceIndex);
                        }

                        startLBA = scsiOP->endLBA + 1;
                        totalSubOpSectorCount += (UINT32) scsiOP->sectorCount;

                    } //while(startLBA <= opEndLBA)



                    //process scsiops
                    {
                        UINT32 scsiOpIndex = 0;

                        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - commandContext->scsiOpCount %d\n", commandContext->scsiOpCount);

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
                            hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - redirect to %d\n", scsiOP->hfilterSubDeviceIndex);
                            targetDeviceObject = hfilterConfig->subDeviceList[scsiOP->hfilterSubDeviceIndex].targetDeviceObject;

                            if(commandContext)
                                hfilterMemoryFree(commandContext);
                            
                            commandContext = NULL;
                            scsiOP = NULL;
                                                      
                            status = HFILTER_SEND_REQUEST;
                            goto hfilterHandleSCSIIrp_end;
                        }


                        //ready for multiple scsiops...

                        commandContext->superSrbDataTransferSize = (UINT64) commandContext->superSrb->DataTransferLength;

                        if(commandContext->superSrbDataTransferSize)
                        {

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
                                hfilterDebugPrint("ERROR: hfilterHandleSCSIIrp - commandContext->superSrbDataTransferBuffer==NULL\n");

                                if(commandContext)
                                    hfilterMemoryFree(commandContext);
                            
                                commandContext = NULL;

				                status = HFILTER_FORWARD_REQUEST;
				                goto hfilterHandleSCSIIrp_end;
                            }

                        } //if(commandContext->superSrbDataTransferSize)

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

        IoMarkIrpPending(Irp);
        return STATUS_PENDING;
    }

    if(status==HFILTER_SEND_REQUEST)
    {
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_SEND_REQUEST\n");

        if(targetDeviceObject)
        {
            return hfilterSendIrpToDriver(targetDeviceObject, Irp);
        } else {
            hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_SEND_REQUEST targetDeviceObject==NULL\n");
            return hfilterSendIrpToNextDriver(DeviceObject, Irp);
        }
    }

    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_INVALID_SUB_DEVICE_INDEX)
        hfilterDebugPrint("INFO: hfilterHandleSCSIIrp - HFILTER_FORWARD_REQUEST\n");

 	//fwd request
	return hfilterSendIrpToNextDriver(DeviceObject, Irp);
}
#endif

#if 0
VOID hfilterSaveSubDeviceConfig(PVOID IoObject, PVOID Context, PIO_WORKITEM IoWorkItem)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    PDEVICE_OBJECT devObj = NULL;
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

    hfilterDebugPrint("INFO: hfilterSaveSubDeviceConfig\n");

    if(IoWorkItem)
        IoFreeWorkItem(IoWorkItem);

    if(Context==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - param error\n");
        return;
	}

    devObj = (DEVICE_OBJECT *) Context;

    hfilterDeviceExtension = (hfilterDeviceExtensionStruct *) devObj->DeviceExtension;

    if(hfilterDeviceExtension==NULL)
	{
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterDeviceExtension is NULL\n");
        return;
	}
 
    if(hfilterDeviceExtension->subDeviceIndex!=HFILTER_SSD_SUB_DEVICE_INDEX)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - not ssd subdevice\n");
        return;
    }

    commandContext = hfilterMemoryAlloc(NonPagedPool, sizeof(hfilterCommandContextStruct));
    


    if(commandContext==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterMemoryAlloc(%d) commandContext\n", sizeof(hfilterCommandContextStruct));
        return;
    }

    memset(commandContext, 0, sizeof(hfilterCommandContextStruct));

    dataBuffer = hfilterMemoryAlloc(NonPagedPool, SUB_DEVICE_BLOCK_SIZE);
       
    if(dataBuffer==NULL)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterMemoryAlloc(%d) dataBuffer\n", sizeof(SUB_DEVICE_BLOCK_SIZE));
        goto hfilterSaveSubDeviceConfig_end;
    }    
    
    memset(dataBuffer, 0, SUB_DEVICE_BLOCK_SIZE);

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
    commandContext->dataTransferBuffer = dataBuffer;
    commandContext->dataTransferBufferSize = SUB_DEVICE_BLOCK_SIZE;


    commandContext->synchronous = 1;

    status = hfilterSendCommand(commandContext);

    if(status!=HFILTER_OK)
    {
        hfilterDebugPrint("ERROR: hfilterSaveSubDeviceConfig - hfilterSendCommand\n");
    }


    //wait and free stuff here...
    
    
hfilterSaveSubDeviceConfig_end:    
    if(commandContext)
        hfilterMemoryFree(commandContext);

    if(dataBuffer)
        hfilterMemoryFree(dataBuffer);

    hfilterDebugPrint(".\n");
}
#endif