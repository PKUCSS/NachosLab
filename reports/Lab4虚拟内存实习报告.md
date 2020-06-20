# 操作系统实习报告4——虚拟内存

2020.4

---

[TOC]

## 内容一 总体概述

本次Lab是在操作系统课程的内存管理内容学习基础上，从Nachos系统对内存管理的原始实现——只支持单用户线程、虚拟内存和物理内存一一对应开始，实现了TLB、页表、缺页中断处理、LazyLoading等常见的内存管理机制，并在Challenge中实现了倒排页表。本次实习内容使我对操作系统从底到顶的虚拟内存机制设计有了更深的了解，深刻体会了操作系统的重要设计思想之一——"虚拟性"(将物理内存封装为用户程序可直接读写的虚拟内存，兼顾正确性和性能)。

## 内容二 完成情况

任务完成情况:

完成所有Exercise与Challenge 2

| Exercise1 | Exercise2 | Exercise3 | Exercise4 | E5   | E6   | E7   | Challenge 1 | Challenge 2 |
| --------- | --------- | --------- | --------- | ---- | ---- | ---- | ----------- | ----------- |
| Y         | Y         | Y         | Y         | Y    | Y    | Y    | N           | Y           |

具体内容：

### Exercise 1 源代码阅读

#### 用户程序与内存管理

> 阅读code/userprog/progtest.cc，着重理解nachos执行用户程序的过程，以及该过程中与内存管理相关的要点。

`protest.cc`中,启动用户程序的函数如下：

~~~cpp
void
StartProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    space = new AddrSpace(executable);    
    currentThread->space = space;

    delete executable;			// close file

    space->InitRegisters();		// set the initial register values
    space->RestoreState();		// load page table register

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}
~~~

其执行用户程序的步骤可以归纳为下图：

~~~mermaid
graph LR
	A[读取可执行文件] -->B[创建新的地址空间]
    B --> C[初始化寄存器状态和页表]
    C -->D[Run]
~~~

Run的定义在`mipssim.cc`中,主要是通过循环调用`OneInstruction(instr);`来进行取值、译码、执行并使时钟前进。这就是nachos执行一个用户程序的流程，其中与内存管理相关的主要是地址空间`AddrSpace`的创建和页表寄存器的加载`space->RestoreState()`.`AdderSpace`定义在`addrspace.cc`中，其构造函数接受一个指向noff格式可执行文件的指针作为参数，首先读取`noff`可执行文件的头部并检查格式(如果是小端表示则转换为大端)，再读取代码段、初始化数据段、未初始化数据段、用户栈的大小，相加得到地址空间的大小，整除页大小得到页数，建立虚拟页和物理页一一对应的页表，再将代码段与数据段加载到内存中。`RestoreState`定义在`addrspace.cc`中，其功能是将CPU的页表指针和页表大小置为用户程序对应的值，为用户程序开始执行做好准备。

#### TLB机制与地址转换

> 阅读code/machine目录下的machine.h(cc)，translate.h(cc)文件和code/userprog目录下的exception.h(cc)，理解当前Nachos系统所采用的TLB机制和地址转换机制。

`machine.h`与`machine.cc`是对CPU硬件的模拟，包括指令解码与执行、寄存器读写、内存读写等，`translate.h`和`translate.cc`定义并实现了`TLB`、页表等实现地址转换的数据结构,`exception.h(cc)`定义了异常处理`ExceptionHandler`(还需要补充实现)。实现地址转换的是`translate.cc`中的`translate`函数，接受虚拟地址、指向物理地址的指针、读写的字节数、是否写这四个参数，将返回的物理地址写入`physAddr`指向的位置，如果发生异常则返回相应异常类型。

~~~cpp
ExceptionType
Machine::Translate(int virtAddr, int* physAddr, int size, bool writing)
~~~

首先进行地址对齐检查，如果发现未对齐的情况则返回地址错误异常。接着检查`TLB`和页表是否存在，按照Nachos现有的设定是页表和`TLB`只能存在一个，后续需要修改。

~~~cpp
// check for alignment errors
    if (((size == 4) && (virtAddr & 0x3)) || ((size == 2) && (virtAddr & 0x1))){
	DEBUG('a', "alignment problem at %d, size %d!\n", virtAddr, size);
	return AddressErrorException;
    }
    
    // we must have either a TLB or a page table, but not both!
    ASSERT(tlb == NULL || pageTable == NULL);	
    ASSERT(tlb != NULL || pageTable != NULL);
~~~

j计算虚拟页号和页内偏移，如果`TLB`为空，则在页表中寻找相应的页表项，如果虚拟页号超过页表大小则抛出地址错误异常，如果对应的页表项无效则抛出缺页异常；如果`TLB`不为空，则在TLB中寻找相应页表项，如果没有找到相应的有效页表项则抛出缺页异常。

~~~cpp
// calculate the virtual page number, and offset within the page,
// from the virtual address
    vpn = (unsigned) virtAddr / PageSize;
    offset = (unsigned) virtAddr % PageSize;
    
    if (tlb == NULL) {		// => page table => vpn is index into table
	if (vpn >= pageTableSize) {
	    DEBUG('a', "virtual page # %d too large for page table size %d!\n", 
			virtAddr, pageTableSize);
	    return AddressErrorException;
	} else if (!pageTable[vpn].valid) {
	    DEBUG('a', "virtual page # %d too large for page table size %d!\n", 
			virtAddr, pageTableSize);
	    return PageFaultException;
	}
	entry = &pageTable[vpn];
    } else {
        for (entry = NULL, i = 0; i < TLBSize; i++)
    	    if (tlb[i].valid && (tlb[i].virtualPage == vpn)) {
		entry = &tlb[i];			// FOUND!
		break;
	    }
	if (entry == NULL) {				// not found
    	    DEBUG('a', "*** no valid TLB entry found for this virtual page!\n");
    	    return PageFaultException;		// really, this is a TLB fault,
						// the page may be in memory,
						// but not in the TLB
	}
    }
~~~

最后，检查读写权限，如果发现试图写入只读页则抛出只读异常，再检查物理页号，如果超过物理页数则抛出`BusErrorException`。标记页表项的`use`和`dirty`位，将物理页号和页大小相乘，加上页内偏移量得出物理地址，写入目标位置，检查物理地址为非负数且小于物理内存大小，返回`NoException`.

~~~  cpp
    if (entry->readOnly && writing) {	// trying to write to a read-only page
	DEBUG('a', "%d mapped read-only at %d in TLB!\n", virtAddr, i);
	return ReadOnlyException;
    }
    pageFrame = entry->physicalPage;

    // if the pageFrame is too big, there is something really wrong! 
    // An invalid translation was loaded into the page table or TLB. 
    if (pageFrame >= NumPhysPages) { 
	DEBUG('a', "*** frame %d > %d!\n", pageFrame, NumPhysPages);
	return BusErrorException;
    }
    entry->use = TRUE;		// set the use, dirty bits
    if (writing)
	entry->dirty = TRUE;
    *physAddr = pageFrame * PageSize + offset;
    ASSERT((*physAddr >= 0) && ((*physAddr + size) <= MemorySize));
    DEBUG('a', "phys addr = 0x%x\n", *physAddr);
    return NoException;
~~~

### Exercise 2 TLB Miss 异常处理

>  修改code/userprog目录下exception.cc中的ExceptionHandler函数，使得Nachos系统可以对TLB异常进行处理（TLB异常时，Nachos系统会抛出PageFaultException，详见code/machine/machine.cc）。

对一个正常的系统，TLB异常时的表现应该是抛出`PageFaultException`,由异常处理函数在页表中寻找相应页表项，找到后放回TLB中(需要实现某种替换算法，见Exercise3)。由于页表失效和TLB失效后抛出的都是`PageFaultException`,需要在ExceptionHandler中区分这两种情况。

由于原始实现中只能使用TLB和页表之一，需要将其注释掉：

~~~c++
    // we must have either a TLB or a page table, but not both!
    // ASSERT(tlb == NULL || pageTable == NULL);	
   //  ASSERT(tlb != NULL || pageTable != NULL);
~~~

另外，在`Machine`类的初始化函数中，默认不使用TLB：

~~~cpp
#ifdef USE_TLB
    tlb = new TranslationEntry[TLBSize];
    for (i = 0; i < TLBSize; i++)
	tlb[i].valid = FALSE;
    pageTable = NULL;
#else	// use linear page table
~~~

需要在`userprog`目录下的makefile中添加-DUSE_TLB选项才能使用TLB。

在`ExceptionHandler`中增加处理`PageFaultException`的判断条件，如果TLB不为空则为TLB MISS产生的缺页异常（目前的实现中物理页和虚拟页一一对应，暂时忽略页表缺页的情况）。对TLB MISS，只需在页表中找到对应页表项，放入TLB中即可。在Machine类中定义tlbReplace函数作为TLB替换的接口，第三部分实现不同的置换算法。(在第三部分一并测试)

~~~cpp
    else if (which == PageFaultException){
        if (machine->tlb == NULL){ // TLB Miss
			int address = machine->ReadRegister(BadVAddrReg);
			machine->tlbReplace(address);
        }
        else{  // Page Table Miss,which is ignored now
        }
    }
~~~

### Exercise 3 置换算法

> 为TLB机制实现至少两种置换算法，通过比较不同算法的置换次数可比较算法的优劣

#### FIFO(先进先出)

如果TLB已满，在TLB中选择最早进入的一项替换。(不需要时间标记，只需要在TLB满时抛弃第一项，将剩余缓存项前移一位，替换最后一项即可)。

~~~cpp
void 
Machine::tlbReplace(int address) {
	unsigned int vpn = (unsigned)address / PageSize;
    int position = -1;
    for (int i = 0; i < TLBSize ; ++i ) {
        if (tlb[i].valid == false){
            position = i;
            break;
        }
    }
    if (position == -1){
        for (int i = 0 ; i < TLBSize-1 ; ++i){
            tlb[i] = tlb[i+1];
        }
        position = TLBSize-1;
    }

    tlb[position].virtualPage = vpn;
    tlb[position].physicalPage = pageTable[vpn].physicalPage;
    tlb[position].valid= true;
    tlb[position].use = false;
    tlb[position].dirty = false;
    
}
~~~

#### Random(随机替换)

如果TLB已满，在TLB随机选择一项替换。

~~~cpp
    if (position == -1){  // random replacement
        position = Random()%TLBSize;
    }
~~~

#### LRU(最近最少使用)

在tlb数据结构中添加LRU一项(记录最近访问时间，越小表示时间越早，范围为[0,tlbSize-1]).每次访问命中时，将对应块的LRU设为`tlbSize-1`,将LRU值大于其原值的缓存项LRU减一。

~~~cpp
#ifdef USE_TLB
    tlb = new TranslationEntry[TLBSize];
    for (i = 0; i < TLBSize; i++){
        tlb[i].valid = false;
        tlb[i].LRU = -1;
    };
~~~

在TLB访问时，维护LRU标记：

~~~cpp
    if (position == -1){  // Least Recently Used 
        int minTag = TLBSize;
        for (int i = 0 ; i < TLBSize; i++){
            if (tlb[i].LRU < minTag){
                minTag = tlb[i].LRU;
                position = i;
            }
        }
        for( int j = 0 ; j < TLBSize ; j++){
            if ( j == position) continue;
            tlb[j].LRU--;
        }
        tlb[position].LRU = TLBSize-1;
    }
~~~

#### 测试

用test目录下的sort程序测试，为了不超过物理内存限制将数组大小改为100，由于Exit()系统调用还未实现，改为Halt系统调用。在访问TLB时统计访问次数和命中次数，在Machine类的析构函数中输出：

~~~cpp
Machine::~Machine()
{
    delete [] mainMemory;
    if (tlb != NULL)
        printf("TLB Hit Rate: %d / %d =  %lf",tlbHits,tlbTimes,float(tlbHits)/float(tlbTimes));
        delete [] tlb;
}
~~~

FIFO算法：

![image-20200329165414928](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200329165414928.png)

随机替换算法：

![image-20200329170507729](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200329170507729.png)

LRU算法：

![image-20200330000847710](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200330000847710.png)

三种替换算法的命中率是：LRU(94.94%)>FIFO(94.10%)>Random(93.67%),LRU能够利用内存访问的局部性，在大多数情况下性能较好且实现简便，后续继续使用LRU算法做替换策略。

### Exercise 4 内存管理全局数据结构

> 设计并实现一个全局性的数据结构（如空闲链表、位图等）来进行内存的分配和回收，并记录当前内存的使用状态。

这里选择用位图实现内存管理的全局数据结构，实际就是Machine类中添加一个长度为物理页数的bool数组成员memoryBitmap(false表示物理页未分配，true表示已分配)。

~~~cpp
	bool memoryBitmap[NumPhysPages] ; // The bitmap for global physical memory
	unsigned int findMemory(){ 
		for(int i = 0 ; i < NumPhysPages ; i++){
			if (memoryBitmap[i] == false) {
				printf("Allocate physical page %d\n",i);
				memoryBitmap[i] = true ;
				return i;
			}
		}
		return -1;
	}
	void ClearMemory(){
		for (int i = 0 ; i< pageTableSize ; i++){
			if (pageTable[i].valid && memoryBitmap[pageTable[i].physicalPage]){
				memoryBitmap[pageTable[i].physicalPage] = false ;
				printf("Deallocate physical page %d\n",pageTable[i].physicalPage);
			}
		}
	}
~~~

线程地址空间初始化时调用分配函数find寻找空闲块并分配：

~~~
for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
	pageTable[i].physicalPage = machine->findMemory();
	pageTable[i].valid = TRUE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
~~~

用户程序退出时，捕获exit系统调用，并调用ClearMemory()释放内存块：

~~~cpp
    else if ((which == SyscallException) && (type == SC_Exit)){
        machine->ClearMemory();
        int nextPC = machine->ReadRegister(NextPCReg);
        machine->WriteRegister(PCReg,nextPC);
    }
~~~

测试结果如下图，正确实现了全局内存的状态表示、分配和回收。

![image-20200330004707864](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200330004707864.png)

### Exercise 5 多线程支持

>  目前Nachos系统的内存中同时只能存在一个线程，我们希望打破这种限制，使得Nachos系统支持多个线程同时存在于内存中。

首先厘清目前的实现为什么不能支持多线程。在给用户程序分配地址空间时，代码段和初始化数据段是在物理内存中连续分配的，初始化时会调用bzero使整个物理内存置零，不能支持多线程。为此，只需注释掉bzero，并将代码段空间和初始化数据段空间改为按页分配即可(实现时按字节逐个分配，不容易出错)。

~~~cpp
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
	
        int code_pos = noffH.code.inFileAddr;   
        for (int i = 0; i < noffH.code.size ;++i){
            int vpn = (noffH.code.virtualAddr+i)/PageSize;
            int offset = (noffH.code.virtualAddr+i)%PageSize;
            int physicalAddress = pageTable[vpn].physicalPage*PageSize + offset;
            executable->ReadAt(&(machine->mainMemory[physicalAddress]),1,code_pos++); 
        }
    }
    
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        int data_pos = noffH.initData.inFileAddr;
            for (int i = 0; i < noffH.initData.size ;++i){
                int vpn = (noffH.initData.virtualAddr+i)/PageSize;
                int offset = (noffH.initData.virtualAddr+i)%PageSize;
                int physicalAddress = pageTable[vpn].physicalPage*PageSize + offset;
                executable->ReadAt(&(machine->mainMemory[physicalAddress]),1,data_pos++); 
        }
    }
~~~

此外，线程切换时TLB会失效，需要在SaveState函数中将所有TLB项置零。

~~~cpp
void AddrSpace::SaveState() 
{
    for (int i = 0 ; i < machine->TLBSize ; ++i){
        machine->tlb[i].valid = false;
    }
}
~~~

测试方法：在`progtest.cc`的`startProcess`函数中创造一个当前用户线程的副本(装载同样的用户程序)并且抢占CPU优先执行，测试选用halt函数，可以看到两个用户线程分别占据了内存不同的物理页，而且后分配的副本所占的空间先释放。这样一来就在内存层面上支持了多线程。

测试函数：

~~~cpp
void
StartProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    OpenFile *executable2 = fileSystem->Open(filename);
    AddrSpace *space2;

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    printf("main user program addrspace initialized\n");
    space = new AddrSpace(executable);    
    currentThread->space = space;
    printf("fork user program addrspace initialized\n");
    Thread *new_thread = new Thread("Fork thread",1);
    space2 = new AddrSpace(executable2);
    new_thread->space = space2;

   
    space2->InitRegisters();	//
    space2->RestoreState(); 
    new_thread->Fork(MultiThreadTest,0);
    currentThread->Yield();

    space->InitRegisters();		// set the initial register values
    space->RestoreState();		// load page table register


    
    printf("Running the main program thread...\n");

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}
~~~

执行结果(fork出的线程执行到halt后就停机了，main线程实际上没有被执行，但能体现内存中存在多个线程)

![image-20200330165113120](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200330165113120.png)



### Exercise 6 缺页中断处理

> 基于TLB机制的异常处理和页面替换算法的实践，实现缺页中断处理（注意！TLB机制的异常处理是将内存中已有的页面调入TLB，而此处的缺页中断处理则是从磁盘中调入新的页面到内存）、页面替换算法等。

(与Exercise7一起实现)

### Exercise 7 Lazy Loading

> 我们已经知道，Nachos系统为用户程序分配内存必须在用户程序载入内存时一次性完成，故此，系统能够运行的用户程序的大小被严格限制在4KB以下。请实现Lazy-loading的内存分配算法，使得当且仅当程序运行过程中缺页中断发生时，才会将所需的页面从磁盘调入内存。

### Challenge 2

> 多级页表的缺陷在于页表的大小与虚拟地址空间的大小成正比， 为了节省物理内存在页表存储上的消耗，请在Nachos系统中实现倒排页表。

**在此将Exercise 6、7与Challenge 2的实现一并介绍并给出测试情况**。Nachos目前的实现中还没有磁盘，要加载超过4KB的用户程序，需要new一个大小为用户程序size的空间为作为磁盘使用(每个虚拟磁盘对应一个用户地址空间，每次启动时将可执行文件的内容全部读进磁盘中，当程序运行中缺页中断发生时将所需页面从磁盘读入内存)。如果使用原来的页表，则每个用户线程都会有一个大小与其虚拟地址空间大小成正比的页表，会极大地浪费物理内存。为此我实现了全局的倒排页表，详细实现和测试如下。

初始化时用户程序的地址空间时，分配一个空间作为磁盘存储可执行文件的内容，并将页数小于物理内存最大页数的限制注释掉,注释掉关于原来的页表初始化的内容。 

~~~cpp
    disk = new char[size];
    // ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory
  // commeneted page-table related code after adding reverse mapping page table
~~~

因为是Lazy Loading，线程地址空间初始化时只将代码段和初始化数据段加载到虚拟的磁盘而不是主存中。

~~~cpp
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
	
        int code_pos = noffH.code.inFileAddr;   
        for (int i = 0; i < noffH.code.size ;++i){
            int vpn = (noffH.code.virtualAddr+i)/PageSize;
            int offset = (noffH.code.virtualAddr+i)%PageSize;
            int physicalAddress = vpn*PageSize + offset;
          //  executable->ReadAt(&(machine->mainMemory[physicalAddress]),1,code_pos++); 
            executable->ReadAt(&(disk[physicalAddress]),1,code_pos++); 
        }
    }
    
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        int data_pos = noffH.initData.inFileAddr;
            for (int i = 0; i < noffH.initData.size ;++i){
                int vpn = (noffH.initData.virtualAddr+i)/PageSize;
                int offset = (noffH.initData.virtualAddr+i)%PageSize;
                int physicalAddress =  vpn*PageSize + offset;
                // executable->ReadAt(&(machine->mainMemory[physicalAddress]),1,data_pos++); 
                executable->ReadAt(&(disk[physicalAddress]),1,data_pos++);
        }
    }
~~~

在`machine.h`中添加倒排页表项的定义(也就是原来的页表项多了线程id一项)：

~~~cpp
// Definition for Global ReverseTranslationEntry 
class ReverseTranslationEntry{
  public:
    int virtualPage;         // The page number in virtual page 
    int physicalPage;        // The physical page number
    bool valid;         
    bool readOnly; 
    bool use;
    bool dirty;        
    int LRU;
    int tid;
}
~~~

在machine类的定义中添加倒排页表的指针，倒排页表的大小为物理页的数目，以物理页为索引关键字。

~~~cpp
ReverseTranslationEntry rt_page_table[NumPhysPages] ; // reverse-mapping page table
~~~

在machine类的构造函数中将倒排页表初始化,每个页表项分配一个物理页：

~~~cpp
    for (i = 0 ; i <numPhysPages; i++){
        rt_page_table[i].physicalPage = this->findMemory();;
        rt_page_table[i].virtualPage = -1;
        rt_page_table[i].valid = false;
        rt_page_table[i].pid = -1;
        rt_page_table[i].readOnly = false;
        rt_page_table[i].use = false;
        rt_page_table[i].LRU = -1;
    }
~~~

为了测试简便起见，添加倒排页表后不使用TLB(),需要在`userprog`目录下的makefile中把-DUSE_TLB选项注释掉。这样一来，每次执行地址翻译时，应该在倒排页表中寻找相应的页表项(有效且tid与当前线程号相同)，若找不到对应的项则抛出缺页异常。

~~~cpp
		ReverseTranslationEntry *rev_entry;
		int current_tid = currentThread->GetThreadID();
		for (rev_entry = NULL,i = 0 ; i < NumPhysPages ; i++) {
			if (   rt_page_table[i].valid && (rt_page_table[i].virtualPage == vpn)&& (rt_page_table[i].tid == current_tid ) {
				rev_entry = &rt_page_table[i];
				break;
			}
		}
		if ( rev_entry == NULL ){
			DEBUG('a',"*** no valid page table entry for this virtual page\n");
			return PageFaultException;
		}
~~~

缺页异常处理器调用`ReverseTableReplace`从磁盘调入相应的页

~~~cpp
int address = machine->ReadRegister(BadVAddrReg);
machine->ReverseTableReplace(address);
~~~

`ReverseTableReplace`使用随机替换算法，在从磁盘载入新页进行替换前，检查倒排页表项的dirty位，如果是脏的则写入对应的磁盘位置再进行物理页的替换：

~~~cpp
void 
Machine::ReverseTableReplace(int address) {
    unsigned int vpn = (unsigned)address / PageSize;
    int position = -1;
    for (int i = 0; i < TLBSize ; ++i ) {
        if (rt_page_table[i].valid == false){
            position = i;
            break;
        }
    }

 	if (position == -1){  // random replacement
        position = Random()%NumPhysPages;
    }

    // If dirty,write back to disk
    if (rt_page_table[position].valid == true && rt_page_table[position].dirty == true){
        int vpn = rt_page_table[position].virtualPage;
        int ppn = rt_page_table[position].physicalPage;
        int timerHandler = rt_page_table[position].tid;
        for ( int j = 0 ; j < PageSize ; ++j){
            thread_poiners[tid]->space->disk[vpn*PageSize+j] = mainMemory[ppn*PageSize+j];
        }
    }

    // read from disk 
    int ppn = rt_page_table[position].physicalPage;
    printf("In thread %d,get physical page %d from virtual disk page %d\n",currentThread->GetThreadID(),ppn,vpn);
    for ( int j = 0 ; j < PageSize; j++){
        mainMemory[ppn*PageSize+j] = thread_poiners[currentThread->GetThreadID()]->space->disk[vpn*PageSize+j];
    }
    rt_page_table[position].virtualPage = vpn;
    rt_page_table[position].valid = true;
    rt_page_table[position].dirty = false;
    rt_page_table[position].tid = currentThread->GetThreadID();

}
~~~

测试：运行halt程序，可以正确实现lazy loading:

![image-20200404233814458](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200404233814458.png)

对内存大小进行压力测试，把sort测试程序中的数组大小改回1024,也能正确执行：

![image-20200404234408210](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200404234408210.png)



## 内容三 遇到的困难与解决方法

### 环境意外崩溃

这次Lab遇到的最大问题是完成过程中由于宿主win10意外强制关机导致了开发环境32位虚拟机崩溃，查阅许多方法都难以修复，好在代码等重要文件都在宿主机的共享文件夹中，最后重装虚拟机解决了此问题。

### 倒排页表抖动

原本在缺页中断+倒排页表的实现中使用LRU替换算法，但使用sort测试时发生抖动(thrash)使性能严重下降，改成随机替换算法解决此问题。

## 内容四 收获与感想

​	这次Lab的内容非常综合而成体系，通过动手实践使我对用户程序加载执行、虚拟内存管理等内容有了更深刻的理解，TLB、页表、物理内存、虚拟内存的概念和关系原本是书本上抽象的概念，通过简单的代码模拟体会深刻了许多。操作系统从底到顶的虚拟内存机制设计完美地体现了其虚拟性以及策略和机制分离的设计思想，令我受益很多。受限于时间和精力，没有彻底完成从TLB到磁盘的多线程联合调试和Challenge1要求的线程挂起，在今后的学习中争取继续补上。

## 内容五 对课程的意见与建议

这样一个比较复杂的Lab应该适当给出一些细节方面的指导(比如编译使用TLB应该添加的编译选项)来引导同学们关注操作系统本质性的设计而不是琐碎的细节。此外，Lab中常要求实现比较复杂的功能却没有给出相应的测试样例或者要求，希望今后的Lab中能给出稍微具体一点的测试要求。

## 内容六 参考文献

无。