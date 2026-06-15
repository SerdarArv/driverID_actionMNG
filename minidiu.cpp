/**
 * minidiu.cpp
 *
 *  Created on: 19 Nov 2019
 *      Author: serdar.guney
 */

/*
$PIMR,,DIA,RUN,A90,RESET,C0*6D\r\n						// Cihazi resetlemek icin gonderilen mesaj
$PIMR,,CFG,ACK,A90,PARAMS*75\r\n						// Reset ve config yukleme ack
$PIMR,,SYS,ACK,A90,INFO,,CID:32FFD7054843323458452243,B0:95.69.150.28,A0:1.0.8.0,NAME:A_SEZGINCONF,TYPE:-*6A\r\n	// GetInfo cevabi
$PIMR,,CFG,NACK,A90,PARAMS*3B\r\n						// Konfig yukleme nack
$PIMR,,INF,VAL,A90,TXT,READY*47\r\n						// Firmware yukleme - Program komutu cevabi Ready
$PIMR,,INF,VAL,A90,TXT,WAITING*47\r\n					// Firmware yukleme - Transfer komutu sonrasi cevap Waiting
$PIMR,,INF,VAL,A90,TXT,COMPLETED*41\r\n					// Firmware yukleme - Transfer islemi bitince cevap
$PIMR,,INF,VAL,A90,ERR,6*27\r\n							// Firmware yukleme - Transfer sonrasi veride problem varsa gelir
$PIMR,,SYS,GET,A06,VER,A0*7E\r\n						// Firmware versiyon istegi
$PIMR,,SYS,VAL,A07,VER,A0,1.1*70\r\n					// Firmware versiyon bilgisi
$PIMR,,STA,VAL,A07,iBTNID,0159E1F418000082*69\r\n		// Okutulan Id
$PIMR,,STA,VAL,A07,NDEFID,0159E1F418000082*69\r\n		// Okutulan NDEF Id
$PIMR,,STA,GET,A07,BTN,VALUES*10\r\n					// Id Okutma suresi
$PIMR,,STA,SET,A06,LED,0,3,1000000,0,2,5,500,500*21\r\n	// Ledi sondur
$PIMR,,STA,SET,A06,LED,0,3,1000000,1,1,5,65000,1*21\r\n	// Yesil Led yak
$PIMR,,STA,SET,A06,LED,0,3,1000000,1,3,5,65000,1*23\r\n	// Kirmizi Led yak
$PIMR,,STA,SET,A06,BUZZ,0,1,0,500,1,0,1000*56\r\n		// Buzzer sustur
$PIMR,,STA,SET,A06,BUZZ,0,255,1,500,n,200,800*65\r\n	// Buzzerı n kez cal
$PIMR,,CFG,SET,A07,BUZMOD,0*7B\r\n                 		// Buzzer Direct
$PIMR,,CFG,SET,A07,BUZMOD,1*7A\r\n                 		// Buzzer Freq
*/

#include "minidiu.h"
#include "circularbuffer.h"
#include "comminterface.h"
#include "configpower.h"
#include "datadriverlist.h"
#include "debugprintf.h"
#include "hwinterface.h"
#include "iointerface.h"
#include "manager.h"
#include <math.h>
#include "serialdevice.h"
#include <byteswap.h>

#if defined TAG
#undef TAG
#endif
#define TAG	"DEV_DIU"
//#define TAG	""

static const uint8_t c_IBTNID_LENGTH = 16;
static const uint8_t c_GET_VERSION_PERIOD = 2;
static const uint8_t c_SELFTEST_RESET_PERIOD = 15;
static const uint16_t c_COMM_HEALTHCHECK_PERIOD = 5;
static const uint16_t c_MAX_SELFTEST_PERIOD = 300; 	/// SelfTest'de minidiu'dan 300 saniye boyunca paket gelmezse BITResult hataya düşer.
static const uint16_t c_QUEUESET_WAIT_TIME = 100 / TICK_PERIOD_MS;

c_Registrar::Registrar c_MiniDIU::reg ( e_DEVICES::DEVICE_MDIU, &c_MiniDIU::CreateObject );

c_MiniDIU::c_MiniDIU( c_SharedList * p_st_list, c_MiniDIUType::s_hw_t * p_hw, e_DEVICES id,  uint32_t StackSize ) : c_DriverIDUnit( p_st_list, true )
,c_DeviceBaseT( p_st_list, p_hw, id, TAG, StackSize, device_mdiu_TASK_PRIORITY )
{
	m_NMEAParser = new c_NMEAParser( );
							//	$PIMR		SYS		STA			VAL		BTN			DOWN	iBTNID		NDEFID		A07		VER		ACK
	m_NMEATokensHashTable = { 1645958845,155377184,155377001,155379652,155358501,831930041,1166202427,111370891,155356201,155379790,155356848 };

	uint8_t queueSetSize = ( m_hw.m_main_uart->m_data_semaphore_size * 2 ) + ( m_bin_semaphore_size * 4 ) + c_DEVICE_QUEUE_SIZE;
	m_QueueSet = osApiQueueSetCreate( queueSetSize );
	osApiAddToQueueSet( m_hw.m_main_uart->m_ReceivedDataSemaphore, m_QueueSet );
	osApiAddToQueueSet( m_hw.m_main_uart->m_DmaErrorSemaphore, m_QueueSet );
	osApiAddToQueueSet( m_ReceiveMessageQueue, m_QueueSet );
	osApiAddToQueueSet( m_BITTimerSemaphore, m_QueueSet );
	osApiAddToQueueSet( m_BuzzerTimerSemaphore, m_QueueSet );
	osApiAddToQueueSet( m_PersonnelTimerSemaphore, m_QueueSet );
}

ARV_StatusTypeDef c_MiniDIU::Init( void )
{
	m_hw.m_pwr_en->Write( IO_STATE::HIGH );
	osApiDelay( 250 );
	InitUART( );

	return ARV_OK;
}

ARV_StatusTypeDef c_MiniDIU::SelfTest( void )
{
	setBITResult( ARV_BUSY );

	return m_bit_result;
}

ARV_StatusTypeDef c_MiniDIU::SetPowerMode( e_ARV_PowerModeDef powermode )
{
	m_power_result = ARV_BUSY;

	if( powermode == e_ARV_PowerModeDef::ARV_PWR_ON )
	{
		Init( );
		Resume( );
		SetDriverStatusLed( );	//	Wake Up. Send Led Status
	}
	else if( powermode == e_ARV_PowerModeDef::ARV_PWR_OFF )
	{
		m_hw.m_main_uart->Deinit( );
		m_hw.m_pwr_en->Write( IO_STATE::LOW );
		Suspend( );
	}
	else if( powermode == e_ARV_PowerModeDef::ARV_PWR_SAVE )
	{
		m_hw.m_main_uart->Deinit( );
		Suspend( );
	}
	m_power_result = ARV_OK;

	return ARV_OK;
}

ARV_StatusTypeDef c_MiniDIU::UpdateConfig( e_CLASSES config_class )
{
	if( config_class != e_CLASSES::CONFIG_MINIDIU )
	{
		return ARV_ERROR;
	}
	auto p = m_struct_list->getInstance< s_minidiu_config_t > ( e_CLASSES::CONFIG_MINIDIU );
	if( p != nullptr )
	{
		auto mdiu_config = p->getStruct( );
		memcpy( (void *)&m_config_diu_base, (const void *)&mdiu_config, sizeof( m_config_diu_base ) );
		//	Buzzer duration Max degeri Konfigurasyonda UINT16_INVALID, CompositeEvent'de UINT8_INVALID geliyor. UINT8_INVALID sabitlendi.
		if( m_config_diu_base.buzzer_duration == UINT16_INVALID )	m_config_diu_base.buzzer_duration = UINT8_INVALID;
		ControlAutoCheckoutTimeout( );
	}
	else
	{
		ERR_PRINT( "MiniDIU config NOT FOUND", TAG );
		return ARV_ERROR;
	}

	return ARV_OK;
}

RunFuncType_t c_MiniDIU::Run( void )
{
	uint32_t ImAliveTickCnt = 0;
	QueueSetMemberHandle_t activatedMember = nullptr;
	uint8_t count = 0;

	Init( );
	SelfTest( );
	UpdateConfig( e_CLASSES::CONFIG_MINIDIU );
	LoadDriverID( );
	osApiTimerStart( m_BITTimer, 1000 );

	for ever
	{
		if( m_suspended_flag )
		{
			CheckSuspend( );
		}
		IamAlive( ImAliveTickCnt );

		activatedMember = osApiSelectFromQueueSet( m_QueueSet, c_QUEUESET_WAIT_TIME );
		if( activatedMember != NULL )
		{
			if( activatedMember == m_hw.m_main_uart->m_ReceivedDataSemaphore )
			{
				if( osApiSemaphoreWait( m_hw.m_main_uart->m_ReceivedDataSemaphore, c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT ) == osOK )
				{
					ReceiveDataHandler( );
				}
			}
			else if( activatedMember == m_hw.m_main_uart->m_DmaErrorSemaphore )
			{
				ERR_PRINT("DMA Error Semaphore Received",TAG);
				if( osApiSemaphoreWait( m_hw.m_main_uart->m_DmaErrorSemaphore, c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT ) == osOK )
				{
					InitUART( );
				}
			}
			else if( activatedMember == m_ReceiveMessageQueue )
			{
				if( ReceiveMessage( 1000 ) == ARV_OK )
				{
					switch( m_rcv_msg.command )
					{
					case c_MiniDIUType::e_mDIU_COMMANDS::BUZZER_ON_:
						SetBuzzerActiveState( true );
						ObjBuzzerControl( m_rcv_msg.buzzer_repeat_count, 0, e_BUZZER_PATTERN::BUZZER_PATTERN_DEFAULT );
						break;
					case c_MiniDIUType::e_mDIU_COMMANDS::BUZZER_OFF_:
						StopBuzzer( );
						break;
					case c_MiniDIUType::e_mDIU_COMMANDS::SET_ACTIVE_DRIVER:
						strcpy( m_activeIDstr, (const char *)m_rcv_msg.id );
						CheckDriverID( );
						break;
					case c_MiniDIUType::e_mDIU_COMMANDS::SET_DRIVER_ACTIVE_MEMBER:
						DEBUG_PRINT("SetDriverActiveState(true) called.", TAG);
						SetDriverActiveState(true);
						break;
					default :
						break;
					}
				}
			}
		}
		LoopController( activatedMember );
		GetFreeStackSize( );
	}
}

void c_MiniDIU::ObjCommHealthCheck( void )
{
	static bool isHeartbeatCheck = false;
	m_tick_counter++;

	if( GetBITResult( ) != ARV_StatusTypeDef::ARV_OK )
	{
		if( m_tick_counter % c_MAX_SELFTEST_PERIOD == 0 )
		{
			WARN_PRINT( "MiniDIU bit Result ERROR", TAG );
			setBITResult( ARV_StatusTypeDef::ARV_ERROR );
		}
		if( m_tick_counter % c_SELFTEST_RESET_PERIOD == 0 )	//Test devam ettigi icin sabit olarak kapatip acalim
		{
			WARN_PRINT( "No MiniDIU Communication, Restarting Device", TAG );
			RestartDevice( );
		}
		if( m_tick_counter % c_GET_VERSION_PERIOD == 0 )
		{
			GetVersion( );
		}
	}
	else
	{
		if( isHeartbeatCheck )
		{
			if( m_tick_counter >= pow( 10, m_hearth_beat_error_count ) )
			{
				m_tick_counter = 0;
				m_hearth_beat_error_count++;
				WARN_PRINT( "No MiniDIU Communication, Restarting Device", TAG );
				RestartDevice( );
				SetDriverStatusLed( );
			}
		}
		else
		{
			isHeartbeatCheck = true;
			INFO_PRINT( "MiniDIU Self Test is OK!!! ",TAG );
			SetDriverStatusLed( );	//	Comm Ok. Send Led Status
			osApiTimerUpdate( m_BITTimer, c_COMM_HEALTHCHECK_PERIOD * 1000 );
		}
		GetVersion( );
	}
}

void c_MiniDIU::GetDeviceConfigMap( std::map< e_CLASSES, c_DeviceInterface * > &device_config_map )
{
	std::map< e_CLASSES, c_DeviceInterface * > config_map;
	config_map.insert( std::pair< e_CLASSES, c_DeviceInterface * > ( e_CLASSES::CONFIG_DRIVERLIST, this ) );
	config_map.insert( std::pair< e_CLASSES, c_DeviceInterface * > ( e_CLASSES::CONFIG_MINIDIU, this ) );
	device_config_map.insert( config_map.begin( ), config_map.end( ) );
}

c_DeviceInterface *c_MiniDIU::CreateObject( c_HwInterface *hw, c_SharedList *p_st_list, e_DEVICES devID )
{
	c_DeviceInterface *temp_device = nullptr;
	s_power_config_t pwr_config = p_st_list->getInstance < s_power_config_t > ( e_CLASSES::CONFIG_POWER )->getStruct ( );
	if( ( pwr_config.device_power_states.fields.MDIU_PWR ) && ( !pwr_config.device_power_states.fields.HUB_PWR && !pwr_config.device_power_states.fields.SDIU_PWR ) ) // smartdiu aciksa bu cihazi uretme
	{
		c_MiniDIUType::s_hw_t mdiu_hw;
		auto inst = p_st_list->getInstance < s_minidiu_config_t > ( e_CLASSES::CONFIG_MINIDIU );
		if( inst == nullptr )
		{
			return temp_device;
		}
		s_minidiu_config_t mdiu_config = inst->getStruct( );
		if( mdiu_config.channel_id == e_CHANNEL_IDs::CHAN_UNKWON )
		{
			return temp_device;
		}
		if( IsSerialPortAvailable( mdiu_config.channel_id, p_st_list ) == false )
		{
			return temp_device;
		}
		mdiu_hw.m_main_uart = hw->getCommHw( MAP_COMM_HW( mdiu_config.channel_id ) );
		if( mdiu_hw.m_main_uart == nullptr )
		{
			return temp_device;
		}
		mdiu_hw.m_pwr_en = hw->getIOHw( MAP_IO_PW_HW( mdiu_config.channel_id ) );
		temp_device = new c_MiniDIU ( p_st_list, &mdiu_hw, devID, configMINIMAL_STACK_SIZE * 10 );
	}
	return temp_device;
}

void c_MiniDIU::RegisterCallbacks( uint8_t *dev_mgr_array )
{
	s_device_message_t::e_DEV_MNG_COMMANDS callback_list[ ] =
	{ s_device_message_t::e_DEV_MNG_COMMANDS::DIU_BUZZER_CONTROL,
			s_device_message_t::e_DEV_MNG_COMMANDS::SET_ACTIVE_DRIVER,
			s_device_message_t::e_DEV_MNG_COMMANDS::SET_DRIVER_ACTIVE_MEMBER };
	TransferCallbacksToDevMng( dev_mgr_array, callback_list, ARRAY_LENGTH( callback_list ) );
}

ARV_StatusTypeDef c_MiniDIU::DeviceManagerAction( s_device_message_t dev_mgr_msg )
{
	MSG minidiu_msg;
	switch ( dev_mgr_msg.command )
	{
	case s_device_message_t::e_DEV_MNG_COMMANDS::DIU_BUZZER_CONTROL:
		minidiu_msg.command = (c_MiniDIUType::e_mDIU_COMMANDS)dev_mgr_msg.buzzer_control.buzzer_command;
		minidiu_msg.buzzer_repeat_count = dev_mgr_msg.buzzer_control.buzzer_repeat_count;
		minidiu_msg.buzzer_period = dev_mgr_msg.buzzer_control.buzzer_period;
		break;
	case s_device_message_t::e_DEV_MNG_COMMANDS::SET_ACTIVE_DRIVER:
		minidiu_msg.command = c_MiniDIUType::e_mDIU_COMMANDS::SET_ACTIVE_DRIVER;
		memcpy(minidiu_msg.id, dev_mgr_msg.eeprom_msg.buf, MAX_DRIVER_ID_LENGTH );
		osApiFree( dev_mgr_msg.eeprom_msg.buf );
		break;
	case s_device_message_t::e_DEV_MNG_COMMANDS::SET_DRIVER_ACTIVE_MEMBER:
		minidiu_msg.command = c_MiniDIUType::e_mDIU_COMMANDS::SET_DRIVER_ACTIVE_MEMBER;
		break;
	default:
		return ARV_ERROR;
		break;
	}

	return PrepareMessageforMe( minidiu_msg );
}

void c_MiniDIU::RestartDevice( void )
{
	m_hw.m_pwr_en->Write( IO_STATE::LOW );
	osApiDelay( 200 );
	Init( );
}

void c_MiniDIU::ProcessNMEAMessage( void )
{
	for( auto it = m_NMEAParser->m_splittedmessage.begin(); it != m_NMEAParser->m_splittedmessage.end(); ++it )
	{
		if( ( !m_config_diu_base.read_ndef && (*it).calc_hash == m_NMEATokensHashTable.iBTNID_Hash ) ||
			( m_config_diu_base.read_ndef && (*it).calc_hash == m_NMEATokensHashTable.NDEFID_Hash ) )
		{	//	NMEA protokolune göre iBTNID iteratorunden sonra gelen ilk deger TAGID oluyor.
			char id_str[ strlen( ( *(it + 1) ).val.c_str() ) + 3 ] = { 0 };	//	reverse edilen id ilk okunan id den uzunluk olarak buyuk olabiliyor, onlem icin size buyutuldu
			strcpy( id_str, ( *(it + 1) ).val.c_str() ); // "iBTNID" stringinden sonraki ilk deger id degeri
			ReverseBtnID( id_str, strlen( id_str ) );
			uint32_t value = 0;
			if( m_isNfcConnected && (*it).calc_hash == m_NMEATokensHashTable.iBTNID_Hash && StrToUint32AndValidate4Byte( id_str, &value ) )
			{
				uint32_t reversed = __builtin_bswap32( value );
				snprintf( id_str, sizeof( id_str ), "%u", reversed );
			}
			DEBUG_PRINT( "STB reversed iBUTTON id Value = %s", TAG, id_str );
			strcpy( m_activeIDstr, id_str );
			CheckDriverID( );
		}
		else if( (*it).calc_hash == m_NMEATokensHashTable.BTN_Hash )
		{	//	NMEA protokolune göre BTN iteratorunden sonra gelen ikinci deger buton okutma suresı(Kisa-Uzun) oluyor.
			char ibtn_state_str[ strlen( ( *(it + 2) ).val.c_str() ) + 1 ] = { 0 };
			strcpy( ibtn_state_str, ( *(it + 2) ).val.c_str() ); // "BTN" stringinden sonraki ikinci deger uzun kisa basis
			DEBUG_PRINT( "STB button state value = %s", TAG, ibtn_state_str );
		}
		else if( (*it).calc_hash == m_NMEATokensHashTable.VER_Hash )
		{	//	NMEA protokolune göre iBTNID iteratorunden sonra gelen ikinci deger versiyon bilgisi oluyor.
			if( GetBITResult( ) != ARV_StatusTypeDef::ARV_OK )
			{
				int major = 0;
				int minor = 0;
				char ver_str[ strlen( ( *(it + 2) ).val.c_str() ) + 1 ] = { 0 };
				strcpy( ver_str, ( *(it + 2) ).val.c_str() ); // "BTN" stringinden sonraki ikinci deger uzun kisa basis
				INFO_PRINT( "STB software version = %s", TAG, ver_str );

				int parsed = sscanf( ver_str, "%d.%d", &major, &minor );
				if( parsed == 2 )
				{
					if( major == 0 )
						m_isNfcConnected = true;
					else
						m_isNfcConnected = false;
				}
				setBITResult( ARV_OK );
			}
			else
			{
				static uint8_t counter = 0;
				if( counter % 10 == 0 )	// Haberlesmenin oldugunu anlamak icin yapildi.
				{
					INFO_PRINT( "MiniDIU Communication OK", TAG );
				}
				counter++;
				m_tick_counter = 0;
				m_hearth_beat_error_count = 1;
			}
		}
		else if( (*it).calc_hash == m_NMEATokensHashTable.ACK_Hash )
		{	//	NMEA protokolune göre ACK iteratorunden sonra gelen ikinci deger komut gonderenin bilgisi oluyor.
			char command_str[ strlen( ( *(it + 2) ).val.c_str() ) + 1 ] = { 0 };
			strcpy( command_str, ( *(it + 2) ).val.c_str() ); // "ACK" stringinden sonraki ikinci deger komut cevabinin LED/BUZZ olduğunu belirtir
			DEBUG_PRINT( "ACK of %s command has been received from STB", TAG, command_str );
		}
	}

	m_NMEAParser->m_splittedmessage.clear();
}

void c_MiniDIU::ReceiveDataHandler( void )
{
	uint16_t dataSize = 0;
	uint8_t dataByte = 0;

	if( m_hw.m_main_uart->m_dma_control->checkTailMember( 1, '\n' ) && m_hw.m_main_uart->m_dma_control->checkTailMember( 2, '\r' ) )
	{
		dataSize = m_hw.m_main_uart->m_dma_control->waitingDataSize( );
		uint8_t rx_data[ dataSize + 1 ] = { 0 };
		for( uint8_t i = 0; i < dataSize; i++ )
		{
			m_hw.m_main_uart->m_dma_control->pop( &dataByte );
			rx_data[ i ] = dataByte;
		}
		if( m_NMEAParser->ParseBuffer( rx_data, dataSize ) == ARV_OK )
			ProcessNMEAMessage( );
		else
			m_NMEAParser->m_splittedmessage.clear();
	}
}

/**
 * @brief BtnID'yi 2'li karakter gruplarını ters sıraya dizerek yeniden oluşturur.
 *
 * Bu fonksiyon, verilen karakter dizisini (BtnID), her 2 karakteri bir grup olarak alır
 * ve bu grupları sondan başa doğru sıralar. Oluşan yeni dizi, orijinal BtnID üzerine yazılır.
 *
 * Eğer oluşan yeni dizinin başı '0' karakteriyle başlıyorsa ve ID uzunluğu iButton ID (16 byte) değilse,
 * baştaki '0' karakteri kaydırılarak silinir.
 *
 * iButton ID'lerde (16 karakterlik olanlar) baştaki '0' korunur.
 *
 * Örnek:
 * Girdi:  "123456"   → Çıktı: "563412"
 * Girdi:  "452301"   → Çıktı: "012345" → başındaki '0' silinir → "12345"
 * Girdi:  "3333222211110000" (iButton) → Çıktı: "0000111122223333" → baştaki '0' korunur
 *
 * @param BtnID  [in/out] Orijinal karakter dizisi, işlem sonrası güncellenir
 * @param length [in]     Karakter dizisinin uzunluğu
 */
void c_MiniDIU::ReverseBtnID( char *BtnID, uint8_t length )
{
	char IDstr[ length + 1 ] = { 0 };
	uint8_t i = 0;
	uint8_t loop = length / 2;	//( length % 2 == 0 ) ? ( length / 2 ) : ( length / 2 + 1 );

	for( int8_t cnt = loop - 1; cnt >= 0; cnt-- )
	{
		IDstr[ i++ ] = BtnID[ ( cnt * 2 ) ] ;
		IDstr[ i++ ] = BtnID[ ( cnt * 2 ) + 1 ];
	}

	if( IDstr[ 0 ] == '0' && length != c_IBTNID_LENGTH )
		memmove( IDstr, IDstr + 1, --length );

	IDstr[ length ] = '\0';
	strcpy( BtnID, IDstr );
}

void c_MiniDIU::GetVersion( void )
{
	CreateNmeaMessageAndSend( e_MESSAGE_TYPE::VERSION );
}

void c_MiniDIU::ObjBuzzerControl( uint16_t duration, uint16_t startTmo, e_BUZZER_PATTERN buzzerPattern )
{
	uint8_t retryCnt1 = 0;
	uint8_t retryCnt2 = 0;

	osApiTimerStop( m_BuzzerTimer, 1000 ); // buzzertimer stop

	if( startTmo == 0 )
		startTmo = 1;

	const struct {
		uint8_t patternCnt;
		uint16_t t_on;
		uint16_t t_off;
	} patternTable[ ] = {
			{ 0, 0, 1000 },		// BUZZER_OFF
			{ 1, 200, 800 },   	// BUZZER_PATTERN_DEFAULT
			{ 1, 100, 100 },  	// BUZZER_PATTERN1
			{ 1, 50, 50 },  	// BUZZER_PATTERN2
			{ 1, 1000, 1000 }	// BUZZER_PATTERN3
	};

	auto pattern = ( duration == 0 ) ? patternTable[ BUZZER_OFF ] : patternTable[ buzzerPattern ];
	if( duration )
	{
		m_active_buzzer_duration = duration;
		duration *= ( 1000.0 / ( pattern.t_on + pattern.t_off ) );
		retryCnt1 = ceil( (double)duration / UINT8_MAX);
		retryCnt2 = duration / retryCnt1;
	}

	CreateNmeaMessageAndSend( e_MESSAGE_TYPE::BUZZER, retryCnt1, pattern.patternCnt, retryCnt2, pattern.t_on, pattern.t_off, startTmo );
}

void c_MiniDIU::ObjLEDControl( e_LED_STATE ledState )
{
	uint8_t patternCnt = 1;

	const struct {
		uint32_t retry;
		uint8_t cnt;
		uint16_t t_on;
		uint16_t t_off;
	} patternTable[ ] = {
			{ 1, 0, 500, 500 },   		// LED OFF
			{ 1000000, 5, 65000, 1 },  	// LED GREEN
			{ 0, 0, 0, 0 },  			// NO COLOR
			{ 1000000, 5, 65000, 1 }	// LED RED
	};

	CreateNmeaMessageAndSend( e_MESSAGE_TYPE::LED, patternTable[ ledState ].retry, patternCnt, patternTable[ ledState ].cnt, patternTable[ ledState ].t_on, patternTable[ ledState ].t_off, 0, ledState );
}

void c_MiniDIU::CreateNmeaMessageAndSend( e_MESSAGE_TYPE msgType, uint32_t retry, uint8_t patternCnt, uint8_t cnt, uint16_t tOn, uint16_t tOff, uint16_t startTmo, uint8_t color )
{
	uint8_t priority = 0;
	uint8_t ledNo = 3;
	uint16_t frequency = 500;
	char msg[ c_TX_BUF_LENGTH ] = { 0 };

	switch( msgType )
	{
	case e_MESSAGE_TYPE::BUZZER:
		m_NMEAParser->CreateNmeaSentence( msg, c_TX_BUF_LENGTH, "$PIMR,,STA,SET,A06,%s,%d,%d,%d,%d,%d,%d,%d", "BUZZ", priority, retry, patternCnt, frequency, cnt, tOn, tOff );
		if( patternCnt )
		{
			memcpy( m_BuzzerMsg, msg, strlen( msg ) );
			osApiTimerUpdate( m_BuzzerTimer, startTmo * 1000 ); // buzzerin calmasi icin gereken bekleme suresi
			return;
		}
		break;
	case e_MESSAGE_TYPE::LED:
		m_NMEAParser->CreateNmeaSentence( msg, c_TX_BUF_LENGTH, "$PIMR,,STA,SET,A06,%s,%d,%d,%d,%d,%d,%d,%d,%d", "LED", priority, ledNo, retry, patternCnt, color, cnt, tOn, tOff );
		break;
	case e_MESSAGE_TYPE::VERSION:
		m_NMEAParser->CreateNmeaSentence( msg, c_TX_BUF_LENGTH, "$PIMR,,SYS,GET,A06,%s,A0", "VER" );
		break;
	default:
		return;
	}
	DEBUG_PRINT( "Transmit->%d:%s", TAG, msgType, msg );
	m_hw.m_main_uart->Transmit( (uint8_t *)msg, strlen( msg ), 100 );
}

bool c_MiniDIU::StrToUint32AndValidate4Byte( const char *str, uint32_t *out )
{
	uint32_t result = 0;

	if( str == NULL || *str == '\0' )
		return false;

	while( *str )
	{
		if( *str < '0' || *str > '9' )
			return false;

		uint8_t digit = ( uint8_t )( *str - '0' );

		// overflow check
		if( result > ( UINT32_INVALID - digit ) / 10 )
			return false;

		result = ( result * 10 ) + digit;
		str++;
	}
	// 4 byte check
	if( result < 0x01000000 )   // 16777216
		return false;

	*out = result;
	return true;
}

void c_MiniDIU::ObjSendBuzzerMsg( void )
{
	DEBUG_PRINT( "Transmit->BuzzerMsg:%s", TAG, m_BuzzerMsg );
	m_hw.m_main_uart->Transmit( ( uint8_t * )m_BuzzerMsg, strlen( m_BuzzerMsg ), 100 );
	DEBUG_PRINT( "Play Buzzer for %d seconds", TAG, m_config_diu_base.buzzer_duration );
	if( m_config_diu_base.buzzer_duration == UINT8_INVALID && m_active_buzzer_duration == m_config_diu_base.buzzer_duration )
	{
		osApiDelay( 1000 );	// Waiting time for minidiu to play again
		osApiTimerUpdate( m_BuzzerTimer, UINT8_INVALID * 1000 );
	}
}

c_SharedList * c_MiniDIU::getSharedStructList( void )
{
	return m_struct_list;
}

ARV_StatusTypeDef c_MiniDIU::ObjSendMsgToActMgr( s_action_message_t send_message )
{
	return SendMessage( send_message, e_MSG_DEST::TO_MANAGER, c_Manager::ACTION_MANAGER );
}

void c_MiniDIU::InitUART( void )
{
	m_hw.m_main_uart->Reinit( 10 );
	m_hw.m_main_uart->Receive( m_main_dma_buffer, c_RX_BUF_LENGTH, c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT );
}

