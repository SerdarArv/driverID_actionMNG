/*
 * datavehiclestop.h
 *
 *  Created on: Jan 4, 2019
 *      Author: ugur.aydin
 */

#ifndef DEVICE_DATA_DATAVEHICLESTOP_H_
#define DEVICE_DATA_DATAVEHICLESTOP_H_

#include "sharedstruct.h"
#include "typedefarv.h"


enum e_VEHICLE_STOP_DATA_PROPERTIES {
	VEHICLE_STOP_STATE		= 0,
	LOCAL_BLOCK_STATE		= 1,
	VEHICLE_STOP_DATA_PROP_MAX
};

enum e_VEHICLE_STOP_STATES {
	NO_STOP_ACTION							= 0,
	VEHICLE_UNBLOCKED						= 1,
	WAITING_FOR_VEHICLE_BLOCK_CONDITIONS	= 2,
	VEHICLE_BLOCKED							= 3,
	WAITING_RELAY_UNBLOCK_RESPONSE 			= 4,
	WAITING_RELAY_BLOCK_RESPONSE			= 5,
	WAITING_ALARM_UNBLOCK_RESPONSE			= 6,
	WAITING_ALARM_BLOCK_RESPONSE			= 7,
};

enum e_LOCAL_BLOCKING_STATE {
	LOCALLY_UNBLOCKED,
	LOCALLY_BLOCKED,
	INVALID_LOCAL_BLOCKING = 255
};


typedef struct {
	e_VEHICLE_STOP_STATES vehicle_blocking_state;
	e_LOCAL_BLOCKING_STATE local_blocking_state;//this data is used to send an locakblocking event.but nobody uses it
}s_vehicle_stop_data_t;

#endif /* DEVICE_DATA_DATAVEHICLESTOP_H_ */
