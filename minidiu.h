/**
 * minidiu.h
 *
 *  Created on: 19 Nov 2019
 *      Author: serdar.guney
 */
#ifndef DEVICE_MINIDIU_H_
#define DEVICE_MINIDIU_H_

#include "nmeaparser.h"
#include "driveridunit.h"
#include "configminidiu.h"
#include "devicebase.h"

static const uint16_t c_RX_BUF_LENGTH = 400;

class c_MiniDIU: public c_DriverIDUnit, public c_DeviceBaseT< s_null_t, c_MiniDIUType >
{
public:
	c_MiniDIU( c_SharedList * p_st_list, c_MiniDIUType::s_hw_t * p_hw, e_DEVICES Id, uint32_t StackSize );

private:
#pragma pack(push, 1)
	struct s_NMEATokensHashTable {
		uint32_t PIMR_Hash;
		uint32_t SYS_Hash;
		uint32_t STA_Hash;
		uint32_t VAL_Hash;
		uint32_t BTN_Hash;
		uint32_t DOWN_Hash;
		uint32_t iBTNID_Hash;
		uint32_t NDEFID_Hash;
		uint32_t A07_Hash;
		uint32_t VER_Hash;
		uint32_t ACK_Hash;
	};
#pragma pack(pop)

	typedef enum
	{
		BUZZER,
		LED,
		VERSION
	}e_MESSAGE_TYPE;

	s_NMEATokensHashTable m_NMEATokensHashTable;

	QueueSetHandle_t m_QueueSet;
	c_NMEAParser *m_NMEAParser;
	uint8_t m_main_dma_buffer[ c_RX_BUF_LENGTH ];
	s_minidiu_config_t m_minidiu_config;
	uint8_t m_hearth_beat_error_count = 1;
	uint32_t m_tick_counter = 0;
	bool m_isNfcConnected = false;

	void InitUART( void );
	void RestartDevice( void );
	void GetVersion( void );
	void ReceiveDataHandler( void );
	void ProcessNMEAMessage( void );
	void ReverseBtnID( char *BtnID, uint8_t length ); 
	void CreateNmeaMessageAndSend( e_MESSAGE_TYPE msgType, uint32_t retry = 0, uint8_t patternCnt = 0, uint8_t cnt = 0, uint16_t tOn = 0, uint16_t tOff = 0, uint16_t startTmo = 0, uint8_t color = 0 );
	bool StrToUint32AndValidate4Byte( const char *str, uint32_t *out );

	static c_Registrar::Registrar reg;

	// c_Thread interface
public:
	RunFuncType_t Run();
	static c_DeviceInterface *CreateObject ( c_HwInterface *hw, c_SharedList *p_st_list, e_DEVICES devID );

	//c_DeviceInterface
public:
	ARV_StatusTypeDef Init( void );
	ARV_StatusTypeDef SelfTest( void );
	ARV_StatusTypeDef SetPowerMode( e_ARV_PowerModeDef powerMode );
	ARV_StatusTypeDef UpdateConfig( e_CLASSES config_class );
	void GetDeviceConfigMap( std::map< e_CLASSES, c_DeviceInterface * > & device_config_map );
	void RegisterCallbacks( uint8_t *dev_mgr_array );
	ARV_StatusTypeDef DeviceManagerAction( s_device_message_t dev_mgr_msg );

	//c_DriverIDUnit
protected:
	c_SharedList *getSharedStructList( ) override;
	virtual void ObjBuzzerControl( uint16_t duration = 0, uint16_t startTmo = 1, e_BUZZER_PATTERN buzzerPattern = e_BUZZER_PATTERN::BUZZER_OFF ) override;
	virtual void ObjLEDControl( e_LED_STATE led_state ) override;
	virtual void ObjSendBuzzerMsg( void ) override;
	virtual ARV_StatusTypeDef ObjSendMsgToActMgr( s_action_message_t send_message ) override;
	virtual void ObjCommHealthCheck( void ) override;
};

#endif /* DEVICE_MINIDIU_H_ */
