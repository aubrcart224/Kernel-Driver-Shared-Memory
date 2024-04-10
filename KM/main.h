#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <fcntl.h>


// Windows headers
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
void DriverUnload(_In_ PDRIVER_OBJECT DriverObject);


// Declaration of a function to initialize shared memory
NTSTATUS InitializeSharedMemory(_In_ PDRIVER_OBJECT DriverObject);

/*
int main() {
    const char* name = "/my_shared_memory";
    const char* message_0 = "Hello, ";
    const char* message_1 = "Shared Memory!";
    int size = 4096; // Shared memory size

    // Create shared memory object
    int shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    // Configure the size of the shared memory object
    ftruncate(shm_fd, size);

    // Memory map the shared memory object
    void* ptr = mmap(0, size, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    // Write to the shared memory object
    sprintf(ptr, "%s", message_0);
    ptr += strlen(message_0);
    sprintf(ptr, "%s", message_1);

    // Wait for input before cleaning up
    printf("Press any key to continue...\n");
    getchar();

    // Clean up
    munmap(ptr, size);
    close(shm_fd);
    shm_unlink(name);

    return EXIT_SUCCESS;
}


//shared mem space attach read detach. 
NTSTATUS device_control(PDEVICE_OBJECT device_object, PIRP irp) {
	UNREFERENCED_PARAMETER(device_object);

	debug_print("[+] Device control called.\n");

	NTSTATUS status = STATUS_UNSUCCESSFUL;

	// we need this ot detrime which code was passed through. 
	PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);

	//access the request object sent from user mode. 
	auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);

	if (stack_irp == nullptr || request == nullptr) {
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return status;
	}



	//Target process we want to R&W memory to. 
	static PEPROCESS target_process = nullptr;

	//switch case to determine which code was passed through.
	const ULONG control_code = stack_irp->Parameters.DeviceIoControl.IoControlCode;
	switch (control_code) {
	case codes::attach: {
		//attach to process 
		status = PsLookupProcessByProcessId(request->process_id, &target_process); //why error here? :(,  fixed :) 
		break;
	}
	case codes::read: {
		//read process memory 
		if (target_process == nullptr) {
			break;
		}

		//copy memory from target process to our buffer 
		status = MmCopyVirtualMemory(target_process, request->target, PsGetCurrentProcess(), request->buffer, request->size, KernelMode, &request->return_size);
		break;
	}
	case codes::write: {
		//write process memory 
		if (target_process == nullptr) {
			break;
		}

		//copy memory from our buffer to target process 
		status = MmCopyVirtualMemory(PsGetCurrentProcess(), request->buffer, target_process, request->target, request->size, KernelMode, &request->return_size);
		break;
	}
	}


	irp->IoStatus.Status = status;
	irp->IoStatus.Information = sizeof(Request);
	return status;


	//IoCompleteRequest(irp, IO_NO_INCREMENT);


	//return irp->IoStatus.Status;
}

*/