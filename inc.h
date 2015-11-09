#ifndef  __INC_H__
#define  __INC_H__
#include "eat_type.h"
#include "eat_interface.h"
#include "app_at_cmd_envelope.h"
extern eat_bool clear_66;
extern eat_bool at_error;
extern eat_bool network_error;
extern eat_bool atd_answer_flag;
//ggggggggggggg
//#define __ABCDEF__
typedef  enum{   
    POWER_ON,  //����׼����
    WAIT_CALL,        //�ȴ�����һ������
    TRIGGER_CALL,//����һ������ 
    CAN_NOT_STOP,//���в�����ֹ
}CALL_ENUM;
typedef  struct{
    CALL_ENUM cal_status;//0:��������״̬ǰ 1:��ʾ�ж��Ѵ�������״̬ 2:��ʾ����״̬������ֹ
    eat_bool  in_the_dialogue;//ָʾ�Ƿ������
    eat_bool  prompt;    //������ʾ��״̬ IDLE ���Բ�������; BUSY ���ɲ��� 
#define IDLE EAT_FALSE
#define BUSY EAT_TRUE
    eat_bool pwrkey;
}status;
typedef enum{   
    ACTIVE     = '0',        
    HELD       = '1',   
    DIALING    = '2',   
    ALERTING   = '3',   
    INCOMING   = '4',   
    WAITING    = '5',   
    DISCONNECT = '6',   
}clcc_enum;

#define PbLength 15 /*�洢�绰�����buffer����*/
typedef struct pb_node{
    struct pb_node * next;
    struct pb_node * prev;
    u8              phone_num[PbLength];
}pb_node;

#define PbLength 15 /*�洢�绰�����buffer����*/
typedef struct{
    u8 phone_num[PbLength];
}_pb_;
typedef struct{
    #define CNT_NUM 5  /*��������绰����ĸ���*/
    _pb_ pb_list[CNT_NUM];
    u8 cnt; /**/
}PB;

typedef enum {
   AT_END,      /*empty queue before and this AT will be alone*/
   AT_APPEND,   /*here other AT exist .it will append this AT in the AT queue */
}At_add_t;
// debug macro
#define DEBUG_SWITCH
//#undef DEBUG_SWITCH
/* print log 0...n---high...low*/
#define log_0(format,arg...)  eat_trace(format,##arg)
#ifdef  DEBUG_SWITCH
#define log_1(format,arg...)  eat_trace(format,##arg)
#define log_2(format,arg...)  eat_trace(format,##arg)
#else
#define log_1(format,arg...)
#define log_2(format,arg...)
#endif
/********************************************************************
 * Macro ͳһ��������������
 ********************************************************************/ 
#define AUD_MAX_PATH_LEN 128
#define PATH  "C:\\User\\recordings\\" /*MAX:128 Bytes*/
#define STR1  PATH"1.wav"    /*���ڿ���*/
#define STR2  PATH"2.wav"    /*���ѽ������ϵͳ���ٰ�һ��ȡ��*/
#define STR3  PATH"3.wav"    /*����ȡ������*/
#define STR4  PATH"4.wav"    /*�����ɹ�*/
#define STR5  PATH"5.wav"    /*�쳣��ά��*/
#define STR6  PATH"6.wav"    /*���ڹػ�*/
 
 #if 1
#define CHARGING             EAT_PIN35_PWM1  //�Ʒ�֪ͨ����
#define CHARGING_SET_MOD()   eat_pin_set_mode(CHARGING,EAT_PIN_MODE_GPIO)
#define CHARGING_GPIO_INIT() eat_gpio_setup(CHARGING,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_LOW)
#define CHARGING_VALID()     eat_gpio_write(CHARGING,EAT_GPIO_LEVEL_HIGH)
#define CHARGING_INVALID()   eat_gpio_write(CHARGING,EAT_GPIO_LEVEL_LOW)

#define INCOMING             EAT_PIN66_STATUS//����֪ͨ����
#define INCOMING_SET_MOD()   eat_pin_set_mode(INCOMING, EAT_PIN_MODE_GPIO)
#define INCOMING_GPIO_INIT() eat_gpio_setup(INCOMING,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_LOW)
#define INCOMING_VALID()     eat_gpio_write(INCOMING,EAT_GPIO_LEVEL_HIGH)
#define INCOMING_INVALID()   eat_gpio_write(INCOMING,EAT_GPIO_LEVEL_LOW)

#define NETWORK_STATUS       EAT_PIN52_NETLIGHT//����״̬
#define NETWORK_SET_MOD()    eat_pin_set_mode(NETWORK_STATUS,EAT_PIN_MODE_GPIO)
#define NETWORK_GPIO_INIT()  eat_gpio_setup(NETWORK_STATUS,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_LOW)
#define NETWORK_VALID()      eat_gpio_write(NETWORK_STATUS,EAT_GPIO_LEVEL_HIGH)
#define NETWORK_INVALID()    eat_gpio_write(NETWORK_STATUS,EAT_GPIO_LEVEL_LOW)


#define DIAL_1               EAT_PIN41_ROW3  //һ����������1
#define DIAL1_SET_MOD()      eat_pin_set_mode(DIAL_1,EAT_PIN_MODE_GPIO)
#define DIAL1_GPIO_INIT()    eat_gpio_setup(DIAL_1,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_HIGH)

#define DIAL_2_EINT          EAT_PIN40_ROW4//һ�������жϽ�
#define DIAL2_SET_MOD()      eat_pin_set_mode(DIAL_2_EINT,EAT_PIN_MODE_EINT);
#define DIAL2_EINT_LOW()     eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_LOW_LEVEL,20,NULL)
#define DIAL2_EINT_HIGH()    eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_HIGH_LEVEL,100,NULL)
#define DIAL2_EINT_LOW_1S()  eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_LOW_LEVEL,40,NULL)
#define DIAL2_EINT_HIGH_1S() eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_HIGH_LEVEL,40,NULL)
#define DIAL2_EINT_PULL_UP()    eat_gpio_set_pull(DIAL_2_EINT,EAT_TRUE,EAT_GPIO_LEVEL_HIGH)
#define DIAL2_EINT_PULL_DOWN()    eat_gpio_set_pull(DIAL_2_EINT,EAT_TRUE,EAT_GPIO_LEVEL_LOW)
#define READ_DIAL_2()        eat_gpio_read(DIAL_2_EINT)
#else
#define CHARGING             EAT_PIN51_COL0  //�Ʒ�֪ͨ����
#define CHARGING_SET_MOD()   eat_pin_set_mode(CHARGING,EAT_PIN_MODE_GPIO)
#define CHARGING_GPIO_INIT() eat_gpio_setup(CHARGING,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_LOW)
#define CHARGING_VALID()     eat_gpio_write(CHARGING,EAT_GPIO_LEVEL_HIGH)
#define CHARGING_INVALID()   eat_gpio_write(CHARGING,EAT_GPIO_LEVEL_LOW)

#define INCOMING             EAT_PIN50_COL1  //����֪ͨ����
#define INCOMING_SET_MOD()   eat_pin_set_mode(INCOMING, EAT_PIN_MODE_GPIO)
#define INCOMING_GPIO_INIT() eat_gpio_setup(INCOMING,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_LOW)
#define INCOMING_VALID()     eat_gpio_write(INCOMING,EAT_GPIO_LEVEL_HIGH)
#define INCOMING_INVALID()   eat_gpio_write(INCOMING,EAT_GPIO_LEVEL_LOW)

#define NETWORK_STATUS       EAT_PIN48_COL3  //����״̬
#define NETWORK_SET_MOD()    eat_pin_set_mode(NETWORK_STATUS,EAT_PIN_MODE_GPIO)
#define NETWORK_GPIO_INIT()  eat_gpio_setup(NETWORK_STATUS,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_LOW)
#define NETWORK_VALID()      eat_gpio_write(NETWORK_STATUS,EAT_GPIO_LEVEL_HIGH)
#define NETWORK_INVALID()    eat_gpio_write(NETWORK_STATUS,EAT_GPIO_LEVEL_LOW)


#define DIAL_1               EAT_PIN49_COL2  //һ����������1
#define DIAL1_SET_MOD()      eat_pin_set_mode(DIAL_1,EAT_PIN_MODE_GPIO)
#define DIAL1_GPIO_INIT()    eat_gpio_setup(DIAL_1,EAT_GPIO_DIR_OUTPUT,EAT_GPIO_LEVEL_LOW)

#define DIAL_2_EINT          EAT_PIN47_COL4//һ�������жϽ�
#define DIAL2_SET_MOD()      eat_pin_set_mode(DIAL_2_EINT,EAT_PIN_MODE_EINT);
#define DIAL2_EINT_LOW()     eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_LOW_LEVEL,20,NULL)//200'ms
#define DIAL2_EINT_HIGH()    eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_HIGH_LEVEL,20,NULL)
#define DIAL2_EINT_LOW_1S()  eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_LOW_LEVEL,1000,NULL)//1's
#define DIAL2_EINT_HIGH_1S() eat_int_setup(DIAL_2_EINT,EAT_INT_TRIGGER_HIGH_LEVEL,1000,NULL)
#define DIAL2_EINT_PULL()    eat_gpio_set_pull(DIAL_2_EINT,EAT_TRUE,EAT_GPIO_LEVEL_HIGH)
#define READ_DIAL_2()        eat_gpio_read(DIAL_2_EINT)
#endif
/*****************************************************************************
 *  ��ʱ��:  
 * [fixed]
 *             1,2 :for thread send AT command ,Do not touch;
 * [flexible]
 *             3:һ������8's����ʱ��
 *             4:����25's����ʱ��
 *             7:power key
 *             8:power key down
 ******************************************************************************/
#define TIMER_8S             EAT_TIMER_3
#define TIMER_25S            EAT_TIMER_4
#define TIMER_PWRKEY         EAT_TIMER_7
#define TIMER_POWERDOWN      EAT_TIMER_8
#define TIMER_CANCEL_DELAY   EAT_TIMER_5
#define TIMER_SOFT_RESET     EAT_TIMER_6

/***************gpio.c  ****************/
void Gpio_init(void );


/***************custom.c****************/
extern void  timer_hdlr(EatTimer_enum timer_id);
extern void status_init(void);
extern eat_bool ph_list_init(void);
extern eat_bool dialing(void);
extern eat_bool white_list(u8 *p);
extern AtCmdRsp pb_cb(const u8 *pRspStr);
extern void atcmd_queue_head_out(void);
extern void simcom_atcmd_queue_init(void);
extern void set_in_the_dialogue(eat_bool bool);
extern eat_bool get_in_the_dialogue(void);
extern void key_hdlr(void);
extern u8 get_key(void);
extern void set_debounce(u8 data);
extern void set_cal_status(CALL_ENUM stat);
extern CALL_ENUM get_cal_status(void);
extern void set_prompt(eat_bool stat);
extern eat_bool get_prompt(void);
extern void pwrkey_hdlr(eat_bool press);
extern void set_pwrkey(eat_bool stat);
extern eat_bool get_pwrkey(void);
extern void at_cluster(void);
extern eat_bool crec(const u8 *path,At_add_t type);
extern eat_bool hang_up(At_add_t type);
extern eat_bool calling(pb_node *ptr);
extern eat_bool answer(void);
extern void ignore_ath_set(eat_bool stat);
extern eat_bool  ignore_ath_get(void);
extern eat_bool cancel_crec_append(void);
extern void  atcmdovertimestop(void);
extern eat_bool get_phone_num(pb_node * str,u8 * phone_ptr);
extern pb_node * get_ph_head(void);

#endif
