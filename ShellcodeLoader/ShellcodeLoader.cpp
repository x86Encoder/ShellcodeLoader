// ShellcodeLoader.cpp: define el punto de entrada de la aplicación de consola.
//

#include "stdafx.h"

using namespace std;

void parse_args(pProgramArgs args, int argc, const char** argv)
{
	string userInput; 

	// make a new ArgumentParser
	ArgumentParser parser;
	parser.appName("ShellcodeLoader");

	// add arguments
	parser.addArgument("-e", "--entrypoint", 1, 1);
	parser.addArgument("-a", "--address", 1, 1);
	parser.addArgument("-r", "--run");
	parser.addArgument("-b", "--break");
	parser.addFinalArgument("file");

	// parse the command-line arguments - throws if invalid format
	parser.parse(argc, argv);

	// retrieve the argument values
	if (parser.exists("e") || parser.exists("entrypoint")) {
		args->offset = stol(parser.retrieve<string>("entrypoint"), 0, 16);
	}
	
	if (parser.exists("a") || parser.exists("address")) {
		args->preferredAddress = stoll(parser.retrieve<string>("address"), 0, 16);
	}

	if (parser.exists("r") || parser.exists("run")) {
		args->nopause = true;
	}

	if (parser.exists("b") || parser.exists("break")) {
		args->debug = true;
		args->nopause = true;
	}
	
	// final argument
	args->file = parser.retrieve<string>("file");

	// check is there a debugger running
	if (IsDebuggerPresent() && !args->debug && !args->nopause) {
		cout << "[?] Debugger detected! Would you like to set a BP before execution? (y/n)" << endl;
		getline(cin, userInput);
		if (userInput == "y" || userInput == "Y") {
			args->debug = true;
			args->nopause = true;
		}
	}
}

int main(int argc, const char** argv)
{
	ProgramArgs args = { "", 0, false, false, 0 };
	char* printDirection;
	FUNC code;
	LPVOID memory = 0;

	// hello message
	system("cls");
	cout << "Welcome to ShellcodeLoader!" << endl;

	//Parse args
	parse_args(&args, argc, argv);
	
	// let's open the file
	cout << "[+] Opening file: " << args.file << endl;
	HANDLE handle = CreateFileA(args.file.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (handle == INVALID_HANDLE_VALUE) {
		cout << "[-] File doesn't exist" << endl;
		return 2;
	}

	// get the file size
	cout << "[+] Getting file size" << endl;
	DWORD size = GetFileSize(handle, 0);

	if (size <= 0) {
		cout << "[-] Error reading size of file" << endl;
		return 3;
	}

	// let's try to allocate memory in the specified address
	if (args.preferredAddress != NULL) {
		cout << "[+] Allocating space in preferred address: ";
		cout << showbase << hex << args.preferredAddress;
		cout << endl;

		memory = VirtualAlloc((LPVOID)args.preferredAddress, size, 0x3000, 0x40);
		if (memory == NULL) {
			cout << "[-] Failed! Trying in another address..." << endl;
			memory = VirtualAlloc(0, size, 0x3000, 0x40);
		}
	}
	else {
		cout << "[+] Allocating space in memory" << endl;
		memory = VirtualAlloc(0, size, 0x3000, 0x40);
	}

	code = (FUNC)memory;
	if (code == NULL) {
		cout << "[-] Error allocating space" << endl;
		return 4;
	}

	// read the file into memory
	cout << "[+] Reading file into buffer" << endl;
	DWORD bytesRead;
	bool read = ReadFile(handle, code, size, &bytesRead, 0);

	if (read == NULL) {
		cout << "[-] Error reading file" << endl;
		return 5;
	}

	// close the handle to the file
	CloseHandle(handle);
	
	// calc the entrypoint to the shellcode
	printDirection = reinterpret_cast<char*>(code);
	if (args.offset != NULL) {
		printDirection += args.offset;
		code = reinterpret_cast<FUNC>(printDirection);
	}

	cout << "[+] Entry point will be at address: ";
	cout << showbase << hex << (long long) code;
	cout << endl;
	
	// stop debugger 
	if (!args.nopause) {
		cout << "[+] Set a breakpoint to the Entry Point manually. Press any key to continue to the shellcode." << endl; 
		getchar();
	}

	// jump to shellcode
	cout << "[+] JUMP AROUND!" << endl;
	executeShellcode(args.debug, code);

	// free the memory allocated for the shellcode
	VirtualFree(memory, size, MEM_RELEASE);

	cout << "[+] Execution finished. Press any key to quit" << endl;
	getchar();

    return 0;
}

int filter(unsigned int code, struct _EXCEPTION_POINTERS *ep) {
	cout << "[!] Unhandled exception was generated during shellcode execution" << endl;
	return EXCEPTION_EXECUTE_HANDLER;
}

void executeShellcode(bool debugging, FUNC shellcode) {
	__try {
		if (debugging) {
			__debugbreak();
		}
		shellcode();
	}
	__except (filter(GetExceptionCode(), GetExceptionInformation())) {
	}
}