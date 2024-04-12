#include "main.h"
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <ntddk.h>
#include <ntdddisk.h>


// Function prototypes
const WCHAR gc_wszDeviceNameBuffer[] = L"\\Device\\ShMem_Test";
const WCHAR gc_wszDeviceSymLinkBuffer[] = L"\\DosDevices\\ShMem_Test";
const WCHAR gc_wszSharedSectionName[] = L"\\BaseNamedObjects\\SharedMemoryTest";


//reset values
PVOID	g_pSharedSection = NULL;
PVOID	g_pSectionObj = NULL;
HANDLE	g_hSection = NULL;

//----------------------------------------------------------------------   

// Define the size of the shared memory
#define SHARED_MEMORY_SIZE 1024

// Global pointer to the shared memory
PVOID g_SharedMemory = NULL;



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

