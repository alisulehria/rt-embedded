#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>

int main() {
	char *SEM_NAME = "/mysem";
	sem_t *sem = sem_open(SEM_NAME, 0);
	if(sem == SEM_FAILED){
		printf("semaphore access failed!\n");
		return 1;
	}

	printf("Semaphore sending...\n");
	sem_post(sem);
	
	sem_close(sem);
	return 0;
}
