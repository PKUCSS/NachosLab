// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synch.h"
#define MAX_PRODUCT 10
// testnum is set in main.cc
int testnum = 1;


class Product{
    public:
        int id;
        Product(){id = 0; } ;
};

List *ProductList ;       // The product pool  
int product_num = 0;
Lock* lock;               // The lock for synchronization
Condition* consumeCondition; // The condition for consuming
Condition* produceCondition; // The condition for producing 

Lock* conditionLock;         // The lock for barrier test 
Condition* barrierCondition; // The condition for barrier 
const int barrierSize = 4 ;
int barrierCnt = 0;

int content = 0; // Content variable for Reader-Writer Problem
int readerCount = 0 ; // The number of readers in the citical zone 
Lock* rcLock ; // The lock for readerCount
Lock* writeLock ; // The lock for content 


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




//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

void PrintFlag(int arg){
    printf("Flag Thread %d runs here!\n",arg);
   // GetThreadStatus();
}

void SimpleThread2(int which)
{
    int num;

    for (num = 0; num < 5; num++) {
        printf("*** thread %d ,uid = %d,tid = %d,priority = %d,looped %d\n",which,
            currentThread->GetUserID(),currentThread->GetThreadID(),currentThread->GetPriority(),num);
        currentThread->Yield();
    }
    Thread* flag = new Thread("Flag",1);
    flag->Fork(PrintFlag,flag->GetThreadID());
    printf("*** thread %d ,uid = %d,tid = %d,priority = %d,ends here\n",which,currentThread->GetUserID(),
    currentThread->GetThreadID(),currentThread->GetPriority());
  //  GetThreadStatus();
}

void SimpleThread3(int which)
{
    int num;

    for (num = 0; num < 4; num++) {
        printf("*** thread %d ,uid = %d,tid = %d,priority = %d,looped %d,used ticks: %d,system ticks: %d\n",which,
            currentThread->GetUserID(),currentThread->GetThreadID(),currentThread->GetPriority(),num,(num+1)*50,stats->totalTicks);
        for(int i = 0 ; i < 5 ; ++i ) interrupt->OneTick(); // advanced for 50 ticks
    }
    currentThread->Finish();
  //  GetThreadStatus();
}

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

void ThreadTest2(){
    DEBUG('t', "Entering ThreadTest2,testing for tid and uid in Lab1");
    for (int i = 0; i < 5; i++) {
        // Generate a Thread object
        Thread *t = new Thread("forked thread");
        t->SetUserID(i*11); // set uid
        // Define a new thread's function and its parameter
        t->Fork(SimpleThread2, t->GetThreadID());
    }
    GetThreadStatus();

}

void ThreadTest3(){
    DEBUG('t',"Entering ThreadTest3,testing for MaxThreadCount Limit");
    printf("Successufully allocated thread ids: ");
    for (int i = 0; i < 130 ; ++i){
        Thread *t = new Thread("forked thread");
        printf(" %d",t->GetThreadID());
    }
}


void ThreadTest4(){
    DEBUG('r',"Entering ThreadTest4,testing for priority scheduling,LAB2");
    printf("Entering ThreadTest4,testing for priority scheduling,LAB2\n");
    currentThread->SetPriority(0);
    Thread *t1 = new Thread("highest", 12);
    Thread *t2 = new Thread("middle", 24);
    Thread *t3 = new Thread("lowest", 36);

    
    

    t3->Fork(SimpleThread2, t3->GetThreadID());
    t2->Fork(SimpleThread2, t2->GetThreadID());
    t1->Fork(SimpleThread2, t1->GetThreadID());


    printf("main thread ends here\n");
}
 
void ThreadTest5(){ // Lab2 Challenge:Time Slice Rounding
    DEBUG('r',"Entering ThreadTest5,testing Lab2 Challenge:Time Slice Rounding");
    printf("Entering ThreadTest5,testing Lab2 Challenge:Time Slice Rounding\n");
    currentThread->SetPriority(0);
    Thread *t1 = new Thread("t1", 12);
    Thread *t2 = new Thread("t2", 12);
    Thread *t3 = new Thread("t3", 24);
    Thread *t4 = new Thread("t4", 24);

    
    
    t4->Fork(SimpleThread3,t4->GetThreadID());
    t3->Fork(SimpleThread3,t3->GetThreadID());
    t2->Fork(SimpleThread3,t2->GetThreadID());
    t1->Fork(SimpleThread3,t1->GetThreadID());


    printf("main thread ends here\n");
}

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


//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	ThreadTest1();
	break;
    case 2:
    ThreadTest2();
    break;
    case 3:
    ThreadTest3();
    break;
    case 4:
    ThreadTest4();
    case 5:
    ThreadTest5();
    break;
    case 6:
    ThreadTest6();
    break;
    case 7:
    ThreadTest7();
    case 8:
    ThreadTest8();
    case 9:
    ThreadTest9();
    break;
    default:
	printf("No test specified.\n");
	break;
    }
}

