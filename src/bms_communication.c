/**************************************************************************
bms_communication.c
 Created on: Feb 11, 2026
     Author: M. Schermutzki
***************************************************************************/
/*** includes *************************************************************/
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "bms_communication.h"
#include "generic_hardware_interface.h"
#include "interrupt_handler.h"
/*** local constants ******************************************************/
#define C_BMS_COM_INSTANCES_MAX (2)
static const uint8_t C_DATA_REQUEST_BITS    =   14u;
static const uint8_t C_DLC_BYTES            =   8u;
/*** definitions***********************************************************/
typedef enum
{
    E_BMS_CMD1_TOTAL_VALUES,
    E_BMS_CMD1_CAPACITY,
    E_BMS_CMD1_SOC_SOH,
    E_BMS_CMD1_CELL_VLTG,
    E_BMS_CMD1_ACCU_STATUS,
    E_BMS_CMD1_ALARM_STATUS,
    E_BMS_CMD1_PROTECT_B,
    E_BMS_CMD1_CHASSIS_ID,
    E_BMS_CMD1_BATTERY_STATUS,
    E_BMS_CMD1_CELL_VLTG_1_TO_4,
    E_BMS_CMD1_CELL_VLTG_5_TO_8,
    E_BMS_CMD1_CELL_VLTG_9_TO_12,
    E_BMS_CMD1_CELL_VLTG_13_TO_16,  
    E_BMS_CMD2_TEMPERATURE_DATA1,
    E_BMS_CMD2_TEMPERATURE_DATA2,
    E_BMS_CMD2_TEMEPRATURE_DATA3,
    E_BMS_CMD2_CHASSIS_VLTG,
    E_BMS_CMD2_CHASSIS_TEMPERATURE, 
    /*=============================*/
    E_BMS_CMD_COUNT
} bms_cmd_list_t;

typedef enum
{
    E_BMS_STATE_IDLE = 0,
    E_BMS_STATE_WAIT_FOR_RESPONSE,
    E_BMS_STATE_EXTRACT_DATA,
    E_BMS_STATE_NEW_DATA_AVALAIBLE,
    E_BMS_STATE_ERROR
} bms_state_t;

/*** structures ***********************************************************/
typedef struct 
{
    uint32_t totalVoltage;
    int32_t totalCurrent;

    uint32_t fullChargeCapacity;
    uint32_t remainingCapacity;

    uint16_t maxCellVoltage;
    uint16_t minCellVoltage;
    uint16_t cellDiffVoltage;

    uint16_t maxTemperature;
    uint16_t lowestTempertaure;
    uint16_t cellTempDifference;

    uint16_t alarmStatusA;
    uint16_t alarmStatusB;
    uint16_t protectA;
    uint16_t protectB;

    uint16_t bmsFailure;

    uint16_t numberOfCells;
    uint16_t cellVoltage[16];
}bms_data_t;

struct bms_com_s
{
    void* handle;
    bms_data_t data;
    com_read_t read;
    com_write_t write;
    com_open_t open;
    com_close_t close;
    uint32_t slaveID;
    bms_state_t state;
    uint8_t sendCount;
    can_frame_t rxFrame;
    bool used;
};

typedef struct 
{
    uint8_t cmdFctTable;
    uint8_t cmdID;
} bms_command_t;

/*** macros ***************************************************************/
/*** local variables ******************************************************/
static bool _initialized = false;
static bms_com_t _instances[C_BMS_COM_INSTANCES_MAX];
static const bms_command_t _command[E_BMS_CMD_COUNT] =
{
    [E_BMS_CMD1_TOTAL_VALUES]           =   {.cmdFctTable = 0x01, .cmdID = 0x00},               
    [E_BMS_CMD1_CAPACITY]               =   {.cmdFctTable = 0x01, .cmdID = 0x01},     
    [E_BMS_CMD1_SOC_SOH]                =   {.cmdFctTable = 0x01, .cmdID = 0x02},
    [E_BMS_CMD1_CELL_VLTG]              =   {.cmdFctTable = 0x01, .cmdID = 0x03},
    [E_BMS_CMD1_ACCU_STATUS]            =   {.cmdFctTable = 0x01, .cmdID = 0x04},   
    [E_BMS_CMD1_ALARM_STATUS]           =   {.cmdFctTable = 0x01, .cmdID = 0x05},   
    [E_BMS_CMD1_PROTECT_B]              =   {.cmdFctTable = 0x01, .cmdID = 0x06},
    [E_BMS_CMD1_CHASSIS_ID]             =   {.cmdFctTable = 0x01, .cmdID = 0x07},
    [E_BMS_CMD1_BATTERY_STATUS]         =   {.cmdFctTable = 0x01, .cmdID = 0x08},   
    [E_BMS_CMD1_CELL_VLTG_1_TO_4]       =   {.cmdFctTable = 0x01, .cmdID = 0x09},       
    [E_BMS_CMD1_CELL_VLTG_5_TO_8]       =   {.cmdFctTable = 0x01, .cmdID = 0x0A},       
    [E_BMS_CMD1_CELL_VLTG_9_TO_12]      =   {.cmdFctTable = 0x01, .cmdID = 0x0B},       
    [E_BMS_CMD1_CELL_VLTG_13_TO_16]     =   {.cmdFctTable = 0x01, .cmdID = 0x0C},           
    [E_BMS_CMD2_TEMPERATURE_DATA1]      =   {.cmdFctTable = 0x02, .cmdID = 0x00},       
    [E_BMS_CMD2_TEMPERATURE_DATA2]      =   {.cmdFctTable = 0x02, .cmdID = 0x01},       
    [E_BMS_CMD2_TEMEPRATURE_DATA3]      =   {.cmdFctTable = 0x02, .cmdID = 0x02},       
    [E_BMS_CMD2_CHASSIS_VLTG]           =   {.cmdFctTable = 0x02, .cmdID = 0x03},   
    [E_BMS_CMD2_CHASSIS_TEMPERATURE]    =   {.cmdFctTable = 0x02, .cmdID = 0x04}                
};
/*** prototypes ***********************************************************/
static void _decodeFrame(bms_com_t* bms);
static bms_state_t _canStatemachine(bms_com_t* bms);
static hal_status_t _sendCanFrame(bms_com_t* bms, bms_command_t cmd);
static void _buildCanFrame(bms_com_t* bms, bms_command_t cmd, can_frame_t* frame);
/*** interrupt service routines *******************************************/
/*** functions ************************************************************/
/*=============================== PRIVATE ==========================================*/
static void _decodeFrame(bms_com_t* bms)
{

    uint8_t fct = _command[bms->sendCount].cmdFctTable;
    uint8_t idx = _command[bms->sendCount].cmdID;
    const uint8_t* d = bms->rxFrame.data;

    if (fct == 0x01) 
    {
        switch (idx)
        {
            case 0x00: // Total Voltage (u32) & Current (i32)
                bms->data.totalVoltage = (uint32_t)((d[3] << 24) | (d[2] << 16) | (d[1] << 8) | d[0]); 
                bms->data.totalCurrent = (int32_t)((d[7] << 24) | (d[6] << 16) | (d[5] << 8) | d[4]); 
                break;

            case 0x01: // Full & Remaining Capacity (u32)
                bms->data.fullChargeCapacity = (uint32_t)((d[3] << 24) | (d[2] << 16) | (d[1] << 8) | d[0]);
                bms->data.remainingCapacity = (uint32_t)((d[7] << 24) | (d[6] << 16) | (d[5] << 8) | d[4]);
                break;

            case 0x03: // Cell Limits & Diff 
                bms->data.maxCellVoltage  = (uint16_t)((d[1] << 8) | d[0]);
                bms->data.minCellVoltage  = (uint16_t)((d[3] << 8) | d[2]);
                bms->data.cellDiffVoltage = (uint16_t)((d[5] << 8) | d[4]);
                break;

            case 0x05: // Alarm Status A & B 
                bms->data.alarmStatusA = (uint16_t)((d[1] << 8) | d[0]);
                bms->data.alarmStatusB = (uint16_t)((d[3] << 8) | d[2]);
                break;

            case 0x06: // Protect A & B 
                bms->data.protectA = (uint16_t)((d[1] << 8) | d[0]);
                bms->data.protectB = (uint16_t)((d[3] << 8) | d[2]);
                break;

            case 0x09: case 0x0A: case 0x0B: case 0x0C: 
            {
                uint8_t startCell = (idx - 0x09) * 4;
                for (int i = 0; i < 4; i++) {
                    bms->data.cellVoltage[startCell + i] = (uint16_t)((d[i*2+1] << 8) | d[i*2]);
                }
                break;
            }
        }
    }
    else if (fct == 0x02) 
    {
        #define K_TO_C(val) ((val - 2731) / 10)

        if (idx == 0x00) 
        { 
            uint16_t rawMax = (uint16_t)((d[1] << 8) | d[0]);
            bms->data.maxTemperature = K_TO_C(rawMax); 
        }
        else if (idx == 0x01) 
        { 
            uint16_t rawMin = (uint16_t)((d[1] << 8) | d[0]); 
            bms->data.lowestTempertaure = K_TO_C(rawMin);
        }
    }
}
/***************************************************************************
 * This function
 **************************************************************************/
static bms_state_t _canStatemachine(bms_com_t* bms)
{
    bool stateChanged = true;
    static uint32_t timeoutRetry = 0; 

    while(stateChanged)
    {
        stateChanged = false; 

        switch(bms->state)
        {
            case E_BMS_STATE_IDLE:
                if(bms->sendCount < E_BMS_CMD_COUNT) 
                {
                    if(_sendCanFrame(bms, _command[bms->sendCount]) == E_HAL_STATUS_OK) 
                    {
                        bms->state = E_BMS_STATE_WAIT_FOR_RESPONSE;
                        timeoutRetry = 0; 
                    }
                }
                else 
                {
                    bms->sendCount = 0; 
                }
                break;

            case E_BMS_STATE_WAIT_FOR_RESPONSE:
            {
                can_frame_t frameBuffer; 
                if(bms->read(bms->handle, &frameBuffer) == E_HAL_STATUS_OK)
                {
                    uint32_t expectedId = ((uint32_t)bms->slaveID << 12) | 
                                          ((uint32_t)_command[bms->sendCount].cmdFctTable << 8) | 
                                           (uint32_t)_command[bms->sendCount].cmdID;

                    if (frameBuffer.id == expectedId) 
                    {
                        bms->rxFrame = frameBuffer;
                        bms->state = E_BMS_STATE_EXTRACT_DATA;
                        stateChanged = true; 
                    }
                }
                else 
                {
                    timeoutRetry++;
                    if(timeoutRetry > 100) {
                        bms->sendCount++; 
                        bms->state = E_BMS_STATE_IDLE;
                        stateChanged = true;
                    }
                }
            }
            break;

            case E_BMS_STATE_EXTRACT_DATA:
                _decodeFrame(bms); 
                bms->state = E_BMS_STATE_NEW_DATA_AVALAIBLE;
                stateChanged = true; 
                break;

            case E_BMS_STATE_NEW_DATA_AVALAIBLE:
                bms->sendCount++;
                bms->state = E_BMS_STATE_IDLE;
                //stateChanged = true; 
                break;

            default: break;
        }
    }
    return bms->state;
}
/***************************************************************************
 * This function
 **************************************************************************/
static void _buildCanFrame(bms_com_t* bms, bms_command_t cmd, can_frame_t* frame)
{

    frame->id = ((uint32_t)bms->slaveID << 12) | 
                ((uint32_t)cmd.cmdFctTable << 8) | 
                 (uint32_t)cmd.cmdID;
    
    frame->length = C_DLC_BYTES; 

    for(uint8_t i = 0; i < C_DLC_BYTES; i++) 
    {
        frame->data[i] = 0x00;
    }
}
/***************************************************************************
 * This function
 **************************************************************************/
static hal_status_t _sendCanFrame(bms_com_t* bms, bms_command_t cmd)
{
    can_frame_t frame; 
    _buildCanFrame(bms, cmd, &frame); 
    
    return bms->write(bms->handle, &frame, 1);
}
/*=============================== PUBLIC ===========================================*/
/***************************************************************************
 * This function
 **************************************************************************/
uint32_t bms_communication_getTotalVoltage(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint32_t retval = bms->data.totalVoltage; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
int32_t bms_communication_getTotalCurrent(bms_com_t* bms) 
{
    assert(bms);
    interrupt_handler_enterCritical();
    int32_t retval = bms->data.totalCurrent; 
    interrupt_handler_leaveCritical();

    return retval;
}
/***************************************************************************
 * This function
 **************************************************************************/
uint32_t bms_communication_getFullCapacity(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint32_t retval = bms->data.fullChargeCapacity; 
    interrupt_handler_leaveCritical();
    
    return retval;
}
/***************************************************************************
 * This function
 **************************************************************************/
uint32_t bms_communication_getRemainingCapacity(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint32_t retval = bms->data.remainingCapacity; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
uint16_t bms_communication_getCellVoltage(bms_com_t* bms, uint8_t cellIndex) 
{
    assert(bms);

    if (cellIndex >= 16) return 0xFFFF; 

    interrupt_handler_enterCritical();
    uint16_t retval = bms->data.cellVoltage[cellIndex]; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
uint16_t bms_communication_getMaxCellVoltage(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint16_t retval = bms->data.maxCellVoltage; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
uint16_t bms_communication_getMinCellVoltage(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint16_t retval = bms->data.minCellVoltage; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
int16_t bms_communication_getMaxTemperature(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    int16_t retval =  (int16_t)bms->data.maxTemperature; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
int16_t bms_communication_getMinTemperature(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    int16_t retval = (int16_t)bms->data.lowestTempertaure; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
uint16_t bms_communication_getAlarmStatusA(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint16_t retval = bms->data.alarmStatusA; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
uint16_t bms_communication_getAlarmStatusB(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint16_t retval = bms->data.alarmStatusB; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
uint16_t bms_communication_getProtectA(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint16_t retval = bms->data.protectA; 
    interrupt_handler_leaveCritical();

    return retval;

}
/***************************************************************************
 * This function
 **************************************************************************/
uint16_t bms_communication_getProtectB(bms_com_t* bms) 
{
    assert(bms);

    interrupt_handler_enterCritical();
    uint16_t retval = bms->data.protectB; 
    interrupt_handler_leaveCritical();

    return retval;
}
/***************************************************************************
 * This function
 **************************************************************************/
bms_status_t bms_communication_cyclic(bms_com_t* bms)
{
    assert(bms);
    bms_state_t state = _canStatemachine(bms);

    if(state == E_BMS_STATE_IDLE)
    {
        return E_BMS_STATUS_IDLE;
    }
    else if(state == E_BMS_STATE_WAIT_FOR_RESPONSE || state == E_BMS_STATE_EXTRACT_DATA || state == E_BMS_STATE_NEW_DATA_AVALAIBLE)
    {
        return E_BMS_STATUS_BUSY;    
    }
    else 
    {
        return E_BMS_STATUS_ERROR;
    }
}
/***************************************************************************
 * This function
 **************************************************************************/
bms_com_t* bms_communication_new(const hardware_interface_t* hw, uint32_t slaveID)
{
    assert(_initialized);
    assert(hw);

    for(uint8_t i = 0; i < C_BMS_COM_INSTANCES_MAX; i++)
    {
        if(!_instances[i].used)
        {
            bms_com_t* retval = &_instances[i];
            retval->handle = hw->halHandle;
            retval->read = hw->comRead;
            retval->write = hw->comWrite;
            retval->open = hw->comOpen;
            retval->close = hw->comClose;
            retval->slaveID = slaveID;
            retval->state = E_BMS_STATE_IDLE;
            retval->sendCount = 0;
            retval->used = true;
            return retval;
        }
    }
    return NULL;
}
/***************************************************************************
 * This function
 **************************************************************************/
void bms_communication_deinit(void)
{
    if(_initialized)

    {
        for (uint8_t i = 0; i < C_BMS_COM_INSTANCES_MAX; i++)
        {
            _instances[i].handle = NULL;
            _instances[i].read = NULL;
            _instances[i].write = NULL;
            _instances[i].open = NULL;
            _instances[i].close = NULL;
            _instances[i].slaveID = 0;
            _instances[i].state = E_BMS_STATE_IDLE;
            _instances[i].used = false;  
        }
        _initialized = false;
    }
}
/***************************************************************************
 * This function
 **************************************************************************/
void bms_communication_init(void)
{
    if(!_initialized)
    {
        interrupt_handler_init();

        for (uint8_t i = 0; i < C_BMS_COM_INSTANCES_MAX; i++)
        {
            _instances[i].used = false;
        }
        _initialized = true;
    }
}
