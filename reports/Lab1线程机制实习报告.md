# 操作系统实习报告1——线程机制

2020.3.3

---

## 内容一 总体概述

本次关于线程机制的Lab是在Nachos环境装配完成后的第一个Lab,我通过阅读源代码深入认识了Linux和Nachos中进程控制块PCB的实现方式(Linux是task_struct，Nachos是Thread)，并在Nachos实现的Thread类基础上添加功能，编程实现了对线程ID、用户ID、最大进程数的维护，并实现了类似于Linux系统中PS命令的功能TS,用于显示当前系统中所有线程的信息和状态。涉及到的知识点主要是进程控制块的数据结构组成，还没有涉及到调度的内容，相对比较简单。

## 内容二 完成情况

### 任务完成列表

| Exercise 1 | Exercise 2 | Exercise 3 | Exercise 4 |
| ---------- | ---------- | ---------- | ---------- |
| Y          | Y          | Y          | Y          |

### Exercise 1 调研

调研Linux 或 Windows 中进程控制块（ PCB ）的基本实现方式，理解与 Nachos 的异同。

#### Linux中PCB实现

Linux中进程控制块对应的代码实现可参见https://elixir.bootlin.com/linux/latest/source/include/linux/sched.h[1]中实现的task_struct结构体。真实的系统中PCB的实现比Nachos中复杂得多，结构体中主要包含以下成员：

- 进程状态**state**和退出状态**exit_state**，state的取值有五种：运行 态 TASK_RUNNING 可中断挂起
  TASK_INTERRUPTIBLE 不可中断刮起 TASK_UNINTERRUPTIBLE ，暂停 态 TASK_STOPPED和僵尸态 TASK_ZOMBIE
- 进程id **pid** 和线程组id **tgid**
- 进程内核栈**stack**，通过**alloc_thread_info**函数分配，通过**free_thread_info**函数释放
- 进程标记**flags**，记录进程状态，用于内核识别进程当前的状态，以备下一步操作
- 表示进程亲属关系的成员：
| 字段	| 描述 |
| ----    | ---- |
| **real_parent**	| 指向其父进程，如果创建它的父进程不再存在，则指向PID为1的init进程 |
| **parent**	| 指向其父进程，当它终止时，必须向它的父进程发送信号。它的值通常与real_parent相同|
| **children**	| 表示链表的头部，链表中的所有元素都是它的子进程|
| **sibling**	| 用于把当前进程插入到兄弟链表中|
| **group_leader**	|  指向其所在进程组的领头进程 |

-  内核栈数据结构描述**thread_info**和**thread_union**,与体系结构相关
- 调度的优先级，**静态优先级static_prio**和**动态优先级rt_priority** 
- 调度策略**policy**和调度两类**sched_class**
- 判断标志exit_code,exit_signal等
- 时间相关成员utime,gtime,prev_stime等
- 信号相关成员signal,sighand,blocked,pending等
- ………

具体成员的取值和含义可参见[Linux进程描述符task_struct结构体详解](https://blog.csdn.net/gatieme/article/details/51383272)。[2]

#### Nachos中PCB实现

Nachos中进程控制模块由thread.cc中的thread类实现，由于是实验demo性质的系统，实现比较简单，扩充前只有name,staus,stacktop,stack四个成员，status有以下四种：

```c++
// Thread state
enum ThreadStatus { JUST_CREATED, RUNNING, READY, BLOCKED };
```

#### 两者异同

Nachos中线程控制块的实现比Linux中简单得多，只定义并维护status(线程状态),name(线程名),stack(栈底指针)stackTop(栈顶指针),machineState(寄存器状态)这几个成员变量，两者主要的不同有：

- Nachos 中没有真正的进程，而是使用线程来替代进程的概念
- Nachos中没有进程树和进程组的概念，线程没有僵尸态，而是采用scheduler定时查询并计时的方式来统一回收僵尸线程 。
- Nachos中线程状态有JUST_CREATED, RUNNING, READY, BLOCKED四种，而Linux中有五种：运行态 TASK_RUNNING，可中断挂起TASK_INTERRUPTIBLE，不可中断挂起TASK_UNINTERRUPTIBLE，暂停态TASK_STOPPED和僵尸态TASK_ZOMBIE。



### Exercise 2 源代码阅读

仔细阅读下列源代码，理解Nachos 现有的线程机制。
code/threads/main.cc和 code/threads/threadtest.cc
code/threads/thread.h和code/threads/thread.cc

#### thread.h和thread.cc

thread.h和thread.cc中定义并实现了Nachos线程机制的核心——Thread类，如第一部分所述，有status(线程状态),name(线程名),stack(栈底指针)stackTop(栈顶指针),machineState(寄存器状态)这几个成员变量，有以下方法：

~~~C++
    // basic thread operations

    void Fork(VoidFunctionPtr func, int arg); 	// Make thread run (*func)(arg)
    void Yield();  				// Relinquish the CPU if any 
						// other thread is runnable
    void Sleep();  				// Put the thread to sleep and 
						// relinquish the processor
    void Finish();  				// The thread is done executing
    
    void CheckOverflow();   			// Check if thread has 
						// overflowed its stack
    void setStatus(ThreadStatus st) { status = st; }
    char* getName() { return (name); }
    void Print() { printf("%s, ", name); }
~~~

仔细阅读源代码后，总结几个重要函数的功能和实现要点如下:

- Fork函数，使线程启动。主要操作是调用 StackAllocate给线程分配栈，并调用调度器Scheduler使指定的线程获得CPU时间片开始运行。注意调度之前要屏蔽中断，之后恢复原来的中断水平。
- Yield函数，如果其他线程可运行则让出CPU时间片。主要操作是屏蔽中断，找到下一个可运行线程，将本线程设为ReadyToRun置于ReadyQueue的尾部，运行下一个可运行线程，恢复原中断水平。正在运行的线程Yield之后，等ReadyQueue前部的其他线程运行完之后就可以被再次调度。
- Sleep函数，因为本线程需要一个锁/信号/条件等资源进入Blocked状态停止运行，需要其他线程发来信号才能重新进入ReadyQueue再被调度。注意在没有其他线程可运行时，要调用interrupt->Idle()等待中断。

#### main.cc和threadtest.cc

main.cc解析启动Nachos的命令行并初始化内核，进而根据需要调用各种测试功能。在thread文件夹下，调用的是threadtest.cc中定义的threadtest函数，而源代码中threadtest只能接收参数1调用ThreadTest1()，做了这样的测试：

~~~c++
//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, 1);
    SimpleThread(0);
}
~~~

注释得很清楚，创建了新线程，Fork出一个线程调用simpleThread函数，再在本线程内调用这个函数，就能看的交互输出( a ping-pong between two threads)。

![image-20200302013438598](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200302013438598.png)

### Exercise 3 拓展线程的数据结构

增加“用户ID ，线程 ID” 两个数据成员，并在 Nachos 现有的线程管理机制中增加对这两个数据成员的维护机制。

#### 实现方法

- 在Thread中添加uid和tid成员,并加入相应的访问接口：

  ~~~c++
  class Thread {
      private:
          int tid;                           
          int uid;                            
  
      public:
      
          int getThreadId() { return tid; }        
          int getUserId() { return uid; }          
          void setUserId(int userId) { uid = userId; }
  }
  ~~~

- 在thread/system.cc中添加定义表示线程ID是否被占用的bool数组ThreadIdOccupied,数组长度由第四题中的最大线程数而定

  ~~~C++
#define MaxThreadCount 128  // Maximum number of threads 
bool ThreadIDOccupied[MaxThreadCount]; // Mark threads as occupied or not 
  ~~~

- Initialize函数中初始化了Nachos内核启动所需的全局数据结构，添加ThreadIdOccupied的初始化:

  ~~~C++
for (int i = 0 ; i < MaxThreadCount ; i++){
    ThreadIDOccupied[i] = false;   // Initialize all thread ids as free 
}
  ~~~

- 在一个线程被创建时分配空闲的tid,uid默认为0：

  ~~~C++
  Thread::Thread(char* threadName)
  {
    for(int i = 0; i < MaxThreadCount; i++){
        if (!ThreadIDOccupied[i]) {
            ThreadIDOccupied[i] = true;
            tid = i;  // allocate thread id 
            break;
        } 
    }
    uid = 0 ;
    name = threadName;
    stackTop = NULL;
    stack = NULL;
    status = JUST_CREATED;
  #ifdef USER_PROGRAM
    space = NULL;
  #endif
  }
  ~~~

- 在Thread的析构函数中释放占用的线程id:

  ~~~c++
  Thread::~Thread()
  {
      DEBUG('t', "Deleting thread \"%s\"\n", name);
  
      ASSERT(this != currentThread);
      if (stack != NULL)
  	DeallocBoundedArray((char *) stack, StackSize * sizeof(int));
      ThreadIDOccupied[this->tid] = false;
  }
  ~~~

#### 测试

仿照threadtest1的思路，两个线程交替执行并打印用户id和自身的线程id:

~~~c++
void SimpleThread2(int which)
{
    int num;

    for (num = 0; num < 5; num++) {
        printf("*** thread %d ,uid = %d,tid = %d ,looped %d",which,
            currentThread->GetUserID(),currentThread->GetThreadID(),num);
        currentThread->Yield();
    }
}

void ThreadTest2(){
    DEBUG('t', "Entering ThreadTest2,testing for tid and uid in Lab1");
    for (int i = 0; i < 5; i++) {
        // Generate a Thread object
        Thread *t = new Thread("forked thread");
        t->setUserId(i*11); // set uid
        // Define a new thread's function and its parameter
        t->Fork(SimpleThread2, t->GetThreadID());
    }

}
~~~

运行 ./nachos -q 2进行测试，功能与预期一致：

![image-20200302023741464](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200302023741464.png)

### Exercise 4 增加全局线程管理机制

在Nachos 中增加对线程数量的限制，使得 Nachos 中最多能够同时存在 128 个线程；仿照Linux 中 PS 命令，增加一个功能 TS(Threads Status) ，能够显示当前系统中所有线程的信息和状态。

#### 线程数量限制

在Thread的构造函数中，添加超过最大线程数时报错退出的功能:

~~~C++
    bool allocated = false;
    for(int i = 0; i < MaxThreadCount; i++){
        if (!ThreadIDOccupied[i]) {
            ThreadIDOccupied[i] = true;
            tid = i;  // allocate thread id
            allocated = true; 
            break;
        } 
    }
    if (!allocated){
         printf("Oops,MaxThreadCount exceeded!");
    }
    ASSERT(allocated);
~~~

编写测试函数，尝试创建超过最大线程数的线程：

~~~C++
void ThreadTest3(){
    DEBUG('t',"Entering ThreadTest3,testing for MaxThreadCount Limit");
    printf("Successufully allocated thread ids: ");
    for (int i = 0; i < 130 ; ++i){
        Thread *t = new Thread("forked thread");
        printf(" %d",t->GetThreadID());
    }
}

~~~

运行 ./nachos -q 3进行测试，功能与预期一致,创建128号进程时报错退出：

![image-20200302025242469](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200302025242469.png)

#### TS功能

TS的实现只要仿照第三题中定义的ThreadIDOccupied，定义一个指向Thread的指针数组thread_poiners并在内核初始化、线程构造和析构时维护，每次执行ts时遍历该数组打印各线程信息，实现如下：

~~~c++
class Thread {
    // .... 
    public:
      ThreadStatus GetThreadStatus(){return status;}
};

void GetThreadStatus(){
    printf("Name    UID    TID    Status  \n");
    char* StatusName[] = {"JUST_CREATED", "RUNNING", "READY", "BLOCKED"};
    for (int i = 0 ; i < MaxThreadCount ; i++){
        if (thread_poiners[i] != NULL){
            printf("%s  %d   %d  %s\n",thread_poiners[i]->getName(),thread_poiners[i]->GetUserID(),
            thread_poiners[i]->GetThreadID(),StatusName[thread_poiners[i]->GetStatus()]);
        }
    }
}
~~~

为了测试，在ThreadTest2末尾调用TS，./nachos -q 2结果如下，输出符合预期：

![image-20200302035228521](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200302035228521.png)

## 内容三：遇到的困难以及解决方法

第四题测试需要在threadtest.cc中调用GetThreadStatus()，原本函数写在system.h中，但编译时报multiple definition.经检查原因是多个.o文件引用了system.h，造成生成main.o时该函数被重复定义。正确做法应该是在system.cc中定义并实现该函数，在system.h中用extern声明即可。

知识点参考：[extern 与头文件(*.h)的区别和联系](https://www.runoob.com/w3cnote/extern-head-h-different.html)[3]

## 内容四：收获及感想

这次Lab的主要收获：

- 通过阅读源代码初步理解Linux线程机制，认识到真实的操作系统考虑的情况远比Nachos中展示的复杂
- 第三题和第四题的实现相对较简单，但遇到的编译错误让我回顾了可执行文件的编译机制，学习了C++中extern的用法避免类似错误。
- 对git的使用更熟练。

## 内容五：对课程的意见或建议报告

一点建议：报告模板应该更新成LaTex或markdown，Word格式在程序员的日常工作中很少用，格式控制也容易出问题。

## 内容六：参考文献

1. yangle4695,详解Linux中的进程描述符task_struct[EB/OL],https://blog.csdn.net/yangle4695/article/details/51590145?depth_1-utm_source=distribute.pc_relevant.none-task&utm_source=distribute.pc_relevant.none-task
2. bootlin,Linux源代码参考[EB/OL],https://elixir.bootlin.com/linux/latest/source/arch/x86
3. runoob,extern 与头文件(*.h)的区别和联系[EB/OL], https://www.runoob.com/w3cnote/extern-head-h-different.html

