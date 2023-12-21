#ifndef _HEADER_H_
#define _HEADER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <time.h>
#include <math.h>
#include <limits.h>

/*:::ERROR SECTION:::*/					 
#define ERROR   if (errno) {fprintf(stderr,		\
					  "<%s> AT LINE:%5d\t:PID=%5d:\tError=%5d (%s)\n", \
					  __FILE__,			\
					  __LINE__,			\
					  getpid(),			\
					  errno,			\
					  strerror(errno));\
						exit(EXIT_FAILURE);}

#define SO_BLOCK_SIZE       5       /*DIMENSIONE MAX DEL BLOCCO*/
#define SO_REGISTRY_SIZE    1000    /*DIMENSIONE MAX DEL REGISTRO, LIBRO MASTRO*/

#define NODEPAY   -1
#define NOHOPS    -1

/* CAUSE CHIUSURA SIMULAZIONE */
enum end_simulation { END_T, FULL_M, DEAD_T, CTRL_C};

/*::: LIBRO MASTRO :::*/
typedef struct _libromastro{
  int     sender;
  int     receiver;
  float   quantita;
  float   reward;
  struct timespec timestamp;
} Mastro;

/*::: DATI :::*/
typedef struct _configuration{
  int   SO_USERS_NUM;
  int   SO_NODES_NUM;
  int   SO_BUDGET_INIT;
  int   SO_REWARD;
  long  SO_MIN_TRANS_GEN_NSEC;
  long  SO_MAX_TRANS_GEN_NSEC;
  int   SO_RETRY; 
  int   SO_TP_SIZE;
  long  SO_MIN_TRANS_PROC_NSEC; 
  long  SO_MAX_TRANS_PROC_NSEC; 
  int   SO_SIM_SEC;
  int   SO_FRIENDS_NUM;
  int   SO_HOPS;
} conf;

/*::: CONTENUTO TRANSAZIONE :::*/
typedef struct _transazione{
  struct timespec timestamp;       /*clock_gettime(...) */
  int     sender;                  /*ID SENDER          */
  int     receiver;                /*ID RICEVITORE      */
  float   quantita;                /*SOLDI DA PAGARE    */
  float   reward;                  /*LA TASSA DEL NODO  */
  int     hops;                    /*I SOLTI POSSIBILI  */
} Transaction;

/*::: MESSAGGIO AL NODO, RICHIESTA DI UNA TRANSAZIONE :::*/
typedef struct _transazioneRequest{
  long id;
  Transaction msg;
} TRequest;

/*::: MESSAGGIO USATO SIA PER I NUOVI AMICI CHE PER TRANSAZIONI NON PROCESSATE*/
typedef struct _msgArrayInt{
  long id;
  int msg;
} Msgarr;

#endif