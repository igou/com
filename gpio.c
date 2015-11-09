/********************************************************************
 *                Copyright Simcom(shanghai)co. Ltd.                   *
 *---------------------------------------------------------------------
 * FileName      :   app_demo_gpio.c
 * version       :   0.10
 * Description   :   
 * Authors       :   fangshengchang
 * Notes         :
 *---------------------------------------------------------------------
 *
 *    HISTORY OF CHANGES
 *---------------------------------------------------------------------
 *0.10  2012-09-24, fangshengchang, create originally.
 *0.20  2013-03-26, maobin, modify the PIN definition, to adapt to the SIM800W and SIM800V
 *
 *--------------------------------------------------------------------
 ********************************************************************/
/********************************************************************
 * Include Files
 ********************************************************************/
#include <stdio.h>
#include <string.h>
#include "eat_modem.h"
#include "eat_interface.h"
#include "eat_periphery.h"
#include "eat_uart.h"
#include "inc.h"

/********************************************************************
 * Macros
 ********************************************************************/
    /**************UART function define begin**************/
#define eat_uart_app EAT_UART_1
#define app_uart_at EAT_UART_NULL
#define app_uart_debug EAT_UART_2

/********************************************************************
* Types
 ********************************************************************/

/********************************************************************
 * Extern Variables (Extern /Global)
 ********************************************************************/
 
/********************************************************************
 * Local Variables:  STATIC
 ********************************************************************/


/********************************************************************
 * External Functions declaration
 ********************************************************************/


/********************************************************************
 * Local Function declaration
 ********************************************************************/

/********************************************************************
 * Local Function
 ********************************************************************/ 

void Gpio_init(void )
{
        CHARGING_SET_MOD();  
        INCOMING_SET_MOD(); 
        NETWORK_SET_MOD();   
        DIAL1_SET_MOD() ;    
        DIAL2_SET_MOD();    
        CHARGING_GPIO_INIT();
        INCOMING_GPIO_INIT();        
        NETWORK_GPIO_INIT(); 
        CHARGING_INVALID();
        INCOMING_VALID();
        NETWORK_INVALID();
        DIAL1_GPIO_INIT();  
#if 0
        /*Note: pull up firstly,then set interrupt level*/
        DIAL2_EINT_PULL_UP();      
        DIAL2_EINT_HIGH();
#else
        /*Note: pull down firstly,then set interrupt level*/
        DIAL2_EINT_PULL_DOWN();
        if(EAT_GPIO_LEVEL_LOW == READ_DIAL_2())
            DIAL2_EINT_HIGH();
        else
            DIAL2_EINT_LOW();
#endif
 }

