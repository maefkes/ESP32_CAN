/**************************************************************************
bg_task.c
 Created on: Feb 20, 2026
     Author: M. Schermutzki
***************************************************************************/
/*** includes *************************************************************/
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include "bg_task.h"
/*** local constants ******************************************************/
#define C_BG_TASK_MAX   (16)
/*** structures ***********************************************************/
typedef struct
{
    bg_task_t task;
    char const* name;
    bg_task_prio_t prio;
} bg_task_list_t;
/*** local constants ******************************************************/
/*** macros ***************************************************************/
/*** local variables ******************************************************/
static bool _initialized = false;
static bg_task_list_t _tasks[C_BG_TASK_MAX];
static uint8_t _taskCount = 0;
/*** prototypes ***********************************************************/
/*** interrupt service routines *******************************************/
/*** functions ************************************************************/
/***************************************************************************
 * This function 
 **************************************************************************/
 void bg_task_cyclic(void)
 {
    assert(_initialized);

    for(uint8_t i = 0; i < _taskCount; i++)
    {
        if(_tasks[i].task != NULL)
        {
             _tasks[i].task();
        }
    }
 }
/***************************************************************************
 * This function 
 * @todo LOCK INTERRUPTS
 **************************************************************************/
void bg_task_add(bg_task_t task, char const* name, bg_task_prio_t prio)
{
    assert(_initialized);

    uint8_t index = 0;

    if(_taskCount < C_BG_TASK_MAX)
    {
        index = _taskCount;

        for(; index > 0; index--)
        {
            if(_tasks[index - 1].prio < prio)
            {
                _tasks[index] = _tasks[index - 1]; 
            }
            else
            {
                break;
            }
        }
        _tasks[index].task = task;
        _tasks[index].name = name;
        _tasks[index].prio = prio;
        _taskCount++;
    }
}
/***************************************************************************
 * This function 
 **************************************************************************/
 void bg_task_init(void)
 {
    if(!_initialized)
    {
        for(uint8_t i = 0; i < C_BG_TASK_MAX; i++)
        {
            _tasks[i].task = NULL;
        }
        _initialized = true;
    }
 }


