#define _GNU_SOURCE

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <semaphore.h>
#include <unistd.h>

#define COUNT  1000

typedef struct
{
    int threadIdx;
} threadParams_t;


// POSIX thread declarations and scheduling attributes
//
pthread_t threads[2];
threadParams_t threadParams[2];
pthread_attr_t rt_sched_attr[2];
int rt_max_prio, rt_min_prio;
struct sched_param rt_param[2];
struct sched_param main_param;
pid_t mainpid;

sem_t sem_inc;

// Unsafe global
int gsum=0;

void *incThread(void *threadp)
{
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    for(i=0; i<COUNT; i++)
    {
        gsum=gsum+i;
        printf("Increment thread idx=%d, gsum=%d\n", threadParams->threadIdx, gsum);
    }
    sem_post(&sem_inc);
}


void *decThread(void *threadp)
{
    
    int i;
    threadParams_t *threadParams = (threadParams_t *)threadp;
    sem_wait(&sem_inc);
    for(i=0; i<COUNT; i++)
    {
        gsum=gsum-i;
        printf("Decrement thread idx=%d, gsum=%d\n", threadParams->threadIdx, gsum);
    }
}




int main (int argc, char *argv[])
{
   int rc;
   int i=0;

   mainpid = getpid();
   cpu_set_t cpuset;
   //CPU_ZERO(&cpuset);
   // without this it becomes very upset?
   //CPU_SET(0, &cpuset);

    // get mix, max prio. since I'm only using FIFO I could really just have these 
    // values be 30 and 29 or whatever, but I'll code like this might ever be used again.
   rt_max_prio = sched_get_priority_max(SCHED_FIFO);
   rt_min_prio = sched_get_priority_min(SCHED_FIFO);
   
   rc=sched_getparam(mainpid, &main_param);
   main_param.sched_priority = rt_max_prio;
   rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);

   sem_init(&sem_inc, 0644, 0);

   rc=pthread_attr_init(&rt_sched_attr[i]);
   rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
   rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
   rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &cpuset);
    rt_param[i].sched_priority=rt_max_prio-i-1;
    pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

   threadParams[i].threadIdx=i;
   pthread_create(&threads[i],   // pointer to thread descriptor
                  &rt_sched_attr[i],     // use default attributes
                  incThread, // thread function entry point
                  (void *)&(threadParams[i]) // parameters to pass in
                 );
    
   i++;

   
   rc=pthread_attr_init(&rt_sched_attr[i]);
   rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
   rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
   rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &cpuset);

   rt_param[i].sched_priority=rt_max_prio-i-1;
   pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);
   threadParams[i].threadIdx=i;
   pthread_create(&threads[i], &rt_sched_attr[i], decThread, (void *)&(threadParams[i]));

   for(i=0; i<2; i++)
     pthread_join(threads[i], NULL);

   printf("TEST COMPLETE\n");

   sem_destroy(&sem_inc);

}
