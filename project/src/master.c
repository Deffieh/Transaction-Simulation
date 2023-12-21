#include "./headers/master.h"

#define Inside 1

conf C;                     /* CONFIGURAZIONE            */
Mastro *libro_mastro;       /* SHM LIBRO MASTRO          */

int 
  /* ID SHM */
  mastro_id,              /* ID SHM LIBRO MASTRO              */
  shm_ind,                /* ID SHM INDEX_MASTER              */
  conf_id,                /* ID SHM CONFIGURATION             */
  id_cu,                  /* ID SHM COUNTER_PROCESS_NODE      */
  iddead,                 /* ID SHM NUMERO USER MORTI         */

  /* QUEUE */
  *queue_transaction,     /* ARRAY QUEUE COMUNICAZIONE CON I NODI */
  master_queue,           /* QUEUE TRANSAZIONIPER NUOVI NODI      */
  trnodes_queue,          /* QUEUE NUMERO TRANSAZIONI NON PROC    */

  /* SEMAPHORI */
  start,                  /* SEMAPHORO DI START PER SINCRONIZZARE I PROCESSI           */
  nodes_sem,              /* SEMAFORO PER I NODI ACCESSO ZONA CRITICA INDEX E LIBRO    */
  user_sem,               /* SEMAFORO PER I USER ACCESSO ZONA CRITICA INDEX E LIBRO    */
  sem_writ,               /* SEMAFORO PER SCRITTURA ACCESSO ZONA CRITICA INDEX E LIBRO */
  sem_dead,               /* SEMAFORO PER ZONA CRITICA USER MORTI                      */

  /* VARIABILI MASTER*/
  myindex,                /* INDEX LETTURA MASTER DEL LIBRO                                        */
  reason_end,             /* CAUSA CHIUSURA DELLA SIMULAZIONE                                      */
  tempo,                  /* TEMPO CONTROLLO DURATA SIMULAZIONE                                    */
  chain_friend,           /* GLI AMICI CHE CHE DEVONO AGGIUNGERE IL NUOVO NODO TRA GLI AMICI       */
  pid_master,             /* SOLUZIONE PER ELIMINARE I PROCESSI NODO NON ANCORA GENERATI COME TALI */

  /* SHM */
  *deaduser,              /* USER MORTI DURANTE LA SIMULAZIONE              */
  *index_mastro,          /* PUNTATORE ALLA DIMENSIONE DEL LIBRO MASTRO     */
  *counter_nodes;         /* CONTATORI NODI ACCESSO ALLA SCRITTURA          */


float *arrayBilancio;       /* ARRAY BILANCIO USERS & NODES     */
float *user_bilancio;       /* ARRAY BILANCIO PER NUM USER >20  */
conf *configshm;            /* MEMORY CONFIGURATION             */
volatile int run = 1;       /* FOR RUNNING SIMULATION           */
struct msqid_ds _buff;      /* BUFFER PER VISUALLIZZARE N MESSG */



int main(int argc, char **argv) 
{
  int i;                            /* CONTATORE PER I VARI FOR                       */
  struct sigaction act;             /* STRUTTURA CHE DESCRIVE LE AZIONI PER I SEGNALI */
  struct timespec request,remaining;/* NECESSARI PER LO NANOSLEEP                     */

  /*::: GESTIONE SEGNALI :::*/
  memset(&act, 0, sizeof(act));
  act.sa_handler = handler;
  set_sig (&act);

  /*::: CONFIGURATION DA conf/data.conf :::*/
  setDataConf();
  printConfig();

  /*::: INIT RESOURCES FOR SIMULATION :::*/
  initResources();
  printf("<MATER> avvio nodi e gli user tra un secondo\n");
  usleep(1000000);
  
  /*::: RUN NODES & USERS :::*/
  runNodes_Users();

  printf("<MASTER> Sincronizzo i nodi e gli utenti\n");
  sem_wait(start);


  /*::: SIMULAZIONE :::*/
  tempo      = 0;
  reason_end = END_T;
  while(run)
  {
    remaining.tv_sec = 1;
    do{
      request.tv_nsec = remaining.tv_nsec;
      request.tv_sec  = remaining.tv_sec; 
      nanosleep(&request, &remaining);
    }while(!compareTimespec(request,remaining));
    tempo++;
    
    /*::: NUMERO DI PROCESSI UTENTE E NODO ATTIVI:::*/
    printf("\n\n\t\t\t[MASTER DICE]: secondi: [%2d]\tuser attivi : [%3d] su [%3d]\tnodi attivi: [%2d]\n\n\n",
                              tempo,C.SO_USERS_NUM - *deaduser, C.SO_USERS_NUM, C.SO_NODES_NUM);
    printbilancio(Inside);
    if(tempo==C.SO_SIM_SEC&&run!=0){
      raise(SIGALRM);
    }
  }
  
  printf("<MASTER> ENDING SIMULATION.. \n");
  while (wait(NULL) > 0) {}

  printEnd();
  freeResources();
  exit(EXIT_SUCCESS);
}

/*::: GESTIONE RISORSE :::*/
void initResources()
{
  int i;
  pid_master = getpid();

    /*:::: SHM SECTION ::::*/
  /*::: LIBRO MASTRO SHMD :::*/
  mastro_id = shmget( getpid(), 
                      SO_BLOCK_SIZE *SO_REGISTRY_SIZE *sizeof(Mastro), 
                      IPC_CREAT | 0666);
  ERROR
  libro_mastro = shmat( mastro_id, NULL, 0);
  ERROR

  /*::: INDICE LIBRO MASTRO SHMD :::*/
  shm_ind = shmget( getpid()+1, 
                      sizeof(int), 
                      IPC_CREAT | 0666);
  ERROR
  index_mastro = shmat( shm_ind, NULL, 0);
  ERROR

  /*::: DATI CONFIGURAZIONE CONDIVISI SHMD :::*/
  conf_id = shmget( getpid()+2, 
                      sizeof(conf), 
                      IPC_CREAT | 0666);
  ERROR
  configshm = shmat( conf_id, NULL, 0);
  ERROR

  /*::: CONTATORE NODI PER MODELLO LETTORI & SCRITTORI SHMD :::*/
  id_cu = shmget( getpid()+3, 
                      sizeof(int), 
                      IPC_CREAT | 0666);
  ERROR
  counter_nodes = shmat( id_cu, NULL, 0);
  ERROR
  /*::: CONTATORE USER MORTI :::*/
  iddead = shmget( getpid()+4, 
                      sizeof(int), 
                      IPC_CREAT | 0666);
  ERROR
      deaduser = shmat( iddead, NULL, 0);
      ERROR

  /*::: ARRAY PER STAMPA DEI BILANCI :::*/
  arrayBilancio = malloc( sizeof(double)*(C.SO_USERS_NUM+C.SO_NODES_NUM));
  user_bilancio = malloc( sizeof(double)*(C.SO_USERS_NUM+C.SO_NODES_NUM));

  for( i=0; i<C.SO_USERS_NUM; i++)
  {
    arrayBilancio[i]=C.SO_BUDGET_INIT;
  }
  for( i=C.SO_USERS_NUM; i<C.SO_USERS_NUM+C.SO_NODES_NUM; i++)
  {
    arrayBilancio[i]=0;
  }

    /*:::: QUEUE SECTION ::::*/
  /*::: ALLOCAZIONE DELLE QUEUE :::*/
  queue_transaction = malloc(sizeof(int) * C.SO_NODES_NUM);
  for( i = 0; i < C.SO_NODES_NUM; i++)
  {
    queue_transaction[i] = msgget( getpid()+i, IPC_CREAT|0666);
    ERROR
  }
  master_queue = msgget( getpid()-1, IPC_CREAT|0666);
    ERROR
  trnodes_queue = msgget( getpid()-2, IPC_CREAT|0666);
    ERROR

    /*:::: SEM SECTION ::::*/
  /*::: SEMAFORO DI SINCRONIZZAZIONE :::*/
  start = semget( getpid(), 1, IPC_CREAT|0666);
  ERROR
  semctl(start, 0, SETVAL, 1);

  /*::: NODI SEM :::*/
  nodes_sem = semget( getpid()+1, 1, IPC_CREAT|0666);
  ERROR
  semctl(nodes_sem, 0, SETVAL, 1);
  
  /*::: USERS SEM :::*/
  user_sem = semget( getpid()+2, 1, IPC_CREAT|0666);
  ERROR
  semctl(user_sem, 0, SETVAL, 1);

  /*::: SCRITTORI SEM QUANDO I NODI SCRIVONO :::*/
  sem_writ = semget( getpid()+3, 1, IPC_CREAT|0666);
  ERROR
  semctl(sem_writ, 0, SETVAL, 1);
  
  /*::: SEMAFORO PER SHM DEL NUMERO DEGLI USER :::*/
  sem_dead = semget( getpid()+4, 1, IPC_CREAT|0666);
  ERROR
  semctl(sem_dead, 0, SETVAL, 1);

  /*::: INIT VARIABLES  :::*/
  *configshm      = C;
  *counter_nodes  = 0;
  *deaduser       = 0;
  myindex         = 0;
  *index_mastro   = 0;
  chain_friend    = 0;
}

void freeResources()
{
  int i;
  /*::: CHIUSURA SHMD :::*/
  shmdt( libro_mastro);
  shmctl( mastro_id, IPC_RMID, NULL);

  shmdt( index_mastro);
  shmctl( shm_ind, IPC_RMID, NULL);
  
  shmdt( configshm);
  shmctl( conf_id, IPC_RMID, NULL);
  
  shmdt( counter_nodes);
  shmctl( id_cu, IPC_RMID, NULL);

  shmdt( deaduser);
  shmctl( iddead, IPC_RMID, NULL);
  
  /*::: CHIUSURA MEMORIA ALLOCATA :::*/
  free(arrayBilancio);
  free(user_bilancio);
  
  /*::: CHIUSURA SEM :::*/
  semctl( start, 0, IPC_RMID);
  semctl( nodes_sem, 0, IPC_RMID);
  semctl( user_sem, 0, IPC_RMID);
  semctl( sem_writ, 0, IPC_RMID);
  semctl( sem_dead, 0, IPC_RMID);

  /*::: CHIUSURA DELLE QUEUE :::*/
  msgctl(master_queue, IPC_RMID, NULL);
  msgctl(trnodes_queue, IPC_RMID, NULL);
  
  for( i=0; i<C.SO_NODES_NUM; i++)
  {
    msgctl( queue_transaction[i], IPC_RMID, NULL);
  }
  free(queue_transaction);
}
  

/*::: NODI & USER CREAZIONE ::*/
void runNodes_Users()
{
  int i;
  /* RUNNING NODES */
  for ( i = 0; i < C.SO_NODES_NUM; i++)
  {
    switch (fork())
    {
      case -1:
        ERROR
      break;
      case 0:
        execNode(i);ERROR
      break;   
    }
  }
  /* RUNNING USERS */
  for ( i = 0; i < C.SO_USERS_NUM; i++)
  {
    switch (fork())
    {
      case -1:
        ERROR
      break;
      case 0:
        execUser(i);
        exit(EXIT_FAILURE);ERROR
      break;   
    }
  }
}

void execNode( int id){
  char buff[5], *args[3];
  sprintf(buff, "%d", id);
  args[0] = "node";
  args[1] = buff;
  args[2] = NULL;
  execve("../SO_PROG/bin/node", args, NULL);
}

void execUser( int id){
  char buff[5], *args[3];
  sprintf(buff, "%d", id);
  args[0] = "user";
  args[1] = buff;
  args[2] = NULL;
  execve("../SO_PROG/bin/user", args, NULL);
}

/*::: CONFIGURAZIONE :::*/
void setDataConf()
{
  FILE *f;
  int check;

  f = fopen("../SO_PROG/conf/data.conf", "r");
  /*::: IF FILE EXIST SET DATA ELSE PUT SOME DEFAULT SETTING :::*/
  if(f){
    check = upgradeStruct(f);
    if(check < 13){
      printf(":::PLS CHECK IF YOU MISS SOME DATA FROM data.conf!!!\n\n");
      exit(EXIT_FAILURE);
    }
  }
  else{
    printf(":::PLS CREATE A FILE DATA.CONF WITH THE CORRECT CONFIGURATION!!!\n\n");
    exit(EXIT_SUCCESS);    
  }

  fclose(f);
}

int upgradeStruct(FILE *f)
{
  char *s;
  int i=0;
  s = malloc( sizeof(char) * 25);
  while (fscanf(f, "%s", s) == 1)
  {
    if(s[0] == '#')
      do{}while (fgetc(f)!='\n');
    else{
      if(strcmp(s,"SO_USERS_NUM") == 0){
        fscanf(f,"%d",&C.SO_USERS_NUM);}
      if(strcmp(s,"SO_NODES_NUM") == 0){
        fscanf(f,"%d",&C.SO_NODES_NUM);}
      if(strcmp(s,"SO_BUDGET_INIT") == 0){
        fscanf(f,"%d",&C.SO_BUDGET_INIT);}
      if(strcmp(s,"SO_REWARD") == 0){
        fscanf(f,"%d",&C.SO_REWARD);}
      if(strcmp(s,"SO_MIN_TRANS_GEN_NSEC") == 0){
        fscanf(f,"%ld",&C.SO_MIN_TRANS_GEN_NSEC);}
      if(strcmp(s,"SO_MAX_TRANS_GEN_NSEC") == 0){
        fscanf(f,"%ld",&C.SO_MAX_TRANS_GEN_NSEC);}
      if(strcmp(s,"SO_RETRY") == 0){
        fscanf(f,"%d",&C.SO_RETRY);}
      if(strcmp(s,"SO_TP_SIZE") == 0){
        fscanf(f,"%d",&C.SO_TP_SIZE);}
      if(strcmp(s,"SO_MIN_TRANS_PROC_NSEC") == 0){
        fscanf(f,"%ld",&C.SO_MIN_TRANS_PROC_NSEC);}
      if(strcmp(s,"SO_MAX_TRANS_PROC_NSEC") == 0){
        fscanf(f,"%ld",&C.SO_MAX_TRANS_PROC_NSEC);}
      if(strcmp(s,"SO_SIM_SEC") == 0){
        fscanf(f,"%d",&C.SO_SIM_SEC);}
      if(strcmp(s,"SO_FRIENDS_NUM") == 0){
        fscanf(f,"%d",&C.SO_FRIENDS_NUM);}
      if(strcmp(s,"SO_HOPS") == 0){
        fscanf(f,"%d",&C.SO_HOPS);}
      i++;
    }
  }
  free(s);
  return i;
}


/*::: DIVERSE STAMPE NECCESSARIE :::*/
void printEnd()
{
  Msgarr trpool;                    /* MESSAGGI CONTENENTI TRANSAZIONI NON PROCESSATE */
  /*::: CAUSA DI END SIMULATION :::*/
  printf("\t\tCAUSA CHIUSURA:.. ");
  switch (reason_end)
  {
    case END_T:
      printf("TRASCORSI : %d secondi\n", C.SO_SIM_SEC);
      break;
    case FULL_M:
      printf("LA CAPACITÀ DEL LIBRO MASTRO ESAURITA %d/%d blocchi\n",SO_REGISTRY_SIZE,SO_REGISTRY_SIZE);
      break;
    case DEAD_T:
      printf("TUTTI I PROCESSI UTENTE SONO TERMINATI\n");
      break;
    case CTRL_C:
      printf("SEGNALE CTRL-Z CHIUSURA DELLA SIMULAZIONE\n");
      break;
  }
  /*::: BILANCIO USERS NODES :::*/
  printbilancio(!Inside);
  printf("\t:::: USER MORTI PREMATURI :%3d\n",*deaduser);
  printf("\t:::: BLOCCHI OCCUPATI = [%d/%d]\n",myindex/SO_BLOCK_SIZE , SO_REGISTRY_SIZE);
  printf("\n");
  /*::: TRANSAZIONI NON PROCESSATE :::*/
  while(msgrcv(trnodes_queue, &trpool, sizeof(trpool), 0, IPC_NOWAIT)>0)
  {
    printf("Nodo[%ld] Transazioni non processate = %d\n",trpool.id-1,trpool.msg);
  }
}

void printConfig(){
  printf(""\
  "-----------------------------------------------------------------------------------------------------------\n"\
  "|N_USERS|N_NODES|BUDGET|REWARDSG|MIN_TRANSG|MAX_TRANS|RETRY|TP_SIZE|MIN_TRANSP|MAX_TRANSP|SIM|FRIENDS|HOPS|\n"\
  "-----------------------------------------------------------------------------------------------------------\n"\
  "|%7d|%7d|%6d|%8d|%10ld|%9ld|%5d|%7d|%10ld|%10ld|%3d|%7d|%4d|\n"\
  "-----------------------------------------------------------------------------------------------------------\n"\
  ,C.SO_USERS_NUM
  ,C.SO_NODES_NUM
  ,C.SO_BUDGET_INIT
  ,C.SO_REWARD
  ,C.SO_MIN_TRANS_GEN_NSEC
  ,C.SO_MAX_TRANS_GEN_NSEC
  ,C.SO_RETRY
  ,C.SO_TP_SIZE
  ,C.SO_MIN_TRANS_PROC_NSEC
  ,C.SO_MAX_TRANS_PROC_NSEC
  ,C.SO_SIM_SEC
  ,C.SO_FRIENDS_NUM
  ,C.SO_HOPS);
}

void printbilancio(int isInside){

  int i ;
  updatebilancio();
  if(C.SO_USERS_NUM < 15 || !isInside ){
    for(i=0 ; i< C.SO_USERS_NUM; i++)
    {
      printf("utente[%d]:%f\n",i,arrayBilancio[i]);
    }
  }
  else{
    stampoUser();
  }
  printf("\n");
  for( i=C.SO_USERS_NUM; i< C.SO_USERS_NUM+C.SO_NODES_NUM; i++ )
  {
    printf("nodo[%d]:%f\n",i-C.SO_USERS_NUM,arrayBilancio[i]);
  }
  printf("\n");
}

void stampoUser()
{
  int i;
  for(i=0;i<C.SO_USERS_NUM;i++){
    user_bilancio[i] = arrayBilancio[i];
  }
  quickSort(user_bilancio,0,C.SO_USERS_NUM-1);
  for(i=0; i<5; i++){
    printf("utente = %f\n", user_bilancio[i]);
  }
  for(i=C.SO_USERS_NUM-5; i<C.SO_USERS_NUM; i++){
    printf("utente = %f\n", user_bilancio[i]);
  }
}

/*::: ORDINA L'ARRAY PER PRENDERE I MIGLIORI E I PEGGIORI BUDGET :::*/
void swap(float *a, float *b) {
  float t = *a;
  *a = *b;
  *b = t;
}
int partition(float array[], int low, int high) {
  float pivot = array[high];
  int i = (low - 1), j;
  for ( j = low; j < high; j++) {
    if (array[j] <= pivot) {
      i++;
      swap(&array[i], &array[j]);
    }
  }
  swap(&array[i + 1], &array[high]);
  return (i + 1);
}
void quickSort(float array[], int low, int high) {
  if (low < high) {
    int pi = partition(array, low, high);
    quickSort(array, low, pi - 1);
    quickSort(array, pi + 1, high);
  }
}

/*::: AGGIORNA IL BILANCIO :::*/
void updatebilancio(){
  for( ; myindex<*index_mastro; myindex++)
  {
    if(libro_mastro[myindex].sender != NODEPAY)
    {
      arrayBilancio[libro_mastro[myindex].sender]   -= ( libro_mastro[myindex].quantita + libro_mastro[myindex].reward );
      arrayBilancio[libro_mastro[myindex].receiver] += libro_mastro[myindex].quantita;
    }
    else{
      arrayBilancio[libro_mastro[myindex].receiver + C.SO_USERS_NUM] += libro_mastro[myindex].quantita;
    }
  }
}

/*::: CREAZIONE NUOVO NODO :::*/
void newNode(){
  int i;
  TRequest tr;  /* PER I MESSAGGI DELLE TRANSAZIONI */
  Msgarr prova; /* */
   
  queue_transaction = realloc(queue_transaction ,sizeof(int) * (C.SO_NODES_NUM+1));
  queue_transaction[C.SO_NODES_NUM] = msgget( getpid()+C.SO_NODES_NUM, IPC_CREAT|0666);
  C.SO_NODES_NUM++;
  configshm->SO_NODES_NUM++;
  arrayBilancio = realloc( arrayBilancio, sizeof(double)*(C.SO_USERS_NUM+C.SO_NODES_NUM));
  arrayBilancio[C.SO_USERS_NUM + C.SO_NODES_NUM - 1] = 0;
  while(msgrcv(master_queue, &tr, sizeof(TRequest), 0, IPC_NOWAIT) > 0)
  {
    msgsnd(queue_transaction[C.SO_NODES_NUM-1], &tr, sizeof(tr), IPC_NOWAIT);
  }
  kill(0, SIGUSR1);/*SEGNALE PER GLI USER*/
  switch (fork())
  {
    case -1:
      ERROR
      break;
    case 0:
      execNode(C.SO_NODES_NUM-1);
      exit(EXIT_FAILURE);
      break;
    default:
      for(i=0; i<C.SO_FRIENDS_NUM; i++,chain_friend++)
      {
        if(chain_friend>=C.SO_NODES_NUM-1){
          chain_friend=0;
        }
        prova.id     = 2;
        prova.msg = C.SO_NODES_NUM-1;
        msgsnd(queue_transaction[chain_friend],&prova, sizeof(prova),IPC_NOWAIT);
      }
      break;
  }
}

/*::: SEGNAL GESTION :::*/
void handler(int sig)
{
  switch (sig) {
    case SIGINT:
      msgctl(master_queue, IPC_STAT, &_buff);
      if(_buff.msg_qnum){
        newNode();}
      break;
    case SIGALRM:
      kill(0,SIGQUIT);
      run =0;
      break;
    case SIGQUIT:
      if(getpid() != pid_master)
      {
        exit(EXIT_SUCCESS);
      }
      break;
    case SIGUSR1:
      if( *deaduser == C.SO_USERS_NUM){
        kill(0,SIGQUIT);
        reason_end = DEAD_T;
        run = 0;
      }
      break;
    case SIGUSR2:
      kill(0,SIGQUIT);
      reason_end = FULL_M;
      run = 0;
      break;
    case SIGTSTP:
      kill(getppid(),SIGCONT);
      kill(0,SIGQUIT);
      reason_end = CTRL_C;
      run=0;
      break;
  }
} 



