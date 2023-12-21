#ifndef _NODE_H
#define _NODE_H

#include "header.h"

/*::: RESOURCES TIME :::*/
void initResources();
void freeResources();

/*::: CONTROLLA SE R È GIA PRESENTE */
int checkFriends(int ,int);

/*::: AGGIORNA LA POOL :::*/
void updatePoolT();

/*::: PROCESSARE LE TRANSAZIONI :::*/
void processTransaction();

/*::: INVIO AD UN AMICO RANDOMICO :::*/
void send_to_friend();

/*::: SLEEP TIME SIMULA PROCESS TRANS :::*/
void sleepTime();

/*::: INVIO DELLE TRANSAZIONI NON PROCESSATE :::*/
void send_trpool();

/*::: SEGNALI :::*/
void handler();

#endif