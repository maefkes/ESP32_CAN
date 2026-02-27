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
    if (bms->sendCount == E_BMS_CMD1_TOTAL_VALUES) 
    {
        bms->data.totalVoltage = (uint32_t)((bms->rxFrame.data[1] << 8) | bms->rxFrame.data[0]);
    }
    else if (bms->sendCount >= E_BMS_CMD1_CELL_VLTG_1_TO_4 && bms->sendCount <= E_BMS_CMD1_CELL_VLTG_13_TO_16)
    {
        uint8_t startIndex = (bms->sendCount - E_BMS_CMD1_CELL_VLTG_1_TO_4) * 4;

        for (uint8_t i = 0; i < 4; i++) 
        {
            uint8_t lowByte = bms->rxFrame.data[i * 2];
            uint8_t highByte = bms->rxFrame.data[(i * 2) + 1];
            bms->data.cellVoltage[startIndex + i] = (uint16_t)((highByte << 8) | lowByte);
        }
    }
}
/***************************************************************************
 * This function
 **************************************************************************/
static bms_state_t _canStatemachine(bms_com_t* bms)
{
    //bool stateChanged = true;

    // while loop makes statemachine faster
    //while(stateChanged)
    {
        //stateChanged = false; 

        switch(bms->state)
        {
            case E_BMS_STATE_IDLE:
                if(bms->sendCount < E_BMS_CMD_COUNT) 
                {
                    if(_sendCanFrame(bms, _command[bms->sendCount]) == E_HAL_STATUS_OK) 
                    {
                        bms->state = E_BMS_STATE_WAIT_FOR_RESPONSE;
                    }
                }
                break;

            case E_BMS_STATE_WAIT_FOR_RESPONSE:
            {
            	can_frame_t frameBuffer; 

                if(bms->read(bms->handle, &frameBuffer) == E_HAL_STATUS_OK)
                {
                	
                    bms->rxFrame = frameBuffer;
                    bms->state = E_BMS_STATE_EXTRACT_DATA;
                    //stateChanged = true; 
                 }
            }
            break;

            case E_BMS_STATE_EXTRACT_DATA:
                _decodeFrame(bms); 
                bms->state = E_BMS_STATE_NEW_DATA_AVALAIBLE;
               // stateChanged = true; 
                break;

            case E_BMS_STATE_NEW_DATA_AVALAIBLE:
                bms->sendCount++;
                bms->state = E_BMS_STATE_IDLE;
                //stateChanged = true; 
                break;

            case E_BMS_STATE_ERROR:
            	break;

            default:
            	break;
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
uint16_t bms_communication_getCellVoltage(bms_com_t* bms, uint8_t cellIndex)
{
    assert(bms);
    
    if (cellIndex >= 16)
    {
        return 0xFFFF; 
    }

    return bms->data.cellVoltage[cellIndex];
}
/***************************************************************************
 * This function
 **************************************************************************/
uint32_t bms_communication_getTotalVoltage(bms_com_t* bms) 
{
    assert(bms);
    return bms->data.totalVoltage;
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

        for (uint8_t i = 0; i < C_BMS_COM_INSTANCES_MAX; i++)
        {
            _instances[i].used = false;
        }
        _initialized = true;
    }
}
