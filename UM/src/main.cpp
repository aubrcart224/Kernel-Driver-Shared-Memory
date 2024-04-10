#include <iostream>
#include <Windows.h>
#include <TLHelp32.h>


static DWORD get_process_id(const wchar_t* process_name) {

	DWORD process_id = 0;

	HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snap_shot == INVALID_HANDLE_VALUE) {
		return process_id;
	}


	PROCESSENTRY32W entry = {};
	entry.dwSize = sizeof(entry);

	if (Process32FirstW(snap_shot, &entry) == TRUE) {
		//check if the first handle is the one we want. 
		if (wcscmp(process_name, entry.szExeFile) == 0)
			process_id = entry.th32ProcessID;
		else {
			while (Process32NextW(snap_shot, &entry) == TRUE) {
				if (_wcsicmp(process_name, entry.szExeFile) == 0) {
					process_id = entry.th32ProcessID;
					break;
				}
			}
		}

	}

	CloseHandle(snap_shot);

	return process_id;
}


static std::uintptr_t get_module_base(const DWORD pid, const wchar_t* module_name) {

	std::uintptr_t module_base = 0;


	// Snap-shot of the process 'modules (dlls)'
	HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if (snap_shot == INVALID_HANDLE_VALUE) {
		return module_base;
	}

	MODULEENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Module32FirstW(snap_shot, &entry) == TRUE) {
		//check if the first handle is the one we want. 
		if (wcscmp(module_name, entry.szModule) != 0) { // maybe sub null ptr here  or NULL (NULL actually works)
			module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
		}
		//if not, loop through the rest of the modules.
		else {
			while (Module32NextW(snap_shot, &entry) == TRUE) {
				if (_wcsicmp(module_name, entry.szModule) != 0) { // maybe sub null ptr here or NULL (NULL actually works)
					module_base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
					break;
				}
			}
		}

	}

	CloseHandle(snap_shot);

	return module_base;
}

namespace driver {
	namespace codes {
		constexpr ULONG attach =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		constexpr ULONG read =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		constexpr ULONG write =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		//namespace codes

	}

	//shared between user and kernel space
	struct Request {

		HANDLE process_id;

		PVOID target;
		PVOID buffer;

		SIZE_T size;
		SIZE_T return_size;

	};

	//attach to process
	bool attach_to_process(HANDLE driver_handle, const DWORD pid) {

		Request r;
		r.process_id = reinterpret_cast <HANDLE>(pid);

		return DeviceIoControl(driver_handle, codes::attach, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);

	}


	//read process memory
	template <class T>
	T read_memory(HANDLE driver_handle, const std::uintptr_t addr) {
		T temp = {};

		Request r;
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = &temp;
		r.size = sizeof(T);

		DeviceIoControl(driver_handle, codes::read, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);

		return temp;

	}

	//write process memory
	template <class T>

	void write_memory(HANDLE driver_handle, const std::uintptr_t addr, const T& value) {
		Request r;
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = (PVOID)&value; //try value
		r.size = sizeof(T);

		DeviceIoControl(driver_handle, codes::write, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
	}


}


int main() {

	//notepad.exe
	//get process id of notepad 
	const DWORD pid = get_process_id(L"notepad.exe");

	if (pid == 0) {
		std::cout << "failed to get process id\n";
		std::cin.get();
		return 1;
	}

	//create handle to driver
	const HANDLE driver = CreateFile(L"\\\\.\\KM", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); // or GGENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr


	if (driver == INVALID_HANDLE_VALUE) {

		std::cout << "failed to get driver handle\n";
		std::cin.get();
		return 1;
	}


	if (driver::attach_to_process(driver, pid) == true) {
		std::cout << "Attachment success\n";
	}

	CloseHandle(driver);

	std::cin.get();

	return 0;
}


