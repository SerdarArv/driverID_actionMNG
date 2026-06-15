/**
 * configdriverlist.h
 *
 *  Created on: Dec 10, 2019
 *      Author: ugur.aydin
 */

#ifndef CONFIGURATION_CONFIGDRIVERLIST_H_
#define CONFIGURATION_CONFIGDRIVERLIST_H_

#include "sharedstruct.h"
#include "arvbinaryprotocoldefinitions.h"
#include "filesystem.h"
#include "datadriverlist.h"
/*!
 * \def MAX_DRIVER_ID_COUNT
 * \brief Bir cihaza maksimum 100 adet surucu id degeri atanabilir,
 * \Ancak Linux cihazlar için bu sayı 1000 olarak sonradan değiştirilmiştir
 */
#ifdef LINUX
#define MAX_DRIVER_ID_COUNT		1000
#else
#define MAX_DRIVER_ID_COUNT		100
#endif
/*!
 * \def MAX_DRIVER_LIST_LENGTH
 * \brief Maksimum 100 id, her id maksimum 24 karakter olabilir. Maksimum boyut 2400 byte
 */
#define MAX_DRIVER_LIST_LENGTH  ( MAX_DRIVER_ID_COUNT * MAX_DRIVER_ID_LENGTH )

/*!
 * \enum e_DRIVERLIST_CONFIG_PROPERTIES
 * \brief CONFIG_DRIVERLIST class property id
 */
enum e_DRIVERLIST_CONFIG_PROPERTIES {
	USER_ID,                  /*!< USER_ID */
	USER_COUNT,               /*!< USER_COUNT */
	USER_SIZE,                /*!< USER_SIZE */
	USER_INDEX,               /*!< USER_INDEX */
	DRIVERLIST_CONFIG_PROP_MAX/*!< DRIVERLIST_CONFIG_PROP_MAX */
};

#pragma pack( push, 1 )
/*!
 * \struct s_driverlist_config_t
 * \brief CONFIG_DRIVERLIST members
 * \note user_size driver id string uzunlugunu gosterir. Mini DIU kullanilirken id uzunluklari (Hex String kullanildigi icin) aynidir.
 * Smart DIU kullanilirken id uzunluklari (Decimal String kullanildigi icin) farkli olabilir.
 */
typedef struct {
	uint8_t user_id[MAX_DRIVER_ID_LENGTH+1];	// buffer, +1 for seperator(null termination)
	int16_t user_count;							// set edilen surucu id sayisi
	uint8_t user_size;							// son set edilen surucu id sayisi
	int16_t user_index;							// id index degeri GET_USER_ID_MESSAGE
}s_driverlist_config_t;
#pragma pack( pop )

class c_DriverListConfig : public c_SharedStruct< s_driverlist_config_t >
{
private:
	/*!
	 * \enum e_UPDATE_TYPE
	 * \brief Driver list update commands
	 */
	enum e_UPDATE_TYPE{
		ADD_ID,   /*!< ADD_ID */
		DELETE_ID,/*!< DELETE_ID */
		DELETE_ALL/*!< DELETE_ALL */
	};
	static osMutexId m_driver_list_mutex;
	static c_DriverListConfig * s_instance;
	const uint16_t c_MUTEX_TIMEOUT = 1000;
	void UpdateConfigStruct(s_driverlist_config_t & config_struct, e_UPDATE_TYPE type, uint8_t length = MAX_DRIVER_ID_LENGTH );
	bool AddSeperatorEndofID( uint8_t * id, uint8_t length );
//	uint8_t * p_driver_list;
//	uint8_t * p_file;
protected:
	c_DriverListConfig();
public:
	// general config class members
	~c_DriverListConfig();
	static c_DriverListConfig * GetInstance();
	ARV_StatusTypeDef LoadConfig( uint8_t class_id );
	ARV_StatusTypeDef SaveConfig( uint8_t class_id );
	void LoadDefaultConfig( uint8_t class_id );
	void DeleteConfig(void);
	static const s_property_specs_t DRIVERLIST_CONF_PROPERTY_SPECS[ ];
	// special config class members
	ARV_StatusTypeDef DeleteDriverId( s_driverlist_config_t * config_struct );
	ARV_StatusTypeDef GetDriverId( int16_t index, uint8_t * driver_id, uint8_t * id_length );
	ARV_StatusTypeDef AddDriverId( uint8_t * driver_id, uint8_t id_length );
	int16_t GetDriverList( uint8_t* & list, uint8_t & id_length );
	bool ChangeMutexState( bool state );
	int32_t SearchIdInFile( uint8_t * driver_id, uint8_t id_count, c_FileSystem * fs, spiffs_file fd, uint32_t file_size, int32_t * empty_offset = nullptr );
	int32_t SearchIndexInFile( int16_t index, uint8_t * length, c_FileSystem * fs, spiffs_file fd, uint32_t file_size);
};


#endif /* CONFIGURATION_CONFIGDRIVERLIST_H_ */
