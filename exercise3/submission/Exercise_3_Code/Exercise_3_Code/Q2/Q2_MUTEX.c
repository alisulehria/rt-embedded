#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h> 
#include <syslog.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>

// Define semaphore and mutex variables
sem_t sem_T1, sem_T2;
pthread_mutex_t lk;

struct timeval tv;

int testDone = 0;

struct Loc {

  struct timespec time_stamp;

  double Yaw;
  double Pitch;
  double Roll;

  double Lat;
  double Long;
  double Alt;
} Global_var;


/*
Generates new data for the cuztomized data structure based on the current time.
The data is generated as follows:
  Roll = sin(time)
  Pitch = cos(time)
  Yaw = tan(time)
  Lat = 0.2 * time
  Long = 0.5 * time
  Alt = 0.8 * time
time is the current time in seconds returned by the system using clock_gettime
with CLOCK_REALTIME. This time is also saved in the custom data structure along
with the generated data
*/
struct Loc Get_New_Val(){

  struct Loc local_var;

  clock_gettime(CLOCK_REALTIME, &local_var.time_stamp);

  local_var.Roll = sin(local_var.time_stamp.tv_sec);
  local_var.Pitch = cos(local_var.time_stamp.tv_sec);
  local_var.Yaw = tan(local_var.time_stamp.tv_sec);

  local_var.Lat = 0.2 * local_var.time_stamp.tv_sec;
  local_var.Long = 0.5 * local_var.time_stamp.tv_sec;
  local_var.Alt = 0.8 * local_var.time_stamp.tv_sec;

  return local_var;
}

/*
Uses the time stamp in the custom data structure to check the integrity of the
data. The data is checked as follows:
  Roll = sin(time)
  Pitch = cos(time)
  Yaw = tan(time)
  Lat = 0.2 * time
  Long = 0.5 * time
  Alt = 0.8 * time
If the data integrity is maintained, the function returns 0. If the data is
corrupted, the function returns 1
*/
int Check_New_Val(struct Loc new_val){

  int ret_val = 0;

  if (new_val.Roll != sin(new_val.time_stamp.tv_sec))
    ret_val = 1;

  if (new_val.Pitch != cos(new_val.time_stamp.tv_sec))
    ret_val = 1;

  if (new_val.Yaw != tan(new_val.time_stamp.tv_sec))
    ret_val = 1;
  
  if (new_val.Lat != 0.2 * new_val.time_stamp.tv_sec)
    ret_val = 1;

  if(new_val.Long != 0.5 * new_val.time_stamp.tv_sec)
    ret_val = 1;

  if(new_val.Alt != 0.8 * new_val.time_stamp.tv_sec)
    ret_val = 1;

  return ret_val;
}

/* 
Task 1 function
Generates new data and writes it to the global variable by acquiring the mutex 
lock. Info is printed in syslog and console
 */
void *Task_1(void *arg){

  struct Loc T1_local_var;
    
  do{
    sem_wait(&sem_T1);
    
    if (testDone)
      break;

    T1_local_var = Get_New_Val();

    pthread_mutex_lock(&lk);
    Global_var = T1_local_var;
    pthread_mutex_unlock(&lk);

    printf("Task 1: Value updated\n");
    syslog(LOG_DEBUG, "Task 1: Value updated\n");
    printf("\n");

  }while(1);
  
  return NULL;
}

/* 
Task 2 function
Reads the global variable and checks for data integrity and prints the result in 
syslog and console. If an error is found, it prints an error message. If no 
error is found, it prints a success message
 */
void *Task_2(void *arg){

  struct Loc T2_local_var;
  int val_check = 0;

  do{
    sem_wait(&sem_T2);
    
    if (testDone)
      break;

     
    // Attempt to acquire the mutex lock and read the global variable
    pthread_mutex_lock(&lk);
    T2_local_var = Global_var;
    pthread_mutex_unlock(&lk);

    // Check the data integrity
    val_check = Check_New_Val(T2_local_var);

    printf("Task 2: value read\n");
    
    syslog(LOG_DEBUG, "Task 2: \n");
    
    syslog(LOG_DEBUG, "Time: %ld sec, %ld nsec\n", 
    T2_local_var.time_stamp.tv_sec, 
    T2_local_var.time_stamp.tv_nsec);
  
    syslog(LOG_DEBUG, "Lat = %f, Long = %f, Alt = %f\n", 
    T2_local_var.Lat, 
    T2_local_var.Long, 
    T2_local_var.Alt);
  
    syslog(LOG_DEBUG, "Roll = %f, Pitch = %f, Yaw = %f\n", 
    T2_local_var.Roll, 
    T2_local_var.Pitch, 
    T2_local_var.Yaw);

    if(val_check){
      printf("!!!!Error in data\n");
      syslog(LOG_PERROR, "!!!!Error in data\n");
    }
    else{
      printf("Data OK\n");
      syslog(LOG_DEBUG, "Data OK\n");
    }
    printf("\n");

  }while(1);

  return NULL;
}

/* 
Timer handler function
This function is called every 1 sec by the timer signal
It posts the semaphore to release the tasks
Task 1 is released every 1 sec and Task 2 is released every 10 sec
The test is stopped after 180 sec
 */
void timer_handler(int signum)
{
  static int count = 0;
  static int release_count = 0;
  count++;
  release_count++;

  sem_post(&sem_T1);

  if (release_count % 10 == 0) {
      sem_post(&sem_T2);
  }
  
  if (count > 180) {
      testDone = 1;
      sem_post(&sem_T1);
      sem_post(&sem_T2);
      printf("Test done\n");
      syslog(LOG_DEBUG, "Test done\n");
  }
}

// Timer initialization function
// Setups the timer to trigger every 1 sec
void *timer_init(void *args){
  // Set up the timer
  struct sigaction sa;
  struct itimerval timer;

  // Install timer_handler as the signal handler for SIGALRM
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = &timer_handler;
  sigaction(SIGALRM, &sa, NULL);

  // Configure the timer to expire after 1 sec and then every 1 sec
  timer.it_value.tv_sec = 1;
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 1;
  timer.it_interval.tv_usec = 0;

  // Start the timer
  setitimer(ITIMER_REAL, &timer, NULL);
  printf("Timer started\n");
  return NULL;
}

// Start function to initialize the semaphores and spawn threads
void start(void)
{
  unsigned int idx = 0;

  // Initialize semaphores and mutexes
  if (sem_init(&sem_T1, 0, 0) != 0){
    perror("Failed to initialize Task 1");
    exit(1);
  }

  if (sem_init(&sem_T2, 0, 0) != 0){
    perror("Failed to initialize Task 2");
    exit(1);
  }

  if (pthread_mutex_init(&lk, NULL) != 0){
    perror("Failed to initialize mutex");
    exit(1);
  }

  cpu_set_t threadcpu;
  CPU_ZERO(&threadcpu);
  int coreid=0;
  printf("using core %d\n",coreid);
  CPU_SET(coreid, &threadcpu);
  if(CPU_ISSET(idx, &threadcpu))  printf(" CPU-%d \n", idx);

  pthread_attr_t rt_sched_attr[2];
  struct sched_param rt_param[2];
  int rt_max_prio = sched_get_priority_max(SCHED_FIFO);
  int rc;

  rc=pthread_attr_init(&rt_sched_attr[0]);
  rc=pthread_attr_setinheritsched(&rt_sched_attr[0], PTHREAD_EXPLICIT_SCHED);
  rc=pthread_attr_setaffinity_np(&rt_sched_attr[0], sizeof(cpu_set_t), &threadcpu);

  rt_param[0].sched_priority=rt_max_prio;
  pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]);


  rc=pthread_attr_init(&rt_sched_attr[1]);
  rc=pthread_attr_setinheritsched(&rt_sched_attr[1], PTHREAD_EXPLICIT_SCHED);
  rc=pthread_attr_setaffinity_np(&rt_sched_attr[1], sizeof(cpu_set_t), &threadcpu);

  rt_param[1].sched_priority=rt_max_prio-1;
  pthread_attr_setschedparam(&rt_sched_attr[1], &rt_param[1]);

  // Setting CPU affinity
  // Create threads for Task1, Task2, and sequencer
  pthread_t thread_T1, thread_T2, thread_timer;

  if (pthread_create(&thread_T1, &rt_sched_attr[0], Task_1, NULL) != 0){
    perror("Failed to create thread for Task 1");
    exit(1);
  }
  else
    printf("Task 1 spawned\n");

  if (pthread_create(&thread_T2, &rt_sched_attr[1], Task_2, NULL) != 0){
    perror("Failed to create thread for Task 2");
    exit(1);
  }
  else
    printf("Task 2 spawned\n");

  if (pthread_create(&thread_timer, NULL, timer_init, NULL) != 0)
  {
    perror("Failed to create timer thread");
    exit(1);
  }
  else
    printf("Timer task spawned\n");

  // Wait for the timer thread to finish
  pthread_join(thread_timer, NULL);

  // Clean up and shutdown
  pthread_join(thread_T1, NULL);
  pthread_join(thread_T2, NULL);

  // Destroy semaphores and mutexes
  sem_destroy(&sem_T1);
  sem_destroy(&sem_T1);
  pthread_mutex_destroy(&lk);
}

int main(void)
{
  syslog(LOG_CRIT, "RTES Q2 MUTEX DEMO task\n");  
  start();
  return 0;
}
