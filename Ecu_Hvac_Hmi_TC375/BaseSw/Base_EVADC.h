#ifndef BASE_EVADC_H_
#define BASE_EVADC_H_

#include "Ifx_Types.h"

void init_EVADC(void);
void read_EVADC_Values(uint16 *res1, uint16 *res2);

#endif
