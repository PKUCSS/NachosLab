# 操作系统实习报告3——线程同步

2020.3

---

[Toc]

## 内容一 总体概述

本次实验的主题是线程同步机制。我在调研了解Linux常见同步机制通过阅读源代码了解Nachos中同步数据结构的基础上，通过修改Nachos底层代码实现了锁与条件变量,并在“生产者-消费者问题”等实例上成功测试。拓展部分实现了barrier和读写锁。

## 内容二 完成情况

任务完成情况：

- [x] Exercise 1
- [x] Exercise 2
- [x] Exercise 3
- [x] Exercise 4
- [x] Challenge 1
- [x] Challenge 2

具体内容：

### Exercise 1 Linux同步机制调研

同步机制是指用于实现控制多个进程/线程按照一定的规则或顺序访问某些系统资源的机制。Linux内核包含了几乎所有现代操作系统使用的同步机制：信号量、原子操作、自旋锁、读写锁、互斥锁、大内核锁、禁止抢占、顺序与屏障等，以下参考IBM社区的博客[Linux 同步方法剖析](https://www.ibm.com/developerworks/cn/linux/l-linux-synchronization.html)和CSDN博客[内核中各种同步机制](https://blog.csdn.net/FreeeLinux/article/details/54267446)给出简要介绍。

#### 原子操作

原子操作不会在执行完毕前被打断，也就是最小的单个执行单位。适用于临界区只有一个变量的情况，与体系架构相关。定义为:

~~~c
typedef struct 
{ 
    volatile int counter; 
} 
atomic_t; 
~~~

`volatile`字段告诉编译器不要对该数据类型进行优化处理，对其访问不是对寄存器的访问，而是对内存的访问。

#### 自旋锁(spin lock)

一个线程获取了某个资源的锁之后，其他试图获取这个锁的线程一直在循环等待获取这个锁，直至锁重新可用。由于线程一直在循环获取这个锁，所以会造成 CPU 处理时间的浪费(忙等待)，因此最好将自旋锁用于很快能处理完的临界区。在单核系统中不适用。自旋锁调用底层既要关中断也要关抢占。

读写自旋锁是一种特殊的自旋锁，即读锁之间不互斥，允许临界区内同时有多个读进程/线程，但读/写和写/写之间还是互斥的。与简单的自旋锁相比可以提高并行效率， 比较适用于读的次数远比写的次数多的临界区资源。

#### 信号量(semaphore)

通过P操作(测试并减一)和V操作(加一)对信号量进行操作，以实现对临界区的保护。与自旋锁的不同在于，因为信号量小于0得不到资源时，进程/线程不会进入忙等待，而是进入TASK_INTERRUPTIBLE_SLEEP状态(可中断睡眠)，等待信号(量)恢复后被唤醒继续执行。在临界区执行时间较长时，效率比自旋锁高。

读写信号量是一种特殊的信号量，即读者之间不互斥，允许临界区内同时有多个读进程/线程，但读/写和写/写之间还是互斥的。与简单的信号量相比可以提高并行效率， 比较适用于读的次数远比写的次数多的临界区资源。

#### 互斥锁(mutex)

与二元信号量类似，区别在于互斥锁只能由同一个进程获取和释放。

#### 条件变量(conditional variables)

利用线程间共享的全局变量进行同步的一种机制，主要包括两个动作：一个线程等待"条件变量的条件成立"而挂起；另一个线程使"条件成立"（给出条件成立信号）。为了防止竞争，条件变量的使用总是和一个互斥锁结合在一起。

#### 大内核锁

大内核锁（Big kernel lock）是Linux细粒度锁引入前使用的粗粒度锁，现在用得不多。

#### 禁止抢占

与自旋锁类似，但只关闭抢占，不关闭中断。

#### 顺序与屏障

保证代码执行顺序不被编译器优化更改，如：

```c++
void thread_worker()
{
    a = 3;
    mb();
    b = 4;
}
```

上述用法就会保证 a 的赋值永远在 b 赋值之前，而不会被编译器优化更改，也属于同步机制的一种。

### Exercise 2 Nachos同步机制源代码阅读

> 阅读下列源代码，理解
> Nachos 现有的同步机制。
> - code/threads/synch.h 和 code/threads/synch.cc
> - code/threads/synchlist.h 和 code/threads/synchlist.cc

#### `synch.h`和`synch.cc`

`synch.h`和`synch.cc`中定义了Nachos用于线程同步的三种数据结构：信号量、锁与条件变量。其中信号量已经实现好，锁与条件变量留作任务实现(Exercise 3)。三种数据结构的构成和功能简介如下：

- 信号量：Semaphore类中具有私有成员变量 value 和队列 queue ，并具有 P 和 V 两种方法，value初始时设定为某个非负值，每次P操作检查 value 是否为 0 ，若为 0 则将该线程入队并 Sleep，等待信号量；若大于 0 ，则将 value 减一后返回。每次V 操作将唤醒 queue 中的一个 sleep线程，并将 value 加一。 Semaphore 中所有操作都是原子性的，即通过手动关中断保证不会被打断。以P操作为例：

  ~~~c
  void
  Semaphore::P()
  {
      IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts 关闭中断
      
      while (value == 0) { 			// semaphore not available
  	queue->Append((void *)currentThread);	// so go to sleep
  	currentThread->Sleep();
      } 
      value--; 					// semaphore available, 
  						// consume its value
      
      (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts 恢复中断
  }
  
  ~~~

- 锁：实际上就是互斥锁，初始代码中只定义未实现。需要实现的成员方法有：

  - Acquire()：获得锁，若锁正被其他线程占用则一直Sleep()到可以获得锁。(原子操作)
  - Release():释放锁，如果等待该锁的队列非空则唤醒其中的一个进程。(原子操作)
  - isHeldByCurrentThread():判断锁是否被当前运行线程持有。

- 条件变量：本身没有值、只有名字，和一个互斥锁一起使用，维护一个等待队列，需要实现的方法有：

  - Wait()：首先释放锁，之后使当前线程睡眠并进入等待队列，最后获得锁(原子操作)
  - Signal():从等待队列中唤醒一个线程(原子操作)
  - Broadcast():广播，唤醒等待队列中所有线程(原子操作)

  需要注意的是，注释中提到条件变量的实现有Mesa style和Hoare Style,而Nachos使用的应该是Mesa Style，即被唤醒的进程只是进入就绪队列，不会抢占唤醒者的CPU。

  ~~~cpp
  // In Nachos, condition variables are assumed to obey *Mesa*-style
  // semantics.  When a Signal or Broadcast wakes up another thread,
  // it simply puts the thread on the ready list, and it is the responsibility
  // of the woken thread to re-acquire the lock (this re-acquire is
  // taken care of within Wait()).  By contrast, some define condition
  // variables according to *Hoare*-style semantics -- where the signalling
  // thread gives up control over the lock and the CPU to the woken thread,
  // which runs immediately and gives back control over the lock to the 
  // signaller when the woken thread leaves the critical section.
  ~~~

#### `synchlist.h`和`synchlist.cc`

实现了一个同步链表`SyncList`,功能与“生产者-消费者问题“中的缓冲区类似，Remove()方法从链表中获取头元素并删除，若链表为空则进入睡眠状态；Append()方法向链尾添加一个元素，若此时size=1则唤醒等待队列中的一个线程。

### Exercise 3 实现锁与条件变量

> 可以使用sleep 和 wakeup 两个原语操作（注意屏蔽系统中断），也可以使用 Semaphore 作为唯一同步原语（不必自己编写开关中断的代码）。

为后续测试方便，首先把线程调度切换回时间片轮转算法：

![image-20200315152444131](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200315152444131.png)

#### 锁

利用一个二元`Semaphore`即可简洁地实现一个互斥锁，信号量的初值赋为1。具体实现如下：

在`synch.h`中为`Lock`类添加信号量`sema`和当前持有者`holdingThread`两个私有变量：

~~~cpp
  private:
    char* name;				// for debugging
    Semaphore sema;   // for synchronization
    Thread* holdingThread ; // Currently holding thread 
    // plus some other stuff you'll need to define
~~~

在`synch.cc`中实现构造函数、Acquire()、Release():

~~~cpp
Lock::Lock(char* debugName) {
    name = debugName;
    sema = Semaphore(debugName,1);
}
Lock::~Lock() {}
void Lock::Acquire() {
    sema.P();
    holdingThread = currentThread;
}
void Lock::Release() {
    holdingThread = NULL;
    sema.V();
}
bool Lock:isHeldByCurrentThread(){ 
    return currentThread == holdingThread;
}
~~~

#### 条件变量

在`synch.h`中为`Condition`类添加一个等待队列`waitingQueue`:

~~~c
  private:
    char* name;
    List *waitingQueue;       // waitingQueue pointer
~~~

在`synch.cc`中，实现Wait、Signal、Broadcast等原子操作，通过开关中断保证其原子性。

构造与析构函数：

~~~cpp
Condition::Condition(char* debugName) { 
    name = debugName;
    waitingQueue = new List;
}
Condition::~Condition() { 
    delete waitingQueue;
}
~~~

`Wait`操作，首先释放锁，在等待队列队尾添加当前线程，再获得锁：

~~~cpp
void Condition::Wait(Lock* conditionLock) { 
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    conditionLock->Release();
    waitingQueue->Append((Thread*)currentThread);
    currentThread->Sleep();
    conditionLock->Acquire();
    (void) interrupt->SetLevel(oldLevel);
 //   ASSERT(FALSE);
}
~~~

`Signal`操作：如果锁被当前进程持有，唤醒等待队列中的一个进程，将其从队列中弹出，进入就绪状态。

~~~cpp
void Condition::Signal(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    if (conditionLock->isHeldByCurrentThread()){
        Thread* thread = (Thread*)waitingQueue->Remove();
        if (thread){
            scheduler->ReadyToRun(thread);
        }
    }
    interrupt->SetLevel(oldLevel);
 }
~~~

`BroadCast`操作：

~~~cpp
void Condition::Broadcast(Lock* conditionLock) { 
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    if (conditionLock->isHeldByCurrentThread()){
        Thread* thread = (Thread*)waitingQueue->Remove();
        while(thread){
            scheduler->ReadyToRun(thread);
            thread = (Thread*)waitingQueue->Remove();
        }
    }
    interrupt->SetLevel(oldLevel);
}
~~~

#### 测试

见Exercise 4的实例测试,分别利用锁和条件变量解决生产者-消费者问题。

### Exercise 4 实现同步互斥实例

> 基于Nachos 中的信号量、锁和条件变量，采用两种方式实现同步和互斥机制应用（其中使用条件变量实 现同步互斥机制为必选题目）。具体可选择 生产者 消费者问题 、 读者写者问题 、 哲学家就餐问题 、 睡眠理发师问题等。（也可选择其他经典的同步互斥问题）

#### 用锁解决生产者-消费者问题

用锁保护产品队列和产品数目临界区，若消费者发现缓冲区为空或生产者发现缓冲区已满，则使本进程休眠，进行调度。

生产者：

~~~cpp
void produce_withLock(int which){
    int i ;
    for( i = 0 ; i < 20 ; ++i ){
        lock->Acquire();
        if (product_num == MAX_PRODUCT){
            printf("Oops!MAX_PRODUCT reached!We will produce later\n");
            lock->Release();
            interrupt->OneTick();
            --i;
        }
        else{
            Product* pro = new Product();
            ProductList->Append((void*)pro);
            printf("Thread %s produced a product,there are %d products now\n",currentThread->getName(),++product_num);
            lock->Release();
            interrupt->OneTick(); // 10 time units to produce a product
        }

    }
    currentThread->Finish();
}
~~~

消费者：

~~~cpp
void consume_withLock(int which){
    int i;
    for ( i = 0; i < 10 ; i++){
        
        lock->Acquire();
        if(ProductList->IsEmpty()){
            printf("Thread %s Oop! no products now\n",currentThread->getName());
            lock->Release();
            interrupt->OneTick(); 
            --i;
            // currentThread->Sleep();
        }else{
            Product * pro = (Product *)ProductList->Remove();
            printf("Thread %s, consumed a product,there are %d products now\n",currentThread->getName(),--product_num);
            delete pro;
           
            lock->Release();
            interrupt->OneTick(); 
        }
    }
} 
~~~

测试函数，一个生产者和两个消费者轮流上CPU:

~~~cpp
void ThreadTest6(){
    DEBUG('r',"Entering ThreadTest6,testing Lab3.4 Consumer-Prodcuter by lock\n");
    printf("Entering ThreadTest6,testing Lab3.4 Consumer-Prodcuter by lock\n");
    lock = new Lock("mutex");
    ProductList = new List();
    currentThread->SetPriority(0);
    Thread *t1 = new Thread("producer", 1);
    Thread *t2 = new Thread("consumer1", 2);
    Thread *t3 = new Thread("consumer2", 2);

   
    t2->Fork(consume_withLock,t2->GetThreadID());
    t1->Fork(produce_withLock,t1->GetThreadID());
    t3->Fork(consume_withLock,t3->GetThreadID());


    printf("main thread ends here\n");
}

~~~

测试：重新编译后输入./nachos -rs -q 6，执行输出如下图：

![image-20200315205110216](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200315205110216.png)

可发现生产者和两个消费者轮流上CPU,当生产者消耗完缓冲区产品时则进入睡眠状态，换为生产者上CPU进行生产，程序输出符合预期。

#### 用条件变量解决生产者-消费者问题

与用锁实现类似，实现两个条件变量`consumeCondition`和`ProduceCondition`，与互斥锁`lock`共同使用。生产者要访问临界区时先尝试获得锁，获得锁之后，检查缓冲区的元素个数，若缓冲区已满则等待在条件变量上，停止生产，等待消费后再生产，然后释放锁，如果有等待条件的消费进程则唤醒一个；消费者要访问临界区时先尝试获得锁，获得锁之后，检查缓冲区的元素个数，若缓冲区为空则等待在条件变量上，停止消费，等待生产后再消费，然后释放锁，如果有等待条件的生产进程则唤醒一个。

~~~cpp
void produce_withCondition(int which){
    int i ;
    for( i = 0 ; i < 20 ; ++i ){
        lock->Acquire();
        while (product_num >= MAX_PRODUCT){
            printf("Oops!MAX_PRODUCT reached!We will produce later\n");
            // lock->Release();
            produceCondition->Wait(lock);
            interrupt->OneTick();
        }
        
        Product* pro = new Product();
        ProductList->Append((void*)pro);
        printf("Thread %s produced a product,there are %d products now\n",currentThread->getName(),++product_num);
        consumeCondition->Signal(lock);
        lock->Release();
        interrupt->OneTick(); // 10 time units to produce a product
 
    }
    currentThread->Finish();
}

void consume_withCondition(int which){
    int i;
    for ( i = 0; i < 10 ; i++){
        
        lock->Acquire();
        while(ProductList->IsEmpty()){
            printf("Thread %s Oop! no products now\n",currentThread->getName());
            //lock->Release();
            consumeCondition->Wait(lock);
            interrupt->OneTick(); 
            
            // currentThread->Sleep();
        }
        
        Product * pro = (Product *)ProductList->Remove();
        printf("Thread %s, consumed a product,there are %d products now\n",currentThread->getName(),--product_num);
        delete pro;
        produceCondition->Signal(lock);
        lock->Release();
        interrupt->OneTick(); 
    }
} 
~~~

测试函数与用锁实现时相似：

~~~cpp
void ThreadTest7(){
    DEBUG('r',"Entering ThreadTest7,testing Lab3.4 Consumer-Prodcuter by conditional variables\n");
    printf("Entering ThreadTest7,testing Lab3.4 Consumer-Prodcuter by conditional variables\n");
    lock = new Lock("mutex");
    consumeCondition = new Condition("consume Condition");
    produceCondition = new Condition("produce Condition");
    ProductList = new List();
    currentThread->SetPriority(0);
    Thread *t1 = new Thread("producer", 1);
    Thread *t2 = new Thread("consumer1", 2);
    Thread *t3 = new Thread("consumer2", 2);

    t1->Fork(produce_withLock,t1->GetThreadID());
    t2->Fork(consume_withLock,t2->GetThreadID());
    t3->Fork(consume_withLock,t3->GetThreadID());
    printf("main thread ends here\n");
}
~~~

运行输出与使用锁实现时相同，证明用条件变量解决生产者-消费者问题正确。测试成功。

![image-20200315212853235](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200315212853235.png)

### Challenge 1 实现barrier

> 可以使用 Nachos 提供的同步互斥机制（如条件变量）来实现 barrier ，使得当且仅当若干个线程同时到达某一点时方可继续执行。

利用信号量来实现barrier。假设有四个需要同时到达某一点才能继续执行的线程，在`threadtest.cc`开头设置计数的全局变量和相应的锁与条件变量：

~~~cpp
Lock* conditionLock;         // The lock for barrier test 
Condition* barrierCondition; // The condition for barrier 
const int barrierSize = 4 ;
int barrierCnt = 0;
~~~

编写测试函数，每个线程到达等待点时将`barrierCount`加一并检查是否达到要求的线程数，如果达到则向等待队列广播信号，若未达到则等待信号，进入等待队列。

~~~cpp
void barrierRun(int which){ 
    
    conditionLock->Acquire();
    barrierCnt++;
    if(barrierCnt == barrierSize){

        printf("thread %s reached savepoint,barrierCnt %d/%d\n",currentThread->getName(),
                barrierCnt,barrierSize);
        barrierCondition->Broadcast(conditionLock);
        conditionLock->Release();
    }
    else{
        printf("thread %s reached savepoint,barrierCnt %d/%d\n",currentThread->getName(),
                barrierCnt,barrierSize);
        barrierCondition->Wait(conditionLock);
        conditionLock->Release();
    }
    printf("After gathering at the savepoint with friends,Thread %s continues\n",currentThread->getName());
    interrupt->OneTick();
}
~~~

测试时，将命名为Alice、Bob、Carol、David的四个进程设成一个barrier执行：

~~~cpp
void ThreadTest8(){ 
    DEBUG('r',"Entering ThreadTest8,testing Lab3 Challenge1:Barrier \n");
    printf("Entering ThreadTest8,testing Lab3 Challenge1:Barrier \n");
    conditionLock = new Lock("mutex");
    barrierCondition = new Condition("Barrier Condition");
    currentThread->SetPriority(0);
    Thread* t1 = new Thread("Alice",1);
    Thread* t2 = new Thread("Bob",1);
    Thread* t3 = new Thread("Carol",1);
    Thread* t4 = new Thread("David",1);
    t1->Fork(barrierRun,t1->GetThreadID());
    t2->Fork(barrierRun,t2->GetThreadID());
    t3->Fork(barrierRun,t3->GetThreadID());
    t4->Fork(barrierRun,t4->GetThreadID());
    printf("main thread ends here\n");
}
~~~

执行结果如下，符合预期：

![image-20200316001907736](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200316001907736.png)

### Challenge 2 实现读写锁

> 基于 Nachos 提供的 lock(synch.h 和 synch.cc) ，实现 read/write lock 。使得若干线程可以同时读取某共享数据区内的数据，但是在某一特定的时刻，只有一个线程可以向该共享数据区写入数据。

实现方案是借助两个锁实现读者优先，首先定义全局变量和锁，含义如注释：

~~~cpp
int content = 0; // Content variable for Reader-Writer Problem
int readerCount = 0 ; // The number of readers in the citical zone 
Lock* rcLock ; // The lock for readerCount
Lock* writeLock ; // The lock for content 
~~~

读时将`readerCount`加一，若加完后恰好为1则获得写锁`writeLock`，保证文件内容不被写者修改；读完后将`readerCount`减一，若减完后恰好为0则释放写锁`writeLock`。锁`rcLock`的作用是保护变量`readerCount`.

~~~cpp
void read(int which){
    for (int i = 0 ; i < 5 ; ++i ) {
        rcLock->Acquire();
        readerCount++;
        if (readerCount == 1){
            writeLock->Acquire();
        }
        rcLock->Release();
        printf("Thread %s read file content: %d\n",currentThread->getName(),content);
        rcLock->Acquire();
        readerCount--;
        if (readerCount == 0){ 
            writeLock->Release();
        }
        rcLock->Release();
        currentThread->Yield();
    }
}

void write(int which){
    for (int i = 0 ; i < 5 ; ++i){
        writeLock->Acquire();
        content++;
        printf("Thread %s write file content as %d\n",currentThread->getName(),content);
        writeLock->Release();
        currentThread->Yield();
    }
}
~~~

编写测试函数，两个写者和读者交替执行：

~~~cpp
void ThreadTest9(){ 
    DEBUG('r',"Entering ThreadTest9,testing Lab3 Challenge2:Read/Wrote Lock \n");
    printf("Entering ThreadTest9,testing Lab3 Challenge2:Read/Wrote Lock \n");
    rcLock = new Lock("the lock for readerCount");
    writeLock = new Lock("the lock for content");
    currentThread->SetPriority(0);
    Thread* t1 = new Thread("Reader Alice",1);
    Thread* t2 = new Thread("Reader Bob",1);
    Thread* t3 = new Thread("Wrtier Carol",1);
    Thread* t4 = new Thread("Writer David",1);
    t3->Fork(write,t3->GetThreadID());
    t2->Fork(read,t2->GetThreadID());
    t1->Fork(read,t1->GetThreadID());
    t4->Fork(write,t4->GetThreadID());
    printf("main thread ends here\n");
}
~~~

执行结果如下，符合预期，读写锁功能正确。

![image-20200316004642974](C:\Users\css\AppData\Roaming\Typora\typora-user-images\image-20200316004642974.png)

## 内容三 遇到的困难和解决方法

#### C++默认构造函数的写法

修改`Semaphore`的构造函数后编译报错，参考网络资料[默认构造函数和构造函数重载](https://blog.csdn.net/simon_2011/article/details/78129898),添加无参数构造函数的定义后编译成功。

#### Lock的写法不当导致段错误

开始时`Lock`中的信号量成员定义为`Semaphore`结构体，在主线程结束后再运行生产者和消费者线程，临时变量已经被删除，引发段错误。将`Lock`中的信号量定义改为`Semaphore`指针，在主线程中new一个新的信号量，避免此错误。

## 内容四 收获与感想

本次Lab动手实践了锁、信号量、条件变量等支持线程同步的数据结构，在生产者-消费者问题、barrier、读者-写者问题等经典案例上成功测试，锻炼了系统底层编程能力，加深了对线程/进程同步机制的理解。

## 内容五 对课程的意见或建议

暂无。

## 内容六 参考文献

- [Linux 同步方法剖析](https://www.ibm.com/developerworks/cn/linux/l-linux-synchronization.html)
- [内核中各种同步机制](https://blog.csdn.net/FreeeLinux/article/details/54267446)
- [默认构造函数和构造函数重载](https://blog.csdn.net/simon_2011/article/details/78129898)