/**
 * driveridunit.h
 *
 *  Created on: 10 Dec 2019
 *      Author: serdar.guney
 */
#ifndef DEVICE_DRIVERIDUNIT_H_
#define DEVICE_DRIVERIDUNIT_H_

#include "configminidiu.h"
#include "datadriverlist.h"
#include "sharedlist.h"
#include "messages.h"

class c_DriverListConfig;
class c_SharedList;

static const uint8_t c_PERSONNEL_LED_SHOWTIME = 2;
static const uint8_t c_TX_BUF_LENGTH = 200;
static const uint16_t c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT = 100 / TICK_PERIOD_MS;

#define StopBuzzer( ) ObjBuzzerControl( )	//0, 1, BUZZER_OFF

class c_DriverIDUnit: public c_SharedStruct< s_driverlist_data_t >
{
public:
	c_DriverIDUnit( c_SharedList *p_st_list, bool useBITTimer = false );
	virtual ~c_DriverIDUnit() { };
	// Only used in DIUs that require BIT functionality (e.g. c_MiniDIU)
	osTimerId m_BITTimer = nullptr;
	osTimerId m_BuzzerTimer;
	osTimerId m_PersonnelTimer;
	osSemaphoreId m_BITTimerSemaphore = nullptr;
	osSemaphoreId m_BuzzerTimerSemaphore;
	osSemaphoreId m_PersonnelTimerSemaphore;

protected:
	typedef enum
	{
		BUZZER_OFF = 0,
		BUZZER_PATTERN_DEFAULT = 1,
		BUZZER_PATTERN1 = 2,
		BUZZER_PATTERN2 = 3,
		BUZZER_PATTERN3 = 4,
	}e_BUZZER_PATTERN;

	typedef enum
	{
		LED_OFF = 0,
		LED_GREEN = 1,
		LED_RED = 3
	}e_LED_STATE;

	typedef enum
	{
		LIST_NOT_FOUND = 0,
		ID_MATCHED = 1,
		NO_MATCH = 2,
	}e_DRIVERLIST_ID_MATCH_RESULT;

	uint16_t m_active_buzzer_duration = 0;
	s_minidiu_config_t m_config_diu_base;
	char m_BuzzerMsg[ c_TX_BUF_LENGTH ] = { 0 };
	char m_activeIDstr[ MAX_DRIVER_ID_LENGTH ] = { 0 };
	void LoopController( QueueSetMemberHandle_t activatedMember );
	void CheckDriverID( void );
	ARV_StatusTypeDef LoadDriverID( void );
	void AutoCheckout( void );
	void ControlAutoCheckoutTimeout( void );
	void SetDriverStatusLed( void );
	void SetBuzzerActiveState( bool state );
	void SetDriverActiveState( bool state );

private:
	enum e_AUTO_CHECKOUT_TIMER_COMMAND {
		STOP_TIMER = 0,
		START_TIMER = 1,
	};

	c_SharedList *m_struct;
	c_DriverListConfig *m_DriverListConfig;
	bool m_isBuzzerActive = false;
	bool m_DriverActiveFlag = false;
	uint64_t m_autochekout_start_time;
	char m_driverIDBufstr[ MAX_DRIVER_ID_LENGTH ] = { 0 };
	char m_zeroBuf[ MAX_DRIVER_ID_LENGTH ] = { 0 };
	ARV_StatusTypeDef SaveDriverID( uint8_t *driverID );
	void CheckIn( uint32_t card_read_cnt );
	e_DRIVERLIST_ID_MATCH_RESULT CheckDriverList( void );
	void InstallAutoCheckoutTimer( void );
	void UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND command );
	void SendStatusMessage( e_DRIVER_STATUS driverStatus, char *id = nullptr );
	uint64_t Hex2Int( char *hex, uint8_t size );

	void ProcessIgnitionAndDriverState( void );
//	void TESTFillDriverList( void );

	// Interface hooks
protected:
	virtual c_SharedList *getSharedStructList( ) = 0;
	virtual void ObjBuzzerControl( uint16_t duration = 0, uint16_t startTmo = 1, e_BUZZER_PATTERN buzzerPattern = e_BUZZER_PATTERN::BUZZER_OFF ) = 0;
	virtual void ObjLEDControl( e_LED_STATE led_state ) = 0;
	virtual void ObjSendBuzzerMsg( void ) = 0;
	virtual ARV_StatusTypeDef ObjSendMsgToActMgr( s_action_message_t send_message ) = 0;
	virtual void ObjCommHealthCheck( void );
};

#endif /* DEVICE_DRIVERIDUNIT_H_ */
