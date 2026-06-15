/**
 * actionmanager.cpp
 *  Created on: Jan 4, 2019
 *      Author: ugur.aydin
 */

#include "actionmanager.h"
#include "configpower.h"
#include "datavehicle.h"
#include "datagnss.h"
#include "filesystem.h"
#include "arvbinaryprotocol.h"
#include "datagsm.h"
#include "configsaveload.h"
#include "datadriverlist.h"
#include "productubloxgnss.h"

#if defined TAG
#undef TAG
#endif
#define TAG	"ACT_MGR"
//#define TAG ""

/*!
 * \def VEHICLE_STOP_CONTROL_PERIOD
 * \brief Vehicle block control timer period (milliseconds)
 */
#define VEHICLE_STOP_CONTROL_PERIOD		1000
/*!
 * \def VEHICLE_STOP_TIMEOUT
 * \brief Vehicle block timeout value (milliseconds)
 */
#define VEHICLE_STOP_TIMEOUT			30000

static const uint16_t c_QUEUESET_WAIT_TIME = 1000 / TICK_PERIOD_MS;
static const uint8_t c_QUEUE_SIZE = 10;
static const uint16_t c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT = 10 / TICK_PERIOD_MS;

void VehicleStopTimerCB( osTimerId xTimer );

/*!
 * \fn  c_ActionManager(c_SharedList*, const char*, e_MANAGERS, unsigned short, char)
 * \brief Constructor
 * \param p_st_list
 * \param p_name
 * \param id
 * \param stack_depth
 * \param priority
 */
c_ActionManager::c_ActionManager(c_SharedList * p_st_list, const char *p_name, e_MANAGERS id, unsigned short stack_depth, char priority) : c_SingletonManager ( p_st_list, p_name, id, stack_depth, priority )
{
	CreateQueues<s_action_message_t>();
	m_struct_list->addInstance( e_CLASSES::DATA_VEHICLE_STOP,static_cast < c_StructBase * > ( this ) );
	m_stop_timer = osApiTimerCreate( "VST", VEHICLE_STOP_TIMEOUT, osTimerOnce, this, VehicleStopTimerCB );
	m_protocol = new c_ArvBinaryProtocol( p_st_list );
	s_manager_info.backup.query_number = RANDOM_QUERY;
	s_manager_info.backup.blocking_state = e_VEHICLE_STOP_STATES::NO_STOP_ACTION;
	s_manager_info.relay_available = false;

	m_data_struct.local_blocking_state = e_LOCAL_BLOCKING_STATE::INVALID_LOCAL_BLOCKING;
	m_data_struct.vehicle_blocking_state = e_VEHICLE_STOP_STATES::NO_STOP_ACTION;
	setStruct( m_data_struct );

	m_queueset = osApiQueueSetCreate( c_BINARY_SEMAPHORE_SIZE + ( c_QUEUE_SIZE * 2 ) );
	m_vehicle_stop_Semaphore = osApiSemaphoreCreate( NULL, c_BINARY_SEMAPHORE_SIZE );
	osApiSemaphoreWait( m_vehicle_stop_Semaphore, 1 );
	osApiAddToQueueSet( m_ReceiveMessageQueue, m_queueset );
	osApiAddToQueueSet( m_vehicle_stop_Semaphore, m_queueset );
}

/*!
 * @brief insert config classes
 * @param managers_config_map
 */
void c_ActionManager::getConfigMap(std::multimap<e_CLASSES, e_MANAGERS> &managers_config_map )
{
	/*!
	 * @brief STB de arac durdurma olacaksa role configurasyonu da gelecegi icin diger config class larini
	 * eklemeye gerek kalmadi
	 */
	std::map< e_CLASSES, e_MANAGERS > config_map;
	config_map.insert( std::pair<e_CLASSES, e_MANAGERS > ( e_CLASSES::CONFIG_POWER, this->Id() ) );
	managers_config_map.insert( config_map.begin(), config_map.end() );
}

/*!
 * \fn void ChangeVehicleStopState(s_action_message_t::e_ACTION_MNG_COMMANDS, uint16_t)
 * \brief Arac blokaj durumunu gunceller, gelen komuta gore gerekli islemleri yapar
 * \param command action manager command
 * \param query query response
 */
void c_ActionManager::ChangeVehicleStopState( s_action_message_t::e_ACTION_MNG_COMMANDS command, uint16_t query )
{
	s_manager_info.backup.query_number = query;													// query response kaydet
	//TODO: eğer blokaj mesajı geldiğinde işlemi yaparken reset yersek, dosyadan tekrar işleme başladığımızda eski gelen blokaj mesajları iptal ediliyor. Neden yeni blokaj mesajı gelmeye devam ediyor?
	CommandCancelControl( );
	switch( command )
	{
	case s_action_message_t::e_ACTION_MNG_COMMANDS::UNBLOCK_VEHICLE:								// bloklamayi kaldir
		INFO_PRINT( "Unblock vehicle command received, query = %d", TAG, s_manager_info.backup.query_number );
		SaveBlockingState( e_VEHICLE_STOP_STATES::WAITING_RELAY_UNBLOCK_RESPONSE);
		SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_PASSIVE );
		break;

	case s_action_message_t::e_ACTION_MNG_COMMANDS::BLOCK_VEHICLE:									// araci blokla komutu alindi
		INFO_PRINT( "Block vehicle command received, query = %d", TAG, s_manager_info.backup.query_number );
		SaveBlockingState( e_VEHICLE_STOP_STATES::WAITING_FOR_VEHICLE_BLOCK_CONDITIONS );
		break;
	default:
		break;
	}
}

/*!
 * @brief cancel previous command
 */
void c_ActionManager::CommandCancelControl( void )
{
	StopTimeoutTimer();																				// stop vehicle stop timer
	if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_FOR_VEHICLE_BLOCK_CONDITIONS )						// bloklama icin bekleniyor
	{
		SendActionResult( e_COMMAND_RESULTS::VEHICLE_COMMAND_CANCEL );							// bloklama iptal istegi gonder
		Delay( 100 );
	}
}

/*!
 * \fn bool ControlBackupValue(void)
 * \brief Control backup vehicle stop state value after reset
 * \return true : blocking process activated, false : blocking process passive
 */
bool c_ActionManager::ControlBackupValue( void )
{
	ARV_StatusTypeDef result = ARV_StatusTypeDef::ARV_ERROR;
	int8_t retry_cnt = 3;
	spiffs_file fd = 0;
	// control relay state
	s_power_config_t pow_conf = m_struct_list->getInstance< s_power_config_t >( e_CLASSES::CONFIG_POWER )->getStruct();	// control relay power
	if( !pow_conf.device_power_states.fields.RELAY_PWR )											// role aktif degilse anlami yok geri don
	{
		return false;
	}
	else
	{
		s_manager_info.relay_available = true;															// set relay status
	}

	c_FileSystem *  file = c_FileSystem::GetInstance();
	do {
		Delay( 1000 );
		retry_cnt--;
		if( fd <= 0 )
		{
			fd = file->Open( "vehiclestop.h", e_FsReadWrite::ARV_READ , "ActionManager" );			// open file to read
		}
		if( fd == SPIFFS_ERR_NOT_FOUND )
		{
			return false;
		}
		else if( fd > 0 )
		{
			result = file->Read( fd, &s_manager_info.backup, sizeof( s_manager_info.backup ) );		// read backup value
			if( result == ARV_StatusTypeDef::ARV_OK  )												// dosyayi okuyabildik
			{
				if( s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::NO_STOP_ACTION )// resetten once aksiyon olmadı
				{
					SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_PASSIVE );
					m_data_struct.vehicle_blocking_state = e_VEHICLE_STOP_STATES::NO_STOP_ACTION;
				}
				else if( s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::VEHICLE_UNBLOCKED )// resetten once araci bloklaj yoktu
				{
					SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_PASSIVE );
					m_data_struct.vehicle_blocking_state = e_VEHICLE_STOP_STATES::VEHICLE_UNBLOCKED;
				}
				else if( s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::VEHICLE_BLOCKED )// resetten once araci bloklamistik
				{
					SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_ACTIVE );
					m_data_struct.vehicle_blocking_state = e_VEHICLE_STOP_STATES::VEHICLE_BLOCKED;
				}
				else if( s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::WAITING_FOR_VEHICLE_BLOCK_CONDITIONS ||// resetten once araci bloklamaya calisiyorduk
						s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::WAITING_RELAY_BLOCK_RESPONSE ||
						s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::WAITING_ALARM_BLOCK_RESPONSE )//
				{
					ChangeVehicleStopState( s_action_message_t::e_ACTION_MNG_COMMANDS::BLOCK_VEHICLE, s_manager_info.backup.query_number );// blokaj icin komut gonder
				}
				else if( s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::WAITING_RELAY_UNBLOCK_RESPONSE ||// resetten once blokaji kaldirmak icin bekliyorduk
						s_manager_info.backup.blocking_state == e_VEHICLE_STOP_STATES::WAITING_ALARM_UNBLOCK_RESPONSE )//
				{
					ChangeVehicleStopState( s_action_message_t::e_ACTION_MNG_COMMANDS::UNBLOCK_VEHICLE, s_manager_info.backup.query_number );// blokaji kalirmak icin komut gonder
				}
				DEBUG_PRINT( "Action Manager started because of backup value", TAG );
				setStruct( m_data_struct );
			}
			if( c_ConfigSaveLoad::CloseFile( file, fd )== ARV_StatusTypeDef::ARV_OK )								// close file
			{
				return true;
			}
		}
	}while( retry_cnt > 0 && result == ARV_StatusTypeDef::ARV_ERROR );
	return false;
}

/*!
 * @brief Araci durdurmak icin sartlari kontrol eder
 */
void c_ActionManager::ControlVehicleStopState( void )
{
	if( s_manager_info.relay_available )																// control relay channel state from power configuration
	{	// 1
		s_vehicle_data_t vehicle_data = m_struct_list->getInstance< s_vehicle_data_t >( e_CLASSES::DATA_VEHICLE )->getStruct();
		DEBUG_PRINT( "WAITING_FOR_VEHICLE_BLOCK_CONDITIONS", TAG );
		if( vehicle_data.vehicle_status == e_VEHICLE_STATUS::VEHICLE_IDLE )					// arac hareketsiz mi?
		{	// 2
			DEBUG_PRINT( "VEHICLE STATUS OK FOR STOP", TAG );
			s_gnss_data_t gnss_data = m_struct_list->getInstance< s_gnss_data_t >( e_CLASSES::DATA_GNSS )->getStruct();
			s_gsm_data_t gsm_data = m_struct_list->getInstance< s_gsm_data_t >( e_CLASSES::DATA_GSM )->getStruct();
			DEBUG_PRINT( "GSM connection: %d; GNSS speed:%0.3f FIX_stat:%d speed_acc:%0.3f pos_ac:%0.3f lat:%0.7f lng:%0.7f", TAG, gsm_data.connection_status,
					gnss_data.speed, gnss_data.gnss_fixed_status, gnss_data.speed_acc_estimate, gnss_data.pos_acc_estimate, gnss_data.latitude, gnss_data.longitude );

			const float c_MIN_SPEED_THRESHOLD = 15.0f;
			float speed_acc_threshold = ( SPD_ACC_THR / 10 );
			speed_acc_threshold = ( speed_acc_threshold < c_MIN_SPEED_THRESHOLD ? c_MIN_SPEED_THRESHOLD : speed_acc_threshold ); //Ilerde SPD_ACC_THR degisirse c_MIN_SPEED_THRESHOLD degerinden assagi olmasin.

			if( ( gnss_data.gnss_fixed_status == e_GNSS_FIX_STATUS::FIX_3D) &&					// control gnss fix status
					( gnss_data.speed_acc_estimate < speed_acc_threshold ) &&					// control speed accuracy 	//Zaten Vehicle Status ile speed karari verilmis. Neden tekrar speed_acc_estimate degerlerine bakiliyor.
					( gnss_data.speed < 1.0f ) &&												// control speed			//Zaten Vehicle Status ile speed karari verilmis. Neden tekrar speed degerlerine bakiliyor.
					( gsm_data.connection_status == e_CONNECT_STAT::CONNECTED ) )				// control gsm connection
			{	// 3
				DEBUG_PRINT( "GSM & GPS IS OK FOR STOP", TAG );
				// control timeout state
				if( osApiTimerIsRunning( m_stop_timer ) == osErrorOS )						// sartlar saglaniyor timer calismiyorsa calistiralim
				{
					osApiTimerStart( m_stop_timer, 100 );
					INFO_PRINT( "30 SEC TIMEOUT ARMED FOR STOP", TAG );
					return;
				}
				else	//timer da calisiyormus sure dolana kadar beklemeye ve kontrol etmeye devam
				{
					return;
				}
			}	// 3
		}	// 2
		else if ( vehicle_data.vehicle_status == e_VEHICLE_STATUS::VEHICLE_STABLE || // engine is already off, no need to wait for conditions
				vehicle_data.vehicle_status == e_VEHICLE_STATUS::VEHICLE_POWERED )
		{
			osApiSemaphoreRelease( m_vehicle_stop_Semaphore ); // let the thread to take action for activating the relay
		}
	}	// 1
	StopTimeoutTimer();																				// stop timeout timer
}

/*!
 * @brief control messages which received from other managers
 * @param msg
 */
void c_ActionManager::MessageController( s_action_message_t &msg )
{
	switch( msg.command )
	{
	case s_action_message_t::e_ACTION_MNG_COMMANDS::UPDATE_CONFIGURATIONS:
	{
		UpdateConfigurationMessage( msg.class_id );
	}
	break;
	//Update config icerisindeki kontrol buradaki hata onleme isine de yaramaz mı? 
	//Bu mesajdan kurtulabilirdik diye dusunuyorum. Tabi bu mesajin başka amaci yoksa.
	//e_DIU_USAGE_METHOD::PERSONNEL_CONTROL oldugunda bu komut geliyor.
	case s_action_message_t::e_ACTION_MNG_COMMANDS::NO_STOP_COMMAND:
	{
		SendDriverStatusMessage( msg.driver_status, msg.driver_id );							// driver status mesaji hazirla
		if( s_manager_info.backup.blocking_strategy == s_backup_t::e_BLOCKING_STRATEGY::LOCAL ||
				m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::VEHICLE_BLOCKED ||
				m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_RELAY_BLOCK_RESPONSE )	//local blocking was used but it will not be used furthermore
		{
			SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_PASSIVE );
			SaveBlockingState( e_VEHICLE_STOP_STATES::NO_STOP_ACTION );				// update vehicle stop backup file
		}
		else
			return;
		break;
	}
	case s_action_message_t::e_ACTION_MNG_COMMANDS::LOCAL_BLOCK_VEHICLE:							// araci lokal blokla
	{
		s_manager_info.backup.blocking_strategy = s_backup_t::e_BLOCKING_STRATEGY::LOCAL;
		SendDriverStatusMessage( msg.driver_status, msg.driver_id );							// driver status mesaji hazirla
		SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_ACTIVE );
		SaveBlockingState( e_VEHICLE_STOP_STATES::WAITING_RELAY_BLOCK_RESPONSE );				// update vehicle stop backup file
	}
	break;
	case s_action_message_t::e_ACTION_MNG_COMMANDS::LOCAL_UNBLOCK_VEHICLE:							// lokal arac blokaji kaldir
	{
		s_manager_info.backup.blocking_strategy = s_backup_t::e_BLOCKING_STRATEGY::LOCAL;
		SendDriverStatusMessage( msg.driver_status, msg.driver_id );							// driver status mesaji hazirla
		SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_PASSIVE );
		SaveBlockingState( e_VEHICLE_STOP_STATES::WAITING_RELAY_UNBLOCK_RESPONSE );				// update vehicle stop backup file
	}
	break;
	case s_action_message_t::e_ACTION_MNG_COMMANDS::BLOCK_VEHICLE:									// araci blokla
	case s_action_message_t::e_ACTION_MNG_COMMANDS::UNBLOCK_VEHICLE:								// arac blokaji kaldir
		s_manager_info.backup.blocking_strategy = s_backup_t::e_BLOCKING_STRATEGY::REMOTE;
		if( s_manager_info.relay_available == false )													// role durumu kontrol, relay is disabled state, send fail response
		{
			s_manager_info.backup.query_number = msg.query_response;
			SendActionResult( e_COMMAND_RESULTS::COMMAND_FAIL );									// arac blokaj icin hata don
			break;
		}
		ChangeVehicleStopState( (s_action_message_t::e_ACTION_MNG_COMMANDS)msg.command, msg.query_response );	// stop state i guncelleyelim
		break;
	case s_action_message_t::e_ACTION_MNG_COMMANDS::RELAY_RESPONSE:									// relay device dan cevap geldi
		if( s_manager_info.backup.blocking_strategy == s_backup_t::e_BLOCKING_STRATEGY::LOCAL )									// lokal bloklama istegi mi - // local blocking active
		{
			if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_RELAY_BLOCK_RESPONSE )// bloklama icin bekliyorduk role den cevap geldiyse islem bitti demektir
			{
				m_data_struct.local_blocking_state = e_LOCAL_BLOCKING_STATE::LOCALLY_BLOCKED;	//this data is used to send an locakblocking event.but nobody uses it
				SaveBlockingState( e_VEHICLE_STOP_STATES::VEHICLE_BLOCKED );							// update vehicle stop backup file
			}
			else if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_RELAY_UNBLOCK_RESPONSE )// bloklama icin bekliyorduk role den cevap geldiyse islem bitti demektir
			{
				m_data_struct.local_blocking_state = e_LOCAL_BLOCKING_STATE::LOCALLY_UNBLOCKED;//this data is used to send an locakblocking event.but nobody uses it
				SaveBlockingState( e_VEHICLE_STOP_STATES::VEHICLE_UNBLOCKED ); 							//No_STOP_ACTION yerine VEHICLE_UNBLOCKED desek ne olurdu?
			}
			break;
		}
		else																						// remote blocking active
		{
			//TODO:Röle mesajı içine sonuç bilgisi eklenecek.
			//Röleden cevap geldiyse mutlaka işlem başarılı mı olmuş demektir? O zaman neden cevap bekliyoz ki? At mesajı gitsin
			//gelen cevap içinde ne işlem yapıldığı bilgisi de olmalı. Belki block ettim diye döndü biz ublock bekliyorduk tamam dedik geçtik. #SO
			if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_RELAY_BLOCK_RESPONSE )																					// bloklama icin cevap bekliyorduk
			{
				SendActionResult( e_COMMAND_RESULTS::VEHICLE_BLOCK_OK );							// arac bloklandi
				SaveBlockingState( e_VEHICLE_STOP_STATES::WAITING_ALARM_BLOCK_RESPONSE );
			}
			else if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_RELAY_UNBLOCK_RESPONSE ||
					m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::VEHICLE_UNBLOCKED )
			{
				SendActionResult( e_COMMAND_RESULTS::VEHICLE_UNBLOCK_OK );							// role cevabi geldi unblock cevabi bekliyorduk
				SaveBlockingState( e_VEHICLE_STOP_STATES::WAITING_ALARM_UNBLOCK_RESPONSE);
			}
		}
		break;
	case s_action_message_t::e_ACTION_MNG_COMMANDS::ALARM_RESPONSE_RECEIVED:						// blokaj sonucu gonderilen mesajin cevabi alindi
		if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_ALARM_BLOCK_RESPONSE )
		{
			SaveBlockingState( e_VEHICLE_STOP_STATES::VEHICLE_BLOCKED );								// update vehicle stop backup file
		}
		else if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_ALARM_UNBLOCK_RESPONSE ||
				m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::VEHICLE_UNBLOCKED )
		{
			SaveBlockingState( e_VEHICLE_STOP_STATES::VEHICLE_UNBLOCKED );
		}
		break;
	case s_action_message_t::e_ACTION_MNG_COMMANDS::UNBLOCK_VEHICLE_WITHOUT_SAVE:
		INFO_PRINT("UNBLOCK_VEHICLE_WITHOUT_SAVE received from power manager. Sending relay passive message.", TAG);
		SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_PASSIVE );

		s_device_message_t msg;
		msg.command = s_device_message_t::e_DEV_MNG_COMMANDS::SET_DRIVER_ACTIVE_MEMBER;
		SendMessage( CreateMessage( msg, e_MANAGERS::DEVICE_MANAGER ) );

		s_power_message_t msg_data;
		msg_data.command = s_power_message_t::e_PWR_MNG_COMMANDS::EVENT_OCCURED;
		msg_data.event_type = s_power_message_t::e_EVENT_TYPES::RELAY_PASSIVE;
		SendMessage( CreateMessage( msg_data, c_Manager::e_MANAGERS::POWER_MANAGER ) );
		break;
	case s_action_message_t::e_ACTION_MNG_COMMANDS::RETURN_TO_BACKUP_VALUES:
		INFO_PRINT("RETURN_TO_BACKUP_VALUES received.", TAG);
		ControlBackupValue( );
		break;
	default:
		break;
	}
	setStruct( m_data_struct );
}

/*!
 * @brief task infinite loop
 */
RunFuncType_t c_ActionManager::Run()
{
	Suspend();
	CheckSuspend();
	static uint32_t alive_counter = 0;
	uint32_t last_control_time = 0;
	auto msg = CreateMessage( m_rcv_msg_data );
	QueueSetMemberHandle_t activatedMember = nullptr;
	IamAlive( alive_counter );
	ControlBackupValue();																			// control backup value

	forever
	{
		if( m_suspended_flag )
		{
			CheckSuspend();
		}
		IamAlive( alive_counter );
		activatedMember = osApiSelectFromQueueSet( m_queueset, c_QUEUESET_WAIT_TIME );
		if( activatedMember != NULL )
		{
			if( activatedMember == m_ReceiveMessageQueue )
			{
				if( osApiMessageGet( m_ReceiveMessageQueue, &msg, c_QUEUESET_WAIT_TIME ) == osOK )	// get received message from queue
				{
					m_rcv_msg_data = ReadMessage ( &msg );
					MessageController ( m_rcv_msg_data );
				}
			}
			else if( activatedMember == m_vehicle_stop_Semaphore )
			{
				if( osApiSemaphoreWait( m_vehicle_stop_Semaphore, c_RECEIVE_DATA_SEMAPHORE_WAIT_TIMEOUT ) == osOK )
				{
					DEBUG_PRINT("30 SEC TIMEOUT IS OK FOR STOP", TAG);
					SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_ACTIVE );
					SaveBlockingState( e_VEHICLE_STOP_STATES::WAITING_RELAY_BLOCK_RESPONSE );
				}
			}
		}
		if( m_data_struct.vehicle_blocking_state == e_VEHICLE_STOP_STATES::WAITING_FOR_VEHICLE_BLOCK_CONDITIONS )									// arac bloklanmaya calisiyorsa her saniye durum kontrol edilecek
		{
			uint32_t sys_tick = osApiKernelSysTick();
			if( ( sys_tick <= last_control_time ) || ( ( sys_tick - last_control_time ) >= VEHICLE_STOP_CONTROL_PERIOD / TICK_PERIOD_MS ) )
			{
				last_control_time = sys_tick;
				ControlVehicleStopState();															// arac durdurma durumunu kontrol edelim
			}
		}
	}
}

/*!
 * @brief send vehicle block/unblock action result
 * @param result
 */
void c_ActionManager::SendActionResult( e_COMMAND_RESULTS result )
{
	///@brief query unknown ise hata var demektir paket gondermeyelim
	if( s_manager_info.backup.query_number == UNKNOWN_QUERY_NUMBER )
	{
		s_manager_info.backup.query_number = RANDOM_QUERY;
		return;
	}

	s_communication_message_t s_com_msg;
	memset( &s_com_msg, 0x00, sizeof( s_communication_message_t ) );
	uint8_t * response_payload = ( uint8_t * )osApiMalloc( MAX_PAYLOAD_LENGTH );
	if( response_payload == nullptr )
	{
		return;
	}
	s_com_msg.length = m_protocol->PrepareSystemCommandResponsePayload( e_MESSAGE_TYPES::VEHICLE_CONTROL_MESSAGE, response_payload, result, true ); // true means send location information
	s_com_msg.type_crypto.fields.type = e_MESSAGE_TYPES::VEHICLE_CONTROL_MESSAGE;
	s_com_msg.query = s_manager_info.backup.query_number;
	s_com_msg.data = &response_payload[0];
	///@brief unblock ya da block isleminin sonucu mutlaka server a gonderilmeli yoksa server in kafasi karisiyor
	if( result != e_COMMAND_RESULTS::VEHICLE_COMMAND_CANCEL )
	{
		s_com_msg.type_crypto.fields.alarm = true;													// this is a alarm message
	}
	if( result == e_COMMAND_RESULTS::VEHICLE_BLOCK_OK)
	{
		DEBUG_PRINT( "Prepare System Command Response Payload : Command Response = %s, query_id = %d", TAG , "VEHICLE_BLOCKED", s_manager_info.backup.query_number );
	}
	else
	{
		DEBUG_PRINT( "Prepare System Command Response Payload : Command Response = %s, query_id = %d", TAG , "VEHICLE_UNBLOCKED", s_manager_info.backup.query_number );
	}

	SendMessage( CreateMessage( s_com_msg, c_Manager::e_MANAGERS::COMMUNICATION_MANAGER ) );		// prepare and send vehicle stop message to communication manager
}

/*!
 * @brief send driver status message payload
 * @param status
 * @param id
 */
void c_ActionManager::SendDriverStatusMessage( uint8_t status, uint8_t * id, bool alarm_state )
{
	s_communication_message_t s_com_msg;
	memset( &s_com_msg, 0x00, sizeof( s_communication_message_t ) );
	uint8_t * response_payload = ( uint8_t * )osApiMalloc( MAX_PAYLOAD_LENGTH );
	if( response_payload == nullptr )
	{
		return;
	}
	// find id length
	uint8_t i = 0;
	for( ; i < MAX_DRIVER_ID_LENGTH; i++ )
	{
		if( id[i] == 0 )
		{
			break;
		}
	}

	s_com_msg.length = m_protocol->PrepareDriverStatusPayload( response_payload, status, id, i );
	if( s_com_msg.length == 0 )
	{
		osApiFree( response_payload );
		return;
	}
	s_com_msg.type_crypto.fields.type = e_MESSAGE_TYPES::DRIVER_STATUS_MESSAGE;
	s_com_msg.data = response_payload;
	s_com_msg.type_crypto.fields.alarm = alarm_state;
	SendMessage( CreateMessage( s_com_msg, c_Manager::e_MANAGERS::COMMUNICATION_MANAGER ) );		// prepare and send vehicle stop message to communication manager
}


/*!
 * @brief Stop timeout timer and clear m_timeout_state
 */
void c_ActionManager::StopTimeoutTimer( void )
{
	if( osApiTimerIsRunning( m_stop_timer ) == osOK )
		osApiTimerStop( m_stop_timer, 100 );															// stop the timer
	INFO_PRINT( "30 SEC TIMEOUT CANCELED FOR STOP", TAG );
}

void c_ActionManager::SaveBlockingState( e_VEHICLE_STOP_STATES state )
{
	UpdateFlashFile( state );								// update vehicle stop backup file
	m_data_struct.vehicle_blocking_state = state;			// data struct guncelle
	setStruct( m_data_struct );
}

/*!
 * @brief update stop vehicle backup file
 * @param value
 * @return
 */
ARV_StatusTypeDef c_ActionManager::UpdateFlashFile( uint8_t value )
{
	ARV_StatusTypeDef result = ARV_StatusTypeDef::ARV_ERROR;
	int8_t retry_cnt = 3;
	spiffs_file fd;
	c_FileSystem * file = c_FileSystem::GetInstance();
	s_manager_info.backup.blocking_state = value;
	do {
		retry_cnt--;
		fd = file->Open( "vehiclestop.h", e_FsReadWrite::ARV_WRITE,"ActionManager" );				// open file for writing
		if( fd > 0 )
		{
			result = file->Write( fd, &s_manager_info.backup, sizeof( s_manager_info.backup ) );	// write backup value
			if( result == ARV_StatusTypeDef::ARV_OK )
			{
				DEBUG_PRINT( "Vehicle stop backup file updated, value = %d", TAG, value );
			}
			c_ConfigSaveLoad::CloseFile( file, fd );	//file->Close(fd);							// close file
		}
	}while( retry_cnt > 0 && result == ARV_StatusTypeDef::ARV_ERROR );

	return result;
}

/*!
 * @brief Update config mesaji alindiginda role durumu kontrol edilecek
 * @param class_id
 */
void c_ActionManager::UpdateConfigurationMessage( uint8_t class_id )
{
	if( class_id == e_CLASSES::CONFIG_POWER )
	{
		s_power_config_t pow_conf = m_struct_list->getInstance< s_power_config_t >( e_CLASSES::CONFIG_POWER )->getStruct();	// get relay states
		if( !m_protocol->StructIsUsable( (uint8_t*)&pow_conf, sizeof( pow_conf ) ) )				// okuyabildik mi?
		{
			return;
		}
		if( pow_conf.device_power_states.fields.RELAY_PWR )											// role aktif mi?
		{
			s_manager_info.relay_available = true;														// set relay status flag
			DEBUG_PRINT( "Vehicle Control State Enabled", TAG );
		}
		else																						// role kullanilabilir degil
		{
			// araci durdurmaya calisiyorsak, durdurmayi iptal edelim
			if( m_data_struct.vehicle_blocking_state != e_VEHICLE_STOP_STATES::NO_STOP_ACTION )
			{
				SaveBlockingState( e_VEHICLE_STOP_STATES::NO_STOP_ACTION);
			}
			s_manager_info.relay_available = false;													// role pasif
			DEBUG_PRINT( "Vehicle Control State Disabled", TAG );
		}
	}
}

void c_ActionManager::SendRelayMessage( s_device_message_t::e_DEV_MNG_COMMANDS command  ) {
	s_device_message_t dev_msg;
	dev_msg.command = command;
	SendMessage( CreateMessage( dev_msg, c_Manager::e_MANAGERS::DEVICE_MANAGER ) );
	if( command == s_device_message_t::e_DEV_MNG_COMMANDS::RELAY_ACTIVE )
	{
		INFO_PRINT( "Relay Activation message sent to Device Manager", TAG );
	}
	else
	{
		INFO_PRINT( "Relay Passivization message sent to Device Manager", TAG );
	}
}

/*!
 * @brief RTOS timer Callback function
 * @param xTimer
 */
void VehicleStopTimerCB( osTimerId xTimer )
{
	osApiSemaphoreRelease( static_cast < c_ActionManager* >( osApiGetTimerIDFromHandle( xTimer ) )-> m_vehicle_stop_Semaphore );
}
