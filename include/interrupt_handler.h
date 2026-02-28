/**************************************************************************
interrupt_handler.h
 Created on: Feb 28, 2026
     Author: M. Schermutzki
*************************************************************************/
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

#ifdef __cplusplus
extern "C" 
{
#endif
/*** includes ***********************************************************/
#include <stdint.h>
#include <stddef.h>
/*** local constants ****************************************************/
/*** macros *************************************************************/
/*** definitions ********************************************************/
/*** functions **********************************************************/
void interrupt_handler_leaveCritical(void);
void interrupt_handler_enterCritical(void);
void interrupt_handler_init(void);

#ifdef __cplusplus
}
#endif
#endif /* MODULE_NAME_H */

