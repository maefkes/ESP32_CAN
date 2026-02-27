/**************************************************************************
can_mock.c
 Created on: Feb 11, 2026
     Author: M. Schermutzki
***************************************************************************/
/*** includes *************************************************************/
#include <assert.h>
#include <string.h>
#include "can_mock.h"
#include "generic_hardware_interface.h"
/*** structures ***********************************************************/
/*** local constants ******************************************************/
#define C_MAX_INSTANCES (2)
/*** macros ***************************************************************/
/*** local variables ******************************************************/
static bool _initialized = false;
static can_t _instances[C_MAX_INSTANCES];
/*** prototypes ***********************************************************/
/*** interrupt service routines *******************************************/
/*** functions ************************************************************/
hal_status_t can_write(can_t* handle, const void* data, uint16_t count)
{
    assert(handle);
    assert(data);

    // 1. Sicherheitsschalter (count wird hier als Anzahl der Frames interpretiert)
    if (handle == NULL || data == NULL || count == 0) 
    {
        return E_HAL_STATUS_ERROR;
    }

    // 2. Cast von void* auf den tatsächlichen Struktur-Typ
    // Damit "sieht" der Mock die Felder .id, .length und .data
    const can_frame_t* sent_msg = (const can_frame_t*)data;

    // 3. WICHTIG: Die ID aus der Struktur in den Mock-Speicher retten
    // Ohne diese Zeile könntest du im Test nicht prüfen, ob (bms->slaveID << 12) stimmt!
    handle->id = sent_msg->id;
    
    // 4. DLC und Daten kopieren
    handle->dlc = (sent_msg->length > 8) ? 8 : sent_msg->length;

    for (uint8_t i = 0; i < handle->dlc; i++)
    {
        handle->data[i] = sent_msg->data[i];
    }

    return E_HAL_STATUS_OK;
}


/**
 * Diese Funktion ist NUR für den Test gedacht.
 * Sie simuliert, dass ein Frame vom CAN-Bus empfangen wurde.
 */
void canMockPushResponse(can_t* handle, const can_frame_t* frame)
{
    assert(handle);
    assert(frame);

    memcpy(&handle->lastReceivedFrame, frame, sizeof(can_frame_t));
    handle->newData = true; 
}

hal_status_t can_read(can_t* handle, can_frame_t* frame)
{
    assert(handle);
    if (handle->newData) 
    {
        handle->newData = false;
        if (frame != NULL) {
            memcpy(frame, &handle->lastReceivedFrame, sizeof(can_frame_t));
        }
        return E_HAL_STATUS_OK;
    }
    return E_HAL_STATUS_BUSY; 
}

hal_status_t can_open(can_t* can, uint32_t baudrate)
{
    can->baudrate = baudrate;
    can->opened = true;
    return E_HAL_STATUS_OK;
}

hal_status_t can_close(can_t* can)
{
    return E_HAL_STATUS_OK;
}

can_t* can_new()
{
    assert(_initialized);

    for(uint8_t i = 0; i < C_MAX_INSTANCES; i++)
    {
        if(!_instances[i].used)
        {
            can_t* retval = &_instances[i];
            _instances[i].opened = true;
            retval->used = true;
            return retval;
        }
    }
    return NULL;
}

void can_deinit(void)
{
    if(_initialized)
    {
        for(uint8_t i = 0; i < C_MAX_INSTANCES; i++)
        {
            _instances[i].used = false;
            _instances[i].id = 0;
            _instances[i].dlc = 0;
            _instances[i].opened = false;
            for(int j=0; j < 8; j++) _instances[i].data[j] = 0;
        }
        _initialized = false;
    }
}

void can_init(void)
{
    if(!_initialized)
    {
        for(uint8_t i = 0; i < C_MAX_INSTANCES; i++)
        {
            _instances[i].used = false;
        }
        _initialized = true;
    }
}

