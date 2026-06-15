/**
 * configdriverlist.cpp
 *
 *  Created on: Dec 10, 2019
 *      Author: ugur.aydin
 */

#include "configdriverlist.h"
#include "configsaveload.h"

#if defined TAG
#undef TAG
#endif
#define TAG "DRVLIST"
//#define TAG ""

/*!
 * \brief surucu id degeleri ile konfigurasyonlarin yazildigi dosya isimleri farklidir.
 * Surucu id degerleri "driverlist.h" dosyasina yazilirken config struct degerleri "configdriverlist.h"
 * dosyasina yazilir.
 */

c_DriverListConfig *c_DriverListConfig::s_instance = nullptr;
osMutexId c_DriverListConfig::m_driver_list_mutex = NULL;

const uint8_t c_SEPERATOR = ',';
const uint8_t c_SEPERATOR_SIZE = 1;

const s_property_specs_t c_DriverListConfig::DRIVERLIST_CONF_PROPERTY_SPECS[ ] = {
		{ e_PROPERTY_TYPES::ARV_NONE,			e_DRIVERLIST_CONFIG_PROPERTIES::DRIVERLIST_CONFIG_PROP_MAX	},
		{ e_PROPERTY_TYPES::ARV_STRING, 		MAX_DRIVER_ID_LENGTH 										},			// driver list memory size
		{ e_PROPERTY_TYPES::ARV_INT16,			sizeof(int16_t)												},			// member count
		{ e_PROPERTY_TYPES::ARV_UINT8,			sizeof(uint8_t)												},			// member size
		{ e_PROPERTY_TYPES::ARV_INT16,			sizeof(int16_t)												}			// member index
};

c_DriverListConfig * c_DriverListConfig::GetInstance()
{
	if( s_instance == nullptr )
	{
		s_instance = new c_DriverListConfig();
	}
	return s_instance;
}

c_DriverListConfig::c_DriverListConfig()
{
	s_instance = this;
	if( m_driver_list_mutex == nullptr ) {
		m_driver_list_mutex = osApiMutexCreate( NULL);
	}
	LoadDefaultConfig( 255 );
}

c_DriverListConfig::~c_DriverListConfig(){}

ARV_StatusTypeDef c_DriverListConfig::LoadConfig( uint8_t class_id )
{
	s_driverlist_config_t config_struct;
	if( c_ConfigSaveLoad::LoadConfig( (uint8_t *)&config_struct, sizeof( config_struct), "configdriverlist.h" ) == ARV_StatusTypeDef::ARV_OK )
		setStruct( config_struct );
	else
		return ARV_StatusTypeDef::ARV_ERROR;
	return ARV_StatusTypeDef::ARV_OK;
}

ARV_StatusTypeDef c_DriverListConfig::SaveConfig( uint8_t class_id)
{
	s_driverlist_config_t config_struct = getStruct();
	return c_ConfigSaveLoad::SaveConfig( (uint8_t *)&config_struct, sizeof( config_struct), "configdriverlist.h" );
}

void c_DriverListConfig::LoadDefaultConfig( uint8_t class_id )
{
	s_driverlist_config_t m_config_struct;
	m_config_struct.user_count = 0;
	memset( m_config_struct.user_id, 0x00, MAX_DRIVER_ID_LENGTH );
	m_config_struct.user_index = 0;
	m_config_struct.user_size = 0;
	setStruct( m_config_struct );
}

/*!
 * @brief Delete driver id from driver list
 * @param driver_id : if driver id is null, delete all driver list
 * @return OK : success, ERROR : failed
 */
ARV_StatusTypeDef c_DriverListConfig::DeleteDriverId( s_driverlist_config_t * config_struct )
{
	c_FileSystem * fs = c_FileSystem::GetInstance();

	ARV_StatusTypeDef result = ARV_StatusTypeDef::ARV_ERROR;

	if( fs != nullptr && ChangeMutexState( true ) ) // check file system instance && get mutex
	{	
		/*!
		 * \brief DELETE_USER_ID_MESSAGE icerisinde USER_ID property degeri null olarak gonderilirse
		 * butun surucu id listesi silinir.
		 */

		spiffs_file fd = -1;
		bool delete_file = false;

		if( config_struct == nullptr || config_struct->user_id[ 0 ] == 0 ) // delete all command
		{
			if( config_struct != nullptr )
			{
				UpdateConfigStruct( *config_struct, e_UPDATE_TYPE::DELETE_ALL );	// update user count and size
			}
			if( fs->FileExists( "driverlist.h" ) == ARV_StatusTypeDef::ARV_OK )
			{
				delete_file = true;
			}
			else
			{
				DEBUG_PRINT("Delete Driver List OK!", TAG);
				result = ARV_StatusTypeDef::ARV_OK;
			}
		}
		else 
		{	// delete only one id
			uint32_t file_size = fs->GetFileSize2( "driverlist.h" );	// get file size
			
			if( file_size > 0 ) // is there a file and file is not empty
			{
				int8_t retry_count = 3;

				do 
				{
					fd = fs->Open("driverlist.h", e_FsReadWrite::ARV_READ_WRITE, "cfgdrvlist");	// open file
				}
				while( fd <= 0 && (--retry_count) > 0 );

				if( fd > 0 ) 
				{
					int32_t addr_in_file = SearchIdInFile( config_struct->user_id, config_struct->user_size, fs, fd, file_size ); // search id in file, retruns id start address in file
					
					if( addr_in_file >= 0 ) // is id found in file
					{ 	
						uint8_t buf[MAX_DRIVER_ID_LENGTH] = {0};
						memset( buf, 0xFF, config_struct->user_size + c_SEPERATOR_SIZE );	// write 0xFF on driver id to delete id

						if( AddSeperatorEndofID( buf, config_struct->user_size ) &&	// change last element as comma
							fs->Seek( fd, addr_in_file, SPIFFS_SEEK_SET ) == ARV_OK &&	// goto id start address
							fs->Write( fd, buf, config_struct->user_size + c_SEPERATOR_SIZE ) == ARV_OK ) // write 0xff on id
						{	
							UpdateConfigStruct( *config_struct, e_UPDATE_TYPE::DELETE_ID );	// update id coount and size
							result = ARV_StatusTypeDef::ARV_OK;

							if( config_struct->user_count == 0 )
							{
								delete_file = true;
							}

							INFO_PRINT( "Driver id= %.*s deleted!, id count=%d", TAG, config_struct->user_size, config_struct->user_id, config_struct->user_count );
						} // else result = ARV_ERROR -> open or write error
					}	// else result = ARV_ERROR -> id couldnt find in file or file couldnt open
				}	// else result = ARV_ERROR -> file couldnt open
			}	// else result = ARV_ERROR -> file empty or not created yet || getfilesize couldnt take mutex
		}

		if( fd > 0 ) 
		{	
			fs->Close( fd ); // close file
		}
		if( delete_file )
		{
			result = fs->Rmfile( "driverlist.h" ); // delete file

			if( result == ARV_StatusTypeDef::ARV_OK )
				INFO_PRINT("Driver List File deleted!", TAG);
			else
				ERR_PRINT("Driver List File Deletion Failed",TAG );
		}

		ChangeMutexState( false );	// release mutex
	} // else result = ARV_ERROR -> take mutex fail

	return result;
}

/*!
 * @brief get driver id with index from driver list
 * @param index
 * @param driver_id
 * @return OK : success, ERROR : failed, BUSY : index is empty
 */
ARV_StatusTypeDef c_DriverListConfig::GetDriverId( int16_t index, uint8_t * driver_id, uint8_t * id_length )
{
	ARV_StatusTypeDef result = ARV_StatusTypeDef::ARV_ERROR;
	c_FileSystem * fs = c_FileSystem::GetInstance();
	spiffs_file fd = -1;
	int8_t retry_count = 3;
	if( fs == nullptr ) {	// couldn't get file system instance
		return ARV_StatusTypeDef::ARV_ERROR;
	}

	if( ChangeMutexState( true ) ) // get mutex
	{
		uint32_t file_size = fs->GetFileSize2( "driverlist.h" );	// get file size
		if( file_size > 0 ) // there is a file and file is not empty
		{
			do
			{
				fd = fs->Open("driverlist.h", e_FsReadWrite::ARV_READ, "cfgdrvlist");
			}
			while( fd <= 0 && (--retry_count) > 0);

			if( fd > 0 )
			{
				int32_t addr_in_file = SearchIndexInFile( index, id_length, fs, fd, file_size );	// search id in file, retruns id start address in file
				if( addr_in_file >= 0)
				{
					if( fs->Seek( fd, addr_in_file, SPIFFS_SEEK_SET ) == ARV_OK && // goto index
						fs->Read( fd, driver_id, *id_length ) == ARV_OK ) // read id from file
					{
						result = ARV_StatusTypeDef::ARV_OK;
						INFO_PRINT( "Driver id= %.*s read!, id length=%d", TAG, id_length, driver_id, *id_length );
					} // else result = ARV_ERROR -> seek or write error
				}
				else
				{
					result = ARV_StatusTypeDef::ARV_BUSY;	// index greater than file size, there is no member for this index
					WARN_PRINT("Empty index", TAG );
				}
			} // else result = ARV_ERROR -> file couldnt open
		} // else result = ARV_ERROR -> file empty or not created yet || getfilesize couldnt take mutex

		if( fd > 0 )
		{
			fs->Close( fd ); // close file
		}

		ChangeMutexState( false );	// release mutex
	} // else result = ARV_ERROR -> take mutex fail
	return result;
}

/*!
 * @brief Add seperator char end of the driver id string
 * @param id id string
 * @param length id length greater than max id length
 * @return true : ok, false : id length
 */
bool c_DriverListConfig::AddSeperatorEndofID( uint8_t * id, uint8_t length )
{
	if( length <= MAX_DRIVER_ID_LENGTH )
	{
		id[length]=',';	// add comma the end of id
		return true;
	}
	return false;
}

/*!
 * @brief add driver id to driver list
 * @param driver_id user id
 * @return result ARV_StatusTypeDef
 */
ARV_StatusTypeDef c_DriverListConfig::AddDriverId( uint8_t * driver_id, uint8_t id_length )
{
	ARV_StatusTypeDef result = ARV_StatusTypeDef::ARV_ERROR;
	c_FileSystem * fs = c_FileSystem::GetInstance();

	if( fs != nullptr && ChangeMutexState( true ) ) // check file system instance && get mutex
	{	
		s_driverlist_config_t config_struct = getStruct();
		uint32_t file_size = fs->GetFileSize2( "driverlist.h" );	// get file size

		spiffs_file fd = -1;
		int8_t retry_count = 3;	

		if( file_size == 0 ) // file empty or not created yet
		{	
			do 
			{
				fd = fs->Open("driverlist.h", e_FsReadWrite::ARV_WRITE, "cfgdrvlist");		//create file
			}
			while( fd <= 0 && (--retry_count) > 0);

			if( fd > 0 )
			{	// we can open the file

				if( AddSeperatorEndofID( driver_id, id_length ) &&	// add seperator end of the id string
					fs->Write( fd, driver_id, id_length + c_SEPERATOR_SIZE ) == ARV_StatusTypeDef::ARV_OK ) // write id to file
				{	
					UpdateConfigStruct( config_struct, e_UPDATE_TYPE::ADD_ID, id_length );	// update id count and size
					INFO_PRINT( "Add OK! id:%.*s, count:%d, file_size:%d", TAG, id_length, driver_id, config_struct.user_count, file_size );
					result = ARV_StatusTypeDef::ARV_OK;
				} // else result = ARV_ERROR
			} // else result = ARV_ERROR
		}
		else if( file_size > 0 ) // is there a file and file is not empty
		{	
			do 
			{
				fd = fs->Open("driverlist.h", e_FsReadWrite::ARV_READ_WRITE, "cfgdrvlist");		//open file
			}
			while( fd <= 0 && ( --retry_count ) > 0 );

			if( fd > 0 ) 
			{
				// search file for same id
				int32_t empty_offset = -1;
				int32_t offset = SearchIdInFile( driver_id, id_length, fs, fd, file_size, &empty_offset );

				if( offset == -1 ) // we can add, couldn't find same id in file
				{	
					if( fs->Seek( fd, empty_offset, SPIFFS_SEEK_SET ) == ARV_OK &&				// goto file end
						AddSeperatorEndofID(driver_id, id_length) &&							// add comma seperator
						fs->Write( fd, driver_id, id_length + c_SEPERATOR_SIZE ) == ARV_OK )  	// write id to file
					{	
						UpdateConfigStruct( config_struct, e_UPDATE_TYPE::ADD_ID, id_length );		// update driver list config struct
						INFO_PRINT( "Add OK! id:%.*s, count:%d, file_size:%d", TAG, id_length, driver_id, config_struct.user_count, file_size );
						result = ARV_StatusTypeDef::ARV_OK;
					}
				}
				else if( offset >= 0 ) // we found same id in the file
				{		
					WARN_PRINT("Same id! id:%s, count:%d, file_size:%d", TAG, driver_id, config_struct.user_count, file_size );
					result = ARV_StatusTypeDef::ARV_OK;
				} // else result = ARV_ERROR -> file couldnt open
			}
		} // else result = ARV_ERROR -> getfilesize couldnt take mutex

		if( fd > 0 ) // close file
		{	
			fs->Close( fd );
		}

		ChangeMutexState( false );	// release mutex
	} // else result = ARV_ERROR -> take mutex fail

	if( result != ARV_StatusTypeDef::ARV_OK ) 
	{
		ERR_PRINT("Set User Id Error",TAG);
	}
	
	return result;
}

/*!
 * @brief get or release mutex
 * @param state : false -> release mutex, true : get mutex
 * @return	false : error, true : ok
 */
bool c_DriverListConfig::ChangeMutexState( bool state )
{
	if( state ) {	// get mutex
		return ( ( osApiMutexWait( m_driver_list_mutex, c_MUTEX_TIMEOUT ) == osStatus::osOK ) ? true : false );
	}
	else {	// release mutexet
		return ( ( osApiMutexRelease( m_driver_list_mutex ) == osStatus::osOK ) ? true : false );
	}
}

/*!
 * @brief update config driver list struct
 * @param type : add id, delete id or delete all
 * @param length : id length
 */
void c_DriverListConfig::UpdateConfigStruct(s_driverlist_config_t & config_struct, e_UPDATE_TYPE type, uint8_t length)
{
	if( type == e_UPDATE_TYPE::ADD_ID )
	{
		config_struct.user_count++;

		if( config_struct.user_count >= MAX_DRIVER_ID_COUNT )
		{
			config_struct.user_count = MAX_DRIVER_ID_COUNT;
		}
	}
	else if( type == e_UPDATE_TYPE::DELETE_ID )
	{
		config_struct.user_count--;

		if( config_struct.user_count < 0 )
		{
			config_struct.user_count = 0;
		}
	}
	else // delete all
	{
		config_struct.user_count = 0;
		config_struct.user_size = 0;
	}

	if( config_struct.user_count == 1 ) // set member size only first member of driver list
	{
		config_struct.user_size = length;
	}
	else if( config_struct.user_count == 0 ) // reset member size, it may change
	{
		config_struct.user_size = 0;
	}

	setStruct(config_struct);
	SaveConfig(e_CLASSES::CONFIG_DRIVERLIST);
}

/*!
 * @brief search received id in file
 * @param driver_id
 * @param id_length
 * @param fs
 * @param fd
 * @param file_size
 * @param empty_offset
 * @return -1 : id couldnt find in file, >= 0 : id offset, -2 : pointer/seek/read error
 */
#pragma GCC push_options
#pragma GCC optimize("O0")
/*!
 * \fn int32_t SearchIdInFile(uint8_t*, uint8_t, c_FileSystem*, spiffs_file, uint32_t, int32_t*)
 * \brief Gonderilen id string i dosya icerisinde var mi yok mu diye kontrol ediliyor. Id bulunursa baslangic adresi donuluyor,
 * bulunamaz ya da hata olursa negatif deger donuluyor
 * \param driver_id aranacak id
 * \param id_length aranan id nin uzunlugu
 * \param fs file system instance
 * \param fd file descriptor
 * \param file_size dosya boyutu
 * \param empty_offset bos id alani baslangic adresi
 * \return id baslangic offset degeri
 */
int32_t c_DriverListConfig::SearchIdInFile( uint8_t* driver_id, uint8_t id_length, c_FileSystem* fs, spiffs_file fd, uint32_t file_size, int32_t* empty_offset )
{
	int32_t offset = -1;

	// Parametre kontrolleri
	if( driver_id == nullptr || id_length == 0 || id_length > MAX_DRIVER_ID_LENGTH || fs == nullptr || fd <= 0 || file_size == 0 )
	{
		ERR_PRINT("%s param error", TAG, __func__ );
		return -2;
	}

	// Chunk Chunk dosya okuma öncesi dosya pointer'ını başa alıyoruz
	if( fs->Seek( fd, 0, SPIFFS_SEEK_SET ) != ARV_OK )
	{
		ERR_PRINT( "Driver list file seek error", TAG );
		return -2;
	}

	const uint16_t read_chunk_size = ( uint16_t )( 10 * MAX_DRIVER_ID_LENGTH );
	uint8_t read_buffer[ read_chunk_size ] = { 0 };
	uint8_t temp_id[ MAX_DRIVER_ID_LENGTH ] = { 0 };
	uint8_t empty_id[ MAX_DRIVER_ID_LENGTH ] = { 0 };
	uint8_t temp_len = 0;
	uint32_t bytes_processed = 0;
	int32_t id_start_offset = 0;
	bool empty_offset_set = false;

	memset( empty_id, 0xFF, MAX_DRIVER_ID_LENGTH );

	while( bytes_processed < file_size )
	{
		uint16_t chunk_len = ( ( file_size - bytes_processed ) > read_chunk_size ) ? read_chunk_size : (uint16_t)( file_size - bytes_processed );
		ARV_StatusTypeDef ret = ARV_ERROR;
		int8_t retry_count = 3;

		do
		{
			ret = fs->Read( fd, read_buffer, chunk_len );
		}
		while( --retry_count > 0 && ret != ARV_OK );

		if( ret != ARV_OK )
		{
			ERR_PRINT( "Driver list file read error", TAG );
			return -2;
		}

		for( uint16_t i = 0; i < chunk_len; i++ ) // tek tek karakterleri kontrol ediyoruz ne zaman "," karakteri gelirse id'yi kontrol ediyoruz
		{
			const uint8_t current_char = read_buffer[ i ];
			const int32_t current_offset = ( int32_t )( bytes_processed + i );

			if( current_char == c_SEPERATOR )
			{
				if( temp_len == id_length ) // Bir id kaydi tamamlandi, once uzunluk uyumunu kontrol et
				{
					if( memcmp( driver_id, temp_id, id_length ) == 0 ) // Eğer id'ler eşleşiyorsa, id'nin başlangıç offset'ini döndür
					{
						DEBUG_PRINT( "%s match offset : %d", TAG, __func__, id_start_offset );
						return id_start_offset;
					}

					// Eğer id eşleşmezse, bu id alanının boş olup olmadığını kontrol et ve eğer daha önce empty offset belirlenmediyse yeni bir empty_offset belirle
					if( empty_offset != nullptr && !empty_offset_set && memcmp( empty_id, temp_id, temp_len ) == 0 )
					{
						// Eğer id alanı boş ise, boş id alanının başlangıç offset'ini kaydet
						empty_offset_set = true;
						*empty_offset = id_start_offset;
						DEBUG_PRINT( "%s empty slot offset : %d", TAG, __func__, id_start_offset );
					}
				}

				// Sonraki kayıtlı id'yi bulabilmek için geçici id buffer'ını temizle ve id başlangıç offset'ini güncelle
				memset( temp_id, 0, temp_len );
				temp_len = 0;
				id_start_offset = current_offset + 1;
			}
			else
			{
				if( temp_len >= MAX_DRIVER_ID_LENGTH ) // Eğer geçici id buffer'ı maksimum uzunluğa ulaşırsa, bu bir format hatasıdır
				{
					ERR_PRINT( "%s Position Error", TAG, __func__ );
					return -2;
				}

				temp_id[ temp_len++ ] = current_char; // Geçici id buffer'ına karakteri ekle ve uzunluğu artır
			}
		}

		// Okunan chunk tamamlandıktan sonra, eğer chunk'ın sonu bir id kaydının ortasında ise, bu durumda temp_id ve temp_len değişkenleri bir sonraki 
		// chunk okumasında devam edecek şekilde kalır, ancak read_buffer temizlenir
		memset( read_buffer, 0, chunk_len );
		bytes_processed += chunk_len;
	}

	if( empty_offset != nullptr && *empty_offset == -1 )
	{
		*empty_offset = file_size; // bos id alani bulamadiysak dosya boyutunu kaydet
	}

	return offset;
}
/*!
 * \fn int32_t SearchIndexInFile(int16_t, uint8_t*, c_FileSystem*, spiffs_file, uint32_t)
 * \brief Driver id dosyasi icerisinde belirtilen indeksli id'nin dosya icerisindeki indeksini doner
 * \param index istenen indeks
 * \param length istenen index deki id nin uzunlugu
 * \param fs file system instance
 * \param fd file descriptor
 * \param file_size dosya boyutu
 * \return -1 : id couldnt find in file, >= 0 : id offset, -2 : pointer/seek/read error
 */
int32_t c_DriverListConfig::SearchIndexInFile( int16_t index, uint8_t * length, c_FileSystem * fs, spiffs_file fd, uint32_t file_size )
{
	int32_t offset = -1;

	if( length == nullptr || fs == nullptr || fd <= 0 || file_size <= 0 ) 	// kontrol
	{
		return -2;
	}

	// Chunk Chunk dosya okuma öncesi dosya pointer'ını başa alıyoruz
	if( fs->Seek( fd, 0, SPIFFS_SEEK_SET ) != ARV_OK )
	{
		return -2;
	}

	const uint16_t read_chunk_size = ( uint16_t )( 10 * MAX_DRIVER_ID_LENGTH );
	uint8_t read_buffer[ read_chunk_size ] = { 0 };
	uint8_t temp_id[ MAX_DRIVER_ID_LENGTH ] = { 0 };
	uint8_t temp_len = 0;
	uint32_t bytes_processed = 0;
	int32_t id_start_offset = 0;
	uint16_t temp_index = 0;

	while( bytes_processed < file_size )
	{
		uint16_t chunk_len = ( ( file_size - bytes_processed ) > read_chunk_size ) ? read_chunk_size : (uint16_t)( file_size - bytes_processed );
		ARV_StatusTypeDef ret = ARV_ERROR;
		int8_t retry_count = 3;

		do
		{
			ret = fs->Read( fd, read_buffer, chunk_len );
		}
		while( --retry_count > 0 && ret != ARV_OK );

		if( ret != ARV_OK )
		{
			ERR_PRINT("driver list file read error",TAG);
			return -2;
		}

		for( uint16_t i = 0; i < chunk_len; i++ )
		{
			const uint8_t current_char = read_buffer[ i ];
			const int32_t current_offset = ( int32_t )( bytes_processed + i );

			if( current_char == c_SEPERATOR )
			{	
				uint8_t empty_id[ MAX_DRIVER_ID_LENGTH ] = { 0 };
				memset( empty_id, 0xFF, MAX_DRIVER_ID_LENGTH );

				if( memcmp( temp_id, empty_id, MAX_DRIVER_ID_LENGTH ) == 0 ) // Eğer id alanı boş ise, bu index'i atla
				{
					index++;
				}
				else 
				{
					if( temp_index == index ) // İstenen index'e ulaşıldığında, id'nin başlangıç offset'ini döndür
					{
						*length = temp_len;
						return id_start_offset;
					}
				}

				// Sonraki kayıtlı id'yi bulabilmek için geçici id buffer'ını temizle ve id başlangıç offset'ini güncelle
				memset( temp_id, 0, temp_len );
				temp_len = 0;
				id_start_offset = current_offset + 1;
				temp_index++;
			}
			else
			{
				if( temp_len >= MAX_DRIVER_ID_LENGTH )
				{
					ERR_PRINT("Position Error",TAG);
					return -2;
				}

				temp_id[ temp_len++ ] = current_char;
			}
		}

		bytes_processed += chunk_len;
	}

	return offset; // aranan id nin offset degeri
}
#pragma GCC pop_options
/*!
 * @brief read all driver list file content, call osApiFree for list pointer after process
 * @param list_pointer
 * @param id_length : single user id length
 * @return -1 : error, 0 : no user_id, > 0 : user id count
 */
int16_t c_DriverListConfig::GetDriverList( uint8_t* & list, uint8_t & id_length )	// UA_NOTE : Bu fonskiyon kullanilmiyor
{
	char buffer[MAX_DRIVER_ID_LENGTH] = { 0 };
	int16_t id_count = -1;	// -1 means is error
	c_FileSystem * fs = c_FileSystem::GetInstance();
	spiffs_file fd = -1;
	int8_t retry_count = 3;
	if( fs == nullptr ) {						// couldn't get file system instance
		return -1;
	}

	int32_t file_size = fs->GetFileSize2( "driverlist.h" );	// get file size
	if( file_size < 0 ) {	// get file size error
		return -1;
	}
	else if( file_size == 0 ) {	// file is empty
		return 0; 		// no element at the list
	}

	if( !ChangeMutexState( true ) ) {	// take mutex error
		return -1;
	}
	// try to open file
	do {
		fd = fs->Open("driverlist.h", e_FsReadWrite::ARV_READ, "cfgdrvlist");
	}while( fd <= 0 && (--retry_count) > 0);

	if( fd <= 0 ) {	// we couldnt open the file
		ChangeMutexState( false );
		return -1;
	}

	// get user id count and user id length
	s_driverlist_config_t conf = getStruct();
	if( conf.user_count == 0 || conf.user_size == 0 ) {		// it may be a get struct error
		ChangeMutexState( false );
		return 0; 	// no element at the list
	}

	int16_t expected_file_size = conf.user_count * conf.user_size;	// calculate expected list length
	// allocate list pointer
	list = (uint8_t*)osApiMalloc(expected_file_size);
	if( list == nullptr ) {	// allocation failed
		ChangeMutexState( false );
		return -1;
	}

	if( file_size == expected_file_size ) {	// no null element
		if( fs->Read( fd, list, file_size ) == ARV_OK ) {
			id_count = conf.user_count;
			id_length = conf.user_size;
		}
	}
	else if( file_size > expected_file_size ) {	// there are null elements
		memset( buffer, 0x00, conf.user_size );
		uint16_t list_index = 0;
		for( uint8_t i = 0; i < file_size; i += conf.user_size ) {
			if( fs->Read( fd, &list[list_index], conf.user_size ) == ARV_OK ) {
				if( memcmp( &list[list_index], buffer, conf.user_size ) ) {	// null element
					list_index += conf.user_size;
				}
			}
		}
		id_count = conf.user_count;
		id_length = conf.user_size;
	}
	else if( file_size < expected_file_size ) {	// file size error
		ChangeMutexState( false );
		return -1;
	}

	if( fd > 0 ) {	// close file
		fs->Close( fd );
	}
	ChangeMutexState( false );	// release mutex
	return id_count;
}

void c_DriverListConfig::DeleteConfig(void)
{
	c_ConfigSaveLoad::DeleteConfig( "configdriverlist.h" );
}

