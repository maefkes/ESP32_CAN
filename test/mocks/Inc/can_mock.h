/**************************************************************************
can_mock.h
 Created on: Feb 11, 2026
     Author: M. Schermutzki

*************************************************************************/
#ifndef CAN_MOCK_H
#define CAN_MOCK_H

#ifdef __cplusplus
extern "C" 
{
#endif
/*** includes ***********************************************************/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "generic_hardware_interface.h"
/*** local constants ****************************************************/
/*** macros *************************************************************/
/*** definitions ********************************************************/
typedef struct can_s
{

    uint32_t id;      
    uint8_t  data[8]; 
    uint8_t  dlc;    

    uint32_t baudrate;
    bool used;
    bool opened;

    bool newData;
    can_frame_t lastReceivedFrame; 
} can_t;
/*** functions **********************************************************/

hal_status_t can_read(can_t* handle, can_frame_t* frame);
hal_status_t can_write(can_t* handle, const void* data, uint16_t count);
hal_status_t can_open(can_t* can, uint32_t baudrate);
hal_status_t can_close(can_t* can);
void canMockPushResponse(can_t* handle, const can_frame_t* frame);
can_t* can_new();
void can_deinit(void);
void can_init();

#ifdef __cplusplus
}
#endif
#endif /* CAN_MOCK_H */

