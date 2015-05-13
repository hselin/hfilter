#include "hfilter.h"

#define IOCTL_HFILTER_INFO  CTL_CODE(FILE_DEVICE_DISK, 0x800, METHOD_BUFFERED,     FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_HFILTER_INFO2 CTL_CODE(FILE_DEVICE_DISK, 0x801, METHOD_OUT_DIRECT,   FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_HFILTER_INFO3 CTL_CODE(FILE_DEVICE_DISK, 0x802, METHOD_BUFFERED,     FILE_READ_DATA | FILE_WRITE_DATA)

PIRP queuedIrpPtr = NULL;
ULONG outputBufferLength = 0;

IO_WORKITEM_ROUTINE_EX hfilterTraceCompletionCallback;

extern int hfilterQueueWorkItem(DEVICE_OBJECT *deviceObj, PIO_WORKITEM_ROUTINE callback, void *context);

NTSTATUS hfilterHandleDeviceControlIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    hfilterDeviceExtensionStruct *hfilterDeviceExtension = NULL;
    PIO_STACK_LOCATION currentIrpStack = NULL;
    NTSTATUS status = STATUS_SUCCESS;

    hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp\n");

    currentIrpStack = IoGetCurrentIrpStackLocation(Irp);

    if(currentIrpStack==NULL)
    {
        return hfilterSendIrpToNextDriver(DeviceObject, Irp);
    }
 
    switch(currentIrpStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_HFILTER_INFO:
            hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - IOCTL_HFILTER_INFO\n");
            
            hfilterDebugPrint("INFO: [InputBufferLength 0x%x] [OutputBufferLength 0x%x]\n", currentIrpStack->Parameters.DeviceIoControl.InputBufferLength, currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength);

            if(Irp->AssociatedIrp.SystemBuffer)
            {
                hfilterPrintBuffer("input buffer", (unsigned char *) Irp->AssociatedIrp.SystemBuffer, currentIrpStack->Parameters.DeviceIoControl.InputBufferLength, 1);
                memset(Irp->AssociatedIrp.SystemBuffer, 0, currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength);
            }
            else
            {
                hfilterDebugPrint("INFO: Irp->AssociatedIrp.SystemBuffer is NULL\n");
            }

            if(1)
            {
                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength;
            } else {
                status = STATUS_INTERNAL_ERROR;
                Irp->IoStatus.Information = 0;
            }

            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);
            return status;
            break;


        case IOCTL_HFILTER_INFO2:
            hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - IOCTL_HFILTER_INFO2\n");
            
            hfilterDebugPrint("INFO: [InputBufferLength 0x%x] [OutputBufferLength 0x%x]\n", currentIrpStack->Parameters.DeviceIoControl.InputBufferLength, currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength);

            if(Irp->AssociatedIrp.SystemBuffer)
            {
                hfilterPrintBuffer("input buffer", (unsigned char *) Irp->AssociatedIrp.SystemBuffer, currentIrpStack->Parameters.DeviceIoControl.InputBufferLength, 1);
            }
            else
            {
                hfilterDebugPrint("INFO: Irp->AssociatedIrp.SystemBuffer is NULL\n");
            }

            if(Irp->MdlAddress)
            {
                UINT8 *outputBuffer = NULL;

                outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

                if(outputBuffer)
                {
                    memset(outputBuffer, 0xE, currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength);
                    //hfilterPrintBuffer("outputBuffer", (unsigned char *) outputBuffer, currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength, 1);
                }
                else
                {
                    hfilterDebugPrint("INFO: outputBuffer is NULL\n");
                }

            }
            else
            {
                hfilterDebugPrint("INFO: Irp->MdlAddress is NULL\n");
            }

            IoMarkIrpPending(Irp);

            queuedIrpPtr = Irp;
            outputBufferLength = currentIrpStack->Parameters.DeviceIoControl.OutputBufferLength;

            return STATUS_PENDING;
            break;

        case IOCTL_HFILTER_INFO3:
            hfilterDebugPrint("INFO: hfilterHandleDeviceControlIrp - IOCTL_HFILTER_INFO3\n");


            hfilterQueueWorkItem(DeviceObject, hfilterTraceCompletionCallback, NULL);

            status = STATUS_SUCCESS;
            Irp->IoStatus.Information = 0;

            Irp->IoStatus.Status = status;
            IoCompleteRequest(Irp, IO_NO_INCREMENT);

            return status;
            break;

        default:
            break;
    }

    return hfilterSendIrpToNextDriver(DeviceObject, Irp);
}



VOID hfilterTraceCompletionCallback(PVOID IoObject, PVOID Context, PIO_WORKITEM IoWorkItem)
{
    hfilterDebugPrint("INFO: hfilterTraceCompletionCallback\n");

    if(IoWorkItem)
        IoFreeWorkItem(IoWorkItem);
                            
    if(queuedIrpPtr)
    {
        PIRP irp = queuedIrpPtr;

        queuedIrpPtr = NULL;


        hfilterDebugPrint("INFO: hfilterTraceCompletionCallback - completing queued IRP\n");
        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = outputBufferLength;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        
    }
    else
    {
        hfilterDebugPrint("INFO: hfilterTraceCompletionCallback - queuedIrpPtr is NULL\n");
    }
}