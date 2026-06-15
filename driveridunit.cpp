/**
 * driveridunit.cpp
 *
 *  Created on: 10 Dec 2019
 *      Author: serdar.guney
 */
#include "driveridunit.h"
#include "configdriverlist.h"
#include "dataio.h"
#include "debugprintf.h"
#include "crcdevice.h"
#include "filesystem.h"
#include "datartc.h"

#if defined TAG
#undef TAG
#endif
#define TAG	"DRV__ID"
//#define TAG	"""

#define PlayBuzzerForDriverCheckinViolation( );	ObjBuzzerControl( m_config_diu_base.buzzer_duration, m_config_diu_base.buzzer_start_tmo, e_BUZZER_PATTERN::BUZZER_PATTERN_DEFAULT );	// PlayBuzzerForDriverCheckinViolation( );

void BITTimerCB( osTimerId xTimer );
void BuzzerTimerCB( osTimerId xTimer );
void PersonnelTimerCB( osTimerId xTimer );

/*!
 * \fn  c_DriverIDUnit( c_SharedList *p_st_list )
 * \brief
 * \param p_st_list
 */
c_DriverIDUnit::c_DriverIDUnit( c_SharedList *p_st_list, bool useBITTimer )
{
	m_struct = p_st_list;
	m_struct->addInstance( e_CLASSES::DATA_DRIVERLIST, static_cast < c_DriverIDUnit* > ( this ) );

	if( useBITTimer )
	{
		m_BITTimer = osApiTimerCreate( "BITTIMER", 1000, osTimerPeriodic, this, BITTimerCB );
		m_BITTimerSemaphore = osApiSemaphoreCreate( NULL, 1 );
		osApiSemaphoreWait( m_BITTimerSemaphore, 1 );
	}
	m_BuzzerTimer = osApiTimerCreate( "BUZZERTIMER", 1000, osTimerOnce, this, BuzzerTimerCB );
	m_BuzzerTimerSemaphore = osApiSemaphoreCreate( NULL, 1 );
	osApiSemaphoreWait ( m_BuzzerTimerSemaphore, 1 );
	m_PersonnelTimer = osApiTimerCreate( "PERSONNELTIMER", c_PERSONNEL_LED_SHOWTIME * 1000, osTimerOnce, this, PersonnelTimerCB );
	m_PersonnelTimerSemaphore = osApiSemaphoreCreate( NULL, 1 );
	osApiSemaphoreWait( m_PersonnelTimerSemaphore, 1 );

	auto driver_data = getStruct( );
	driver_data.auto_checkout_status = e_AUTO_CHECKOUT_STATUS::AUTO_CHECKOUT_PASSIVE;
	memset( driver_data.active_id, 0, sizeof( driver_data.active_id ) );
	setStruct( driver_data );
	m_DriverListConfig = c_DriverListConfig::GetInstance();
	m_config_diu_base = { };
	memset( m_driverIDBufstr, 0, MAX_DRIVER_ID_LENGTH );
	UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND::STOP_TIMER );
}

/*!
 * \fn void LoopController( QueueSetMemberHandle_t activatedMember )
 * \brief
 * \return
 */
void c_DriverIDUnit::LoopController( QueueSetMemberHandle_t activatedMember )
{
	if( activatedMember == m_BITTimerSemaphore )
	{
		if( osApiSemaphoreWait( m_BITTimerSemaphore, c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT ) == osOK )
		{
			ObjCommHealthCheck( );
		}
	}
	else if( activatedMember == m_BuzzerTimerSemaphore )
	{
		if( osApiSemaphoreWait( m_BuzzerTimerSemaphore, c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT ) == osOK )
		{
			ObjSendBuzzerMsg( );
		}
	}
	else if( activatedMember == m_PersonnelTimerSemaphore )
	{
		if( osApiSemaphoreWait( m_PersonnelTimerSemaphore, c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT ) == osOK )
		{
			ObjLEDControl( e_LED_STATE::LED_RED );
		}
	}

	ProcessIgnitionAndDriverState( );
}

/*!
 * \fn c_DriverIDUnit::e_DRIVERLIST_ID_MATCH_RESULT CheckDriverList( void )
 * \brief
 * \return
 */
c_DriverIDUnit::e_DRIVERLIST_ID_MATCH_RESULT c_DriverIDUnit::CheckDriverList( void )
{
	c_FileSystem * fs = c_FileSystem::GetInstance();
	spiffs_file fd = -1;
	int8_t retry_count = 3;
	e_DRIVERLIST_ID_MATCH_RESULT status = e_DRIVERLIST_ID_MATCH_RESULT::LIST_NOT_FOUND;
	int32_t file_size = 0;

	file_size = fs->GetFileSize2( "driverlist.h" );	// get file size
	if( file_size <= 0 )
	{	// get file size error or file isempty
		return status;
	}
	if( !m_DriverListConfig->ChangeMutexState( true ) )
	{// take mutex error
		ERR_PRINT( "Driver List mutex error", TAG );
		return status;
	}
	// try to open file
	do {
		fd = fs->Open( "driverlist.h", e_FsReadWrite::ARV_READ, "cfgdrvlist" );
	}while( fd <= 0 && (--retry_count ) > 0 );

	if( fd > 0 )
	{
		if( m_DriverListConfig->SearchIdInFile( ( uint8_t* )m_activeIDstr, strlen( m_activeIDstr ), fs, fd, file_size) >= 0 )
		{
			osApiDelay( 10 );
			status = e_DRIVERLIST_ID_MATCH_RESULT::ID_MATCHED; // ID match found
		}
		else
		{
			status = e_DRIVERLIST_ID_MATCH_RESULT::NO_MATCH;
		}
		retry_count = 3;
		while( --retry_count > 0 && ( fs->Close( fd ) != ARV_OK ) );	// close file
	}
	else
	{
		DEBUG_PRINT( "DriverID list NOT found", TAG );
		status = e_DRIVERLIST_ID_MATCH_RESULT::LIST_NOT_FOUND; // list error
	}
	m_DriverListConfig->ChangeMutexState( false );

	return status;
}

/*!
 * \fn void CheckDriverID( void )
 * \brief
 */
void c_DriverIDUnit::CheckDriverID( void )
{
	static uint32_t card_read_cnt = 0;

	// lokal blocking aktif iken id listede yoksa herhangi bir islem yapilmayacak
	if( m_config_diu_base.block_option == e_DIU_VEHICLE_BLOCK_OPTION::LOCAL_BLOCKING && CheckDriverList() == e_DRIVERLIST_ID_MATCH_RESULT::NO_MATCH )
	{
		memset( m_activeIDstr, 0, MAX_DRIVER_ID_LENGTH );
		memcpy( m_activeIDstr, m_driverIDBufstr, MAX_DRIVER_ID_LENGTH );
		e_IO_STATUS io_status = getSharedStructList()->getInstance< s_io_data_t > ( e_CLASSES::DATA_IGNITION )->getStruct().io_status;
		if( io_status == e_IO_STATUS::IO_ACTIVE && !m_DriverActiveFlag ) // ignition aktif ve surucu karti basmamissa buzzer otmeye hazirlanacak
		{
			PlayBuzzerForDriverCheckinViolation( );
			DEBUG_PRINT( "Unknown driver.Play buzzer %d seconds later", TAG, m_config_diu_base.buzzer_start_tmo );
		}
		return;
	}

	if( !memcmp( m_driverIDBufstr, m_activeIDstr, sizeof( m_activeIDstr ) ) && m_config_diu_base.usage_method == e_DIU_USAGE_METHOD::DRIVER_CONTROL )
	{  // id eslesmesi ve driver control aktifse bu surucuye cikis yaptir
		s_io_data_t ignData = getSharedStructList()->getInstance< s_io_data_t > ( e_CLASSES::DATA_IGNITION )->getStruct( );
		if( ignData.io_status == e_IO_STATUS::IO_ACTIVE && m_DriverActiveFlag ) // kontak acik ve aktif driver ayni kart. çıkış yapmayalım
		{
			if( m_isBuzzerActive )
			{
				INFO_PRINT( "BuzzerActive, Resend PlayBuzzer", TAG );
				ObjSendBuzzerMsg( );
			}
			INFO_PRINT( "CheckOUT not suitable while driving %s Card Read Cnt :%d", TAG, m_activeIDstr, ++card_read_cnt );
			return;
		}
		INFO_PRINT( "CheckOUT for driver : %s Card Read Cnt :%d", TAG, m_activeIDstr, ++card_read_cnt );
		// bu id icin cikis mesaji at
		memset( m_driverIDBufstr, 0, sizeof( m_driverIDBufstr ) );
		SetDriverActiveState( false );
		UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND::STOP_TIMER ); // zaten cikis yaptik buna gerek kalmadi

		auto driver_data = getStruct( );
		driver_data.auto_checkout_status = e_AUTO_CHECKOUT_STATUS::AUTO_CHECKOUT_PASSIVE;
		driver_data.driver_status = e_DRIVER_STATUS::DRIVER_CHECKOUT;
		memset( driver_data.active_id, 0, sizeof( driver_data.active_id ) );
		setStruct( driver_data );
		osApiDelay( 100 );
		SaveDriverID( driver_data.active_id );
		SetBuzzerActiveState( false );
		SendStatusMessage( e_DRIVER_STATUS::DRIVER_CHECKOUT );
	}
	else // giris
	{
		CheckIn( card_read_cnt );
	}
}

/*!
 * \fn void AutoCheckout( void )
 * \brief
 */
void c_DriverIDUnit::AutoCheckout( void )
{
	uint32_t sys_tick = osApiKernelSysTick( );
	static uint32_t last_control_time = 0;

	if( sys_tick < last_control_time || ( sys_tick - last_control_time ) >= pdMS_TO_TICKS( 1000 ) )
	{
		last_control_time = sys_tick;
	}
	else
	{
		return;	// control just once at every second
	}

	if( m_autochekout_start_time == 0 || m_autochekout_start_time == UINT64_MAX )
	{
		return; // there is no active auto checkout timer
	}

	auto inst = getSharedStructList()->getInstance< s_rtc_data_t > ( e_CLASSES::DATA_RTC );
	if( inst == nullptr )
	{
		return;	// we couldn't read rtc data struct
	}

	int64_t checkout_time = inst->getStruct().timestamp - m_autochekout_start_time;	// get ms from autochekout start to now
	if( checkout_time < 0 )
	{
		return; // there is a problem with rtc timestamp
	}

	checkout_time /= 1000;	// convert ms to second
	if( checkout_time < m_config_diu_base.checkout_tmo )
	{
		return;		// it has not been checkout time yet
	}
	// Checkout current driver
	// cikis icin mesaj vs. gerekli islemleri yap
	memset( m_driverIDBufstr, 0, MAX_DRIVER_ID_LENGTH );
	SetDriverActiveState( false ); // artik aktif surucu yok. Otomatik cikis yapildi artik led kirmizi yansin.
	SetBuzzerActiveState( false );
	INFO_PRINT( "Auto Checkout Timer fired.Checkout for driver : %s", TAG, m_activeIDstr );
	SendStatusMessage( e_DRIVER_STATUS::DRIVER_CHECKOUT );

	auto driver_data = getStruct( );
	driver_data.auto_checkout_status = e_AUTO_CHECKOUT_STATUS::AUTO_CHECKOUT_PASSIVE;
	driver_data.driver_status = e_DRIVER_STATUS::DRIVER_CHECKOUT;
	memset( driver_data.active_id, 0, sizeof( driver_data.active_id ) );
	setStruct( driver_data );
	SaveDriverID( driver_data.active_id );
	UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND::STOP_TIMER );
}

/*!
 * \fn void InstallAutoCheckoutTimer( void )
 * \brief
 */
void c_DriverIDUnit::InstallAutoCheckoutTimer( void )
{
	UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND::START_TIMER );
	auto driver_data = getStruct( );
	driver_data.auto_checkout_status = e_AUTO_CHECKOUT_STATUS::AUTO_CHECKOUT_ACTIVE;
	setStruct( driver_data );
	DEBUG_PRINT( "Ignition off %d seconds later automatic checkout will be done", TAG, m_config_diu_base.checkout_tmo );
}

void c_DriverIDUnit::CheckIn( uint32_t card_read_cnt )
{
	if( m_DriverActiveFlag && m_config_diu_base.usage_method == e_DIU_USAGE_METHOD::DRIVER_CONTROL )
	{  // mevcut surucu varsa ve driver control aktifse onceki surucuye cikis yaptir
		SetBuzzerActiveState( false );
		SendStatusMessage( e_DRIVER_STATUS::DRIVER_CHECKOUT, m_driverIDBufstr );
		INFO_PRINT( "CheckOUT for driver : %s", TAG, m_driverIDBufstr );
	}

	if( m_config_diu_base.usage_method == e_DIU_USAGE_METHOD::PERSONNEL_CONTROL )
	{	// personel modunda calisiyorsa eger led 2 sn lik yesil yanip tekrar kirmizi yanar.Timer bu yüzden baslar
		osApiTimerStart( m_PersonnelTimer, 100 );
		DEBUG_PRINT( "Personnel Timer Start", TAG );
	}
	// driverid icin giris mesaji at
	INFO_PRINT( "CheckIN for driver : %s Card Read Cnt:%d", TAG, m_activeIDstr, ++card_read_cnt );
	memcpy( m_driverIDBufstr, m_activeIDstr, sizeof( m_activeIDstr ) );
	SetDriverActiveState( true );
	//ledi yesil yak
	//buzzer aktif ise kapat
	UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND::STOP_TIMER );	// zaten giris yaptik buna gerek kalmadi
	auto driver_data = getStruct( );
	driver_data.auto_checkout_status = e_AUTO_CHECKOUT_STATUS::AUTO_CHECKOUT_PASSIVE;
	driver_data.driver_status = e_DRIVER_STATUS::DRIVER_CHECKIN;
	strcpy( (char *)driver_data.active_id, (const char *)m_activeIDstr );
	setStruct( driver_data );
	osApiDelay( 100 );
	StopBuzzer( );
	SaveDriverID( driver_data.active_id );
	SendStatusMessage( e_DRIVER_STATUS::DRIVER_CHECKIN );

	s_io_data_t ignData =  getSharedStructList()->getInstance< s_io_data_t > ( e_CLASSES::DATA_IGNITION )->getStruct( );
	if ( (uint8_t)ignData.io_status == e_IO_STATUS::IO_PASSIVE && m_config_diu_base.checkout_option == e_DIU_CHECKOUT_OPTION::AUTOMATIC_CHECKOUT )
	{ // ignition pasifken surucu giris yaparsa ve oto checkout aktifse	otomatik cikis icin hazirlan
		InstallAutoCheckoutTimer( );
	}
}

/*!
 * @brief update auto checkout timer
 * @param command : 0 -> stop, 1 -> start/restart
 */
void c_DriverIDUnit::UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND command )
{
	m_autochekout_start_time = UINT64_MAX;

	if( command == e_AUTO_CHECKOUT_TIMER_COMMAND::STOP_TIMER || m_config_diu_base.usage_method == e_DIU_USAGE_METHOD::PERSONNEL_CONTROL )
	{
		return;
	}
	// start or restart timer block
	int8_t retry_count = 3;
	do {
		auto inst = getSharedStructList()->getInstance< s_rtc_data_t > ( e_CLASSES::DATA_RTC );
		if( inst != nullptr )
		{
			m_autochekout_start_time = inst->getStruct().timestamp;
		}
	}while( --retry_count > 0 || m_autochekout_start_time == UINT64_MAX || m_autochekout_start_time == 0 );
}

/*!
 * \fn void SendStatusMessage( e_DRIVER_STATUS driverStatus, char *id )
 * \brief Gelen surucu id ve status'e gore arac blokaj uygulanip uygulanmayacagina karar verir
 * \param driverStatus
 * \param id
 */
void c_DriverIDUnit::SendStatusMessage( e_DRIVER_STATUS driverStatus, char *id )
{
	s_action_message_t send_message;
	send_message.query_response = RANDOM_QUERY;
	send_message.driver_status = ( uint8_t )driverStatus;
	e_DRIVERLIST_ID_MATCH_RESULT ID_match_result = CheckDriverList( );

	if( m_config_diu_base.block_option == e_DIU_VEHICLE_BLOCK_OPTION::LOCAL_BLOCKING ) // if local blocking is active . check driverlist
	{
		if( ( driverStatus == e_DRIVER_STATUS::DRIVER_CHECKIN && ID_match_result == e_DRIVERLIST_ID_MATCH_RESULT::ID_MATCHED ) || ID_match_result == e_DRIVERLIST_ID_MATCH_RESULT::LIST_NOT_FOUND ) // ID matched and checkin
		{
			send_message.command = s_action_message_t::LOCAL_UNBLOCK_VEHICLE; // unlock engine. vehicle ready to go
		}
		else  // no match or checkout
		{
			send_message.command = s_action_message_t::LOCAL_BLOCK_VEHICLE; // not allowed to go
		}
	}
	else
	{
		send_message.command = s_action_message_t::NO_STOP_COMMAND;
	}

	if( id == nullptr )
	{
		strcpy( (char *)send_message.driver_id, (const char *)m_activeIDstr );
	}
	else
	{
		strcpy( (char * )send_message.driver_id, (const char *)id );
	}

	if( ObjSendMsgToActMgr( send_message ) == ARV_ERROR )
	{
		ERR_PRINT( "can not send diu msg to Action manager", TAG );
	}
}

/**
 * hex2int
 * take a hex string and convert it to a 64bit number (max 16 hex digits)
 */
uint64_t c_DriverIDUnit::Hex2Int( char *hex, uint8_t size )
{
	uint64_t _result = 0;
	uint64_t _resultPtr = reinterpret_cast< uint64_t >( &_result );

	for( int i = 0; i < size; i+= 2 )
	{
		int _multiplierFirstValue = 0, _addonSecondValue = 0;

		char _firstChar = hex[ i ];
		if( _firstChar >= 0x30 && _firstChar <= 0x39 )
			_multiplierFirstValue = _firstChar - 0x30;
		else if( _firstChar >= 0x41 && _firstChar <= 0x46 )
			_multiplierFirstValue = 10 + ( _firstChar - 0x41 );

		char _secndChar = hex[ i + 1 ];
		if( _secndChar >= 0x30 && _secndChar <= 0x39 )
			_addonSecondValue = _secndChar - 0x30;
		else if( _secndChar >= 0x41 && _secndChar <= 0x46 )
			_addonSecondValue = 10 + ( _secndChar - 0x41 );

		*(uint8_t *)( _resultPtr + ( size / 2 ) - ( i / 2 ) - 1 ) = (uint8_t)( _multiplierFirstValue * 16 + _addonSecondValue );
	}

	return _result;
}

/*!
 * \fn ARV_StatusTypeDef SaveDriverID( uint8_t *driverID )
 * \brief
 * \param driverID
 * \return
 */
ARV_StatusTypeDef c_DriverIDUnit::SaveDriverID( uint8_t *driverID )
{
	c_FileSystem * f = c_FileSystem::GetInstance();						// file system instancer
	c_CrcDevice * crc = c_CrcDevice::Instance();						// crc instance
	uint16_t crc_value = 0;

	if( f == nullptr || crc == nullptr )
	{
		ERR_PRINT( "ActiveID FILE SAVE ERROR nullptr", TAG );
		return ARV_StatusTypeDef::ARV_ERROR;
	}

	spiffs_file fd = f->Open( "ActiveID", e_FsReadWrite::ARV_WRITE , "DriverIDUnit" );
	if( fd > 0 && f->Write ( fd, driverID, sizeof( m_activeIDstr ) ) == ARV_StatusTypeDef::ARV_OK )
	{
		crc_value = (uint16_t)crc->Calculate( (uint8_t *)driverID,  sizeof( m_activeIDstr ) );
		if( f->Write( fd, &crc_value, sizeof( crc_value ) ) == ARV_StatusTypeDef::ARV_OK && f->Close( fd ) == ARV_StatusTypeDef::ARV_OK )
		{
			return ARV_StatusTypeDef::ARV_OK;
		}
	}
	else
	{
		ERR_PRINT( "ActiveID FILE SAVE ERROR ", TAG );
	}
	if( fd > 0 )
	{					// dosya acilmis fakat kapatilmadan hata alinmis ise dosyayi kapat
		f->Close( fd );
	}

	return ARV_OK;
}

/*!
 * \fn ARV_StatusTypeDef LoadDriverID( void )
 * \brief
 * \return
 */
ARV_StatusTypeDef c_DriverIDUnit::LoadDriverID( void )
{
	c_FileSystem * f = c_FileSystem::GetInstance();						// file system instance
	c_CrcDevice * crc = c_CrcDevice::Instance();						// crc instance
	uint16_t crc_value = 0;

	auto driver_data = getStruct( );
	if( f == nullptr || crc == nullptr )
	{
		ERR_PRINT( "ActiveID FILE LOAD ERROR nullptr", TAG );
		memset( driver_data.active_id, 0, sizeof( driver_data.active_id ) );
		setStruct( driver_data );
		return ARV_StatusTypeDef::ARV_ERROR;	//Dummy return for X2
	}

	spiffs_file fd = f->Open( "ActiveID", e_FsReadWrite::ARV_READ , "DriverIDUnit" );
	if( fd > 0 && f->Read( fd, m_activeIDstr, sizeof( m_activeIDstr ) ) == ARV_StatusTypeDef::ARV_OK  &&
	f->Read( fd, &crc_value, sizeof( crc_value ) ) == ARV_StatusTypeDef::ARV_OK && f->Close( fd ) == ARV_StatusTypeDef::ARV_OK )
	{
		fd = -1;
		if( crc_value == ( uint16_t )crc->Calculate( (uint8_t *)m_activeIDstr, sizeof( m_activeIDstr ) ) )
		{
			if( !memcmp( m_activeIDstr, m_zeroBuf, sizeof( m_zeroBuf ) ) ) // surucu ID si 0 ise (surucu yok)
			{
				SetDriverActiveState( false );
				driver_data.driver_status = e_DRIVER_STATUS::DRIVER_CHECKOUT;
				DEBUG_PRINT( "No Active Driver Before Reset", TAG );
			}
			else
			{
				if( m_config_diu_base.usage_method == e_DIU_USAGE_METHOD::PERSONNEL_CONTROL )
					SetDriverActiveState( false );
				else
					SetDriverActiveState( true );
				driver_data.driver_status = e_DRIVER_STATUS::DRIVER_CHECKIN;
				memcpy( m_driverIDBufstr, m_activeIDstr, sizeof( m_activeIDstr ) );
				DEBUG_PRINT( "Loaded Driver ID from extflash = %s", TAG, m_activeIDstr );
			}
			memcpy( driver_data.active_id, m_activeIDstr, sizeof( m_activeIDstr ) );
			setStruct( driver_data );
			return ARV_StatusTypeDef::ARV_OK;//Dummy return for X2
		}
		else
		{
			ERR_PRINT("ActiveID FILE LOAD ERROR", TAG);
			memset(driver_data.active_id, 0, sizeof(driver_data.active_id));
		}
	}
	else
	{
		ERR_PRINT( "ActiveID FILE LOAD ERROR", TAG );
		memset( driver_data.active_id, 0, sizeof( driver_data.active_id ) );
	}
	if( fd > 0 )
	{					// dosya acilmis fakat kapatilmadan hata alinmis ise dosyayi kapat
		f->Close( fd );
	}
	setStruct(driver_data);

	return ARV_OK;//Dummy return for X2
}

void c_DriverIDUnit::SetDriverStatusLed( void )
{
	( m_DriverActiveFlag ) ? ObjLEDControl( e_LED_STATE::LED_GREEN ) : ObjLEDControl( e_LED_STATE::LED_RED );
}

void c_DriverIDUnit::SetDriverActiveState( bool state )
{
	if( state )
	{
		m_DriverActiveFlag = true;
		ObjLEDControl( e_LED_STATE::LED_GREEN );
	}
	else
	{
		m_DriverActiveFlag = false;
		ObjLEDControl( e_LED_STATE::LED_RED );
	}
}

void c_DriverIDUnit::SetBuzzerActiveState( bool state )
{
	m_isBuzzerActive = state;
	if( !state )
	{
		StopBuzzer( );
	}
}

/*!
 * \fn void ProcessIgnitionAndDriverState( void )
 * \brief
 * \return
 */
void c_DriverIDUnit::ProcessIgnitionAndDriverState( void )
{
	static uint8_t ignStatusBuf = UINT8_INVALID;	// unkown state at first time
	s_io_data_t ignData = getSharedStructList()->getInstance< s_io_data_t > ( e_CLASSES::DATA_IGNITION )->getStruct();

	if( ignStatusBuf != (uint8_t)ignData.io_status )
	{
		if( ignData.io_status == e_IO_STATUS::IO_ACTIVE && m_DriverActiveFlag )
		{
			UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND::STOP_TIMER );	// ignition var ve surucu aktif yanlislikla cikis yapmayalim
			auto driver_data = getStruct( );
			driver_data.auto_checkout_status = e_AUTO_CHECKOUT_STATUS::AUTO_CHECKOUT_PASSIVE;
			setStruct( driver_data );
			DEBUG_PRINT( "Ignition ON. Driver card has been read.", TAG );
		}
		else if( ignData.io_status == e_IO_STATUS::IO_ACTIVE && !m_DriverActiveFlag ) // ignition aktif ve surucu karti basmamissa buzzer otmeye hazirlanacak
		{
			PlayBuzzerForDriverCheckinViolation( );
			DEBUG_PRINT( "Ignition ON but NO driver.Turn ON red led and play buzzer %d seconds later", TAG, m_config_diu_base.buzzer_start_tmo );
			ObjLEDControl( e_LED_STATE::LED_RED );	// led kirmizi yansin
		}
		else if( ignData.io_status == e_IO_STATUS::IO_PASSIVE && m_DriverActiveFlag ) // ignition pasif ve surucu cikis yapmadiysa otomatik cikis icin kontrol yap
		{
			if( m_config_diu_base.checkout_option == e_DIU_CHECKOUT_OPTION::AUTOMATIC_CHECKOUT )
			{
				InstallAutoCheckoutTimer( );
				DEBUG_PRINT( "Ignition OFF.The driver card has been read.Ready for Auto Checkout", TAG );
			}
			else
			{
				DEBUG_PRINT(" Ignition OFF and driver OK.Waiting for manuel Checkout ", TAG );
			}
		}
		else if( ignData.io_status == e_IO_STATUS::IO_PASSIVE && !m_DriverActiveFlag ) // ignition pasif ve surucu yoksa baslangic durumuna hazir ol
		{
			ObjLEDControl( e_LED_STATE::LED_RED );
			UpdateAutoCheckoutTimer( e_AUTO_CHECKOUT_TIMER_COMMAND::STOP_TIMER );	// zaten checkout yaptik buna gerek kalmadi
			auto driver_data = getStruct( );
			driver_data.auto_checkout_status = e_AUTO_CHECKOUT_STATUS::AUTO_CHECKOUT_PASSIVE;
			setStruct( driver_data );
			osApiDelay( 100 );
			StopBuzzer( );
			DEBUG_PRINT( "Ignition OFF and NO driver. Back to initial state ", TAG );
		}
		ignStatusBuf = (uint8_t)ignData.io_status;
	}
	AutoCheckout( );		// Control Auto Checkout
}

/*!
 * \fn void ObjCommHealthCheck( void )
 * \brief Virtual Function
 */
void c_DriverIDUnit::ObjCommHealthCheck( void )
{

}

/*!
 * \fn void ControlAutoCheckoutTimeout( void )
 * \brief
 */
void c_DriverIDUnit::ControlAutoCheckoutTimeout( void )
{
	if( m_config_diu_base.checkout_option == e_DIU_CHECKOUT_OPTION::AUTOMATIC_CHECKOUT && m_config_diu_base.checkout_tmo == 0 )
	{
		m_config_diu_base.checkout_tmo = 1;
	}
	if( m_config_diu_base.block_option == e_DIU_VEHICLE_BLOCK_OPTION::LOCAL_BLOCKING && m_config_diu_base.checkout_tmo < 15 )
	{
		m_config_diu_base.checkout_tmo = 15;	//local blocking ise role baglantisi kontagi ayirdigi icin hemen cikis yaparsak deadlock'a giriyor
	}
}

void BITTimerCB( osTimerId xTimer )
{
	osApiSemaphoreRelease( static_cast < c_DriverIDUnit* >( osApiGetTimerIDFromHandle( xTimer ) )->m_BITTimerSemaphore );
}

void BuzzerTimerCB( osTimerId xTimer )
{
	osApiSemaphoreRelease( static_cast < c_DriverIDUnit* >( osApiGetTimerIDFromHandle( xTimer ) )->m_BuzzerTimerSemaphore );
}

void PersonnelTimerCB( osTimerId xTimer )
{
	osApiSemaphoreRelease( static_cast < c_DriverIDUnit* >( osApiGetTimerIDFromHandle( xTimer ) )->m_PersonnelTimerSemaphore );
}

#if 0
/*!
 * \fn void TESTFillDriverList( void )
 * \brief
 */
void c_DriverIDUnit::TESTFillDriverList( void )
{
	uint8_t message[] =  "69000018F5832B01";
	uint8_t message2[] = "82000018F4E15901";
	uint8_t message3[] = "69000018F5832B02";
	uint8_t message4[] = "82000018F4E15902";
	uint8_t message5[] = "69000018F5832B03";
	uint8_t message6[] = "82000018F4E15903";
	uint8_t message7[] = "69000018F5832B04";
	uint8_t message8[] = "82000018F4E15904";
	uint8_t message9[] = "69000018F5832B05";
	uint8_t message10[] ="82000018F4E15905";
	//	uint8_t message11[] ="69000018F5832B06";
	//	uint8_t message12[] ="82000018F4E15906";
	//	uint8_t message13[] ="69000018F5832B07";
	//	uint8_t message14[] ="82000018F4E15907";
	//	uint8_t message15[] ="69000018F5832B08";
	//	uint8_t message16[] ="82000018F4E15908";
	//	uint8_t message17[] ="69000018F5832B09";
	//	uint8_t message18[] ="82000018F4E15909";
	//	uint8_t message19[] ="69000018F5832B10";
	//	uint8_t message20[] ="82000018F4E15910";
	//	uint8_t message21[] ="69000018F5832B11";
	//	uint8_t message22[] ="82000018F4E15911";
	//	uint8_t message23[] ="69000018F5832B12";
	//	uint8_t message24[] ="82000018F4E15912";
	//	uint8_t message25[] ="69000018F5832B13";
	//	uint8_t message26[] ="82000018F4E15913";
	//	uint8_t message27[] ="69000018F5832B14";
	//	uint8_t message28[] ="82000018F4E15914";
	m_DriverListConfig->AddDriverId(message, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message2, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message3, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message4, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message5, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message6, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message7, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message8, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message9, sizeof(message)-1);
	m_DriverListConfig->AddDriverId(message10, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message11, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message12, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message13, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message14, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message15, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message16, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message17, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message18, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message19, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message20, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message21, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message22, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message23, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message24, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message25, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message26, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message27, sizeof(message)-1);
	//	m_DriverListConfig->AddDriverId(message28, sizeof(message)-1);
}
#endif
