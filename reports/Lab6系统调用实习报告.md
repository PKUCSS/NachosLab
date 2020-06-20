# 操作系统实习报告6——系统调用

2020.5

[TOC]

## 内容一 总体概述

本次实习希望在阅读Nachos系统调用相关源代码的基础上，通过修改Nachos系统的底层源代码，达到“实现系统调用”的目标。

系统调用（system call），指运行在用户空间的程序向操作系统内核请求需要更高权限运行的服务。系统调用提供用户程序与操作系统之间的接口。大多数系统交互式操作需求在内核态运行。如设备IO操作或者进程间通信。Nachos的初始版本只为用户程序提供了Halt系统调用，本次实习将提供10个系统调用(文件系统相关5个，用户程序相关5个)。

## 内容二 完成情况

### 任务完成列表

| Exercise 1 | Exercise 2 | Exercise 3 | Exercise 4 | Exercise 5 |
| ---------- | ---------- | ---------- | ---------- | ---------- |
| T          | Y          | Y          | Y          | Y          |

### 第一部分 理解Nachos系统调用

#### Exercise 1 源代码阅读

> 阅读与系统调用相关的源代码，理解系统调用的实现原理。
>
> - code/userprog/syscall.h
>
> - code/userprog/exception.cc
>
> - code/test/start.s

- `syscall.h`

  定义了11个系统调用号并声明了相应的系统调用函数。

  ~~~c
  #define SC_Halt		0
  #define SC_Exit		1
  #define SC_Exec		2
  #define SC_Join		3
  #define SC_Create	4
  #define SC_Open		5
  #define SC_Read		6
  #define SC_Write	7
  #define SC_Close	8
  #define SC_Fork		9
  #define SC_Yield	10
  ~~~

- `exception.cc`

  定义`ExceptionHandler`，根据异常类型和2号寄存器保存的具体类型进行相应的处理。初始版本只实现了`Halt`和`Exit`系统调用的处理，之前的虚拟内存Lab中实现了`PageFault`的处理，这次要增加10个系统调用的处理。从注释可以看出，系统调用号由寄存器`r2`传递，`r4`、`r5`、`r6`、`r7`传递系统调用的第1-4个参数。如果系统调用有返回值，要写回`r2`寄存器。为了避免反复执行系统调用，在返回前还要将PC+4.

  ~~~c
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
  ~~~

- `start.s`

  汇编代码，定义了用户程序`main`函数启动的入口和系统调用的入口，需要实现的10个系统调用启动对应的汇编代码已经完成。

通过阅读以上源代码，可以梳理出Nachos系统调用的过程：

- `OneInstruction`函数判断当当前指令是系统调用，转入`start.s`

- 通过过`start.s`确定系统调用入口，通过寄存器`r2`传递系统调用号，转入`exception. cc` (此时系统调用参数位于相应寄存器）

-  ` exception .cc `通过系统调用识別号识别系统调用，进行相关处理，此处理过程中调用内核系统调用函数（实现在`syscall.h`)，返回前更新`PCReg`的值。

-  系统调用结束，程序继续执行

 因此，要添加新的系统调用，要在`syscall.h`中增加新的系统调用号和函数声明，在`start.s`中增加新的汇编代码入口，在`exception.cc`中添加相应的条件判断调用处理函数。由于要添加的系统调用都已经声明好，只需要添加具体实现和`exception.cc`中判断、调用，并编写测试函数验证正确性。

为了方便更新PC,在`machine`类中添加一个`AdvancePC`函数,在每次系统调用返回前调用。

~~~cpp
void
Machine::AdvancePC(){
	WriteRegister(PrevPCReg,registers[PCReg]);
	WriteRegister(PCReg,registers[PCReg]+4);
	WriteRegister(NextPCReg,registers[NextPCReg]+4);
}
~~~



### 第二部分  文件系统相关的系统调用

#### Exercise 2 系统调用实现

>  类比Halt的实现，完成与文件系统相关的系统调用：Create, Open，Close，Write，Read。Syscall.h文件中有这些系统调用基本说明。

`syscall.h`中已经声明了文件系统调用的功能、参数和返回值：

~~~cpp
/* Create a Nachos file, with "name" */
void Create(char *name);

/* Open the Nachos file "name", and return an "OpenFileId" that can 
 * be used to read and write to the file.
 */
OpenFileId Open(char *name);

/* Write "size" bytes from "buffer" to the open file. */
void Write(char *buffer, int size, OpenFileId id);

/* Read "size" bytes from the open file into "buffer".  
 * Return the number of bytes actually read -- if the open file isn't
 * long enough, or if it is an I/O device, and there aren't enough 
 * characters to read, return whatever is available (for I/O devices, 
 * you should always wait until you can return at least one character).
 */
int Read(char *buffer, int size, OpenFileId id);

/* Close the file, we're done reading and writing to it. */
void Close(OpenFileId id);
~~~

##### Create

参数是文件名指针，需要读出文件后调用`filesystem`的`Create`函数，没有返回值。

~~~cpp
    else if ((which == SyscallException) && (type == SC_Create)){
        printf("SC_CREATE called\n");
        int address = machine->ReadRegister(4);
        int len = 0 ,value = 1;
        while(value != 0 ){ 
            machine->ReadMem(address++,1,&value);
			len++;
        }
        char* fileName = new char[len];
        address -= len;
		for(int i = 0; i < len; i++){
			machine->ReadMem(address+i,1,&value);
			fileName[i] = (char)value;
		}
        fileSystem->Create(fileName,100);
        machine->AdvancePC();

    }
~~~

##### OpenFile

参数是文件名指针，返回打开文件的id。

~~~cpp
    else if ((which == SyscallException) && (type == SC_Open)){
        printf("SC_Open called\n");
        int address = machine->ReadRegister(4);
        int len = 0 ,value = 1;
        while(value != 0 ){ 
            machine->ReadMem(address++,1,&value);
			len++;
        }
        char* fileName = new char[len];
        address -= len;
		for(int i = 0; i < len; i++){
			machine->ReadMem(address+i,1,&value);
			fileName[i] = (char)value;
		}
		OpenFile *openfile = fileSystem->Open(fileName);
		machine->WriteRegister(2,int(openfile));
        machine->AdvancePC();
		
    }
~~~

##### Write

参数是写入源的缓冲区指针、字节数和打开文件id,没有返回值。如果是标准输出则调用`putchar`向控制台输出，否则调用`OpenFile`类的`Write`函数。

~~~cpp
    else if((which == SyscallException) && (type == SC_Write)){
		printf("system call write.\n");
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
~~~

##### Read

参数是目标缓冲区的指针、字节数和打开文件id，返回值是实际读取的字节数。如果是标准输入则调用`getchar`从控制台读入，否则调用`OpenFile`类的`Read`函数。

~~~cpp
	else if((which == SyscallException) && (type == SC_Read)){
		printf("system call read.\n");
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
~~~

##### Close

参数是打开文件id,没有返回值。

~~~cpp
	else if((which == SyscallException) && (type == SC_Close)){
		printf("system call close.\n");
		int fd = machine->ReadRegister(4);
		OpenFile *openfile = (OpenFile*)fd;
		delete openfile;
		machine->AdvancePC();
	}
~~~

#### Exercise 3  编写用户程序

>  编写并运行用户程序，调用练习2中所写系统调用，测试其正确性。

​		

改写`test/sort.c`,创建名为testFile的文件，从控制台读入字符串，写入该文件，再读出该文件内容输出到控制台中。

~~~c
    Read(input,10,0);
    Create("testFile");
    fd = Open("testFile");
    Write(input,10,fd);
    Close(fd);
    fd = Open("testFile");
    Read(output,10,fd);
    Close(fd);
    Write(output,10,1);
~~~

重新编译，在`usrprog`目录下运行`./nachos -x ../test/sort`:

![image-20200507142506972](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200507142506972.png)

各系统调用都正确运行，最后从文件读出的内容和输出的一致，实现正确。

### 第三部分 执行用户程序相关的系统调用

#### Exercise 4 系统调用实现

>  实现如下系统调用：Exec，Fork，Yield，Join，Exit。Syscall.h文件中有这些系统调用基本说明。

`syscall.h`中已经声明了这些系统调用的功能、参数和返回值：

~~~cpp

/* This user program is done (status = 0 means exited normally). */
void Exit(int status);

/* Run the executable, stored in the Nachos file "name", and return the 
 * address space identifier
 */
SpaceId Exec(char *name);
 
/* Only return once the the user program "id" has finished.  
 * Return the exit status.
 */
int Join(SpaceId id); 	
 /* Fork a thread to run a procedure ("func") in the *same* address space 
 * as the current thread.
 */
void Fork(void (*func)());

/* Yield the CPU to another runnable thread, whether in this address space 
 * or not. 
 */
void Yield();		
~~~

##### Exit

没有返回值，参数是状态status(0表示用户程序正常退出)。

~~~c
    else if ((which == SyscallException) && (type == SC_Exit)){
       // machine->ClearMemory();
        printf("SC_EXIT called\n");
        // int nextPC = machine->ReadRegister(NextPCReg);
        // machine->WriteRegister(PCReg,nextPC);
        int status = machine->ReadRegister(4);
		printf("thread %s exit with status %d.\n",currentThread->GetThreadID(),status);
		machine->AdvancePC();
		currentThread->Finish();
    }
~~~

##### Exec

参数是可执行文件名，执行后返回其`SpaceID`.实现时Fork一个新的线程执行该可执行文件，返回线程ID.

~~~c
    else if ((which == SyscallException) && (type == SC_Exec) ) {
        printf("SC_Exec called\n");
        int address = machine->ReadRegister(4);
		Thread *newThread = new Thread("new thread");
		newThread->Fork(exec_func,address);
		machine->WriteRegister(2,newThread->GetThreadID());
		machine->AdvancePC();
    }
~~~

`exec_func`是自定义的执行可执行文件的函数，功能是根据地址解析出文件名，创建新的地址空间，调用`machine->run()`执行。

~~~c
void exec_func(char* fileName){
		printf("Try to open file %s.\n",fileName);
		OpenFile *executable = fileSystem->Open(fileName);
		AddrSpace *space;
		if (executable == NULL) {
			printf("Unable to open file %s\n", fileName);
			return;
		}
	    space = new AddrSpace(executable);    
	    currentThread->space = space;
	    delete executable;			

	    space->InitRegisters();		
	    space->RestoreState();		
	    machine->Run();
}
~~~

##### Join

参数为`SpaceID`，等待对应的用户程序结束后返回。

~~~cpp
	else if((which == SyscallException) && (type == SC_Join)){
		printf("SC_Join called\n");
		int threadId = machine->ReadRegister(4);
		while(ThreadIDOccupied[threadId] == true)
			currentThread->Yield();
		machine->AdvancePC();
	}
~~~

##### Fork

Fork函数，复制一个子线程执行参数所指向的函数，没有返回值。

##### Yield

本线程让出CPU,没有参数和返回值。

~~~c
    else if((which == SyscallException) && (type == SC_Yield)){
		printf("SC_Yield Called for thread %s\n",currentThread->getName());
		machine->AdvancePC();
		currentThread->Yield();
	}
~~~

#### Exercise 5  编写用户程序

>  编写并运行用户程序，调用练习4中所写系统调用，测试其正确性。

把`sort`的`main`函数内容改写如下：

~~~cpp
	Create("2.txt");
	Fork(func);
	fd = Exec("../test/halt");
	Join(fd);
    Exit(0);
~~~

`func`的定义：

~~~c
void func(){
	Create("1.txt");
}
~~~

运行测试程序，新添加的系统调用都能正常执行：

![image-20200507192601708](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200507192601708.png)

## 内容三 遇到的困难与解决方案

### 测试程序编译

刚开始试图添加新的测试文件，但修改Makefile时出现问题。为简便起见，直接在`sort.c`中编写测试代码。

## 内容四 收获与感想

本次Lab是本课程最后一个必做Lab,在Nachos提供好的接口基础上完成并测试了十个系统调用，起到了用户态和内核态之间的桥梁作用。通过本次Lab,我对Nachos的内核和用户程序加载方式有了更深的认识，并领悟到了系统调用在操统系统中的重要作用——用户通过内核执行关键操作，以保证系统的安全性和鲁棒性。

## 内容五 对课程的意见与建议

本学期的课程即将结束，整体上非常充实，在Nachos上实践和课堂讨论使我对操作系统的认识大大加深了。在不能返校的特殊时期这门课程依然保持这高质量，非常感谢老师和助教学长学姐们的工作。对课程的一点小建议：每次完成新功能后的测试函数应该适当给出一部分，方便统一标准进行测试。

## 内容六 参考文献

无。

