// For the heap message queue, take this message copy version and create a global heap and queue and dequeue pointers to that
// heap instead of the full data in the messages.
//
// Otherwise, the code should not be substantially different.
//
// You can set up an array in the data segement as a global or use malloc and free for a true heap message queue.
//
// Either way, the queue and dequeue should be ZERO copy.
//
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <pthread.h>
#include <mqueue.h>
#include <unistd.h>

// On Linux the file systems slash is needed
#define SNDRCV_MQ "/send_receive_mq"
#define PTR_MSG_SIZE    (sizeof(void *))
#define MAX_MSG_SIZE 128
#define ERROR (-1)

struct mq_attr mq_attr;


pthread_t th_receive, th_send; // create threads
pthread_attr_t attr_receive, attr_send;
struct sched_param param_receive, param_send;

static char canned_msg[] = "This is a test, and only a test, in the event of real emergency, you would be instructed...."; // Message to be sent

/* receives pointer to heap, reads it, and deallocate heap memory */

void *receiver(void *arg)
{
    mqd_t mymq;
    char ptr_buffer[PTR_MSG_SIZE];
    void *heap_ptr;
    int priority;
    int rc;

    printf("[Receiver] Thread started\n");

    // Open the queue (must match the name & attributes from the sender)
    mymq = mq_open(SNDRCV_MQ, O_CREAT | O_RDWR, S_IRWXU, &mq_attr);
    if(mymq == (mqd_t)ERROR)
    {
        perror("[Receiver] mq_open");
        pthread_exit(NULL);
    }

    while(1)
    {
        // Block until a pointer-sized message arrives
        rc = mq_receive(mymq, ptr_buffer, PTR_MSG_SIZE, &priority);
        if(rc == ERROR)
        {
            perror("[Receiver] mq_receive");
            break; // or continue, depending on desired logic
        }
        else
        {
            // Copy the pointer bytes out of ptr_buffer
            memcpy(&heap_ptr, ptr_buffer, sizeof(void *));

            printf("[Receiver] Got pointer: %p, priority=%d\n", heap_ptr, priority);

            // Print what is stored in that allocated buffer
            printf("[Receiver] Message content: %s\n", (char *)heap_ptr);

            // Since the sender allocated memory dynamically, we can free it here
            free(heap_ptr);
            printf("[Receiver] Freed pointer: %p\n\n", heap_ptr);
        }
    }

    mq_close(mymq);
    pthread_exit(NULL);
}


void *sender(void *arg)
{
    mqd_t mymq;
    char ptr_buffer[PTR_MSG_SIZE];
    void *heap_ptr;
    int rc;
    int priority = 30; // Arbitrary priority

    printf("[Sender] Thread started\n");

    // Open the queue (same name & attributes)
    mymq = mq_open(SNDRCV_MQ, O_CREAT | O_RDWR, S_IRWXU, &mq_attr);
    if(mymq == (mqd_t)ERROR)
    {
        perror("[Sender] mq_open");
        pthread_exit(NULL);
    }

    while(1)
    {
        // Allocate space for our message
        heap_ptr = malloc(strlen(canned_msg) + 1);
        if(!heap_ptr)
        {
            perror("[Sender] malloc");
            break; 
        }

        // Copy the string into the newly allocated buffer
        strcpy((char *)heap_ptr, canned_msg);

        // Package the pointer into ptr_buffer so we can send it via mq_send
        memcpy(ptr_buffer, &heap_ptr, sizeof(void *));

        // Send pointer with some priority
        rc = mq_send(mymq, ptr_buffer, PTR_MSG_SIZE, priority);
        if(rc == ERROR)
        {
            perror("[Sender] mq_send");
            // If we fail here, we should free the pointer or handle it
            free(heap_ptr);
            break;
        }
        else
        {
            printf("[Sender] Sent pointer: %p\n", heap_ptr);
        }

        // Sleep just to simulate a periodic send
        sleep(1);
    }

    mq_close(mymq);
    pthread_exit(NULL);
}


void main(void)
{
  int i=0, rc=0;
  /* setup common message q attributes */
  mq_attr.mq_maxmsg = 10;
  mq_attr.mq_msgsize = PTR_MSG_SIZE;
  
  mq_attr.mq_flags = 0;

  int rt_max_prio, rt_min_prio;

  rt_max_prio = sched_get_priority_max(SCHED_FIFO);
  rt_min_prio = sched_get_priority_min(SCHED_FIFO);


  // Create two communicating processes right here
  //receiver((void *)0);
  //sender((void *)0);
  
  //exit(0); 
  
  //creating prioritized thread
  
  //initialize  with default atrribute
  rc = pthread_attr_init(&attr_receive);
  //specific scheduling for Receiving
  rc = pthread_attr_setinheritsched(&attr_receive, PTHREAD_EXPLICIT_SCHED);
  rc = pthread_attr_setschedpolicy(&attr_receive, SCHED_FIFO); 
  param_receive.sched_priority = rt_min_prio;
  pthread_attr_setschedparam(&attr_receive, &param_receive);
  
  //initialize  with default atrribute
  rc = pthread_attr_init(&attr_send);
  //specific scheduling for Receiving
  rc = pthread_attr_setinheritsched(&attr_send, PTHREAD_EXPLICIT_SCHED);
  rc = pthread_attr_setschedpolicy(&attr_send, SCHED_FIFO); 
  param_send.sched_priority = rt_max_prio;
  pthread_attr_setschedparam(&attr_send, &param_send);
  
  if((rc=pthread_create(&th_send, &attr_send, sender, NULL)) == 0)
  {
    printf("\n\rSender Thread Created with rc=%d\n\r", rc);
  }
  else 
  {
    perror("\n\rFailed to Make Sender Thread\n\r");
    printf("rc=%d\n", rc);
  }

  if((rc=pthread_create(&th_receive, &attr_receive, receiver, NULL)) == 0)
  {
    printf("\n\r Receiver Thread Created with rc=%d\n\r", rc);
  }
  else
  {
    perror("\n\r Failed Making Reciever Thread\n\r"); 
    printf("rc=%d\n", rc);
  }

  printf("pthread join send\n");  
  pthread_join(th_send, NULL);

  printf("pthread join receive\n");  
  pthread_join(th_receive, NULL);
}
