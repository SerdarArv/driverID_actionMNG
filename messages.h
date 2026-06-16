#ifndef MESSAGES_H
#define MESSAGES_H

#include "typedefarv.h"
#include "configclasses.h"
#include "commandsystem.h"
#include "devices.h"
#include "devicetypes.h"
#include "productmessages.h"
#include "transparentcommand.h"

#pragma pack(push, 1)
struct s_message_base {
	uint8_t command;
	uint8_t class_id;
};

typedef struct : s_message_base {
	enum e_SYS_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		SET_MNG_LIST,
		MAIN_DEV_TEST_FINISH,
		OPT_DEV_CREATE_FINISH,
		DEVICES_READY,
		CONFIG_BLOCKED_TO_FORMAT_FLASH,
		CONFIG_READY,
		MANAGERS_START,
		MANAGERS_STOP, // periodic-region-event-comm. - config
		IAMALIVE,
		COMMAND_SIZE
	};
	uint8_t * list;
}s_system_message_t;

typedef struct : s_message_base {
	enum e_MSG_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		GET_MNG_LIST,
		UPDATE_CONFIG_LIST,
		COMMAND_SIZE
	};
}s_msg_mng_message_t;

typedef struct : s_message_base {
	enum e_PER_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		GET_NEXT_REPORT_TIME,
		MNG_START,
		SEND_REPORT,
		SEND_WAKEUP,
		SHIFT_REPORT,
		UNKNOWN_SENSOR,
		LOW_POWER_REPORT_MODE,
		CONFIG_REPORT_MODE,
		COMMAND_SIZE
	};
}s_periodic_message_t;

typedef struct : s_message_base {
	enum e_EVENT_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		ACTIVATE_SOS,
		SOS_PASSIVE,
		MNG_START,
		REGION_CHECK_START,
		DELETE_BACKUP_FILE,
		CREATE_EXT_EVENT_MESSAGE,
		COMMAND_SIZE
	};
	uint16_t event_id;
#ifdef LINUX
	struct {
		uint16_t event_id;
		uint8_t event_type;
		uint16_t event_type_id;
		uint32_t event_duration;
		uint8_t event_state;
		uint16_t event_instance_id;
		uint8_t class_id;
		uint8_t property_id;
		uint64_t sensor_id;
		uint8_t event_operator;
		s_arv_value_t min_threshold;
		s_arv_value_t max_threshold;
		s_arv_value_t previous_value;
		s_arv_value_t current_value;
	}s_external_event_structure;
#endif
}s_event_message_t;

typedef struct : s_message_base {
	enum e_REGION_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		MNG_START,
		COMMAND_SIZE
	};
	uint64_t deleted_region_id;
}s_region_message_t;

typedef struct : s_message_base {
	enum e_DEV_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		CONFIG_READY,
		SET_DEV_STATUS,
		GET_DEV_STATUS,
		RTC_DEV_MSG,
		SLEEP_CPU,
		CLR_RST_SOURCE,
		CLEAR_MAN_WATCHDOG,
		RESET_MCU,
		GET_CRC_GSM_ADDRESS,
		GET_FLASH_ADDRESS,
		BLINK_LED,
		TURN_ON_LED,
		TURN_OFF_LED,
		SEND_SMS,
		SERVER_CONNECTION_OK,
		IWDG_CLEAR_REQ,
		EEPROM_CMD,
		FORCE_GNSS_ASSIST,
		SET_ODOMETER_OFFSET,
		ALL_LED_GREEN,
		ALL_LED_RED,
		ASSIST_GNSS_PACKET,
		READ_BL_VERSION,
		LOAD_ODOMETER,
		GSM_STATUS_CHANGE,
		CAN_I_DELETE_YOU,
		UNREGISTER_ME,
		POST_TRANSPARENT,
		FILE_UPLOAD_STATE,
		SOS_SENT_OK,
		SOS_SENT_ERROR,
		OLED_CLOSING,
		COMM_WAITING,
		STEP_COUNTER_RESET,
		STARTUSB,
		RELAY_ACTIVE,
		RELAY_PASSIVE,
		BUZZER_CONTROL,
		DIU_BUZZER_CONTROL,
		LED_CONTROL,
		HUB_FW_UPDATE,
		HUB_FW_UPDATE_CANCEL,
		UPDATE_WARN_INFO,
		ASSIST_RESP_OK,
		CAN_GENERAL_REPORT_PREPARED,
		CAN_VIN_REPORT_PREPARED,
		MAIN_DEV_TEST_FINISH,
		SET_ACTIVE_DRIVER,
		GENERIC_OUTPUT_CONTROL,
		SET_DRIVER_ACTIVE_MEMBER,
		COMMAND_SIZE
	};
	e_ARV_PowerModeDef devices_power_states[ e_DEVICES::DEVICE_MAX_SIZE ];
	e_SYSTEM_POWER_STATE m_power_mode;
	c_RtcDeviceType::s_msg_t m_rtc_msg;
	uint8_t led_command;
	PRODUCT_DEVICE_MESSAGE_PARTS				///@brief buzzer in kullanilmadigi durumlarda definition degeri bos birakilmali
	c_EepromType::s_msg_t eeprom_msg;
	uint8_t * agnss_packet;
	union {
		struct {
			uint8_t file_type;
			uint32_t file_size;
			uint16_t file_crc;
			char file_name[15];
			uint8_t file_name_length;
			uint16_t chunk_count;
			uint16_t query_id;
			uint16_t chunk_size;
		}fw_info;
		s_transparent_message_t transparent;	// TODO : bu struct icerisindeki data icin malloc yapiliyor, device isi bitince free etmeli!!!!
		uint64_t external_device_error;			// smarthub daki device larin hatalari icin eklendi.
	};
	uint16_t agnss_packet_length;
	int8_t file_upload_state;
	uint32_t warning_code;
}s_device_message_t;

typedef struct : s_message_base  {
	enum e_PWR_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		EVENT_OCCURED,
		NEXT_REPORT_TIME,
		COMM_MANAGER_STATUS,
		COMMAND_RESPONSE,
		DEV_PWR_MOD_CALL_OK,
		MANAGERS_SUSPEND_OK,
        LINUX_APPS_SLEEP_OK,
		PWR_MNG_SIZE
	};

	enum e_COMM_MNG_STATUS {
		COMM_MNG_IDLE,
		COMM_MNG_BUSY,
		COMM_MNG_SIZE
	};

	enum e_EVENT_TYPES {
		GENERIC_EVENT,
//		IMU_EVENT,
		USB_PLUG_EVENT,
		USB_UNPLUG_EVENT,
//		SOS_BTN_PRESS,
//		SHORT_ON_OFF_BTN_PRESS,
		LONG_ON_OFF_BTN_PRESS,
//		BOTH_BTN_PRESSED,
		NOISY_BTN_PRESS,
		RTC_ALARM_A_EVENT,
		RTC_ALARM_B_EVENT,
		RTC_WAKEUP_EVENT,
		RTC_CMD_OK,
        RTC_CMD_ERROR,
        CPU_CMD_OK,
		CPU_CMD_ERROR,
		RTC_SYNC_STATUS_CHANGED,
		SHUTDOWN_PERMISSION_ACCEPTED,
		SHUTDOWN_PERMISSION_REJECTED,
		CRITICAL_BATTERY_LEVEL,
//		TAMPER_SW_ACTIVE,
		IGNITION_ACTIVE,
//		EXT_IN_ACTIVE,
		AGNSS_SUCCESS,
		AGNSS_REQUEST,
//		HALL_EFFECT_INT,
		SENT_EVENT_PACKET,
		CPU_TEMP_HIGH,
		RELAY_PASSIVE,
		e_EVENT_TYPES_SIZE
	};

	enum e_PWR_MNG_COMMANDS_RESPONSE {
		CMD_OK,
		CMD_ERROR,
		COMMAND_RESPONSE_SIZE
	};

	e_EVENT_TYPES  event_type;
	e_COMM_MNG_STATUS comm_mng_status;
	e_PWR_MNG_COMMANDS_RESPONSE command_response;
	uint32_t time_to_next_sleep; 		// in seconds it must be filled by periodic manager
    uint8_t sleep_type;///@note bunu kimse kullanmiyor
}s_power_message_t;

// communication manager a gelen mesaj byte array seklinde gelecek
typedef struct :s_message_base {
	enum e_COM_MNG_STATE {
		MNG_DUMMY,
		MNG_STOP,
		MNG_START,
	};

	enum e_AGNSS_PACKET_RESULT {
		RESULT_ERROR,
		RESULT_OK
	};

	enum e_COMM_MNG_COMMANDS {
		UPDATE_CONFIGURATIONS = 0,
		SET_COMM_MANAGER_STATUS,
		GET_COMM_MANAGER_STATUS_SLEEP,
		GET_COMM_MANAGER_STATUS_SHUTDOWN,
		SET_CRC_GSM_ADDRESS,
		PREPARE_MESSAGE_PACKET,
		FOTA_RESULT,
		DELETE_ALL_LOGS,
		CONNECTION_RESET,
		REQUEST_AGNSS,								// request agnss file download from gateway
		RESPONSE_AGNSS,								// agnss packet response
		SEND_WFD_PACKET,
		COM_PARAMETERS_CHANGED,
		RET_TO_DEF_OK,
		SEND_INIT_MESSAGE,
		SEND_HUB_INFO,
		SENSOR_FW_UPDATE_RESULT,
		SEND_TRANSPARENT,							// send transparent message to receiver
		/*!
		 * diger device ya da manager lardan file upload istegi com mng a UPLOAD_FILE komutu ile gonderilecek,
		 * dosya adi data pointer a yazilacak
		 * dosya adi uzunlugu length e yazilacak
		 */
		UPLOAD_FILE,
		PREPARE_WARN_MESSAGE,
		COMM_MNG_SIZE
	};

	e_COM_MNG_STATE state;
	union
	{
		struct
		{
			uint16_t type		: 8;				// 8 bit message type
			uint16_t log_info	: 1;				// if packet is log packet set this bit
			uint16_t crypto		: 3;				// 3 bit crypto type
			uint16_t alarm		: 1;				// message alarm or not
			uint16_t reserved	: 3;				// reserved for future usage
		}fields;
		uint16_t value;
	}type_crypto;

	uint16_t query;
	uint16_t length;								// tacho data length
	uint8_t * data;									// data pointer
	uint8_t buffer;									// 0 : error, 1 : ok
	void * crc_addr;
	void * gsm_addr;
	uint8_t fota_result;
	e_COM_NETWORK_TYPES network;
	bool node_name_change;
	bool reset_after_response;
	struct {
		uint16_t session_id;
		uint16_t message_id;
		uint8_t receiver;							// comms 0xFE
		uint8_t function;							// e_TRANSPARENT_MESSAGE_FUNCTIONS
	}transparent;
}s_communication_message_t;


typedef struct : s_message_base {
	enum e_CONFIG_MNG_COMMANDS {
		NO_COMMAND,
		UPDATE_CONFIGURATIONS,
		SET_FLASH_ADDRESS,
		SMS_SET_SERVER_PARAMS,
		COM_TEST_OK,
		RETURN_DEFAULT_CONFIGURATIONS,
		CHANGE_FIRMWARE_STABILITY,
		EEPROM_RESPONSE,
		COM_CONF_RESPONSE,
		INIT_RESP_RCV,
        UPDATE_REQUEST,///@note : bunu kimse kullanmiyor
		CONFIG_MNG_SIZE
	};

	union {
		struct {
			union {
				struct {
					uint16_t type : 8;					// 8 bit message type
					uint16_t log_info : 1;				// if packet is log packet set this bit
					uint16_t crypto :3;					// 3 bit crypto type
					uint16_t reserved : 4;				// reserved for future usage
				}fields;
				uint16_t value;
			}type_crypto;
			uint8_t sequence_control;
			uint16_t query_response;
			uint16_t payload_length;
			uint8_t * data;
			e_COM_NETWORK_TYPES network;
			bool node_name_change;
		}packet;
		c_EepromType::s_msg_t eeprom_msg;
	};
	void * flash_addr;
}s_config_message_t;

typedef struct : s_message_base {
    enum e_ACTION_MNG_COMMANDS {
        UPDATE_CONFIGURATIONS = 0,
        NO_STOP_COMMAND,
        BLOCK_VEHICLE,
        UNBLOCK_VEHICLE,
        RELAY_RESPONSE,
        LOCAL_BLOCK_VEHICLE,
        LOCAL_UNBLOCK_VEHICLE,
		UNBLOCK_VEHICLE_WITHOUT_SAVE,
		RETURN_TO_BACKUP_VALUES,
        ALARM_RESPONSE_RECEIVED
    };
    uint16_t query_response;
    uint8_t driver_status;                            	/// 0 : driver checkout, 1 : driver checkin
    uint8_t driver_id[24];                            	/// driver id
}s_action_message_t;

typedef struct : s_message_base {
    enum e_SENSOR_FUSION_MNG_COMMANDS {
        UPDATE_CONFIGURATIONS = 0,
        UPDATE_IGNITION_COUNTER,
		UPDATE_STATE_COUNTER
    };
    uint32_t time;
}s_sensorfusion_message_t;

#pragma pack(pop)
#endif // MESSAGES_H
