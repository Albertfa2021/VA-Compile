/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2021
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

// Temporarily "disable" debug to include Python.
// This is done, so that the Python debug libraries are not needed, even when building
// this module in debug mode.
#ifdef _DEBUG
#	define TMP_DEBUG
#	undef _DEBUG
#endif
#include <Python.h>
#ifdef TMP_DEBUG
#	define _DEBUG
#	undef TMP_DEBUG
#endif

#include <VA.h>
#include <VANet.h>
#include <string.h>

// If you want to extend the va Python pSelf interface, also add
// the function to the va_methods table in vasingleton.cpp - otherwise they will not show up.
// Documentation goes into vasingletondoc.hpp

static IVANetClient* g_pVANetClient = nullptr; //!< Static pointer to VANetClient instance
static PyObject* g_pVAError         = nullptr; //!< Static pointer to error instance

// Ugly definitions to ease try-catching VA exceptions
#define VAPY_REQUIRE_CONN_TRY \
	try                       \
	{                         \
		RequireCoreAvailable( );
#define VAPY_CATCH_RETURN                                                \
	}                                                                    \
	catch( const CVAException& oError )                                  \
	{                                                                    \
		PyErr_SetString( PyExc_Exception, oError.ToString( ).c_str( ) ); \
		return NULL;                                                     \
	}

//! Helper for API dev
static PyObject* not_implemented( PyObject*, PyObject* )
{
	VA_EXCEPT_NOT_IMPLEMENTED;
};

//! Raises an exception if core is not available
static void RequireCoreAvailable( )
{
	if( !g_pVANetClient )
		VA_EXCEPT2( NETWORK_ERROR, "VA client not available, please connect first" );

	if( !g_pVANetClient->GetCoreInstance( ) )
		VA_EXCEPT2( NETWORK_ERROR, "VA client available, but access to VA interface failed. Please reconnect." );
};

std::string SaveStringToUnicodeConversion( const std::string& sInputString )
{
	std::string sOutputString = sInputString;
	const Py_ssize_t iLength  = sInputString.length( );
	char* pcBuffer( &sOutputString[0] );
	for( Py_ssize_t i = 0; i < iLength; i++ )
		if( pcBuffer[i] < 0 )
			pcBuffer[i] = '_';
	return &sOutputString[0];
};

PyObject* ConvertFloatVectorToPythonList( const std::vector<float> vfValues )
{
	PyObject* pList = PyList_New( vfValues.size( ) );

	for( Py_ssize_t i = 0; i < PyList_GET_SIZE( pList ); i++ )
		PyList_SetItem( pList, i, PyLong_FromDouble( vfValues[i] ) );

	return pList;
};

PyObject* ConvertDoubleVectorToPythonList( const std::vector<double> vdValues )
{
	PyObject* pList = PyList_New( vdValues.size( ) );

	for( Py_ssize_t i = 0; i < PyList_GET_SIZE( pList ); i++ )
		PyList_SetItem( pList, i, PyLong_FromDouble( vdValues[i] ) );

	return pList;
};

PyObject* ConvertIntVectorToPythonList( const std::vector<int> viValues )
{
	PyObject* pList = PyList_New( viValues.size( ) );

	for( Py_ssize_t i = 0; i < PyList_GET_SIZE( pList ); i++ )
		PyList_SetItem( pList, i, PyLong_FromLong( viValues[i] ) );

	return pList;
};

//! Helper to convert recursively from VAStruct to Python dict
PyObject* ConvertVAStructToPythonDict( const CVAStruct& oInStruct )
{
	PyObject* pOutDict = PyDict_New( );

	CVAStruct::const_iterator cit = oInStruct.Begin( );
	while( cit != oInStruct.End( ) )
	{
		const std::string sKey( ( *cit++ ).first );
		const CVAStructValue& oValue( oInStruct[sKey] );

		PyObject* pNewValue = nullptr;
		if( oValue.IsBool( ) )
		{
			pNewValue = PyBool_FromLong( bool( oValue ) );
		}
		else if( oValue.IsInt( ) )
		{
			pNewValue = PyLong_FromLong( int( oValue ) );
		}
		else if( oValue.IsDouble( ) )
		{
			pNewValue = PyFloat_FromDouble( double( oValue ) );
		}
		else if( oValue.IsString( ) )
		{
			pNewValue = PyUnicode_FromString( SaveStringToUnicodeConversion( std::string( oValue ) ).c_str( ) );
		}
		else if( oValue.IsStruct( ) )
		{
			pNewValue = ConvertVAStructToPythonDict( oValue );
		}
		else if( oValue.IsData( ) )
		{
			pNewValue = PyByteArray_FromStringAndSize( (char*)oValue.GetData( ), oValue.GetDataSize( ) );
			Py_INCREF( pNewValue );
		}
		else if( oValue.IsSampleBuffer( ) )
		{
			const CVASampleBuffer& oSampleBuffer( oValue );
			pNewValue = PyList_New( oSampleBuffer.GetNumSamples( ) );
			Py_INCREF( pNewValue );
			for( int i = 0; i < oSampleBuffer.GetNumSamples( ); i++ )
				PyList_SetItem( pNewValue, i, PyFloat_FromDouble( oSampleBuffer.GetDataReadOnly( )[i] ) );
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret value of key '" + sKey + "' as a supported python dict type. Value was" + oValue.ToString( ) );
		}

		if( !pNewValue )
			VA_EXCEPT2( INVALID_PARAMETER, "Could not create python object from value of key '" + sKey + "'. Value was" + oValue.ToString( ) );

		if( PyDict_SetItemString( pOutDict, sKey.c_str( ), pNewValue ) == -1 )
			VA_EXCEPT2( INVALID_PARAMETER, "Could not create python object from value of key '" + sKey + "'. Value was" + oValue.ToString( ) );
	}

	return pOutDict;
};

//! Helper to convert recursively from Python dict to VAStruct
CVAStruct ConvertPythonDictToVAStruct( PyObject* pInDict )
{
	CVAStruct oReturn;

	if( pInDict == nullptr )
		return oReturn;

	PyObject* pKeyList   = PyDict_Keys( pInDict );
	PyObject* pValueList = PyDict_Values( pInDict );

	for( Py_ssize_t i = 0; i < PyList_Size( pKeyList ); i++ )
	{
		PyObject* pKey   = PyList_GetItem( pKeyList, i );
		PyObject* pValue = PyList_GetItem( pValueList, i );
		char* pcKeyName  = nullptr;
		if( !PyArg_Parse( pKey, "s", &pcKeyName ) )
			VA_EXCEPT2( INVALID_PARAMETER, "Invalid key '" + std::string( pcKeyName ) + "'" );

		if( Py_None == pValue )
		{
			oReturn[pcKeyName] = false;
		}
		else if( PyBool_Check( pValue ) )
		{
			oReturn[pcKeyName] = ( PyLong_AsLong( pValue ) != 0 );
		}
		else if( PyLong_Check( pValue ) )
		{
			oReturn[pcKeyName] = PyLong_AsLong( pValue );
		}
		else if( PyFloat_Check( pValue ) )
		{
			oReturn[pcKeyName] = PyFloat_AsDouble( pValue );
		}
		else if( PyUnicode_Check( pValue ) )
		{
			char* pcStringValue = nullptr;
			if( !PyArg_Parse( pValue, "s", &pcStringValue ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Invalid string value at key '" + std::string( pcKeyName ) + "': " + std::string( pcStringValue ) );
			oReturn[pcKeyName] = std::string( pcStringValue );
		}
		else if( PyDict_Check( pValue ) )
		{
			oReturn[pcKeyName] = ConvertPythonDictToVAStruct( pValue );
		}
		else if( PyList_Check( pValue ) )
		{
			// Sample buffer
			CVASampleBuffer oBuffer( int( PyList_Size( pValue ) ) );
			for( int n = 0; n < oBuffer.GetNumSamples( ); n++ )
			{
				PyObject* pSample = PyList_GetItem( pValue, n );
				if( !PyFloat_Check( pSample ) )
				{
					VA_EXCEPT2( INVALID_PARAMETER, "Samples must be floating point values" );
				}
				else
				{
					oBuffer.vfSamples[n] = float( PyFloat_AsDouble( pSample ) );
				}
			}
			oReturn[pcKeyName] = oBuffer;
		}
		else if( PyBytes_Check( pValue ) )
		{
			// Data blob
			size_t iBytes      = PyBytes_Size( pValue );
			char* pcData       = PyBytes_AsString( pValue );
			oReturn[pcKeyName] = CVAStructValue( pcData, int( iBytes ) );
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Could not interpret value of key '" + std::string( pcKeyName ) + "' as a supported VAStruct type." )
		}
	}

	return oReturn;
};

CVAAcousticMaterial ConvertPythonDictToAcousticMaterial( PyObject* pMaterial )
{
	CVAAcousticMaterial oMaterial;
	VA_EXCEPT_NOT_IMPLEMENTED;
	return oMaterial;
}


// ------------------------------- Python module extension methods

static PyObject* connect( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	if( !g_pVANetClient )
		g_pVANetClient = IVANetClient::Create( );

	if( g_pVANetClient->IsConnected( ) )
	{
		PyErr_WarnEx( NULL, "Was still connected, forced disconnect.", 1 );
		g_pVANetClient->Disconnect( );
	}

	static char* pKeyWordList[] = { "name", "port", NULL };
	const char* pcFormat        = "|si:connect";
	char* pcServerIP            = nullptr;
	int iServerPort             = 12340;

	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcServerIP, &iServerPort ) )
		return NULL;

	std::string sServerIP = pcServerIP ? std::string( pcServerIP ) : "localhost";

	if( IVANetClient::VA_NO_ERROR == g_pVANetClient->Initialize( sServerIP, iServerPort ) )
		return PyBool_FromLong( 1 );

	PyErr_SetString( PyExc_ConnectionError, std::string( "Could not connect to " + sServerIP + " on " + std::to_string( (long)iServerPort ) ).c_str( ) );
	return NULL;
};

static PyObject* disconnect( PyObject*, PyObject* )
{
	if( !g_pVANetClient )
		return PyBool_FromLong( 0 );

	return PyBool_FromLong( g_pVANetClient->Disconnect( ) );
};

static PyObject* is_connected( PyObject*, PyObject* )
{
	if( !g_pVANetClient )
		return PyBool_FromLong( 0 );
	else
		return PyBool_FromLong( g_pVANetClient->IsConnected( ) );
};

static PyObject* reset( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	g_pVANetClient->GetCoreInstance( )->Reset( );
	Py_INCREF( Py_None );
	return Py_None;
	VAPY_CATCH_RETURN;
};

static PyObject* get_version( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	CVAVersionInfo oInfo;
	g_pVANetClient->GetCoreInstance( )->GetVersionInfo( &oInfo );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( oInfo.ToString( ) ).c_str( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* get_modules( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;

	std::vector<CVAModuleInfo> voModuleInfos;
	g_pVANetClient->GetCoreInstance( )->GetModules( voModuleInfos );

	PyObject* pModuleList = PyList_New( voModuleInfos.size( ) );

	for( size_t i = 0; i < voModuleInfos.size( ); i++ )
	{
		CVAModuleInfo& oModule( voModuleInfos[i] );
		PyObject* pModuleInfo = Py_BuildValue( "{s:i,s:s,s:s}", "index", i, "name", SaveStringToUnicodeConversion( oModule.sName ).c_str( ), "description",
		                                       SaveStringToUnicodeConversion( oModule.sDesc ).c_str( ) );
		PyList_SetItem( pModuleList, i, pModuleInfo ); // steals reference
	}

	return pModuleList;

	VAPY_CATCH_RETURN;
};

static PyObject* call_module( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "module_name", "arguments_dict", NULL };
	const char* pcFormat        = "sO!:call_module";
	char* pcModuleName          = nullptr;
	PyObject* pArgumentsDict    = nullptr;

	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcModuleName, &pArgumentsDict ) )
		return NULL;

	std::string sModuleName = pcModuleName ? std::string( pcModuleName ) : "";
	CVAStruct oInArgs       = ConvertPythonDictToVAStruct( pArgumentsDict );
	CVAStruct oOutArgs      = g_pVANetClient->GetCoreInstance( )->CallModule( sModuleName, oInArgs );

	return ConvertVAStructToPythonDict( oOutArgs );

	VAPY_CATCH_RETURN;
};

static PyObject* add_search_path( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "directory_path", NULL };
	const char* pcFormat        = "s:add_search_path";
	char* pcPath                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcPath ) )
		return NULL;

	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->AddSearchPath( std::string( pcPath ) ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_search_paths( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	CVAStruct oPaths = g_pVANetClient->GetCoreInstance( )->GetSearchPaths( );
	return ConvertVAStructToPythonDict( oPaths );
	VAPY_CATCH_RETURN;
};

static PyObject* get_update_locked( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->GetUpdateLocked( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* lock_update( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	g_pVANetClient->GetCoreInstance( )->LockUpdate( );
	Py_INCREF( Py_None );
	return Py_None;
	VAPY_CATCH_RETURN;
};

static PyObject* unlock_update( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->UnlockUpdate( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* create_directivity_from_file( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "path", "name", NULL };
	const char* pcFormat        = "s|s:create_directivity_from_file";
	char* pcPath                = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcPath, &pcName ) )
		return NULL;

	std::string sName = pcName ? std::string( pcName ) : "";
	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->CreateDirectivityFromFile( std::string( pcPath ), sName ) );

	VAPY_CATCH_RETURN;
};

static PyObject* delete_directivity( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:delete_directivity";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;
	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->DeleteDirectivity( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_directivity_info( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_directivity_info";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	CVADirectivityInfo oInfo = g_pVANetClient->GetCoreInstance( )->GetDirectivityInfo( iID );

	PyObject* pInfo = Py_BuildValue( "{s:i,s:s,s:i,s:i,s:s}", "id", oInfo.iID, "name", SaveStringToUnicodeConversion( oInfo.sName ).c_str( ), "class", oInfo.iClass,
	                                 "references", oInfo.iReferences, "description", SaveStringToUnicodeConversion( oInfo.sDesc ).c_str( ) );

	return pInfo;

	VAPY_CATCH_RETURN;
};

static PyObject* get_directivity_infos( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;

	std::vector<CVADirectivityInfo> voInfos;
	g_pVANetClient->GetCoreInstance( )->GetDirectivityInfos( voInfos );

	PyObject* pInfoList = PyList_New( voInfos.size( ) );

	for( size_t i = 0; i < voInfos.size( ); i++ )
	{
		CVADirectivityInfo& oInfo( voInfos[i] );
		PyObject* pInfo = Py_BuildValue( "{s:i,s:s,s:i,s:i,s:s}", "id", oInfo.iID, "name", SaveStringToUnicodeConversion( oInfo.sName ).c_str( ), "class", oInfo.iClass,
		                                 "references", oInfo.iReferences, "description", SaveStringToUnicodeConversion( oInfo.sDesc ).c_str( ) );
		PyList_SetItem( pInfoList, i, pInfo ); // steals reference
	}

	return pInfoList;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_ids( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	std::vector<int> viIDs;
	g_pVANetClient->GetCoreInstance( )->GetSoundSourceIDs( viIDs );
	return ConvertIntVectorToPythonList( viIDs );
	VAPY_CATCH_RETURN;
};

static PyObject* create_sound_source( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "name", NULL };
	const char* pcFormat        = "s:create_sound_source";
	char* pcName                = nullptr;

	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcName ) )
		return NULL;

	std::string sName = pcName ? std::string( pcName ) : "PySoundSource";
	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->CreateSoundSource( sName ) );
	VAPY_CATCH_RETURN;
};


static PyObject* create_sound_source_explicit_renderer( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "renderer", "name", NULL };
	const char* pcFormat        = "ss:create_sound_source_explicit_renderer";
	char* pcRenderer            = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcRenderer, &pcName ) )
		return NULL;

	std::string sRenderer = pcRenderer ? std::string( pcRenderer ) : "Unspecified";
	std::string sName     = pcName ? std::string( pcName ) : "PySoundSource_" + sRenderer;
	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->CreateSoundSourceExplicitRenderer( sRenderer, sName ) );

	VAPY_CATCH_RETURN;
};

static PyObject* delete_sound_source( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:delete_sound_source";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->DeleteSoundSource( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_enabled( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "enabled", NULL };
	const char* pcFormat        = "i|b:set_sound_source_enabled";
	long iID                    = -1;
	bool bEnabled               = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &bEnabled ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundSourceEnabled( iID, bEnabled );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_enabled( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_enabled";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetSoundSourceEnabled( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_name( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_name";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyUnicode_FromString( SaveStringToUnicodeConversion( g_pVANetClient->GetCoreInstance( )->GetSoundSourceName( iID ) ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_signal_source( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_signal_source";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyUnicode_FromString( SaveStringToUnicodeConversion( g_pVANetClient->GetCoreInstance( )->GetSoundSourceSignalSource( iID ) ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_signal_source( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "signalsource", NULL };
	const char* pcFormat        = "is:set_sound_source_signal_source";
	long iID                    = -1;
	char* pcSignalSourceID;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pcSignalSourceID ) )
		return NULL;

	std::string sSIgnalSourceID = pcSignalSourceID ? std::string( pcSignalSourceID ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSoundSourceSignalSource( iID, sSIgnalSourceID );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* remove_sound_source_signal_source( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:remove_sound_source_signal_source";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->RemoveSoundSourceSignalSource( iID );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "short_mode", NULL };
	const char* pcFormat        = "i|b:get_sound_source_auralization_mode";
	long iID                    = -1;
	bool bShortMode             = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	const int iAM         = g_pVANetClient->GetCoreInstance( )->GetSoundSourceAuralizationMode( iID );
	const std::string sAM = SaveStringToUnicodeConversion( IVAInterface::GetAuralizationModeStr( iAM, bShortMode ) );

	return PyUnicode_FromString( sAM.c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "auralizationmode", NULL };
	const char* pcFormat        = "is:set_sound_source_auralization_mode";
	long iID                    = -1;
	char* pcAM                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pcAM ) )
		return NULL;

	std::string sAM      = pcAM ? std::string( pcAM ) : "";
	const int iCurrentAM = g_pVANetClient->GetCoreInstance( )->GetSoundSourceAuralizationMode( iID );
	const int iAM        = IVAInterface::ParseAuralizationModeStr( sAM, iCurrentAM );
	g_pVANetClient->GetCoreInstance( )->SetSoundSourceAuralizationMode( iID, iAM );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "parameters", NULL };
	const char* pcFormat        = "iO!:set_sound_source_parameters";
	long iID                    = -1;
	PyObject* pParameters       = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pParameters ) )
		return NULL;

	CVAStruct oParameters = ConvertPythonDictToVAStruct( pParameters );
	g_pVANetClient->GetCoreInstance( )->SetSoundSourceParameters( iID, oParameters );
	return Py_None;

	VAPY_CATCH_RETURN;
};


static PyObject* get_sound_source_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "io!:get_sound_source_parameters";
	long iID                    = -1;
	PyObject* pParameters       = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pParameters ) )
		return NULL;

	CVAStruct oParameters = ConvertPythonDictToVAStruct( pParameters );
	CVAStruct oReturn     = g_pVANetClient->GetCoreInstance( )->GetSoundSourceParameters( iID, oParameters );
	return ConvertVAStructToPythonDict( oReturn );

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_directivity( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_directivity";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->GetSoundSourceDirectivity( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_directivity( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "directivity", NULL };
	const char* pcFormat        = "ii:set_sound_source_directivity";
	long iID                    = -1;
	long iDirectivityID         = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &iDirectivityID ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundSourceDirectivity( iID, iDirectivityID );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_sound_power( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_sound_power";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetSoundSourceSoundPower( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_sound_power( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "gain", NULL };
	const char* pcFormat        = "id:set_sound_source_sound_power";
	long iID                    = -1;
	double dPower               = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &dPower ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundSourceSoundPower( iID, dPower );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "muted", NULL };
	const char* pcFormat        = "i|b:set_sound_source_muted";
	long iID                    = -1;
	bool bMuted                 = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &bMuted ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundSourceMuted( iID, bMuted );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_muted";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetSoundSourceMuted( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_source_position( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_position";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	VAVec3 v3Pos = g_pVANetClient->GetCoreInstance( )->GetSoundSourcePosition( iID );

	PyObject* pPosList = PyList_New( 3 );
	PyList_SetItem( pPosList, 0, PyFloat_FromDouble( v3Pos.x ) );
	PyList_SetItem( pPosList, 1, PyFloat_FromDouble( v3Pos.y ) );
	PyList_SetItem( pPosList, 2, PyFloat_FromDouble( v3Pos.z ) );

	return pPosList;

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_position( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "pos", NULL };
	const char* pcFormat        = "i(ddd):set_sound_source_position";
	long iID                    = -1;
	VAVec3 v3Pos;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &v3Pos.x, &v3Pos.y, &v3Pos.z ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundSourcePosition( iID, v3Pos );
	return Py_None;

	VAPY_CATCH_RETURN;
};


static PyObject* get_sound_source_orientation_vu( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_source_orientation_vu";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	VAVec3 v3View, v3Up;
	g_pVANetClient->GetCoreInstance( )->GetSoundSourceOrientationVU( iID, v3View, v3Up );

	return Py_BuildValue( "(ddd)(ddd)", v3View.x, v3View.y, v3View.z, v3Up.x, v3Up.y, v3Up.z );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_source_orientation_vu( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "view", "up", NULL };
	const char* pcFormat        = "i(ddd)(ddd):set_sound_source_orientation_vu";
	long iID                    = -1;
	VAVec3 v3View, v3Up;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &v3View.x, &v3View.y, &v3View.z, &v3Up.x, &v3Up.y, &v3Up.z ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundSourceOrientationVU( iID, v3View, v3Up );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_ids( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	std::vector<int> viIDs;
	g_pVANetClient->GetCoreInstance( )->GetSoundReceiverIDs( viIDs );

	PyObject* pIDList = PyList_New( viIDs.size( ) );
	for( Py_ssize_t i = 0; i < PyList_GET_SIZE( pIDList ); i++ )
		PyList_SetItem( pIDList, i, PyLong_FromLong( viIDs[i] ) );

	return pIDList;

	VAPY_CATCH_RETURN;
};

static PyObject* create_sound_receiver( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "name", NULL };
	const char* pcFormat        = "s:create_sound_receiver";
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcName ) )
		return NULL;

	std::string sName = pcName ? std::string( pcName ) : "PySoundReceiver";
	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->CreateSoundReceiver( sName ) );

	VAPY_CATCH_RETURN;
};

static PyObject* create_sound_receiver_explicit_renderer( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "renderer", "name", NULL };
	const char* pcFormat        = "ss:create_sound_receiver_explicit_renderer";
	char* pcRenderer            = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcRenderer, &pcName ) )
		return NULL;

	std::string sRenderer = pcRenderer ? std::string( pcRenderer ) : "Unspecified";
	std::string sName     = pcName ? std::string( pcName ) : "PySoundReceiver_" + sRenderer;
	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->CreateSoundReceiverExplicitRenderer( sRenderer, sName ) );

	VAPY_CATCH_RETURN;
};

static PyObject* delete_sound_receiver( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:delete_sound_receiver";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->DeleteSoundReceiver( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_enabled( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "enabled", NULL };
	const char* pcFormat        = "i|b:set_sound_receiver_enabled";
	long iID                    = -1;
	bool bEnabled               = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &bEnabled ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverEnabled( iID, bEnabled );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_enabled( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_enabled";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetSoundReceiverEnabled( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_name( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_name";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyUnicode_FromString( SaveStringToUnicodeConversion( g_pVANetClient->GetCoreInstance( )->GetSoundReceiverName( iID ) ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "short_mode", NULL };
	const char* pcFormat        = "i|b:get_sound_receiver_auralization_mode";
	long iID                    = -1;
	bool bShortMode             = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &bShortMode ) )
		return NULL;


	const int iAM         = g_pVANetClient->GetCoreInstance( )->GetSoundReceiverAuralizationMode( iID );
	const std::string sAM = SaveStringToUnicodeConversion( IVAInterface::GetAuralizationModeStr( iAM, bShortMode ) );
	return PyUnicode_FromString( sAM.c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "auralization_mode", NULL };
	const char* pcFormat        = "is:set_sound_receiver_auralization_mode";
	long iID                    = -1;
	char* pcAM                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pcAM ) )
		return NULL;

	const std::string sAM = pcAM ? std::string( pcAM ) : "";
	const int iCurrentAM  = g_pVANetClient->GetCoreInstance( )->GetSoundReceiverAuralizationMode( iID );
	const int iAM         = IVAInterface::ParseAuralizationModeStr( sAM, iCurrentAM );
	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverAuralizationMode( iID, iAM );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "parameters", NULL };
	const char* pcFormat        = "iO!:set_sound_receiver_parameters";
	long iID                    = -1;
	PyObject* pParameters       = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pParameters ) )
		return NULL;

	CVAStruct oParameters = ConvertPythonDictToVAStruct( pParameters );
	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverParameters( iID, oParameters );
	return Py_None;

	VAPY_CATCH_RETURN;
};


static PyObject* get_sound_receiver_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "io!:get_sound_receiver_parameters";
	long iID                    = -1;
	PyObject* pParameters       = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pParameters ) )
		return NULL;

	CVAStruct oParameters = ConvertPythonDictToVAStruct( pParameters );
	CVAStruct oReturn     = g_pVANetClient->GetCoreInstance( )->GetSoundReceiverParameters( iID, oParameters );
	return ConvertVAStructToPythonDict( oReturn );

	VAPY_CATCH_RETURN;
};


static PyObject* get_sound_receiver_directivity( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_directivity";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyLong_FromLong( g_pVANetClient->GetCoreInstance( )->GetSoundReceiverDirectivity( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_directivity( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "directivity", NULL };
	const char* pcFormat        = "ii:set_sound_receiver_directivity";
	long iID                    = -1;
	long iDirectivityID         = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &iDirectivityID ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverDirectivity( iID, iDirectivityID );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "muted", NULL };
	const char* pcFormat        = "i|b:set_sound_receiver_muted";
	long iID                    = -1;
	bool bMuted                 = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &bMuted ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverMuted( iID, bMuted );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_muted";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetSoundReceiverMuted( iID ) );

	VAPY_CATCH_RETURN;
};


static PyObject* get_sound_receiver_position( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_position";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	VAVec3 v3Pos = g_pVANetClient->GetCoreInstance( )->GetSoundReceiverPosition( iID );

	PyObject* pPosList = PyList_New( 3 );
	PyList_SetItem( pPosList, 0, PyFloat_FromDouble( v3Pos.x ) );
	PyList_SetItem( pPosList, 1, PyFloat_FromDouble( v3Pos.y ) );
	PyList_SetItem( pPosList, 2, PyFloat_FromDouble( v3Pos.z ) );

	return pPosList;

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_position( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "pos", NULL };
	const char* pcFormat        = "i(ddd):set_sound_receiver_position";
	long iID                    = -1;
	VAVec3 v3Pos;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &v3Pos.x, &v3Pos.y, &v3Pos.z ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverPosition( iID, v3Pos );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_orientation_vu( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_orientation_vu";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	VAVec3 v3View, v3Up;
	g_pVANetClient->GetCoreInstance( )->GetSoundReceiverOrientationVU( iID, v3View, v3Up );

	return Py_BuildValue( "(ddd)(ddd)", v3View.x, v3View.y, v3View.z, v3Up.x, v3Up.y, v3Up.z );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_orientation_vu( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "view", "up", NULL };
	const char* pcFormat        = "i(ddd)(ddd):set_sound_receiver_orientation_vu";
	long iID                    = -1;
	VAVec3 v3View, v3Up;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &v3View.x, &v3View.y, &v3View.z, &v3Up.x, &v3Up.y, &v3Up.z ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverOrientationVU( iID, v3View, v3Up );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_real_world_position( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_real_world_position";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	VAVec3 v3Pos, vView, vUp;
	g_pVANetClient->GetCoreInstance( )->GetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, vView, vUp );

	PyObject* pPosList = PyList_New( 3 );
	PyList_SetItem( pPosList, 0, PyFloat_FromDouble( v3Pos.x ) );
	PyList_SetItem( pPosList, 1, PyFloat_FromDouble( v3Pos.y ) );
	PyList_SetItem( pPosList, 2, PyFloat_FromDouble( v3Pos.z ) );

	return pPosList;

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_real_world_position( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "pos", NULL };
	const char* pcFormat        = "i(ddd):set_sound_receiver_real_world_position";
	long iID                    = -1;
	VAVec3 v3Pos;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &v3Pos.x, &v3Pos.y, &v3Pos.z ) )
		return NULL;

	VAVec3 v3PosDummy, v3View, v3Up;
	g_pVANetClient->GetCoreInstance( )->GetSoundReceiverRealWorldPositionOrientationVU( iID, v3PosDummy, v3View, v3Up );
	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View, v3Up );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_receiver_real_world_orientation_vu( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_receiver_real_world_orientation_vu";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	VAVec3 v3Pos, v3View, v3Up;
	g_pVANetClient->GetCoreInstance( )->GetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View, v3Up );

	return Py_BuildValue( "(ddd)(ddd)", v3View.x, v3View.y, v3View.z, v3Up.x, v3Up.y, v3Up.z );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_receiver_real_world_orientation_vu( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "view", "up", NULL };
	const char* pcFormat        = "i(ddd)(ddd):set_sound_receiver_real_world_orientation_vu";
	long iID                    = -1;
	VAVec3 v3View, v3Up;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &v3View.x, &v3View.y, &v3View.z, &v3Up.x, &v3Up.y, &v3Up.z ) )
		return NULL;

	VAVec3 v3Pos, v3View_dummy, v3Up_dummy;
	g_pVANetClient->GetCoreInstance( )->GetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View_dummy, v3Up_dummy );
	g_pVANetClient->GetCoreInstance( )->SetSoundReceiverRealWorldPositionOrientationVU( iID, v3Pos, v3View, v3Up );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* create_signal_source_buffer_from_file( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "path", "name", NULL };
	const char* pcFormat        = "s|s:create_signal_source_buffer_from_file";
	char* pcPath                = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcPath, &pcName ) )
		return NULL;

	std::string sName         = pcName ? std::string( pcName ) : "";
	std::string sSignalSource = g_pVANetClient->GetCoreInstance( )->CreateSignalSourceBufferFromFile( std::string( pcPath ), sName );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSignalSource ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* create_signal_source_prototype_from_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "parameters", "name", NULL };
	const char* pcFormat        = "O!|s:create_signal_source_prototype_from_parameters";
	PyObject* pParameters       = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pParameters, &pcName ) )
		return NULL;

	CVAStruct oParameters = ConvertPythonDictToVAStruct( pParameters );
	std::string sName     = pcName ? std::string( pcName ) : "";
	auto sID              = g_pVANetClient->GetCoreInstance( )->CreateSignalSourcePrototypeFromParameters( oParameters, sName );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sID ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* create_signal_source_text_to_speech( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "name", NULL };
	const char* pcFormat        = "|s:create_signal_source_text_to_speech";
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcName ) )
		return NULL;

	std::string sName         = pcName ? std::string( pcName ) : "";
	std::string sSignalSource = g_pVANetClient->GetCoreInstance( )->CreateSignalSourceTextToSpeech( sName );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSignalSource ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* create_signal_source_sequencer( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "name", NULL };
	const char* pcFormat        = "|s:create_signal_source_sequencer";
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcName ) )
		return NULL;

	std::string sName         = pcName ? std::string( pcName ) : "";
	std::string sSignalSource = g_pVANetClient->GetCoreInstance( )->CreateSignalSourceSequencer( sName );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSignalSource ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* create_signal_source_network_stream( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "interface", "port", "name", NULL };
	const char* pcFormat        = "si|s:create_signal_source_network_stream";
	char* pcInterface           = nullptr;
	int iPort                   = -1;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcInterface, &iPort, &pcName ) )
		return NULL;

	std::string sName         = pcName ? std::string( pcName ) : "";
	std::string sInterface    = pcInterface ? std::string( pcInterface ) : "";
	std::string sSignalSource = g_pVANetClient->GetCoreInstance( )->CreateSignalSourceNetworkStream( sInterface, iPort, sName );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSignalSource ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* create_signal_source_engine( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "name", NULL };
	const char* pcFormat        = "O!|s:create_signal_source_engine";
	PyObject* pParameters       = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pParameters, &pcName ) )
		return NULL;

	std::string sName         = pcName ? std::string( pcName ) : "";
	CVAStruct oParameters     = ConvertPythonDictToVAStruct( pParameters );
	std::string sSignalSource = g_pVANetClient->GetCoreInstance( )->CreateSignalSourceEngine( oParameters, sName );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSignalSource ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* create_signal_source_machine( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "name", NULL };
	const char* pcFormat        = "O!|s:create_signal_source_machine";
	PyObject* pParameters       = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pParameters, &pcName ) )
		return NULL;

	CVAStruct oParameters     = ConvertPythonDictToVAStruct( pParameters );
	std::string sName         = pcName ? std::string( pcName ) : "";
	std::string sSignalSource = g_pVANetClient->GetCoreInstance( )->CreateSignalSourceMachine( oParameters, sName );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSignalSource ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* delete_signal_source( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signal_source", NULL };
	const char* pcFormat        = "s:delete_signal_source";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	bool bRet                 = g_pVANetClient->GetCoreInstance( )->DeleteSignalSource( sSignalSource );
	return PyBool_FromLong( bRet );

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_info( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:get_signal_source_info";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	CVASignalSourceInfo oInfo = g_pVANetClient->GetCoreInstance( )->GetSignalSourceInfo( sSignalSource );

	PyObject* pInfo = Py_BuildValue( "{s:s,s:s,s:s,s:i,s:i,s:s}", "id", SaveStringToUnicodeConversion( oInfo.sID ).c_str( ), "name",
	                                 SaveStringToUnicodeConversion( oInfo.sName ).c_str( ), "state", SaveStringToUnicodeConversion( oInfo.sState ).c_str( ), "type",
	                                 oInfo.iType, "references", oInfo.iReferences, "description", SaveStringToUnicodeConversion( oInfo.sDesc ).c_str( ) );

	return pInfo;

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_infos( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;

	std::vector<CVASignalSourceInfo> voInfos;
	g_pVANetClient->GetCoreInstance( )->GetSignalSourceInfos( voInfos );

	PyObject* pInfoList = PyList_New( voInfos.size( ) );

	for( size_t i = 0; i < voInfos.size( ); i++ )
	{
		CVASignalSourceInfo& oInfo( voInfos[i] );
		PyObject* pInfo = Py_BuildValue( "{s:s,s:s,s:s,s:i,s:i,s:s}", "id", SaveStringToUnicodeConversion( oInfo.sID ).c_str( ), "name",
		                                 SaveStringToUnicodeConversion( oInfo.sName ).c_str( ), "state", SaveStringToUnicodeConversion( oInfo.sState ).c_str( ), "type",
		                                 oInfo.iType, "references", oInfo.iReferences, "description", SaveStringToUnicodeConversion( oInfo.sDesc ).c_str( ) );
		PyList_SetItem( pInfoList, i, pInfo ); // steals reference
	}

	return pInfoList;

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_buffer_playback_state( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:get_signal_source_buffer_playback_state";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	int iState                = g_pVANetClient->GetCoreInstance( )->GetSignalSourceBufferPlaybackState( sSignalSource );
	return PyLong_FromLong( iState );

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_buffer_playback_state_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:get_signal_source_buffer_playback_state_str";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	int iState                = g_pVANetClient->GetCoreInstance( )->GetSignalSourceBufferPlaybackState( sSignalSource );
	std::string sState        = g_pVANetClient->GetCoreInstance( )->GetPlaybackStateStr( iState );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sState ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_buffer_playback_action( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "action", NULL };
	const char* pcFormat        = "si:va_set_signal_source_buffer_playback_action";
	char* pcSignalSource        = nullptr;
	int iAction                 = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &iAction ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceBufferPlaybackAction( sSignalSource, iAction );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_buffer_playback_action_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "action_string", NULL };
	const char* pcFormat        = "ss:set_signal_source_buffer_playback_action_str";
	char* pcSignalSource        = nullptr;
	char* pcAction              = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &pcAction ) )
		return NULL;

	std::string sAction = pcAction ? std::string( pcAction ) : "";
	int iAction         = g_pVANetClient->GetCoreInstance( )->ParsePlaybackAction( sAction );

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceBufferPlaybackAction( sSignalSource, iAction );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_buffer_playback_position( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "playback_position", NULL };
	const char* pcFormat        = "si:set_signal_source_buffer_playback_position";
	char* pcSignalSource        = nullptr;
	int iPosition               = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &iPosition ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceBufferPlaybackPosition( sSignalSource, iPosition );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_buffer_looping( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:get_signal_source_buffer_looping";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetSignalSourceBufferLooping( sSignalSource ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_buffer_looping( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "enabled", NULL };
	const char* pcFormat        = "s|b:set_signal_source_buffer_looping";
	char* pcSignalSource        = nullptr;
	bool bEnabled               = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &bEnabled ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceBufferLooping( sSignalSource, bEnabled );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_machine_start_machine( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:set_signal_source_machine_start_machine";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceMachineStartMachine( sSignalSource );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_machine_halt_machine( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:set_signal_source_machine_halt_machine";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceMachineHaltMachine( sSignalSource );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_machine_state_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:get_signal_source_machine_state_str";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	std::string sState        = g_pVANetClient->GetCoreInstance( )->GetSignalSourceMachineStateStr( sSignalSource );

	return PyUnicode_FromString( SaveStringToUnicodeConversion( sState ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_machine_speed( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "speed", NULL };
	const char* pcFormat        = "sd:set_signal_source_machine_speed";
	char* pcSignalSource        = nullptr;
	double dSpeed;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &dSpeed ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceMachineSpeed( sSignalSource, dSpeed );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_machine_speed( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", NULL };
	const char* pcFormat        = "s:get_signal_source_machine_speed";
	char* pcSignalSource        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	double dSpeed             = g_pVANetClient->GetCoreInstance( )->GetSignalSourceMachineSpeed( sSignalSource );

	return PyFloat_FromDouble( dSpeed );

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_machine_start_file( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "filepath", NULL };
	const char* pcFormat        = "ss:set_signal_source_machine_start_file";
	char* pcSignalSource        = nullptr;
	char* pcPath                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &pcPath ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	std::string sPath         = pcPath ? std::string( pcPath ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceMachineStartFile( sSignalSource, sPath );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_machine_idle_file( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "filepath", NULL };
	const char* pcFormat        = "ss:set_signal_source_machine_idle_file";
	char* pcSignalSource        = nullptr;
	char* pcPath                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &pcPath ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	std::string sPath         = pcPath ? std::string( pcPath ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceMachineIdleFile( sSignalSource, sPath );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_machine_stop_file( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "filepath", NULL };
	const char* pcFormat        = "ss:vset_signal_source_machine_stop_file";
	char* pcSignalSource        = nullptr;
	char* pcPath                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &pcPath ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	std::string sPath         = pcPath ? std::string( pcPath ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceMachineStopFile( sSignalSource, sPath );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_signal_source_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "signalsource", "parameters", NULL };
	const char* pcFormat        = "iO!:get_signal_source_parameters";
	char* pcSignalSource        = nullptr;
	PyObject* pParameters       = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &pParameters ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	CVAStruct oParameters     = ConvertPythonDictToVAStruct( pParameters );
	CVAStruct oReturn         = g_pVANetClient->GetCoreInstance( )->GetSignalSourceParameters( sSignalSource, oParameters );
	return ConvertVAStructToPythonDict( oReturn );

	VAPY_CATCH_RETURN;
};

static PyObject* set_signal_source_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "io!:set_signal_source_parameters";
	char* pcSignalSource        = nullptr;
	PyObject* pParameters       = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcSignalSource, &pParameters ) )
		return NULL;

	std::string sSignalSource = pcSignalSource ? std::string( pcSignalSource ) : "";
	CVAStruct oParameters     = ConvertPythonDictToVAStruct( pParameters );
	g_pVANetClient->GetCoreInstance( )->SetSignalSourceParameters( sSignalSource, oParameters );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_portal_ids( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	std::vector<int> viIDs;
	g_pVANetClient->GetCoreInstance( )->GetSoundPortalIDs( viIDs );

	PyObject* pIDList = PyList_New( viIDs.size( ) );
	for( Py_ssize_t i = 0; i < PyList_GET_SIZE( pIDList ); i++ )
		PyList_SetItem( pIDList, i, PyLong_FromLong( viIDs[i] ) );

	return pIDList;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_portal_name( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_portal_name";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyUnicode_FromString( SaveStringToUnicodeConversion( g_pVANetClient->GetCoreInstance( )->GetSoundPortalName( iID ) ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_portal_name( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "name", NULL };
	const char* pcFormat        = "is:set_sound_portal_name";
	long iID                    = -1;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &pcName ) )
		return NULL;

	std::string sName = pcName ? std::string( pcName ) : "";
	g_pVANetClient->GetCoreInstance( )->SetSoundPortalName( iID, sName );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_sound_portal_enabled( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "i:get_sound_portal_enabled";
	long iID                    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID ) )
		return NULL;

	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetSoundPortalEnabled( iID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_sound_portal_enabled( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "name", NULL };
	const char* pcFormat        = "i|b:set_sound_portal_enabled";
	long iID                    = -1;
	bool bEnabled               = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iID, &bEnabled ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetSoundPortalEnabled( iID, bEnabled );
	return Py_None;

	VAPY_CATCH_RETURN;
};


static PyObject* get_homogeneous_medium_sound_speed( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetHomogeneousMediumSoundSpeed( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* set_homogeneous_medium_sound_speed( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "sound_speed", NULL };
	const char* pcFormat        = "d:set_homogeneous_medium_sound_speed";
	double dSoundSpeed          = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dSoundSpeed ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetHomogeneousMediumSoundSpeed( dSoundSpeed );
	return Py_None;

	VAPY_CATCH_RETURN;
};
static PyObject* get_homogeneous_medium_temperature( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetHomogeneousMediumTemperature( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* set_homogeneous_medium_temperature( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "temperature", NULL };
	const char* pcFormat        = "d:set_homogeneous_medium_temperature";
	double dTemperature         = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dTemperature ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetHomogeneousMediumTemperature( dTemperature );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_homogeneous_medium_static_pressure( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetHomogeneousMediumStaticPressure( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* set_homogeneous_medium_static_pressure( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "static_pressure", NULL };
	const char* pcFormat        = "d:set_homogeneous_medium_static_pressure";
	double dStaticPressure      = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dStaticPressure ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetHomogeneousMediumStaticPressure( dStaticPressure );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_homogeneous_medium_relative_humidity( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetHomogeneousMediumRelativeHumidity( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* set_homogeneous_medium_relative_humidity( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "relative_humidity", NULL };
	const char* pcFormat        = "d:set_homogeneous_medium_relative_humidity";
	double dRelativeHumidity    = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dRelativeHumidity ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetHomogeneousMediumRelativeHumidity( dRelativeHumidity );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_homogeneous_medium_shift_speed( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	VAVec3 v3ShiftSpeed = g_pVANetClient->GetCoreInstance( )->GetHomogeneousMediumShiftSpeed( );

	PyObject* pList = PyList_New( 3 );
	PyList_SetItem( pList, 0, PyFloat_FromDouble( v3ShiftSpeed.x ) );
	PyList_SetItem( pList, 1, PyFloat_FromDouble( v3ShiftSpeed.y ) );
	PyList_SetItem( pList, 2, PyFloat_FromDouble( v3ShiftSpeed.z ) );

	return pList;
	VAPY_CATCH_RETURN;
};

static PyObject* set_homogeneous_medium_shift_speed( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "shift_speed", NULL };
	const char* pcFormat        = "(ddd):set_homogeneous_medium_shift_speed";
	VAVec3 v3ShiftSpeed;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &v3ShiftSpeed.x, &v3ShiftSpeed.y, &v3ShiftSpeed.z ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetHomogeneousMediumShiftSpeed( v3ShiftSpeed );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_homogeneous_medium_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "arguments", NULL };
	const char* pcFormat        = "|O!:get_homogeneous_medium_parameters";
	PyObject* pParamArgs        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pParamArgs ) )
		return NULL;

	CVAStruct oArgs       = ConvertPythonDictToVAStruct( pParamArgs );
	CVAStruct oParameters = g_pVANetClient->GetCoreInstance( )->GetHomogeneousMediumParameters( oArgs );

	return ConvertVAStructToPythonDict( oParameters );
	VAPY_CATCH_RETURN;
};

static PyObject* set_homogeneous_medium_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "parameters", NULL };
	const char* pcFormat        = "O!:set_homogeneous_medium_parameters";
	PyObject* pParameters       = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pParameters ) )
		return NULL;

	CVAStruct oParameters = ConvertPythonDictToVAStruct( pParameters );

	g_pVANetClient->GetCoreInstance( )->SetHomogeneousMediumParameters( oParameters );
	return Py_None;

	VAPY_CATCH_RETURN;
};


static PyObject* get_acoustic_material_infos( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;

	std::vector<CVAAcousticMaterial> voInfos;
	g_pVANetClient->GetCoreInstance( )->GetAcousticMaterialInfos( voInfos );

	PyObject* pList = PyList_New( voInfos.size( ) );

	for( size_t i = 0; i < voInfos.size( ); i++ )
	{
		CVAAcousticMaterial& oInfo( voInfos[i] );
		PyObject* pInfo = Py_BuildValue(
		    "{s:i,s:i,s:O!,s:s,s:O!,s:O!,s:O!,s:O!}", "id", oInfo.iID, "class", oInfo.iType, "parameters", ConvertVAStructToPythonDict( oInfo.oParams ), "name",
		    SaveStringToUnicodeConversion( oInfo.sName ).c_str( ), "absorption_values", ConvertFloatVectorToPythonList( oInfo.vfAbsorptionValues ), "scattering_values",
		    ConvertFloatVectorToPythonList( oInfo.vfScatteringValues ), "transmission_values", ConvertFloatVectorToPythonList( oInfo.vfTransmissionValues ) );
		PyList_SetItem( pList, i, pInfo ); // steals reference
	}

	return pList;

	VAPY_CATCH_RETURN;
};

static PyObject* create_acoustic_material_from_file( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "parameters", "name", NULL };
	const char* pcFormat        = "s|s:create_acoustic_material_from_file";
	char* pcFilePath            = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcFilePath, &pcName ) )
		return NULL;

	std::string sFilePath = pcFilePath ? std::string( pcFilePath ) : "";
	std::string sName     = pcName ? std::string( pcName ) : "";
	const int iID         = g_pVANetClient->GetCoreInstance( )->CreateAcousticMaterialFromFile( sFilePath, sName );
	return PyLong_FromLong( iID );

	VAPY_CATCH_RETURN;
};

static PyObject* create_acoustic_material_from_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "parameters", "name", NULL };
	const char* pcFormat        = "O!|s:create_acoustic_material_from_parameters";
	PyObject* pParams           = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pParams, &pcName ) )
		return NULL;

	const CVAStruct oParams = ConvertPythonDictToVAStruct( pParams );
	std::string sName       = pcName ? std::string( pcName ) : "";
	const int iID           = g_pVANetClient->GetCoreInstance( )->CreateAcousticMaterialFromParameters( oParams, sName );
	return PyLong_FromLong( iID );

	VAPY_CATCH_RETURN;
};

static PyObject* create_acoustic_material( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "material", "name", NULL };
	const char* pcFormat        = "O!|s:create_acoustic_material";
	PyObject* pMaterial         = nullptr;
	char* pcName                = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pMaterial, &pcName ) )
		return NULL;

	const CVAAcousticMaterial oMaterial = ConvertPythonDictToAcousticMaterial( pMaterial );
	std::string sName                   = pcName ? std::string( pcName ) : "";
	const int iID                       = g_pVANetClient->GetCoreInstance( )->CreateAcousticMaterial( oMaterial, sName );
	return PyLong_FromLong( iID );

	VAPY_CATCH_RETURN;
};

static PyObject* get_rendering_modules( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;

	std::vector<CVAAudioRendererInfo> voInfos;
	g_pVANetClient->GetCoreInstance( )->GetRenderingModules( voInfos );

	PyObject* pList = PyList_New( voInfos.size( ) );

	for( size_t i = 0; i < voInfos.size( ); i++ )
	{
		CVAAudioRendererInfo& oInfo( voInfos[i] );
		PyObject* pInfo = Py_BuildValue(
		    "{s:b,s:s,s:s,s:s,s:s,s:b,s:b,s:O}", "enabled", oInfo.bEnabled, "class", SaveStringToUnicodeConversion( oInfo.sClass ).c_str( ), "description",
		    SaveStringToUnicodeConversion( oInfo.sDescription ).c_str( ), "id", SaveStringToUnicodeConversion( oInfo.sID ).c_str( ), "output_recording_file_path",
		    SaveStringToUnicodeConversion( oInfo.sOutputRecordingFilePath ).c_str( ), "output_detector_enabled", oInfo.bOutputDetectorEnabled, "output_recording_enabled",
		    oInfo.bOutputRecordingEnabled, "parameters", ConvertVAStructToPythonDict( oInfo.oParams ) );
		PyList_SetItem( pList, i, pInfo ); // steals reference
	}

	return pList;

	VAPY_CATCH_RETURN;
};

static PyObject* get_rendering_module_gain( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "s:get_rendering_module_gain";
	char* pcID                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetRenderingModuleGain( sID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_rendering_module_gain( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "gain", NULL };
	const char* pcFormat        = "sd:set_rendering_module_gain";
	char* pcID                  = nullptr;
	double dGain                = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &dGain ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	g_pVANetClient->GetCoreInstance( )->SetRenderingModuleGain( sID, dGain );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_rendering_module_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "short_mode", NULL };
	const char* pcFormat        = "s|b:get_rendering_module_auralization_mode";
	char* pcID                  = nullptr;
	bool bShortMode             = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &bShortMode ) )
		return NULL;

	std::string sID       = pcID ? std::string( pcID ) : "";
	const int iAM         = g_pVANetClient->GetCoreInstance( )->GetRenderingModuleAuralizationMode( sID );
	const std::string sAM = IVAInterface::GetAuralizationModeStr( iAM, bShortMode );

	return PyUnicode_FromString( SaveStringToUnicodeConversion( sAM ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_rendering_module_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "auralization_mode", NULL };
	const char* pcFormat        = "ss:set_rendering_module_auralization_mode";
	char* pcID                  = nullptr;
	char* pcAM                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &pcAM ) )
		return NULL;

	std::string sID       = pcID ? std::string( pcID ) : "";
	const std::string sAM = pcAM ? std::string( pcAM ) : "";

	const int iCurrentAM = g_pVANetClient->GetCoreInstance( )->GetRenderingModuleAuralizationMode( sID );
	const int iAM        = IVAInterface::ParseAuralizationModeStr( sAM, iCurrentAM );
	g_pVANetClient->GetCoreInstance( )->SetRenderingModuleAuralizationMode( sID, iAM );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_rendering_module_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "renderer_id", "arguments", NULL };
	const char* pcFormat        = "s|O!:get_rendering_module_parameters";
	char* pcID                  = nullptr;
	PyObject* pArguments        = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &PyDict_Type, &pArguments ) )
		return NULL;

	std::string sID       = pcID ? std::string( pcID ) : "";
	const CVAStruct oArgs = ConvertPythonDictToVAStruct( pArguments );
	const CVAStruct oRet  = g_pVANetClient->GetCoreInstance( )->GetRenderingModuleParameters( sID, oArgs );

	return ConvertVAStructToPythonDict( oRet );

	VAPY_CATCH_RETURN;
};

static PyObject* set_rendering_module_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "parameters", NULL };
	const char* pcFormat        = "sO!:set_rendering_module_parameters";
	char* pcID                  = nullptr;
	PyObject* pParams           = NULL;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &PyDict_Type, &pParams ) )
		return NULL;

	std::string sID             = pcID ? std::string( pcID ) : "";
	const CVAStruct oParameters = ConvertPythonDictToVAStruct( pParams );
	g_pVANetClient->GetCoreInstance( )->SetRenderingModuleParameters( sID, oParameters );
	return Py_None;

	VAPY_CATCH_RETURN;
};


static PyObject* set_rendering_module_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "muted", NULL };
	const char* pcFormat        = "s|b:set_rendering_module_muted";
	char* pcID                  = nullptr;
	bool bMuted                 = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &bMuted ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	g_pVANetClient->GetCoreInstance( )->SetRenderingModuleMuted( sID, bMuted );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_rendering_module_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "s:get_rendering_module_muted";
	char* pcID                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetRenderingModuleMuted( sID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_reproduction_modules( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;

	std::vector<CVAAudioReproductionInfo> voInfos;
	g_pVANetClient->GetCoreInstance( )->GetReproductionModules( voInfos );

	PyObject* pList = PyList_New( voInfos.size( ) );

	for( size_t i = 0; i < voInfos.size( ); i++ )
	{
		CVAAudioReproductionInfo& oInfo( voInfos[i] );
		PyObject* pInfo = Py_BuildValue( "{s:b,s:s,s:s,s:s}", "enabled", oInfo.bEnabled, "class", SaveStringToUnicodeConversion( oInfo.sClass ).c_str( ), "description",
		                                 SaveStringToUnicodeConversion( oInfo.sDescription ).c_str( ), "id", SaveStringToUnicodeConversion( oInfo.sID ).c_str( ) );
		PyList_SetItem( pList, i, pInfo ); // steals reference
	}

	return pList;

	VAPY_CATCH_RETURN;
};

static PyObject* get_reproduction_module_gain( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "s:get_reproduction_module_gain";
	char* pcID                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetReproductionModuleGain( sID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_reproduction_module_gain( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "gain", NULL };
	const char* pcFormat        = "sd:set_reproduction_module_gain";
	char* pcID                  = nullptr;
	double dGain                = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &dGain ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	g_pVANetClient->GetCoreInstance( )->SetReproductionModuleGain( sID, dGain );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_reproduction_module_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "arguments", NULL };
	const char* pcFormat        = "s|O!:get_reproduction_module_parameters";
	char* pcID                  = nullptr;
	PyObject* pArguments        = NULL;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &PyDict_Type, &pArguments ) )
		return NULL;

	std::string sID       = pcID ? std::string( pcID ) : "";
	const CVAStruct oArgs = ConvertPythonDictToVAStruct( pArguments );
	const CVAStruct oRet  = g_pVANetClient->GetCoreInstance( )->GetReproductionModuleParameters( sID, oArgs );

	return ConvertVAStructToPythonDict( oRet );

	VAPY_CATCH_RETURN;
};

static PyObject* set_reproduction_module_parameters( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "parameters", NULL };
	const char* pcFormat        = "sO!:set_reproduction_module_parameters";
	char* pcID                  = nullptr;
	PyObject* pParams           = NULL;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &PyDict_Type, &pParams ) )
		return NULL;

	std::string sID             = pcID ? std::string( pcID ) : "";
	const CVAStruct oParameters = ConvertPythonDictToVAStruct( pParams );
	g_pVANetClient->GetCoreInstance( )->SetReproductionModuleParameters( sID, oParameters );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_reproduction_module_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", "muted", NULL };
	const char* pcFormat        = "s|b:set_reproduction_module_muted";
	char* pcID                  = nullptr;
	bool bMuted                 = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID, &bMuted ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	g_pVANetClient->GetCoreInstance( )->SetReproductionModuleMuted( sID, bMuted );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_reproduction_module_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "id", NULL };
	const char* pcFormat        = "s:get_reproduction_module_muted";
	char* pcID                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcID ) )
		return NULL;

	std::string sID = pcID ? std::string( pcID ) : "";
	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetReproductionModuleMuted( sID ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_input_gain( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetInputGain( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* set_input_gain( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "gain", NULL };
	const char* pcFormat        = "d:set_input_gain";
	double dGain                = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dGain ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetInputGain( dGain );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_input_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "muted", NULL };
	const char* pcFormat        = "|b:set_input_muted";
	bool bMuted                 = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &bMuted ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetInputMuted( bMuted );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_input_muted( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetInputMuted( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* get_output_gain( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetInputGain( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* set_output_gain( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "gain", NULL };
	const char* pcFormat        = "d:set_output_gain";
	double dGain                = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dGain ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetOutputGain( dGain );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* set_output_muted( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "muted", NULL };
	const char* pcFormat        = "|b:set_output_muted";
	bool bMuted                 = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &bMuted ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetOutputMuted( bMuted );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_output_muted( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyBool_FromLong( g_pVANetClient->GetCoreInstance( )->GetOutputMuted( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* get_global_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "short_mode", NULL };
	const char* pcFormat        = "|b:get_global_auralization_mode";
	bool bShortMode             = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &bShortMode ) )
		return NULL;

	const int iAM         = g_pVANetClient->GetCoreInstance( )->GetGlobalAuralizationMode( );
	const std::string sAM = SaveStringToUnicodeConversion( IVAInterface::GetAuralizationModeStr( iAM, bShortMode ) );

	return PyUnicode_FromString( sAM.c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* set_global_auralization_mode( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "auralization_mode", NULL };
	const char* pcFormat        = "s:set_global_auralization_mode";
	char* pcAM                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcAM ) )
		return NULL;

	std::string sAM = pcAM ? std::string( pcAM ) : "";

	const int iCurrentAM = g_pVANetClient->GetCoreInstance( )->GetGlobalAuralizationMode( );
	const int iAM        = IVAInterface::ParseAuralizationModeStr( sAM, iCurrentAM );
	g_pVANetClient->GetCoreInstance( )->SetGlobalAuralizationMode( iAM );

	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* get_core_clock( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;
	return PyFloat_FromDouble( g_pVANetClient->GetCoreInstance( )->GetCoreClock( ) );
	VAPY_CATCH_RETURN;
};

static PyObject* set_core_clock( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "time", NULL };
	const char* pcFormat        = "d:set_core_clock";
	double dTime                = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dTime ) )
		return NULL;

	g_pVANetClient->GetCoreInstance( )->SetCoreClock( dTime );
	return Py_None;

	VAPY_CATCH_RETURN;
};

static PyObject* substitute_macro( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "rawstring", NULL };
	const char* pcFormat        = "s:substitute_macro";
	char* pcRawString           = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcRawString ) )
		return NULL;

	std::string sSubstitutedString = g_pVANetClient->GetCoreInstance( )->SubstituteMacros( std::string( pcRawString ) );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSubstitutedString ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* find_file_path( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "rawpath", NULL };
	const char* pcFormat        = "s:find_file_path";
	char* pcRawFilePath         = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcRawFilePath ) )
		return NULL;

	std::string sSubstitutedPath = g_pVANetClient->GetCoreInstance( )->FindFilePath( std::string( pcRawFilePath ) );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sSubstitutedPath ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_core_configuration( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "filter_enabled", NULL };
	const char* pcFormat        = "|b:get_core_configuration";
	bool bFilterEnabled         = true;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &bFilterEnabled ) )
		return NULL;

	CVAStruct oCoreConfig = g_pVANetClient->GetCoreInstance( )->GetCoreConfiguration( bFilterEnabled );
	return ConvertVAStructToPythonDict( oCoreConfig );

	VAPY_CATCH_RETURN;
};

static PyObject* get_hardware_configuration( PyObject*, PyObject* )
{
	VAPY_REQUIRE_CONN_TRY;

	CVAStruct oHWConfig = g_pVANetClient->GetCoreInstance( )->GetHardwareConfiguration( );
	return ConvertVAStructToPythonDict( oHWConfig );

	VAPY_CATCH_RETURN;
};

static PyObject* get_file_list( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "recursive", "filter_suffix_mask", NULL };
	const char* pcFormat        = "|bs:get_file_list";
	bool bFilterEnabled         = true;
	char* pcFilterSuffixMask    = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &bFilterEnabled, &pcFilterSuffixMask ) )
		return NULL;

	std::string sFilterSuffixMask = pcFilterSuffixMask ? std::string( pcFilterSuffixMask ) : "*";
	CVAStruct oFileList           = g_pVANetClient->GetCoreInstance( )->GetFileList( bFilterEnabled, sFilterSuffixMask );
	return ConvertVAStructToPythonDict( oFileList );

	VAPY_CATCH_RETURN;
};


static PyObject* get_log_level_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "loglevel", NULL };
	const char* pcFormat        = "i:get_log_level_str";
	int iLogLevel               = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iLogLevel ) )
		return NULL;

	std::string sLogLevel = g_pVANetClient->GetCoreInstance( )->GetLogLevelStr( iLogLevel );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sLogLevel ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* parse_auralization_mode_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "auralization_mode_string", NULL };
	const char* pcFormat        = "s:parse_auralization_mode_str";
	char* pcAM                  = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcAM ) )
		return NULL;

	int iAM = g_pVANetClient->GetCoreInstance( )->ParseAuralizationModeStr( std::string( pcAM ) );
	return PyLong_FromLong( iAM );

	VAPY_CATCH_RETURN;
};

static PyObject* get_auralization_mode_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "auralization_mode", "short_name", NULL };
	const char* pcFormat        = "i|b:get_auralization_mode_str";
	int iAM                     = -1;
	bool bShort                 = false;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iAM, &bShort ) )
		return NULL;

	std::string sAM = g_pVANetClient->GetCoreInstance( )->GetAuralizationModeStr( iAM, bShort );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sAM ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_volume_str_decibel( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "gain", NULL };
	const char* pcFormat        = "d:get_volume_str_decibel";
	double dGain                = -1.0f;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &dGain ) )
		return NULL;

	std::string sGainDB = g_pVANetClient->GetCoreInstance( )->GetVolumeStrDecibel( dGain );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sGainDB ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* parse_playback_state_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "state_string", NULL };
	const char* pcFormat        = "s:parse_playback_state_str";
	char* pcState               = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcState ) )
		return NULL;

	int iState = g_pVANetClient->GetCoreInstance( )->ParsePlaybackState( std::string( pcState ) );
	return PyLong_FromLong( iState );

	VAPY_CATCH_RETURN;
};

static PyObject* parse_playback_action_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "action_string", NULL };
	const char* pcFormat        = "s:parse_playback_action_str";
	char* pcAction              = nullptr;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &pcAction ) )
		return NULL;

	int iState = g_pVANetClient->GetCoreInstance( )->ParsePlaybackState( std::string( pcAction ) );
	return PyLong_FromLong( iState );

	VAPY_CATCH_RETURN;
};

static PyObject* get_playback_state_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "state", NULL };
	const char* pcFormat        = "i:get_playback_state_str";
	int iState                  = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iState ) )
		return NULL;

	std::string sState = g_pVANetClient->GetCoreInstance( )->GetPlaybackStateStr( iState );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sState ).c_str( ) );

	VAPY_CATCH_RETURN;
};

static PyObject* get_playback_action_str( PyObject*, PyObject* pArgs, PyObject* pKeywordTuple )
{
	VAPY_REQUIRE_CONN_TRY;

	static char* pKeyWordList[] = { "state", NULL };
	const char* pcFormat        = "i:get_playback_action_str";
	int iAction                 = -1;
	if( !PyArg_ParseTupleAndKeywords( pArgs, pKeywordTuple, pcFormat, pKeyWordList, &iAction ) )
		return NULL;

	std::string sAction = g_pVANetClient->GetCoreInstance( )->GetPlaybackActionStr( iAction );
	return PyUnicode_FromString( SaveStringToUnicodeConversion( sAction ).c_str( ) );

	VAPY_CATCH_RETURN;
};
