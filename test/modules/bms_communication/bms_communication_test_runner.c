/******************************************************************************************************************
 * bms_communication_test_runner.c
 *  Created on: Feb 11, 2026
 *      Author: M. Schermutzki
 *****************************************************************************************************************/
 #include "unity_fixture.h"
/*** TEST CASE RUNNER ********************************************************************************************/
TEST_GROUP_RUNNER(BmsCommunication) 
{
    RUN_TEST_CASE(BmsCommunication, instancesWereCreatedSuccessfully);
    //RUN_TEST_CASE(BmsCommunication, canCommunicationOpenedSuccessfully);
    RUN_TEST_CASE(BmsCommunication, checkFirstRequestFrame);
    RUN_TEST_CASE(BmsCommunication, statemachineRunsCompleteCycle);
    RUN_TEST_CASE(BmsCommunication, readingTotalVoltage);
    RUN_TEST_CASE(BmsCommunication, cellVoltagesAreSavedIntoCorrectArraySlots);
    RUN_TEST_CASE(BmsCommunication, getCellVoltageBoundaryCheck);
}

/*** MANUALLY TEST LIST ******************************************************************************************/
/***
 * [x]  istances are successfully initialized
 * [x]   module opens the interface to can
 * [x]   send a valid test command example: reference table 0x01 (command total voltage)
 * [x]   statemachine runs a complete cycle
 * [x]   reading total voltage
 * []   reading total current
 * []   reading full charge capacity
 * []   reading remainign capacity
 * []   readinmg SOC
 * []   reading SOH
 * []   reading cycles
 * []   reading max cell voltage
 * []   reading min cell voltage
 * []   reading cell differential voltage
 * []   reading max temperature
 * []   reading lowest temperature
 * []   reading cell temperature difference
 * []   reading system status
 * []   reading voltage of each cell (16 pieces)
 * []   module closes the interface to can
 */
