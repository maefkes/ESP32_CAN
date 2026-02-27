/**************************************************************************
application.c
 Created on: 22.02.2026 (Ported to ESP32 TWAI)
 Author: M. Schermutzki 
***************************************************************************/
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "Arduino.h"        
#include "driver/twai.h"    
#include "application.hpp"
#include "bg_task.h"
#include "bms_communication.h"
#include "generic_hardware_interface.h"

/*** local constants ******************************************************/
static char const* C_MODULE_NAME = "Application";

// Pins für ESP32 (Anpassbar)
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

/*** local variables ******************************************************/
static bool _initialized = false;
static hardware_interface_t _bmsInit1;
static bms_com_t* _bms1 = NULL;
static uint32_t _bms1Id = 0x1FFFC;

/*** prototypes ***********************************************************/
static void _cyclic(void);
static void _setupBmsCom(void); 
hal_status_t _can_write(void* handle, void* data, uint8_t length);
hal_status_t _can_read(void* handle, void* data);

/*=============================== PRIVATE ==========================================*/

static void _cyclic(void)
{
    assert(_initialized);
    assert(_bms1);
    bms_communication_cyclic(_bms1);
}

static void _setupBmsCom(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) 
    {
        twai_start();
    }

    _bmsInit1.halHandle = NULL; // not needed on ESP32 (global handle)
    _bmsInit1.comRead = (com_read_t)_can_read;
    _bmsInit1.comWrite = (com_write_t)_can_write;
    _bmsInit1.comOpen = NULL;
    _bmsInit1.comClose = NULL;

    _bms1 = bms_communication_new(&_bmsInit1, _bms1Id);
}

/***************************************************************************
 * ESP32 Implementierung von can_write
 **************************************************************************/
hal_status_t _can_write(void* handle, void* data, uint8_t length)
{
    can_frame_t* frame = (can_frame_t*)data;
    twai_message_t tx_msg = {0}; 

    tx_msg.identifier = (frame->id & 0x1FFFFFFF); 
    tx_msg.extd = 1;               
    tx_msg.rtr = 0;
    tx_msg.data_length_code = frame->length;
    
    for (uint8_t i = 0; i < frame->length; i++) 
    {
        tx_msg.data[i] = frame->data[i];
    }

    if (twai_transmit(&tx_msg, pdMS_TO_TICKS(10)) == ESP_OK) 
    {
        return E_HAL_STATUS_OK;
    }
    return E_HAL_STATUS_ERROR;
}

/***************************************************************************
 * 
 **************************************************************************/
hal_status_t _can_read(void* handle, void* data)
{
    can_frame_t* target = (can_frame_t*)data;
    twai_message_t rx_msg;

    if (twai_receive(&rx_msg, 0) == ESP_OK) 
    {
        target->id = rx_msg.identifier;
        target->length = rx_msg.data_length_code;
        
        for (int i = 0; i < rx_msg.data_length_code; i++) {
            target->data[i] = rx_msg.data[i];
        }
        return E_HAL_STATUS_OK;
    }
    return E_HAL_STATUS_BUSY;
}

/*=============================== PUBLIC ===========================================*/

void app_init(void)
{
    if(!_initialized)
    {
        bg_task_init();
        bg_task_add(_cyclic, C_MODULE_NAME, E_BG_TASK_PRIO_LOW);
        bms_communication_init();

        _setupBmsCom();
        _initialized = true;
    }
}