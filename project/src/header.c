#include "./headers/header.h"

/*:: SERVE PER SINCRONIZZARE I PROCESSI ALL'AVVIO ::*/
int sem_sync(int semid) {
  struct sembuf buf;
  buf.sem_num = 0;
  buf.sem_op = 0;
  buf.sem_flg = 0;
  return semop(semid, &buf, 1);
}

/*:: OCCUPA IL SEMAFORO APPENA LIBERO ::*/
int sem_wait(int semid) {
	struct sembuf sops;
	sops.sem_num = 0;
	sops.sem_op = -1;
	sops.sem_flg = 0;
	return semop(semid, &sops, 1);
}

/*:: RILASCIA IL SEMAFORO ::*/
int sem_free(int semid) {
	struct sembuf sops;
	sops.sem_num = 0;
	sops.sem_op = 1;
	sops.sem_flg = 0;
	return semop(semid, &sops, 1);
}

/*:: CONFRONTA DUE TIMESPEC (secondi e nanosecondi)::*/
int compareTimespec(struct timespec request, struct timespec remaining){
  return (request.tv_nsec==remaining.tv_nsec)&&(request.tv_sec== remaining.tv_sec);
}

/*:: SEGNALI ::*/
void set_sig ( struct sigaction *act)
{
	sigaction(SIGINT,  act, 0);
	sigaction(SIGALRM, act, 0);
	sigaction(SIGQUIT, act, 0);
	sigaction(SIGUSR1, act, 0);
	sigaction(SIGUSR2, act, 0);
	sigaction(SIGTSTP, act, 0);
}

