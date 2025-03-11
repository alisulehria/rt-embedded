#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>

int main() {
	char *SEM_NAME = "/mysem";
	sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0644, 0);

	if(sem == SEM_FAILED) {
		perror("sem_open");
		return 1;
	}
	printf("Process waiting for semaphore...\n");
	sem_wait(sem);
	printf("Semaphore received!\n");

	sem_close(sem);
	sem_unlink(SEM_NAME);
	return 0;

}



