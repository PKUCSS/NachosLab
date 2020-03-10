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


// testnum is set in main.cc
int testnum = 1;

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
    default:
	printf("No test specified.\n");
	break;
    }
}

