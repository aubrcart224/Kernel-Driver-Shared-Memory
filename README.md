# Shared Memory Kernel Driver

This project implements a Windows kernel driver that creates and manages a section of shared memory. This allows kernel-mode processes to share data efficiently with each other or with user-mode processes. The driver demonstrates fundamental operations such as creating a device, mapping shared memory, and handling I/O requests.


```
THIS IS DEAD CODE
Since this driving is getting put to use I cant have it chilling on git unfortunatly,
the driver will be finished in a pirvate repo, if you are wanting to see the full code please reach out via email
 and I will be happy to send it to you if possible. 
``` 

## Features

- **Device Creation**: Setup a character device to interact with user-mode applications.
- **Shared Memory**: Allocate and manage shared memory accessible by both kernel and user-mode.
- **Driver Communication**: Use IOCTLs to facilitate user-mode to kernel-mode communication.
- **Security Handling**: Template functions to handle security attributes of shared resources.

## Prerequisites

To build and run this kernel driver, you will need:
- Windows 10 or higher
- Windows Driver Kit (WDK)
- Visual Studio 2019 or newer

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Installation

1. **Not for sale.**

   ```bash
   this will break your computer want the working code, email me. 
