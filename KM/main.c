
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

//----------------- Functions --------------------------------//   

// Driver entry point
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
    DbgPrint(("Shared memory initialized at address: %p\n", g_SharedMemory));

    return STATUS_SUCCESS;
}

NTSTATUS CreateSharedMemory() {

    //------------------mostly mapping stuff---------------------

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    UNICODE_STRING uSectionName = { 0 };
    OBJECT_ATTRIBUTES objAttr = { 0 };
    LARGE_INTEGER lMaxSize;

	// init section name and obj attributes
    RtlInitUnicodeString(&uSectionName, gc_wszSharedSectionName);
    InitializeObjectAttributes(&objAttr, &uSectionName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	// Set the maximum size of the shared memory
    lMaxSize.HighPart = 0;
    lMaxSize.LowPart = SHARED_MEMORY_SIZE; //1024 * 10;
    
    //create section
    ntStatus = ZwCreateSection(&g_hSection, SECTION_ALL_ACCESS, &objAttr, &lMaxSize, PAGE_READWRITE, SEC_COMMIT, NULL);
    if (ntStatus != STATUS_SUCCESS) {
        DbgPrint("ZwCreateSection failed! Status: %p\n", ntStatus);
        return ntStatus;
    }

    //map section
	PVOID baseAddress = NULL;
	SIZE_T viewSize = SHARED_MEMORY_SIZE; // 1024 * 10
    ntStatus = ZwMapViewOfSection(g_hSection, NtCurrentProcess(), &baseAddress, 0, viewSize, NULL, &viewSize, ViewUnmap, 0, PAGE_READWRITE);
    if (ntStatus != STATUS_SUCCESS)
    {
        DbgPrint("ZwCreateSection fail! Status: %p\n", ntStatus);
		ZwClose(g_hSection);
		g_hSection = NULL;
        return ntStatus;
    }

	g_pSharedSection = baseAddress; //set global pointer to shared memory 
    DbgPrint("Shared memory created and mapped successfully at address %p\n");

    // Initialize the shared memory
    RtlZeroMemory(g_pSharedSection, SHARED_MEMORY_SIZE); 

    // Write a test message to the shared memory
    PCHAR TestString = "Message from kernel";
    memcpy(g_pSharedSection, TestString, strlen(TestString) + 1);  // +1 to include null terminator  

    return ntStatus; 
}

// Read from the shared memory
NTSTATUS ReadSharedMemory(void) {
	
    //initialize shared memory
    if (!g_hSection) {
        DbgPrint("Shared memory handle is not initialized.\n");
        return;
	}

    // Unmap shared memory if already mapped
    if (g_pSharedSection) {
        NTSTATUS unmapStatus = ZwUnmapViewOfSection(NtCurrentProcess(), g_pSharedSection); 
        //ZwUnmapViewOfSection(NtCurrentProcess(), g_pSharedSection);// unmap shared mem, return handle, specify unmap process
        if (unmapStatus != STATUS_SUCCESS) {
            DbgPrint("Failed to unmap shared memory: %p\n", unmapStatus);
            return unmapStatus; 
        }
        g_pSharedSection = NULL; //clear pointer after unmapping
    }

    //map shared memory

    DbgPrint(("Reading from shared memory: %s\n", (PCHAR)g_SharedMemory));

    SIZE_T ulViewSize = 10240; // 1024 * 10
    NTSTATUS ntStatus = ZwMapViewOfSection(g_hSection, NtCurrentProcess(), &g_pSharedSection, 0, ulViewSize, NULL, &ulViewSize, ViewShare, 0, PAGE_READWRITE | PAGE_NOCACHE);
    if (ntStatus != STATUS_SUCCESS)
    {
        DbgPrint("ZwMapViewOfSection fail! Status: %p\n", ntStatus);
        ZwClose(g_hSection);
        return ntStatus;
    }
    DbgPrint("ZwMapViewOfSection completed!\n");
    DbgPrint("Shared memory read data: %s\n", (PCHAR)g_pSharedSection);
    return STATUS_SUCCESS; 

}

//Dead func for now writing is a dectection vector 
NTSTATUS WriteSharedMemory(PVOID Data, SIZE_T Size) {
}

// Initialize the security descriptor for the shared memory
NTSTATUS InitializeSecurityDescriptor() {
    PSECURITY_DESCRIPTOR pSecurityDescriptor;
    NTSTATUS status;

    // Allocate memory for the security descriptor
    pSecurityDescriptor = ExAllocatePoolWithTag(NonPagedPoolNx, SECURITY_DESCRIPTOR_MIN_LENGTH, 'sdct');
    if (!pSecurityDescriptor) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Zero out the memory to avoid uninitialized memory
    RtlZeroMemory(pSecurityDescriptor, SECURITY_DESCRIPTOR_MIN_LENGTH);

    //Initialize and set up security attributes
    status = RtlCreateSecurityDescriptor(pSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);  // call function to set up security attributes
    if (!NT_SUCCESS(status)) {
		ExFreePool(pSecurityDescriptor); // Free the memory if the function fails
        return status;
    }

	//set up security attributes
    status = SetupSecurityAttributes(pSecurityDescriptor); 
    if (!NT_SUCCESS(status)) { 
        ExFreePool(pSecurityDescriptor); 
        return status; 
    } 

    // Use the security descriptor where needed, then free when done

    ExFreePool(pSecurityDescriptor);
    return STATUS_SUCCESS;
    
}
 
NTSTATUS SetupSecurityAttributes(PSECURITY_DESCRIPTOR pSecurityDescriptor) {

    NTSTATUS status = STATUS_SUCCESS; 
    PACL pAcl = NULL; 
    BOOLEAN bAclPresent, bAclDefaulted; 

    // Assuming implementation exists that creates and sets SIDs and ACLs
    // Example:
    // PCHAR pAclBuffer = ExAllocatePoolWithTag(NonPagedPool, AclSize, 'dacl');
    // InitializeAcl(pAcl, AclSize, ACL_REVISION);


    // Query DACL from the security descriptor (if already existing)
    status = RtlGetDaclSecurityDescriptor(pSecurityDescriptor, &bAclPresent, &pAcl, &bAclDefaulted);
    if (NT_SUCCESS(status) && bAclPresent && pAcl) {
        // Modify DACL as necessary
    }
    else {
        // Create a new DACL and set it to the security descriptor
    }

    // Free resources if allocated
    if (pAcl) {
        ExFreePool(pAcl);
    }

    return status;

}

// Create a security descriptor and ACL for the shared memory
NTSTATUS OnIRPWrite(PDEVICE_OBJECT pDriverObject, PIRP pIrp)
{
    UNREFERENCED_PARAMETER(pDriverObject);

    char szBuffer[255] = { 0 };
    strncpy(szBuffer, pIrp->AssociatedIrp.SystemBuffer, sizeof(szBuffer) - 1); // Secure copying
	szBuffer[sizeof(szBuffer) - 1] = '\0'; // Ensure null-termination of string
    DbgPrint("User message received: %s(%u)", szBuffer, strlen(szBuffer));
   
    if (!strncmp(szBuffer, "read_shared_memory", strlen("read_shared_memory")))
    {
        NTSTATUS status = ReadSharedMemory(); // Assume it returns an NTSTATUS 
        if (!NT_SUCCESS(status)) {
            return status; 
        }
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
    NTSTATUS status = STATUS_SUCCESS;

    switch (pStack->MajorFunction)
    {
    case IRP_MJ_WRITE:
        OnIRPWrite(pDriverObject, pIrp);
        break;

    default:
        pIrp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        break;
    }
    return status;
}

// Unload routine // good and done 
VOID OnDriverUnload(IN PDRIVER_OBJECT pDriverObject)
{
    UNREFERENCED_PARAMETER(pDriverObject);

    DbgPrint("Driver unload routine triggered!\n");

    // Unmap the shared memory section if mapped
    if (g_pSharedSection){
        ZwUnmapViewOfSection(NtCurrentProcess(), g_pSharedSection);
        g_pSharedSection = NULL;
	}

    // Dereference the section object if referenced
    if (g_pSectionObj){
        ObDereferenceObject(g_pSectionObj);
        g_pSectionObj = NULL;
	}

    // Close the section handle if open
    if (g_hSection){ 
        ZwClose(g_hSection); 
        g_hSection = NULL; 
	}
}

// thread stuff in here need to work out for later
VOID GuardedRegions() {

    // make a function to get around gaured regions in mem
    NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath); {
        // Initialize driver
        // Possibly set up initial hidden threads here
        HideThreads(); // Custom function to hide threads
        return STATUS_SUCCESS;
    }

    void SomeThreadFunction(); {
        // Function that gets called in a new thread
        // Implement hiding mechanisms when this thread is created
    }

    NTSTATUS CreateHiddenThread(); {
        HANDLE hThread;
        // Create a thread
        PsCreateSystemThread(&hThread, ..., (PKSTART_ROUTINE)SomeThreadFunction, ...);
        // Immediately manipulate or hide the thread
        HideSpecificThread(hThread);
        return STATUS_SUCCESS;
    }

    void HideThreads(); {
        // Code to manipulate thread visibility
        // This might involve DKOM or other techniques
    }

    void HideSpecificThread(HANDLE hThread); {
        // Code to hide a specific thread, possibly manipulating its properties
    }



} 

