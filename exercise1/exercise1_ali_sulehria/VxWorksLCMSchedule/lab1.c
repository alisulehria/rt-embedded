/****************************************************************************/
/*                                                                          */
/* Sam Siewert - 2005                                                       */
/*                                                                          */
/****************************************************************************/
#define _GNU_SOURCE
#include "vxWorks.h"
#include "semLib.h"
#include "sysLib.h"
#include "wvLib.h"
#include <syslog.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define FIB_LIMIT_FOR_32_BIT 47
#define FIB10_ITERATIONS 170000  //meant to be ~10 ms
#define FIB20_ITERATIONS 340000

SEM_ID semF10, semF20; 
int abortTest = 0;
UINT32 seqIterations = FIB_LIMIT_FOR_32_BIT;
UINT32 idx = 0, jdx = 1;
UINT32 fib = 0, fib0 = 0, fib1 = 1;
UINT32 fib10Cnt=0, fib20Cnt=0;
char ciMarker[]="CI";


#define FIB_TEST(seqCnt, iterCnt)      \
   for(idx=0; idx < iterCnt; idx++)    \
   {                                   \
      fib = fib0 + fib1;               \
      while(jdx < seqCnt)              \
      {                                \
         fib0 = fib1;                  \
         fib1 = fib;                   \
         fib = fib0 + fib1;            \
         jdx++;                        \
      }                                \
   }                                   \


/* Iterations, 2nd arg must be tuned for any given target type
   using windview
   
   170000 <= 10 msecs on Pentium at home
   
   Be very careful of WCET overloading CPU during first period of
   LCM.
   
 */

// Helper: get current time in milliseconds (using CLOCK_MONOTONIC)
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

void fib10(void)
{
   while(!abortTest)
   {
    semTake(semF10, WAIT_FOREVER);
    double start = get_time_ms();
    FIB_TEST(seqIterations, FIB10_ITERATIONS);
    fib10Cnt++;
    double end = get_time_ms();
    double elapsed = end - start;
    printf("Fib10 executed: %f ms", elapsed);
   }
}

void fib20(void)
{
   while(!abortTest)
   {
    semTake(semF20, WAIT_FOREVER);
    double start = get_time_ms();
    FIB_TEST(seqIterations, FIB20_ITERATIONS);
    fib20Cnt++;
    double end = get_time_ms();
    double elapsed = end - start;
    printf("Fib10 executed: %f ms", elapsed);
   }
}

void shutdown(void)
{
	abortTest=1;
  pthread_join(thd_fib10, NULL);
  pthread_join(thd_fib20, NULL);
  pthread_join(thd_seq, NULL);
  sem_destroy(&semF10);
  sem_destroy(&semF20);
  syslog(LOG_INFO, "RT Schedule Program ending");
  closelog();
  return 0;
}

void Sequencer(void)
{

  printf("Starting Sequencer\n");

  /* Just to be sure we have 1 msec tick and TOs */
  sysClkRateSet(1000);

  /* Set up service release semaphores */
  semF10 = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
  semF20 = semBCreate(SEM_Q_FIFO, SEM_EMPTY);

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  struct sched_param param;
  param.sched_priority = 80;
  pthread_attr_setschedparam(&attr, &param);

/* 
  if(taskSpawn("serviceF10", 21, 0, 8192, fib10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
  {
    printf("F10 task spawn failed\n");
  }
  else
    printf("F10 task spawned\n");

  if(taskSpawn("serviceF20", 22, 0, 8192, fib20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR) 
  {
    printf("F20 task spawn failed\n");
  }
  else
    printf("F20 task spawned\n"); 
   
*/

  pthread_t thd_fib10, thd_fib20;
  if (pthread_create(&thd_fib10, &attr, fib10, NULL) != 0) {
      printf("F10 thread spawn failed\n");
      exit(EXIT_FAILURE);
  }
  if (pthread_create(&thd_fib20, &attr, fib20, NULL) != 0) {
      printf("F20 thread spawn failed\n");
      exit(EXIT_FAILURE);
  }

  /* Simulate the C.I. for S1 and S2 and mark on windview and log
     wvEvent first because F10 and F20 can preempt this task!
   */
  if(wvEvent(0xC, ciMarker, sizeof(ciMarker)) == ERROR)
	  printf("WV EVENT ERROR\n");
  semGive(semF10); semGive(semF20);


  /* Sequencing loop for LCM phasing of S1, S2
   */
  while(!abortTest)
  {

	  /* Basic sequence of releases after CI */
    syslog(LOG_INFO, "CI: releasing Fib10 and Fib20");
    taskDelay(20); semGive(semF10);
    syslog(LOG_INFO, "Sequencer: released Fib10 at +20ms");
    taskDelay(20); semGive(semF10);
    syslog(LOG_INFO, "Sequencer: released Fib10 at +40ms");
    taskDelay(10); semGive(semF20);
    syslog(LOG_INFO, "Sequencer: released Fib20 at +50ms");
    taskDelay(10); semGive(semF10);
    syslog(LOG_INFO, "Sequencer: released Fib10 at +60ms");
    taskDelay(20); semGive(semF10);
    syslog(LOG_INFO, "Sequencer: released Fib10 at +80ms");
    taskDelay(20);
    syslog(LOG_INFO, "Sequencer: Done at +100ms");
	  
	  /* back to C.I. conditions, log event first due to preemption */
	  if(wvEvent(0xC, ciMarker, sizeof(ciMarker)) == ERROR)
		  printf("WV EVENT ERROR\n");
	  semGive(semF10); semGive(semF20);
  }  
 
  pthread_exit(NULL);
}

void start(void)
{
	abortTest=0;
  pthread_t thd_seq;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  struct sched_param param;
  param.sched_priority = 80;
  pthread_attr_setschedparam(&attr, &param);

  if(pthread_create(&thd_seq, &attr, Sequencer, NULL) != 0) {
      printf("Sequencer thread spawn failed.");
      exit(EXIT_FAILURE);
  }   
}                                               
