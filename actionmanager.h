/**
 * actionmanager.h
 *
 *  Created on: Jan 4, 2019
 *      Author: ugur.aydin
 */

#ifndef CORE_APP_ACTIONMANAGER_H_
#define CORE_APP_ACTIONMANAGER_H_

#include "manager.h"
#include "datavehiclestop.h"
#include "commandsystem.h"
#include "configreport.h"

class c_ArvBinaryProtocol;
class c_ActionManager : public c_SingletonManager < c_ActionManager, s_action_message_t >, public c_SharedStruct < s_vehicle_stop_data_t >
{
public:
	c_ActionManager(c_SharedList *p_st_list, const char *name, e_MANAGERS id, unsigned short stack_depth, char priority);
	void getConfigMap( std::multimap< e_CLASSES, e_MANAGERS > & managers_config_map );
	RunFuncType_t Run();
	osSemaphoreId m_vehicle_stop_Semaphore;
private:
	QueueSetHandle_t m_queueset;
#pragma pack( push, 1 )
	/*!
	 * \struct s_backup_t
	 * \brief vehicle block backup file structure
	 */
	typedef struct {
		enum e_BLOCKING_STRATEGY
		{
			LOCAL,
			REMOTE
		} blocking_strategy;
		uint8_t blocking_state;					///@brief vehicle blocking state
		uint16_t query_number;					///@brief vehicle block command query response id
	}s_backup_t;
#pragma pack( pop )
	/*!
	 * \struct s_manager_info
	 * \brief manager control struct
	 */
	struct {
		s_backup_t backup;						///@brief backup struct
		bool relay_available;						///@brief power config relay power bit state
	}s_manager_info;
	osTimerId m_stop_timer;
	c_ArvBinaryProtocol * m_protocol;
	s_vehicle_stop_data_t m_data_struct;

	void ChangeVehicleStopState( s_action_message_t::e_ACTION_MNG_COMMANDS command, uint16_t query);
	bool ControlBackupValue( void );
	void ControlVehicleStopState( void );
	void CommandCancelControl( void );
	void MessageController( s_action_message_t &msg );
	void SendActionResult( e_COMMAND_RESULTS result );
	void SendDriverStatusMessage( uint8_t status, uint8_t * id, bool alarm_state = false );
	void StopTimeoutTimer( void );
	void SendRelayMessage(  s_device_message_t::e_DEV_MNG_COMMANDS command );
	void SaveBlockingState(  e_VEHICLE_STOP_STATES state  );
	void UpdateConfigurationMessage( uint8_t class_id );
	ARV_StatusTypeDef UpdateFlashFile( uint8_t value );
};

#endif /* CORE_APP_ACTIONMANAGER_H_ */
