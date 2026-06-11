#ifndef __ENCODER_H
#define __ENCODER_H

#include "main.h"

void Encoder_Init(uint16_t stepCnt);
void Encoder_ClearCount(void);
void Encoder_SetCount(uint16_t cnt);
int16_t Encoder_GetCount(void);

#endif
