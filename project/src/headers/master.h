#ifndef _MASTER_H
#define _MASTER_H

#include "header.h"

/*::: GESTIONE RISORSE :::*/
void initResources();
void freeResources();

/*::: NODI & USER CREAZIONE ::*/
void runNodes_Users();
void execNode( int);
void execUser( int);

/*::: CONFIGURAZIONE :::*/
void setDataConf();
int upgradeStruct(FILE*);

/*::: DIVERSE STAMPE NECCESSARIE :::*/
void printEnd();
void printConfig();
void printbilancio(int);
void stampoUser();


/*::: ORDINA L'ARRAY PER PRENDERE I MIGLIORI E I PEGGIORI BUDGET :::*/
void swap(float*, float*);
int partition(float array[], int low, int high);
void quickSort(float array[], int low, int high);

/*::: AGGIORNA IL BILANCIO :::*/
void updatebilancio();

/*::: CREAZIONE NUOVO NODO :::*/
void newNode();

/*::: SEGNAL GESTION :::*/
void handler(int);

#endif