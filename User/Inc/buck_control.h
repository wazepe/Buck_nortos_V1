#ifndef __BUCK_CONTROL_H
#define __BUCK_CONTROL_H

#include "main.h"

void Buck_Init(void);
void Buck_DisableOutput(void);
void Buck_SetOutputVoltage(float targetVoltage);
uint8_t Buck_ReadVoltageCurrent(float *voltage, float *current);

#endif
