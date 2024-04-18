
#include "main.h"
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>

#include <ntdddisk.h>

// Constants
#define SHARED_MEMORY_SIZE 10240  // 1024 * 10, increased clarity
#define DEVICE_NAME L"\\Device\\ShMem_Test"
#define DEVICE_SYMLINK L"\\DosDevices\\ShMem_Test"
#define SHARED_SECTION_NAME L"\\BaseNamedObjects\\SharedMemoryTest"


// Global variables // reset values to NULL 
PVOID g_pSharedSection = NULL; // shared memory section
HANDLE g_hSection = NULL; // handle to the shared memory section
PVOID g_SharedMemory = NULL; // shared memory pointer
PVOID	g_pSectionObj = NULL; // section object pointer


// Function prototypes
const WCHAR gc_wszDeviceNameBuffer[] = L"\\Device\\ShMem_Test";
const WCHAR gc_wszDeviceSymLinkBuffer[] = L"\\DosDevices\\ShMem_Test";
const WCHAR gc_wszSharedSectionName[] = L"\\BaseNamedObjects\\SharedMemoryTest";

//----------------------------------------------------------------------   

// Read from the shared memory
VOID ReadSharedMemory() {
	
    //initialize shared memory
    if (!g_hSection)
        return;

    //check if mem is mapped /= NULL 
    if (g_pSharedSection)
        ZwUnmapViewOfSection(NtCurrentProcess(), g_pSharedSection);// unmap shared mem, return handle, specify unmap process

    KdPrint(("Reading from shared memory: %s\n", (PCHAR)g_SharedMemory))
    SIZE_T ulViewSize = 1024 * 10;
    NTSTATUS ntStatus = ZwMapViewOfSection(g_hSection, NtCurrentProcess(), &g_pSharedSection, 0, ulViewSize, NULL, &ulViewSize, ViewShare, 0, PAGE_READWRITE | PAGE_NOCACHE);
    if (ntStatus != STATUS_SUCCESS)
    {
        DbgPrint("ZwMapViewOfSection fail! Status: %p\n", ntStatus);
        ZwClose(g_hSection);
        return;
    }
    DbgPrint("ZwMapViewOfSection completed!\n");

    DbgPrint("Shared memory read data: %s\n", (PCHAR)g_pSharedSection);



}
	


NTSTATUS CreateSharedMemory() {

    //------------------mostly mapping stuff---------------------

     NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    
     UNICODE_STRING uSectionName = { 0 };
     RtlInitUnicodeString(&uSectionName, gc_wszSharedSectionName);

     OBJECT_ATTRIBUTES objAttr = { 0 };
     InitializeObjectAttributes(&objAttr, &uSectionName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

     //create section
     LARGE_INTEGER lMaxSize = { 0 }; 
     lMaxSize.HighPart = 0; 
     lMaxSize.LowPart = 1024 * 10; 
     ntStatus = ZwCreateSection(&g_hSection, SECTION_ALL_ACCESS, &objAttr, &lMaxSize, PAGE_READWRITE, SEC_COMMIT, NULL);
     if (ntStatus != STATUS_SUCCESS)
     {
         DbgPrint("ZwCreateSection fail! Status: %p\n", ntStatus);
         return ntStatus;
     }

     //else enabled
     DbgPrint("ZwCreateSection completed!\n");

     //map section
     if (ntStatus != STATUS_SUCCESS)
     {
         DbgPrint("ObReferenceObjectByHandle fail! Status: %p\n", ntStatus);
         return ntStatus;
     }
     DbgPrint("ObReferenceObjectByHandle completed!\n");


     // --------------------------------------------------------------------------

     // Create a security descriptor and ACL for the shared memory
     PACL pACL = NULL;
     PSECURITY_DESCRIPTOR pSecurityDescriptor = { 0 };
     ntStatus = CreateStandardSCAndACL(&pSecurityDescriptor, &pACL);
     if (ntStatus != STATUS_SUCCESS)
     {
         DbgPrint("CreateStandardSCAndACL fail! Status: %p\n", ntStatus);
         ObDereferenceObject(g_pSectionObj);
         ZwClose(g_hSection);
         return ntStatus;
     }

     ExFreePool(pACL); //free pool
     ExFreePool(pSecurityDescriptor);


     //--------------------------------------------------------------------------
     //create section view
     SIZE_T ulViewSize = 0; 
     ntStatus = ZwMapViewOfSection(g_hSection, NtCurrentProcess(), &g_pSharedSection, 0, lMaxSize.LowPart, NULL, &ulViewSize, ViewShare, 0, PAGE_READWRITE | PAGE_NOCACHE); 
     if (ntStatus != STATUS_SUCCESS) 
     {  
         DbgPrint("ZwMapViewOfSection fail! Status: %p\n", ntStatus);  
         ObDereferenceObject(g_pSectionObj); 
         ZwClose(g_hSection);  
         return ntStatus;  
     }
     DbgPrint("ZwMapViewOfSection completed!\n"); 

     PCHAR TestString = "Message from kernel"; 
     memcpy(g_pSharedSection, TestString, 19);  
     ReadSharedMemory(); 

     return ntStatus; 


}

// Create a security descriptor and ACL for the shared memory
NTSTATUS OnIRPWrite(PDEVICE_OBJECT pDriverObject, PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDriverObject);

    char szBuffer[255] = { 0 };
    strcpy(szBuffer, pIrp->AssociatedIrp.SystemBuffer);
    DbgPrint("User message received: %s(%u)", szBuffer, strlen(szBuffer));

    if (!strcmp(szBuffer, "read_shared_memory"))
    {
        ReadSharedMemory();
    }

    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = strlen(szBuffer);
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// Create a security descriptor and ACL for the shared memory
NTSTATUS OnMajorFunctionCall(PDEVICE_OBJECT pDriverObject, PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDriverObject);

    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
    switch (pStack->MajorFunction)
    {
    case IRP_MJ_WRITE:
        OnIRPWrite(pDriverObject, pIrp);
        break;

    default:
        pIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    }
    return STATUS_SUCCESS;
}


NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath) 
{
    // Process params
    UNREFERENCED_PARAMETER(pRegistryPath);

    if (!pDriverObject)
    {
        DbgPrint("ShareMemTestSys driver entry is null!\n");
        return STATUS_FAILED_DRIVER_ENTRY;
    }

    // Hello world!
    DbgPrint("Driver loaded, system range start in %p, Our entry at: %p\n", MmSystemRangeStart, DriverEntry);

    // Register unload routine
    pDriverObject->DriverUnload = &OnDriverUnload;

    // Veriable decleration
    NTSTATUS ntStatus = 0;

    // Normalize name and symbolic link.
    UNICODE_STRING deviceNameUnicodeString, deviceSymLinkUnicodeString;
    RtlInitUnicodeString(&deviceNameUnicodeString, gc_wszDeviceNameBuffer);
    RtlInitUnicodeString(&deviceSymLinkUnicodeString, gc_wszDeviceSymLinkBuffer);

    // Create the device.
    PDEVICE_OBJECT pDeviceObject = NULL;
    ntStatus = IoCreateDevice(pDriverObject, 0, &deviceNameUnicodeString, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);
    if (ntStatus != STATUS_SUCCESS)
    {
        DbgPrint("ShareMemTestSys IoCreateDevice fail! Status: %p\n", ntStatus);
        return ntStatus;
    }

    // Create the symbolic link
    ntStatus = IoCreateSymbolicLink(&deviceSymLinkUnicodeString, &deviceNameUnicodeString);
    if (ntStatus != STATUS_SUCCESS)
    {
        DbgPrint("ShareMemTestSys IoCreateSymbolicLink fail! Status: %p\n", ntStatus);
        return ntStatus;
    }

    // Register driver major callbacks
    for (ULONG t = 0; t <= IRP_MJ_MAXIMUM_FUNCTION; t++)
        pDriverObject->MajorFunction[t] = &OnMajorFunctionCall;

    CreateSharedMemory();

    pDeviceObject->Flags |= DO_BUFFERED_IO;

    DbgPrint("ShareMemTestSys driver entry completed!\n");

    return STATUS_SUCCESS;
}



VOID OnDriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
    UNREFERENCED_PARAMETER(pDriverObject);

    DbgPrint("Driver unload routine triggered!\n");

    if (g_pSharedSection)
        ZwUnmapViewOfSection(NtCurrentProcess(), g_pSharedSection);

    if (g_pSectionObj)
        ObDereferenceObject(g_pSectionObj);

    if (g_hSection)
        ZwClose(g_hSection);

    UNICODE_STRING symLink;
    RtlInitUnicodeString(&symLink, gc_wszDeviceSymLinkBuffer);

    IoDeleteSymbolicLink(&symLink);
    if (pDriverObject && pDriverObject->DeviceObject)
    {
        IoDeleteDevice(pDriverObject->DeviceObject);
    }
}





NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
    
    DriverObject->DriverUnload = DriverUnload;
    KdPrint(("Driver Entry called\n"));

    // Initialize shared memory
    NTSTATUS status = InitializeSharedMemory(DriverObject);

    if (!NT_SUCCESS(status)) {
        // Handle error
        return status;
    }

    // Rest of driver entry code...
    //shared memory space implementation 

    return STATUS_SUCCESS;
}



//shared mem space implementation
NTSTATUS InitializeSharedMemory(_In_ PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);

    NTSTATUS status;
    PHYSICAL_ADDRESS HighestAcceptableAddress = { .QuadPart = -1 };
    SIZE_T commitSize = SHARED_MEMORY_SIZE;
    HANDLE sectionHandle = NULL;

    
    DbgPrintEx(0, 0, "calling CreateSharedMemory...\n");

    // Create a section object for the shared memory
    status = ZwCreateSection(
        &sectionHandle,
        SECTION_ALL_ACCESS,
        NULL,
        (PLARGE_INTEGER)&commitSize,
        PAGE_READWRITE,
        SEC_COMMIT,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        // Handle error
        return status;
    }

    // Map the section into kernel address space
    status = ZwMapViewOfSection(
        sectionHandle,
        ZwCurrentProcess(),
        &g_SharedMemory,
        0,
        SHARED_MEMORY_SIZE,
        NULL,
        &commitSize,
        ViewShare,
        0,
        PAGE_READWRITE
    );

    if (!NT_SUCCESS(status)) {
        // Handle error
        ZwClose(sectionHandle);
        return status;
    }

    ZwClose(sectionHandle);

    // The shared memory is now available at g_SharedMemory
    KdPrint(("Shared memory initialized at address: %p\n", g_SharedMemory));

    return STATUS_SUCCESS;
}







void DriverUnload(_In_ PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    KdPrint(("Driver Unload called\n"));

    // Clean up the shared memory
    if (g_SharedMemory) {
        ZwUnmapViewOfSection(ZwCurrentProcess(), g_SharedMemory);
    }

    // Rest of driver unload code...
}

