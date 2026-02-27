/**************************************************************************
bms_communication.h
 Created on: Feb 11, 2026
     Author: M. Schermutzki
*************************************************************************/
#ifndef BMS_COMMUNICATION_H
#define BMS_COMMUNICATION_H

#ifdef __cplusplus
extern "C" 
{
#endif
/*** includes ***********************************************************/
#include <stdint.h>
#include <stddef.h>
#include "generic_hardware_interface.h"
/*** local constants ****************************************************/
/*** macros *************************************************************/
/*** definitions ********************************************************/
typedef enum 
{
    E_BMS_STATUS_IDLE,   
    E_BMS_STATUS_BUSY,    
    E_BMS_STATUS_ERROR    
} bms_status_t;
typedef struct bms_com_s bms_com_t;
/*** functions **********************************************************/
uint32_t bms_communication_getTotalVoltage(bms_com_t* bms);
uint16_t bms_communication_getCellVoltage(bms_com_t* bms, uint8_t cellIndex);

bms_status_t bms_communication_cyclic(bms_com_t* bms);

bms_com_t* bms_communication_new(const hardware_interface_t* hw, uint32_t slaveID);
void bms_communication_deinit(void);
void bms_communication_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BMS_COMMUNICATION_H */

