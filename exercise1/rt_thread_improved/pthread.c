// Sam Siewert, July 2016
//
// Check to ensure all your CPU cores on in an online state.
//
// Check /sys/devices/system/cpu or do lscpu.
//
// Tegra is normally configured to hot-plug CPU cores, so to make all available,
// as root do:
//
// echo 0 > /sys/devices/system/cpu/cpuquiet/tegra_cpuquiet/enable
// echo 1 > /sys/devices/system/cpu/cpu1/online
// echo 1 > /sys/devices/system/cpu/cpu2/online
// echo 1 > /sys/devices/system/cpu/cpu3/online

#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#define NUM_THREADS (4)
#define NUM_CPUS (1)

#define NSEC_PER_SEC (1000000000)
#define NSEC_PER_MSEC (1000000)
#define NSEC_PER_MICROSEC (1000)
#define DELAY_TICKS (1)
#define ERROR (-1)
#define OK (0)


unsigned int idx = 0, jdx = 1;
unsigned int seqIterations = 47;
unsigned int reqIterations = 10000000;
volatile unsigned int fib = 0, fib0 = 0, fib1 = 1;

int numberOfProcessors;

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

typedef struct
{
    int threadIdx;
} threadParams_t;


// POSIX thread declarations and scheduling attributes
//
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];
pthread_attr_t rt_sched_attr[NUM_THREADS];
int rt_max_prio, rt_min_prio;
struct sched_param rt_param[NUM_THREADS];
struct sched_param main_param;
pthread_attr_t main_attr;
pid_t mainpid;


int delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
  int dt_sec=stop->tv_sec - start->tv_sec;
  int dt_nsec=stop->tv_nsec - start->tv_nsec;

  if(dt_sec >= 0)
  {
    if(dt_nsec >= 0)
    {
      delta_t->tv_sec=dt_sec;
      delta_t->tv_nsec=dt_nsec;
    }
    else
    {
      delta_t->tv_sec=dt_sec-1;
      delta_t->tv_nsec=NSEC_PER_SEC+dt_nsec;
    }
  }
  else
  {
    if(dt_nsec >= 0)
    {
      delta_t->tv_sec=dt_sec;
      delta_t->tv_nsec=dt_nsec;
    }
    else
    {
      delta_t->tv_sec=dt_sec-1;
      delta_t->tv_nsec=NSEC_PER_SEC+dt_nsec;
    }
  }

  return(1);
}


void *counterThread(void *threadp)
{
    int sum=0, i, cpucore;
    pthread_t thread;
    cpu_set_t cpuset;
    struct timespec start_time = {0, 0};
    struct timespec finish_time = {0, 0};
    struct timespec thread_dt = {0, 0};
    threadParams_t *threadParams = (threadParams_t *)threadp;

    clock_gettime(CLOCK_REALTIME, &start_time);

    thread=pthread_self();
    cpucore=sched_getcpu();

    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    // COMPUTE SECTION
    for(i=1; i < (((threadParams->threadIdx)+1)*100); i++)
        sum=sum+i;

    FIB_TEST(seqIterations, reqIterations);
    // END COMPUTE SECTION
 
    printf("\nThread idx=%d, sum[0...%d]=%d\n", 
           threadParams->threadIdx,
           ((threadParams->threadIdx)+1)*100, sum);

    printf("Thread idx=%d ran on core=%d, affinity contained:", threadParams->threadIdx, cpucore);
    for(i=0; i<numberOfProcessors; i++)
        if(CPU_ISSET(i, &cpuset))  printf(" CPU-%d ", i);
    printf("\n");

    clock_gettime(CLOCK_REALTIME, &finish_time);
    delta_t(&finish_time, &start_time, &thread_dt);

    printf("\nThread idx=%d ran %ld sec, %ld msec (%ld microsec)\n", threadParams->threadIdx, thread_dt.tv_sec, (thread_dt.tv_nsec / NSEC_PER_MSEC), (thread_dt.tv_nsec / NSEC_PER_MICROSEC));

    pthread_exit(&sum);
}


void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
     case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:
           printf("Pthread Policy is SCHED_OTHER\n");
           break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }

}



int main (int argc, char *argv[])
{
   int rc;
   int i, scope, idx;
   cpu_set_t allcpuset;
   cpu_set_t threadcpu;
   int coreid;

   printf("This system has %d processors configured and %d processors available.\n", get_nprocs_conf(), get_nprocs());

   numberOfProcessors = get_nprocs_conf(); 
   printf("number of CPU cores=%d\n", numberOfProcessors);

   CPU_ZERO(&allcpuset);

   for(i=0; i < numberOfProcessors; i++)
       CPU_SET(i, &allcpuset);

   if(numberOfProcessors >= NUM_CPUS)
       printf("Using sysconf number of CPUS=%d, count in set=%d\n", numberOfProcessors, CPU_COUNT(&allcpuset));
   else
   {
       numberOfProcessors=NUM_CPUS;
       printf("Using DEFAULT number of CPUS=%d\n", numberOfProcessors);
   }
        
   mainpid=getpid();

   rt_max_prio = sched_get_priority_max(SCHED_FIFO);
   rt_min_prio = sched_get_priority_min(SCHED_FIFO);

   print_scheduler();
   rc=sched_getparam(mainpid, &main_param);
   main_param.sched_priority=rt_max_prio;
   rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
   if(rc < 0) perror("main_param");
   print_scheduler();


   pthread_attr_getscope(&main_attr, &scope);

   if(scope == PTHREAD_SCOPE_SYSTEM)
     printf("PTHREAD SCOPE SYSTEM\n");
   else if (scope == PTHREAD_SCOPE_PROCESS)
     printf("PTHREAD SCOPE PROCESS\n");
   else
     printf("PTHREAD SCOPE UNKNOWN\n");

   printf("rt_max_prio=%d\n", rt_max_prio);
   printf("rt_min_prio=%d\n", rt_min_prio);


   for(i=0; i < NUM_THREADS; i++)
   {
       CPU_ZERO(&threadcpu);
       coreid=i%numberOfProcessors;
       printf("Setting thread %d to core %d\n", i, coreid);
       CPU_SET(coreid, &threadcpu);
       for(idx=0; idx<numberOfProcessors; idx++)
           if(CPU_ISSET(idx, &threadcpu))  printf(" CPU-%d ", idx);
       printf("\nLaunching thread %d\n", i);

       rc=pthread_attr_init(&rt_sched_attr[i]);
       rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
       rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
       rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

       rt_param[i].sched_priority=rt_max_prio-i-1;
       pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

       threadParams[i].threadIdx=i;

       pthread_create(&threads[i],   // pointer to thread descriptor
                      (void *)0,     // use default attributes
                      counterThread, // thread function entry point
                      (void *)&(threadParams[i]) // parameters to pass in
                     );

   }

   for(i=0;i<NUM_THREADS;i++)
       pthread_join(threads[i], NULL);

   printf("\nTEST COMPLETE\n");
}
