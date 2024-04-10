#include "main.h"
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <ntddk.h>
#include <ntdddisk.h>

// Define the size of the shared memory
#define SHARED_MEMORY_SIZE 1024

// Global pointer to the shared memory
PVOID g_SharedMemory = NULL;



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


void DriverUnload(_In_ PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    KdPrint(("Driver Unload called\n"));

    // Clean up the shared memory
    if (g_SharedMemory) {
        ZwUnmapViewOfSection(ZwCurrentProcess(), g_SharedMemory);
    }

    // Rest of driver unload code...
}

//shared mem space implementation
NTSTATUS InitializeSharedMemory(_In_ PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);

    NTSTATUS status;
    PHYSICAL_ADDRESS HighestAcceptableAddress = { .QuadPart = -1 };
    SIZE_T commitSize = SHARED_MEMORY_SIZE;
    HANDLE sectionHandle = NULL;

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


