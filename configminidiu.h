/**
 * configminidiu.h
 *
 *  Created on: Dec 10, 2019
 *      Author: ugur.aydin
 */

#ifndef CONFIGURATION_CONFIGMINIDIU_H_
#define CONFIGURATION_CONFIGMINIDIU_H_

#include "sharedstruct.h"
#include "commonio.h"
#include "arvbinaryprotocoldefinitions.h"

/*!
 * \enum e_DIU_USAGE_METHOD
 * \brief
 */
enum e_DIU_USAGE_METHOD {
	DRIVER_CONTROL,  /*!< DRIVER_CONTROL */
	PERSONNEL_CONTROL/*!< PERSONNEL_CONTROL */
};

/*!
 * \enum e_DIU_CHECKOUT_OPTION
 * \brief
 */
enum e_DIU_CHECKOUT_OPTION {
	MANUAL_CHECKOUT,  /*!< MANUAL_CHECKOUT */
	AUTOMATIC_CHECKOUT/*!< AUTOMATIC_CHECKOUT */
};

/*!
 * \enum e_DIU_VEHICLE_BLOCK_OPTION
 * \brief Surucu tanima birimi arac blokaj opsiyonlari
 */
enum e_DIU_VEHICLE_BLOCK_OPTION {
	NO_BLOCKING,    /*!< NO_BLOCKING : Arac blokaj yok*/
	SERVER_BLOCKING,/*!< SERVER_BLOCKING : Arac blokaja server dan gelecek mesajla yapilacak*/
	LOCAL_BLOCKING  /*!< LOCAL_BLOCKING : Surucu listesi kontrol edilerek cihaz blokalama yapip yampmayacagina kendi karar verecek*/
};

/*!
 * \enum e_MINIDIU_CONFIG_PROPERTIES
 * \brief CONFIG_MINIDIU class propery ids
 */
enum e_MINIDIU_CONFIG_PROPERTIES {
	MINIDIU_STATUS,          /*!< MINIDIU_STATUS : enable/disable*/
	MINIDIU_CHAN_ID,         /*!< MINIDIU_CHAN_ID : serial port*/
	MINIDIU_USAGE_METHOD,    /*!< MINIDIU_USAGE_METHOD : Driver mode, personel mode*/
	MINIDIU_CHECKOUT_OPTION, /*!< MINIDIU_CHECKOUT_OPTION : manual checkout, driver chekout*/
	MINIDIU_CHECKOUT_TMO,    /*!< MINIDIU_CHECKOUT_TMO : auto check out timeout*/
	MINIDIU_BLOCK_OPTION,    /*!< MINIDIU_BLOCK_OPTION : e_DIU_VEHICLE_BLOCK_OPTION */
	MINIDIU_BUZZER_START_TMO,/*!< MINIDIU_BUZZER_START_TMO : Buzzer calmaya baslamadan once beklenecek sure (saniye)*/
	MINIDIU_BUZZER_DURATION, /*!< MINIDIU_BUZZER_DURATION : Buzzer calmas suresi (saniye)*/
	MINIDIU_READ_NDEF,		 /*!< MINIDIU_READ_NDEF : enable/disable*/
	MINIDIU_CONFIG_PROP_MAX  /*!< MINIDIU_CONFIG_PROP_MAX */
};

#pragma pack( push, 1 )
/*!
 * \struct s_minidiu_config_t
 * \brief CONFIG_MINIDIU class members
 */
typedef struct {
	e_STATUSES status;							/// enable or disable diu
	e_CHANNEL_IDs channel_id;					/// port id
	e_DIU_USAGE_METHOD usage_method;			/// driver or personnel control
	e_DIU_CHECKOUT_OPTION checkout_option;		/// automatic or manual checkout
	uint32_t checkout_tmo;						/// automatic checkout timeout
	e_DIU_VEHICLE_BLOCK_OPTION block_option;	/// vehicle block opttion
	uint16_t buzzer_start_tmo;					/// buzzer start timeout
	uint16_t buzzer_duration;					//! @brief buzzer active duration (uint16_t) tipinde fakat Mini STB bu degeri (uint8_t) tipinde kullaniyor. 0xFF'den buyuk degerler anlamsiz. 0xFF geldiginde bu buzzer surekli calsin demek.
	e_STATUSES read_ndef;
}s_minidiu_config_t;
#pragma pack( pop )

class c_MinidiuConfig : public c_SharedStruct< s_minidiu_config_t>
{
public:
	c_MinidiuConfig();
	~c_MinidiuConfig();
	ARV_StatusTypeDef LoadConfig( uint8_t class_id );
	ARV_StatusTypeDef SaveConfig( uint8_t class_id );
	void LoadDefaultConfig( uint8_t class_id );
	void DeleteConfig(void);
	static const s_property_specs_t MINIDIU_CONF_PROPERTY_SPECS[ ];
};


#endif /* CONFIGURATION_CONFIGMINIDIU_H_ */
