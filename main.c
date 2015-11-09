/********************************************************************
 *                Copyright Simcom(shanghai)co. Ltd.                   *
 *---------------------------------------------------------------------
 * FileName      :   app_demo_timer.c
 * version       :   0.10
 * Description   :   
 * Authors       :   maobin
 * Notes         :
 *---------------------------------------------------------------------
 *
 *    HISTORY OF CHANGES
 *---------------------------------------------------------------------
 *0.10  2012-09-24, maobin, create originally.
 *
 *--------------------------------------------------------------------
 * File Description
 * AT+CEAT=parma1,param2
 * param1 param2 
 *   1      1    Test millisecond level timer, Start timer1 timer2 timer3
 *   1      2    Stop timer1, timer2, timer3
 *
 *   2      1    Test microsecond level timer, Start gpt timer
 *   2      2    Stop gpt timer
 *   
 *   3      1    Get elapsed time from the boot to the present
 *   3      2    Get time interval with the last time
 *
 *   4      1    Test RTC, get current rtc value
 *   4      2    Set rtc value
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
#include "eat_uart.h"
#include "eat_timer.h" 
#include "eat_socket.h"
#include "eat_clib_define.h" //only in main.c
#include "app_at_cmd_envelope.h"
#include "inc.h"
/********************************************************************
 * Macros
 ********************************************************************/
#define EAT_UART_RX_BUF_LEN_MAX 2048
#define EAT_MEM_MAX_SIZE 10*1024//100*1024

/********************************************************************
 * Types
 ********************************************************************/
typedef void (*app_user_func)(void*);

/********************************************************************
 * Extern Variables (Extern /Global)
 ********************************************************************/
 
/********************************************************************
 * Local Variables:  STATIC
 ********************************************************************/
//static u8 rx_buf[EAT_UART_RX_BUF_LEN_MAX + 1] = {0};
//static u32 gpt_timer_counter = 0;
//static u32 time1 = 0;
static u8 s_memPool[EAT_MEM_MAX_SIZE];
//static s8 socket_id = 0;

/********************************************************************
 * External Functions declaration
 ********************************************************************/
extern void APP_InitRegions(void);

/********************************************************************
 * Local Function declaration
 ********************************************************************/
void app_main(void *data);
void app_func_ext1(void *data);
extern void app_at_cmd_envelope(void *data);
/********************************************************************
 * Local Function
 ********************************************************************/
#pragma arm section rodata = "APP_CFG"
APP_ENTRY_FLAG 
#pragma arm section rodata

#pragma arm section rodata="APPENTRY"
	const EatEntry_st AppEntry = 
	{
		app_main,
		app_func_ext1,
		(app_user_func)app_at_cmd_envelope,//app_user1,
		(app_user_func)EAT_NULL,//app_user2,
		(app_user_func)EAT_NULL,//app_user3,
		(app_user_func)EAT_NULL,//app_user4,
		(app_user_func)EAT_NULL,//app_user5,
		(app_user_func)EAT_NULL,//app_user6,
		(app_user_func)EAT_NULL,//app_user7,
		(app_user_func)EAT_NULL,//app_user8,
		EAT_NULL,
		EAT_NULL,
		EAT_NULL,
		EAT_NULL,
		EAT_NULL,
		EAT_NULL
	};
#pragma arm section rodata


void app_func_ext1(void *data)
{
	/*This function can be called before Task running ,configure the GPIO,uart and etc.
	   Only these api can be used:
		 eat_uart_set_debug: set debug port
		 eat_pin_set_mode: set GPIO mode
		 eat_uart_set_at_port: set AT port
	*/
	eat_uart_set_debug(EAT_UART_1);
    eat_uart_set_at_port(EAT_UART_2);// UART1 is as AT PORT
}

static u8 buf[64] = {0};

/*
 *   core initialization is ok? 
 *   check wether core return "Call Ready"  // and "SMS Ready" 
 *   core_init==1,true; otherwise,false
 */
typedef u8 core_init_t;
eat_bool core_is_ready(const char * str)
{
    static core_init_t core_init = 0;
    const char * p = str;
    const char * pStr[]={"Call Ready","SMS Ready"};

    while(p){
         while(AT_CMD_CR == *p || AT_CMD_LF == *p)
            {
             p++;
            }
         if(!strncmp(pStr[0],p,strlen(pStr[0])))         /*call ready */
             core_init++;     //  +1
             
         p = (char *)strchr(p,AT_CMD_LF);
    }
    
    if(1 == core_init)
        {
        core_init = 0;
        return EAT_TRUE;
        }
    else 
        return EAT_FALSE;
}

void app_main(void *data)
{   
    EatEvent_st event;
    u16 len = 0;
    EatUartConfig_st uart_config;
    eat_bool ret;

    APP_InitRegions();//Init app RAM
    APP_init_clib(); //C library initialize, second step
    #if 0
   if(eat_uart_open(EAT_UART_1 ) == EAT_FALSE)
    {
	    eat_trace("[%s] uart(%d) open fail!", __FUNCTION__, EAT_UART_1);
    }
	
    uart_config.baud = EAT_UART_BAUD_115200;
    uart_config.dataBits = EAT_UART_DATA_BITS_8;
    uart_config.parity = EAT_UART_PARITY_NONE;
    uart_config.stopBits = EAT_UART_STOP_BITS_1;
    if(EAT_FALSE == eat_uart_set_config(EAT_UART_1, &uart_config))
    {
        eat_trace("[%s] uart(%d) set config fail!", __FUNCTION__, EAT_UART_1);
    }
 #endif
    log_0("app_main ENTRY");
    Gpio_init();
    ret = eat_mem_init(s_memPool,EAT_MEM_MAX_SIZE);
    if (!ret)
        log_0("ERROR: eat memory initial error!");
    
    eat_timer_start(TIMER_SOFT_RESET,600000);
    at_cluster();

    while(EAT_TRUE)
    {
        eat_get_event(&event);
        log_2("MSG id%x", event.event);
        switch(event.event)
        {
          case EAT_EVENT_TIMER :
           {
             EatTimer_enum timer_id = event.data.timer.timer_id;
             timer_hdlr(timer_id);
           } 
             break;
          case EAT_EVENT_INT :      
             log_0("INTERRUPT IS COMING");
             if(get_key())/*donw = 1 */
                key_hdlr();               
             break;
             
           case EAT_EVENT_USER_MSG:
            {
             u8 data = event.data.user_msg.data[0];
             set_debounce(data);
            }
              break;
           case EAT_EVENT_KEY:
           {
            eat_bool press = event.data.key.is_pressed;
            log_0("power key");
            pwrkey_hdlr(press);
           }
              break;
              
          case EAT_EVENT_MDM_READY_WR:
              break;
              
          case EAT_EVENT_UART_READY_RD:
            log_2("EAT_EVENT_UART_READY_RD");
              break;
          case EAT_EVENT_MDM_READY_RD:
              {        
                  len = 0;
                           /* boot info(such as:RDY ;+CFUN: 1  ;+CPIN: READY)
                            * will be sent to here(main task);
                            */
                  len = eat_modem_read(buf, 2048); /*necessary:Read it out*/ 
                  buf[len] = 0;
                  log_2("main task buf (%s)",buf);
                  #if 0
                 if(core_is_ready(buf))
                    {
                     eat_trace("core is ready");
                     eat_timer_start(TIMER_SOFT_RESET,600000);
                     at_cluster();
                   }  
                 #endif
               }
              break;    
          case EAT_EVENT_UART_SEND_COMPLETE :
              break;
          default:
              break;
        }

    }

}


