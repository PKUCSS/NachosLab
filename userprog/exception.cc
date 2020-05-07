// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "addrspace.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
        DEBUG('a', "Shutdown, initiated by user program.\n");
        printf("SC_Halt called\n");
        //machine->ClearMemory();
        interrupt->Halt();
    } 
    else if ((which == SyscallException) && (type == SC_Exit)){
       // machine->ClearMemory();
        printf("SC_EXIT called\n");
        int nextPC = machine->ReadRegister(NextPCReg);
        machine->WriteRegister(PCReg,nextPC);
    }
    else if ((which == SyscallException) && (type == SC_Create)){
        printf("SC_CREATE called\n");
        int address = machine->ReadRegister(4);
        int len = 0 ,value = 1;
        while(value != 0 ){ 
            machine->ReadMem(address++,1,&value);
			len++;
        }
        address = address-len;
        char* fileName = new char[len];
		for(int i = 0; i < len; i++){
			machine->ReadMem(address+i,1,&value);
			fileName[i] = (char)value;
		}
        printf("Create File: %s\n",fileName);
        fileSystem->Create(fileName,100);
        machine->AdvancePC();

    }
    else if ((which == SyscallException) && (type == SC_Open)){
        printf("SC_Open called\n");
        int address = machine->ReadRegister(4);
        int len = 0 ,value = 1;
        while(value != 0 ){ 
            machine->ReadMem(address++,1,&value);
			len++;
        }
        char* fileName = new char[len];
        address = address-len;
		for(int i = 0; i < len; i++){
			machine->ReadMem(address+i,1,&value);
			fileName[i] = (char)value;
		}
        printf("Open File: %s\n",fileName);
		OpenFile *openfile = fileSystem->Open(fileName);
		machine->WriteRegister(2,int(openfile));
        machine->AdvancePC();
		
    }
    else if((which == SyscallException) && (type == SC_Write)){
		printf("SC_Write Called\n");
		int address = machine->ReadRegister(4);
		int len = machine->ReadRegister(5);
		int fd = machine->ReadRegister(6);
		char* data = new char[len];
		int value;
		for(int i = 0; i < len; i++){
			machine->ReadMem(address+i,1,&value);
			data[i] = char(value);
		}
		if(fd != 1){
			OpenFile *openfile = (OpenFile*)fd;
			openfile->Write(data,len);
		}
		else{
			for(int i = 0; i < len; i++)
				putchar(data[i]);
		}
		machine->AdvancePC();
	}
    else if((which == SyscallException) && (type == SC_Read)){
		printf("SC_Read called\n");
		int address = machine->ReadRegister(4);
		int len = machine->ReadRegister(5);
		int fd = machine->ReadRegister(6);
		char* data = new char[len];
		int result;
		if(fd != 0){
			OpenFile *openfile = (OpenFile*)fd;
			result = openfile->Read(data,len);
		}
		else{
			for(int i = 0; i < len; i++)
				data[i] = getchar();
			    result = len;
		} 
		for(int i = 0; i < result; i++){
			machine->WriteMem(address+i,1,int(data[i]));
		}
		machine->WriteRegister(2,result);
		machine->AdvancePC();
	}
    else if((which == SyscallException) && (type == SC_Close)){
		printf("SC_Close called\n");
		int fd = machine->ReadRegister(4);
		OpenFile *openfile = (OpenFile*)fd;
		delete openfile;
		machine->AdvancePC();
	}
    else if (which == PageFaultException){
        //printf("PageFaultException\n");
        if (machine->tlb != NULL){ // TLB Miss
            // printf("TLB Miss\n");
			int address = machine->ReadRegister(BadVAddrReg);
			machine->tlbReplace(address);
        }
        else{  // Reverse Page Table Miss
            int address = machine->ReadRegister(BadVAddrReg);
            machine->ReverseTableReplace(address);
        }
    }
    else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
