/**************************************************************************
generic_hardware_interface.h
 *  Created on: Feb 11, 2026
 *      Author: M. Schermutzki
*************************************************************************/
#ifndef GENERIC_HARDWARE_INTERFACE_H
#define GENERIC_HARDWARE_INTERFACE_H

#ifdef __cplusplus
extern "C" 
{
#endif
/*** includes ***********************************************************/
#include <stdint.h>
#include <stddef.h>
/*** local constants ****************************************************/
/*** definitions ********************************************************/
typedef enum
{
    E_HAL_STATUS_OK         =   0,
    E_HAL_STATUS_BUSY       =   1,
    E_HAL_STATUS_ERROR      =   2,
    E_HAL_STATUS_TIMEOUT    =   3
} hal_status_t;

typedef struct 
{
    uint32_t id;
    uint8_t  length;
    uint8_t  data[8];
} can_frame_t;

// function pointer for hardware intependend communication
typedef hal_status_t (*com_read_t)(void* handle, void* data);
typedef hal_status_t (*com_write_t)(void* handle, const void* data, uint16_t count); 
typedef hal_status_t (*com_open_t)(void* handle, uint32_t baudrate);
typedef hal_status_t (*com_close_t)(void* handle);

typedef struct 
{
    void* halHandle;    // pointer to the specific hardware instance   
    com_read_t comRead;
    com_write_t comWrite;
    com_open_t comOpen;
    com_close_t comClose;
} hardware_interface_t;

#ifdef __cplusplus
}
#endif
#endif /* GENERIC_HARDWARE_INTERFACE_H */

