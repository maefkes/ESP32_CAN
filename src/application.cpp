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
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4
#define LOG_INTERVAL_MS 2000 
/*** local variables ******************************************************/
static bool _initialized = false;
static hardware_interface_t _bmsInit1;
static bms_com_t* _bms1 = NULL;
static uint32_t _bms1Id = 0x1FFFC; 
static uint32_t _lastLogTime = 0;
/*** prototypes ***********************************************************/
static void _cyclic(void);
static void _setupBmsCom(void); 
static void _logBmsData(void);
hal_status_t _can_write(void* handle, void* data, uint8_t length);
hal_status_t _can_read(void* handle, void* data);
/*=============================== PRIVATE ==========================================*/
/***************************************************************************
 * This function
 **************************************************************************/
static void _cyclic(void)
{
    assert(_initialized);
    assert(_bms1);
    
    bms_status_t status = bms_communication_cyclic(_bms1);

    if(status == E_BMS_STATUS_IDLE)
    {
        uint32_t now = millis();
        if (now - _lastLogTime >= LOG_INTERVAL_MS) 
        {
            _logBmsData();
            _lastLogTime = now;
        }
    }
}
/***************************************************************************
 * This function
 **************************************************************************/
static void _logBmsData(void)
{
    Serial.println("\n--- BMS DATA REPORT (ESP32) ---");
    
    
    Serial.print("Gesamtspannung: "); Serial.print(bms_communication_getTotalVoltage(_bms1)); Serial.println(" mV"); 
    Serial.print("Gesamtstrom:    "); Serial.print(bms_communication_getTotalCurrent(_bms1)); Serial.println(" mA"); 

    Serial.print("Volle Kap.:     "); Serial.print(bms_communication_getFullCapacity(_bms1)); Serial.println(" mAH"); 
    Serial.print("Restkapazität:  "); Serial.print(bms_communication_getRemainingCapacity(_bms1)); Serial.println(" mAH"); 

    Serial.print("Temp Max:       "); Serial.print(bms_communication_getMaxTemperature(_bms1)); Serial.println(" °C"); 
    Serial.print("Temp Min:       "); Serial.print(bms_communication_getMinTemperature(_bms1)); Serial.println(" °C"); 

    Serial.print("Alarm Status:   A:0x"); Serial.print(bms_communication_getAlarmStatusA(_bms1), HEX); 
    Serial.print(" B:0x"); Serial.println(bms_communication_getAlarmStatusB(_bms1), HEX); 
    Serial.print("Protection:     A:0x"); Serial.print(bms_communication_getProtectA(_bms1), HEX); 
    Serial.print(" B:0x"); Serial.println(bms_communication_getProtectB(_bms1), HEX); 

    Serial.println("Zellspannungen:");
    for (uint8_t i = 0; i < 16; i++) 
    {
        uint16_t volt = bms_communication_getCellVoltage(_bms1, i); 
        Serial.print("  C"); Serial.print(i + 1); Serial.print(": ");
        Serial.print(volt); Serial.print(" mV");
        
        if ((i + 1) % 4 == 0) Serial.println(); else Serial.print(" | ");
    }
    Serial.println("-------------------------------");
}
/***************************************************************************
 * This function
 **************************************************************************/
static void _setupBmsCom(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) 
    {
        twai_start();
    }

    _bmsInit1.halHandle = NULL; 
    _bmsInit1.comRead = (com_read_t)_can_read;
    _bmsInit1.comWrite = (com_write_t)_can_write;
    _bmsInit1.comOpen = NULL;
    _bmsInit1.comClose = NULL;

    _bms1 = bms_communication_new(&_bmsInit1, _bms1Id);
}

/***************************************************************************
 * This function
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
 * This function
 **************************************************************************/
hal_status_t _can_read(void* handle, void* data)
{
    can_frame_t* target = (can_frame_t*)data;
    twai_message_t rx_msg;


    if (twai_receive(&rx_msg, pdMS_TO_TICKS(20)) == ESP_OK) 
    {
        if (!(rx_msg.rtr)) 
        {
            target->id = rx_msg.identifier;
            target->length = rx_msg.data_length_code;
            
            for (int i = 0; i < rx_msg.data_length_code; i++) {
                target->data[i] = rx_msg.data[i];
            }
            return E_HAL_STATUS_OK;
        }
    }
    return E_HAL_STATUS_BUSY; 
}

/*=============================== PUBLIC ===========================================*/
/***************************************************************************
 * This function
 **************************************************************************/
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