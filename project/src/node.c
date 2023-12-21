#include "./headers/node.h"

conf *C;                    /* CONFIGURAZIONE            */
Mastro *libro_mastro;       /* SHM LIBRO MASTRO          */
Transaction *transPool;     /* TRANSACTION POOL          */

int 
  /* ID SHM */
  mastro_id,              /* ID SHM LIBRO MASTRO              */
  shm_ind,                /* ID SHM INDEX_MASTER              */
  conf_id,                /* ID SHM CONFIGURATION             */
  id_cu,                  /* ID SHM COUNTER_PROCESS_NODE      */

  /* QUEUE */
  queue_transaction,      /* QUEUE TRANSAZIONI      */
  master_queue,           /* QUEUE TR FOR NEW NODE  */
  trnodes_queue,          /* QUEUE TR in the pool   */
  *queue_Friends,         /* QUEUE FRIENDS          */

  /* SEMAPHORI */
  start,                  /* SEMAPHORO DI START                                        */
  nodes_sem,              /* SEMAFORO PER I NODI ACCESSO ZONA CRITICA INDEX E LIBRO    */
  user_sem,               /* SEMAFORO PER I USER ACCESSO ZONA CRITICA INDEX E LIBRO    */
  sem_writ,               /* SEMAFORO PER SCRITTURA ACCESSO ZONA CRITICA INDEX E LIBRO */ 
  
  /* VARIABILI NODI*/
  id,                     /* ID NODE                                      */
  *nodes_Friends,         /* ARRAY RANDOM FRIEND                          */
  count,                  /* 2 TENTATIVI DI INVIO MANDO UNA TRAD UN AMICO */

  /* SHM */
  nfriends,               /* NUMERO AMICI                                   */
  *index_mastro,          /* PUNTATORE ALLA DIMENSIONE DEL LIBRO MASTRO     */
  *counter_nodes;         /* CONTATORI NODI ACCESSO ALLA SCRITTURA          */

unsigned int sizePool;


int main(int argc, char **argv)
{
  struct sigaction act;

  /*::: GESTIONE SEGNALI :::*/
  memset(&act, 0, sizeof(act));
  act.sa_handler = handler;
  set_sig (&act);

  /*::: INITIALIZZO LE RISORSE PER LA SIMULAZIONE :::*/
  sscanf(argv[1], "%d", &id);
  initResources();
  srand(id + time(NULL) % getpid());
  srandom(id + time(NULL) % getpid());
            
  /*::: SYNCRO TIME :::*/
  sem_sync(start);

  /*::: CORE NODES :::*/
  while (1)
  {
    updatePoolT();
    processTransaction();
  }
  exit(EXIT_SUCCESS);  
}

/*::: RESOURCES TIME :::*/
void initResources()
{
  int i, r; /*I = CONTATORE FOR, R = RANDOM */

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

  /*::: CONTATORE NODES PER MODELLI READERS AND WRITER SHMD :::*/
  id_cu = shmget( getppid()+3, 0, 0666);
  ERROR
  counter_nodes = shmat( id_cu, NULL, 0);
  ERROR

  /*::: ARRAY TRANSACTION POOL :::*/
  transPool = malloc(sizeof(Transaction) * C->SO_TP_SIZE);
  sizePool = 0;

  /*::: ARRAY NODI AMICI + QUEUE AMICI :::*/
  nodes_Friends = malloc( C->SO_FRIENDS_NUM * sizeof(int));
  queue_Friends = malloc( C->SO_FRIENDS_NUM * sizeof(int));
  srand(id + time(NULL) % getpid());
  for( i=0; i<C->SO_FRIENDS_NUM; i++){
    do{
      r = rand() % C->SO_NODES_NUM;
    }while(checkFriends(r,i));
    nodes_Friends[i] = r;
    queue_Friends[i] = msgget( getppid()+r,0666);
  }
  count = 0;
  nfriends = C->SO_FRIENDS_NUM;

    /*:::: QUEUE SECTION ::::*/
  /*:: MIA QUEUE::*/
  queue_transaction = msgget( getppid()+id, 0666);
  ERROR
  /*:: MASTER QUEUE ::*/
  master_queue = msgget( getppid()-1, 0666);
  ERROR
  /*:: MASTER TRANSAZIONI NON PROCESSATE QUEUE ::*/
  trnodes_queue = msgget( getppid()-2, 0666);
  ERROR

    /*:::: SEM SECTION ::::*/
  /*::: START SEM :::*/
  start = semget( getppid(), 0, 0666);
  ERROR
  /*::: NODI SEM :::*/
  nodes_sem = semget( getppid()+1, 0, 0666);
  ERROR
  /*::: USER  SEM :::*/
  user_sem = semget( getppid()+2, 0, 0666);
  ERROR
  /*::: NODI SEM QUANDO SCRIVONO :::*/
  sem_writ = semget( getppid()+3, 0, 0666);
  ERROR
}

void freeResources()
{
  shmdt(libro_mastro);
  shmdt(index_mastro);
  shmdt(C);
  shmdt(counter_nodes);

  free(nodes_Friends);
  free(queue_Friends);
  free(transPool);
}

/*::: CONTROLLA SE R È GIA PRESENTE */
int checkFriends(int r, int i){  /*posizione i attuale di quanto è pieno*/
  int j=0;
  if(r==id){
    i=0; j=-1;
  }
  while(i!=0 && j<i && r != nodes_Friends[j]){j++;}
  return (j<i) ? 1 : 0;
}

/*::: AGGIORNA LA POOL :::*/
void updatePoolT()
{
  TRequest tr;  /*::: MESSAGGI TRANSAZIONI*/
  int nodeF;    /*::: NODE FRIEND*/
  Msgarr nuovoAmico;

  /*::: RIEMPO LA TRANSACTION POOL DI TUTTE LE TRANSAZIONI RICEVUTE :::*/
  while(msgrcv(queue_transaction, &nuovoAmico, sizeof(nuovoAmico), 2, IPC_NOWAIT)>0)
  {
    nodes_Friends = realloc(nodes_Friends, C->SO_NODES_NUM*sizeof(int));
    queue_Friends = realloc(queue_Friends, C->SO_NODES_NUM*sizeof(int));
    nodes_Friends [nfriends] = nuovoAmico.msg;
    queue_Friends [nfriends] = msgget(getppid()+nuovoAmico.msg, 0666);
    nfriends++;
  }

  while (msgrcv(queue_transaction, &tr, sizeof(TRequest), 1, IPC_NOWAIT)>0)
  {
    /*::: RIEMPO LA TRANSACTION POOL :::*/
    if(sizePool < C->SO_TP_SIZE){
      transPool[sizePool].sender    = tr.msg.sender;
      transPool[sizePool].receiver  = tr.msg.receiver;
      transPool[sizePool].quantita  = tr.msg.quantita;
      transPool[sizePool].reward    = tr.msg.reward;
      transPool[sizePool].timestamp = tr.msg.timestamp;
      sizePool++;

    }else{
      switch (tr.msg.hops)
      {        
        case 0:
          msgsnd(master_queue, &tr, sizeof(TRequest),IPC_NOWAIT);
          kill(getppid(), SIGINT);
          break;

        default:
          if(tr.msg.hops==NOHOPS)
            {tr.msg.hops = C->SO_HOPS;}
          else
            {tr.msg.hops--;}
          nodeF = rand()%C->SO_FRIENDS_NUM;
          msgsnd(queue_Friends[nodeF], &tr, sizeof(tr), IPC_NOWAIT);
          break;
      }
    }
  }
}

/*::: PROCESSARE LE TRANSAZIONI :::*/
void processTransaction()
{
  int j;
  struct timespec timestamp;
  float pay_value=0;    
  /*::: CONTROLLO SE LA TRANSACTION POOL HA I BLOCCHI NECESSARI :::*/
  if( sizePool >= (SO_BLOCK_SIZE-1) ){
    /*::: REWARDS FOR NODE :::*/
    for( j=0; j<(SO_BLOCK_SIZE-1); j++){
      pay_value += transPool[j].reward;
    }
    clock_gettime( CLOCK_REALTIME , &timestamp);
    
    /*::: SIMULAZIONE ELABORAZIONE BLOCCO :::*/
    sleepTime();
    
    /*::: SEMAFORI :::*/
    sem_wait(nodes_sem);
    *counter_nodes+=1;
    if(*counter_nodes == 1){
      sem_wait(user_sem);
    }
    sem_free(nodes_sem);

        sem_wait(sem_writ);

        /*::: scrivolibro():::*/
    if(*index_mastro < (SO_BLOCK_SIZE*SO_REGISTRY_SIZE)){
      for( j=0; j<(SO_BLOCK_SIZE-1); j++){
          libro_mastro[*index_mastro].sender    = transPool[j].sender;
          libro_mastro[*index_mastro].receiver  = transPool[j].receiver;
          libro_mastro[*index_mastro].quantita  = transPool[j].quantita;
          libro_mastro[*index_mastro].timestamp = transPool[j].timestamp;
          libro_mastro[*index_mastro].reward    = transPool[j].reward;
          *index_mastro+=1;
          }

      libro_mastro[*index_mastro].sender    = NODEPAY;
      libro_mastro[*index_mastro].receiver  = id;
      libro_mastro[*index_mastro].quantita  = pay_value;
      libro_mastro[*index_mastro].timestamp = timestamp;
      *index_mastro+=1;
    }else
    {
      kill( getppid(), SIGUSR2);
    }

        sem_free(sem_writ);

    /*::: SEMAFORO :::*/
    sem_wait(nodes_sem);
    *counter_nodes-=1;
    if(*counter_nodes == 0){
      sem_free(user_sem);
    }
    sem_free(nodes_sem);


    /*::: translare tutto :::*/
    /*::: shifttrpool*/
    for( j=0; j< (sizePool-(SO_BLOCK_SIZE-1)); j++){
      transPool[j].sender     = transPool[j+SO_BLOCK_SIZE-1].sender;
      transPool[j].receiver   = transPool[j+SO_BLOCK_SIZE-1].receiver;
      transPool[j].reward     = transPool[j+SO_BLOCK_SIZE-1].reward;
      transPool[j].quantita   = transPool[j+SO_BLOCK_SIZE-1].quantita;
      transPool[j].timestamp  = transPool[j+SO_BLOCK_SIZE-1].timestamp;
    }
    sizePool -= (SO_BLOCK_SIZE-1);
    count++;
  }
  if(count>2 && sizePool > 0){
    count = 0;
    send_to_friend();
  }  
}

/*::: INVIO AD UN AMICO RANDOMICO :::*/
void send_to_friend(){
  int nodeF,j;
  TRequest tr;
  if( sizePool > 0 ){
    tr.msg.sender     = transPool[0].sender;
    tr.msg.receiver   = transPool[0].receiver;
    tr.msg.quantita   = transPool[0].quantita;
    tr.msg.reward     = transPool[0].reward;
    tr.msg.timestamp  = transPool[0].timestamp;
    tr.msg.hops       = NOHOPS;

    nodeF = rand()%C->SO_FRIENDS_NUM;
    msgsnd(queue_Friends[nodeF], &tr, sizeof(tr), IPC_NOWAIT);
    
    for( j=0; j< (sizePool-1); j++){
      transPool[j].sender     = transPool[j+1].sender;
      transPool[j].receiver   = transPool[j+1].receiver;
      transPool[j].reward     = transPool[j+1].reward;
      transPool[j].quantita   = transPool[j+1].quantita;
      transPool[j].timestamp  = transPool[j+1].timestamp;
    }
    sizePool -= 1;
  }
}

/*::: SLEEP TIME SIMULA PROCESS TRANS :::*/
void sleepTime()
{
  long time_sleep;
  struct timespec request, remaining;

  time_sleep = random()%(   C->SO_MAX_TRANS_PROC_NSEC - 
                            C->SO_MIN_TRANS_PROC_NSEC   ) + 
                              C->SO_MIN_TRANS_PROC_NSEC;
  remaining.tv_sec   = time_sleep/1000000000L;
  remaining.tv_nsec  = time_sleep%1000000000L;
  do{
    request.tv_nsec = remaining.tv_nsec;
    request.tv_sec  = remaining.tv_sec; 
    nanosleep(&request, &remaining);
  }while(!compareTimespec(request,remaining));
}

/*::: INVIO DELLE TRANSAZIONI NON PROCESSATE :::*/
void send_trpool(){
  Msgarr trpool;
  trpool.id=id+1;
  trpool.msg=sizePool;
  msgsnd(trnodes_queue, &trpool, sizeof(trpool),IPC_NOWAIT);
}

/*::: SEGNALI :::*/
void handler(int sig)
{
  switch (sig) {
    case SIGINT:
      break;
    case SIGQUIT:
      /*::: FINE SIMULAZIONE NODI :::*/
      send_trpool();
      freeResources();
      exit(EXIT_SUCCESS);
      break;
    case SIGALRM:
      break;
    case SIGUSR1:
      break;
    case SIGTSTP:
      break;
    case SIGUSR2:
      break;
  }
} 
