#include "./headers/user.h"

conf *C;                    /* CONFIGURAZIONE            */
Mastro *libro_mastro;       /* SHM LIBRO MASTRO          */

int 
  /* ID SHM */
  mastro_id,              /* ID SHM LIBRO MASTRO              */
  shm_ind,                /* ID SHM INDEX_MASTER              */
  conf_id,                /* ID SHM CONFIGURATION             */
  iddead,                 /* ID SHM NUMERO USER MORTI         */

  /* QUEUE */
  *queue_transaction,     /* ARRAY QUEUE TRANSAZIONI */

  /* SEMAPHORI */
  start,                  /* SEMAPHORO DI START PER SINCRONIZZARE I PROCESSI           */
  user_sem,               /* SEMAFORO PER I USER ACCESSO ZONA CRITICA INDEX E LIBRO    */
  sem_dead,               /* SEMAFORO PER ZONA CRITICA USER MORTI                      */
  
  /* VARIABILI USER*/
  id,                     /* ID USER                          */
  myindex,                /* INDEX LETTURA MASTER DEL LIBRO   */
  retry,                  /* TENTATIVI INVIO TRANSAZIONI      */

  /* SHM */
  *deaduser,              /* NUMERO USER MORTI                              */
  *index_mastro;          /* PUNTATORE ALLA DIMENSIONE DEL LIBRO MASTRO     */

float bilancio;           /* BILANCIO USER  */
volatile int run = 1;     /* VARIABLE WHILE */

int main(int argc, char **argv)
{
  struct sigaction act;
  /*::: SEGNAL GESTION :::*/ 
  memset(&act, 0, sizeof(act));
  act.sa_handler = handler;
  set_sig (&act);

  /*::: INIZIALIZZO LE MIE RISORSE PER LA SIMULAZIONE :::*/
  sscanf(argv[1], "%d", &id);
  initResources();
  srand(time(NULL) + getpid());
  srandom(time(NULL) + getpid());
  retry=0;

  /*::: SYNCRO TIME :::*/  
  sem_sync(start);

  while (retry < C->SO_RETRY)
  {
    updateBilancio();
    if(bilancio >= 2)
    {
      send_transiction();
      retry=0;
    }
    else{
      retry++;
    }
  }
  run = 0;
  sem_wait(sem_dead);
    *deaduser+=1;
  sem_free(sem_dead);
  
  kill(getppid(),SIGUSR1);
  raise(SIGQUIT);
  freeResources();
  exit(EXIT_SUCCESS);
}


/*::: RESOURCES TIME :::*/
void initResources()
{
  int i;
    /*:::: SHM SECTION ::::*/
  /*::: LIBRO MASTRO SHMD :::*/
  mastro_id = shmget( getppid(), 0, 0666);
  ERROR
  libro_mastro = shmat(mastro_id, NULL, 0);
  ERROR

  /*::: INDICE LIBRO MASTRO SHMD :::*/
  shm_ind = shmget( getppid()+1, 0, 0666);
  ERROR
  index_mastro = shmat(shm_ind, NULL, 0);
  ERROR

  /*::: DATI CONFIGURAZIONE CONDIVISI SHMD :::*/
  conf_id = shmget( getppid()+2, 0, 0666);
  ERROR
  C = shmat(conf_id, NULL, 0);
  ERROR

  /*::: CONTATORE USER MORTI :::*/
  iddead = shmget( getppid()+4, 0, 0666);
  ERROR
  deaduser = shmat(iddead, NULL, 0);
  ERROR

    /*:::: QUEUE SECTION ::::*/
  /*::: Allocazione queue :::*/
  queue_transaction = malloc(sizeof(int) * (*C).SO_NODES_NUM);
  ERROR
  for( i=0; i<(*C).SO_NODES_NUM; i++){
    queue_transaction[i]= msgget( getppid()+i, 0666);
    ERROR
  }

    /*:::: SEM SECTION ::::*/
  /*::: START SEM :::*/
  start = semget( getppid(), 0, 0666);
  ERROR
  /*::: READERS SEM :::*/
  user_sem = semget( getppid()+2, 0, 0666);
  ERROR
  sem_dead = semget( getppid()+4, 0, 0666);
  ERROR

  /*::: INIT VARIABILI :::*/
  bilancio = (float) C->SO_BUDGET_INIT;
  myindex=0;
}

void freeResources()
{
  /*::: :::*/
  shmdt(libro_mastro);
  shmdt(index_mastro);
  shmdt(C);
  shmdt(deaduser);
  free(queue_transaction);
}

/*::: AGGIORNA IL BILANCIO :::*/
void updateBilancio()
{
  /*::: zona critica user :::*/
  sem_wait(user_sem);
  /*::: bilancio = budget + entrate - uscite :::*/
  /*::: myindex è l'indice di letture dell'user :::*/
  for(;myindex<*index_mastro;myindex++){
      
    if(id==libro_mastro[myindex].sender){
    }
    else if(libro_mastro[myindex].sender> NODEPAY && id==libro_mastro[myindex].receiver){
      bilancio += libro_mastro[myindex].quantita;
    }  
  }
  sem_free(user_sem);
}

/* GENERA UN NUMERO CASUALE TRA MIN E MAX */
float randfrom(float min, float max) 
{
  float range = (max - min); 
  float div = RAND_MAX / range;
  return min + (rand() / div);
}

/*::: INVIO UNA TRANSAZIONE :::*/
void send_transiction()
{
  TRequest tr;
  float random_value,
        value,
        reward;
  int random_user,
      random_node;

  random_node = rand()%(*C).SO_NODES_NUM;
  do{
    random_user = rand()%(*C).SO_USERS_NUM;
  }while(random_user == id);
  
  random_value  = randfrom( 2, bilancio);
  reward        = (random_value*C->SO_REWARD)/100;
  value         = random_value - reward; 

  tr.id = 1;
  clock_gettime( CLOCK_REALTIME , &tr.msg.timestamp);
  tr.msg.sender   = id;
  tr.msg.receiver = random_user;
  tr.msg.quantita = value;
  tr.msg.reward   = reward;
  tr.msg.hops     = NOHOPS;
  msgsnd(queue_transaction[random_node], &tr, sizeof(TRequest), IPC_NOWAIT);

  /*::: SIMULAZIONE TEMPO GENERAZIONE TRANSAZIONE :::*/
  sleepTime();
  bilancio -= random_value;
}

/*::: SLEEP TIME SIMULA GEN TRANS :::*/
void sleepTime()
{
  long time_sleep;
  struct timespec request, remaining;

  time_sleep = (random()%(  C->SO_MAX_TRANS_GEN_NSEC - 
                            C->SO_MIN_TRANS_GEN_NSEC) ) +  
                              C->SO_MIN_TRANS_GEN_NSEC;
  remaining.tv_sec   = time_sleep/1000000000L;
  remaining.tv_nsec  = time_sleep%1000000000L;
  do{
    request.tv_nsec = remaining.tv_nsec;
    request.tv_sec  = remaining.tv_sec; 
    nanosleep(&request, &remaining);
  }while(!compareTimespec(request,remaining));
}

/*::: SEGNALI :::*/
void handler(int sig)
{
  switch (sig) {
    case SIGINT:/*SEGNALE PER INVIARE UNA TRANSAZIONE*/
      if(bilancio >= 2)
      {
        send_transiction();
        retry=0;
      }
      break;
    case SIGQUIT:
      run=0;
      freeResources();
      exit(EXIT_SUCCESS);
      break;
    case SIGALRM:
      break;
    case SIGUSR1:
      if(run==1){
        queue_transaction = realloc(queue_transaction ,sizeof(int) * (C->SO_NODES_NUM));
        queue_transaction[C->SO_NODES_NUM-1]= msgget( getppid()+(C->SO_NODES_NUM-1), 0666);}
      break;
    case SIGUSR2:
      break;
    case SIGTSTP:
      break;
  }
}


