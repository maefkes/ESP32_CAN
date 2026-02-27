/******************************************************************************************************************
 * bms_communication_test.c
 *  Created on: Feb 11, 2026
 *      Author: M. Schermutzki
 *****************************************************************************************************************/
 #include "unity_fixture.h"
 #include "bms_communication.h"
 #include "can_mock.h"
/*** includes ****************************************************************************************************/
/*================================================ DEFINE TEST GROUP ============================================*/
/*** define ******************************************************************************************************/
TEST_GROUP(BmsCommunication);
/*** local variables *********************************************************************************************/
static hardware_interface_t _bmsInit1, _bmsInit2;
static bms_com_t* _bms1 = NULL; 
static bms_com_t* _bms2 = NULL;
static uint32_t _bms1Id = 0x1FFFC;
static uint32_t _bms2Id = 0x1FFFD;

/*** setup *******************************************************************************************************/
TEST_SETUP(BmsCommunication) 
{
    can_init();
    bms_communication_init();

    _bmsInit1.halHandle = (void*)can_new();
    _bmsInit1.comRead = (com_read_t)can_read;
    _bmsInit1.comWrite = (com_write_t)can_write;
    _bmsInit1.comOpen = (com_open_t)can_open;
    _bmsInit1.comClose = (com_close_t)can_close;

    _bmsInit2.halHandle = (void*)can_new();
    _bmsInit2.comRead = (com_read_t)can_read;
    _bmsInit2.comWrite = (com_write_t)can_write;
    _bmsInit2.comOpen = (com_open_t)can_open;
    _bmsInit2.comClose = (com_close_t)can_close;
    
    _bms1 = bms_communication_new(&_bmsInit1, _bms1Id);
    _bms2 = bms_communication_new(&_bmsInit2,  _bms2Id);
}
/*** tear down ***************************************************************************************************/
TEST_TEAR_DOWN(BmsCommunication) 
{
    can_deinit();
    bms_communication_deinit();
}
/*==================================================== TEST LIST ================================================*/
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
TEST(BmsCommunication, instancesWereCreatedSuccessfully) 
{
    TEST_ASSERT_NOT_NULL(_bms1);
    TEST_ASSERT_NOT_NULL(_bms2);
}
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
/*TEST(BmsCommunication, canCommunicationOpenedSuccessfully)
{
    can_t* mockData = (can_t*)_bmsInit1.halHandle;

    bms_communication_start(_bms1);
    TEST_ASSERT_EQUAL_UINT32(500000, mockData->baudrate);
    TEST_ASSERT_TRUE(mockData->opened);

}*/
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
TEST(BmsCommunication, checkFirstRequestFrame)
{
    can_t* mockData = (can_t*)_bmsInit1.halHandle; 

    bms_communication_cyclic(_bms1);

    TEST_ASSERT_TRUE(mockData->opened);                     // sent message successfull?
    TEST_ASSERT_EQUAL_HEX32(0x1FFFC100, mockData->id);      // message to be send has the expected format?
    TEST_ASSERT_EQUAL(8, mockData->dlc);                    // has dlc the expected length (look at datasheet, DLC = 8)
    
    for(uint8_t i = 0; i < 8; i++) 
    {
        TEST_ASSERT_EQUAL_UINT8(0x00, mockData->data[i]);   // 8 bit data has to be zero
    }
}
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
TEST(BmsCommunication, statemachineRunsCompleteCycle)
{
    can_t* mockData = (can_t*)_bmsInit1.halHandle; 

    // 1. Request senden
    bms_communication_cyclic(_bms1);

    // 2. Response mit NEUEM Wert simulieren (z.B. 4000mV -> 0x0FA0)
    can_frame_t response = { .id = 0x7FFF0100, .length = 8, .data = {0xA0, 0x0F} }; 
    canMockPushResponse(mockData, &response); 
    
    // 3. Cyclic aufrufen. State-Machine verarbeitet 0x0FA0 
    // und springt sofort zum nächsten Request (BUSY)
    bms_status_t status = bms_communication_cyclic(_bms1);
    
    TEST_ASSERT_EQUAL(E_BMS_STATUS_BUSY, status); 
    
    // 4. Prüfen, ob die 4000mV im "Lager" angekommen sind
    TEST_ASSERT_EQUAL_UINT32(4000, bms_communication_getTotalVoltage(_bms1));
}
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
TEST(BmsCommunication, readingTotalVoltage)
{
    uint32_t voltage = bms_communication_getTotalVoltage(_bms1);
    TEST_ASSERT_EQUAL_UINT32(4000, voltage);
}
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
TEST(BmsCommunication, cellVoltagesAreSavedIntoCorrectArraySlots)
{
    can_t* mockData = (can_t*)_bmsInit1.halHandle; 
    uint32_t expectedIdCells5to8 = 0x7FFF010A; // Die ID laut Protokoll

    // Wir lassen die Statemachine laufen, bis sie die ID für Zellen 5-8 sendet
    // Wir begrenzen die Versuche auf 20, damit der Test bei Fehlern nicht endlos läuft
    bool found = false;
    for(int i = 0; i < 20; i++) 
    {
        bms_communication_cyclic(_bms1); // Führt Request aus
        
        if(mockData->id == expectedIdCells5to8) 
        {
            // Gefunden! Jetzt Antwort füttern
            can_frame_t resp = 
            {
                .id = expectedIdCells5to8,
                .length = 8,
                .data = {0x11, 0x0F, 0x22, 0x0F, 0x33, 0x0F, 0x44, 0x0F} 
            };
            canMockPushResponse(mockData, &resp);
            bms_communication_cyclic(_bms1); // Antwort verarbeiten
            found = true;
            break;
        }
        
        // Wenn es nicht die richtige ID war, schieben wir eine Dummy-Antwort rein,
        // damit die Statemachine zum nächsten Befehl weitergeht
        can_frame_t dummy = {0};
        canMockPushResponse(mockData, &dummy);
        bms_communication_cyclic(_bms1); 
    }

    TEST_ASSERT_TRUE_MESSAGE(found, "ID für Zellen 5-8 wurde nie gesendet!");
    
    // Check: Zelle 6 (Array Index 5) -> 0x0F22 = 3874
    TEST_ASSERT_EQUAL_UINT16(3874, bms_communication_getCellVoltage(_bms1, 5));
}
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
TEST(BmsCommunication, getCellVoltageBoundaryCheck)
{
    uint16_t voltage = 0;

    voltage = bms_communication_getCellVoltage(_bms1, -1);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, voltage);

    voltage = bms_communication_getCellVoltage(_bms1, 16);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, voltage);

    voltage = bms_communication_getCellVoltage(_bms1, 5);
    TEST_ASSERT_EQUAL_UINT16(3874, voltage);
}
/*****************************************************************************************************************
* This test
******************************************************************************************************************/
/*TEST(BmsCommunication, canCommunicationClosesSuccessfully)
{
    can_t* mockData = (can_t*)_bmsInit1.halHandle;
    TEST_ASSERT_FALSE(mockData->comPortOpen);
}*/
//ifdef test
//#define STATIC
//#else
//#define STATIC static 
//#endif
