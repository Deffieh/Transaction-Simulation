#ifndef _USER_H
#define _USER_H

#include "header.h"

/*::: GETIONE RISORSE :::*/
void initResources();
void freeResources();

/*::: AGGIORNA IL BILANCIO :::*/
void updateBilancio();

/* GENERA UN NUMERO CASUALE TRA MIN E MAX */
float randfrom(float, float);

/*::: INVIO UNA TRANSAZIONE :::*/
void send_transiction();

/*::: SLEEP TIME SIMULA GEN TRANS :::*/
void sleepTime();

/*::: SEGNALI :::*/
void handler();

#endif