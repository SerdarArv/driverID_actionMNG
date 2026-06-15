/**
 * configminidiu.cpp
 *
 *  Created on: Dec 10, 2019
 *      Author: ugur.aydin
 */

#include "configminidiu.h"
#include "configsaveload.h"

const s_property_specs_t c_MinidiuConfig::MINIDIU_CONF_PROPERTY_SPECS[ ] = {
		{ e_PROPERTY_TYPES::ARV_NONE,			e_MINIDIU_CONFIG_PROPERTIES::MINIDIU_CONFIG_PROP_MAX	},
		{ e_PROPERTY_TYPES::ARV_UINT8, 			sizeof( uint8_t )										},			// status
		{ e_PROPERTY_TYPES::ARV_UINT8, 			sizeof( uint8_t ) 										},			// channel id
		{ e_PROPERTY_TYPES::ARV_UINT8, 			sizeof( uint8_t ) 										},			// usage method
		{ e_PROPERTY_TYPES::ARV_UINT8, 			sizeof( uint8_t ) 										},			// checkout option
		{ e_PROPERTY_TYPES::ARV_UINT32,			sizeof( uint32_t ) 										},			// checkout timeout
		{ e_PROPERTY_TYPES::ARV_UINT8, 			sizeof( uint8_t ) 										},			// vehicle block option
		{ e_PROPERTY_TYPES::ARV_UINT16, 		sizeof( uint16_t ) 										},			// buzzer start timeout
		{ e_PROPERTY_TYPES::ARV_UINT16, 		sizeof( uint16_t ) 										},			// buzzer active duration
		{ e_PROPERTY_TYPES::ARV_UINT8, 			sizeof( uint8_t )										}			// NFC read ndef
};

c_MinidiuConfig::c_MinidiuConfig()
{
	LoadDefaultConfig( 255 );
}

c_MinidiuConfig::~c_MinidiuConfig()
{}

ARV_StatusTypeDef c_MinidiuConfig::LoadConfig( uint8_t class_id )
{
	s_minidiu_config_t config_struct;
	if( c_ConfigSaveLoad::LoadConfig( (uint8_t *)&config_struct, sizeof( config_struct), "configminidiu.h" ) == ARV_StatusTypeDef::ARV_OK )
		setStruct( config_struct );
	else
		return ARV_StatusTypeDef::ARV_ERROR;
	return ARV_StatusTypeDef::ARV_OK;
}

ARV_StatusTypeDef c_MinidiuConfig::SaveConfig( uint8_t class_id)
{
	s_minidiu_config_t config_struct = getStruct();
	return c_ConfigSaveLoad::SaveConfig( (uint8_t *)&config_struct, sizeof( config_struct), "configminidiu.h" );
}

void c_MinidiuConfig::LoadDefaultConfig( uint8_t class_id )
{
	// optinal device default status value disable
	s_minidiu_config_t m_config_struct;
	m_config_struct.status = e_STATUSES::STATUS_DISABLE;
	m_config_struct.channel_id = e_CHANNEL_IDs::CHAN_UNKWON;
	m_config_struct.usage_method = e_DIU_USAGE_METHOD::DRIVER_CONTROL;
	m_config_struct.checkout_option = e_DIU_CHECKOUT_OPTION::MANUAL_CHECKOUT;
	m_config_struct.checkout_tmo = 0;
	m_config_struct.block_option = e_DIU_VEHICLE_BLOCK_OPTION::NO_BLOCKING;
	m_config_struct.buzzer_start_tmo = 10;
	m_config_struct.buzzer_duration = 30;
	m_config_struct.read_ndef = e_STATUSES::STATUS_DISABLE;
	setStruct( m_config_struct );
}

void c_MinidiuConfig::DeleteConfig(void)
{
	c_ConfigSaveLoad::DeleteConfig( "configminidiu.h" );
}
