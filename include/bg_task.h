/**************************************************************************
bg_task.h
 Created on: Feb 20, 2026
     Author: M. Schermutzki

*************************************************************************/
#ifndef BG_TASK_H
#define BG_TASK_H

#ifdef __cplusplus
extern "C" 
{
#endif
/*** includes ***********************************************************/
#include <stddef.h>
/*** local constants ****************************************************/
/*** macros *************************************************************/
/*** definitions ********************************************************/
typedef enum
{
    E_BG_TASK_PRIO_LOW,
    E_BG_TASK_PRIO_MID,
    E_BG_TASK_PRIO_HIGH
}bg_task_prio_t;
typedef void (*bg_task_t)(void);
/*** functions **********************************************************/
void bg_task_cyclic(void);
void bg_task_add(bg_task_t task, char const* name, bg_task_prio_t prio);
void bg_task_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BG_TASK_H */

