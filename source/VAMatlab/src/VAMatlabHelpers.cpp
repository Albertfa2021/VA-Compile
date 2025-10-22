/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2022
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

#include "VAMatlabHelpers.h"

#include <VAException.h>
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <sstream>
#include <stdint.h>

bool matlabIsScalar( const mxArray* A )
{
	mwSize s = mxGetNumberOfDimensions( A );

	// Es müssen zwei Dimensionen vorliegen
	if( s != 2 )
		return false;
	const mwSize* d = mxGetDimensions( A );

	// Beide Dimensionen müssen 1 sein
	return ( ( d[0] == 1 ) && ( d[1] == 1 ) );
}

bool matlabIsVector( const mxArray* A, int& size )
{
	mwSize s = mxGetNumberOfDimensions( A );

	// Es müssen zwei Dimensionen vorliegen
	if( s != 2 )
		return false;
	const mwSize* d = mxGetDimensions( A );

	// Mindestens eine der beiden Dimensionen muss 1 sein
	if( d[0] == 1 && d[1] > 1 )
	{
		size = (int)d[1];
		return true;
	}

	if( d[1] == 1 && d[0] > 1 )
	{
		size = (int)d[0];
		return true;
	}

	return false;
}

bool matlabIsVector( const mxArray* A )
{
	int iDummy;
	return matlabIsVector( A, iDummy );
}

bool matlabIsRowVector( const mxArray* A )
{
	mwSize s = mxGetNumberOfDimensions( A );
	if( s != 2 )
		return false;
	const mwSize* d = mxGetDimensions( A );
	return ( d[0] == 1 );
}

bool matlabIsColumnVector( const mxArray* A )
{
	mwSize s = mxGetNumberOfDimensions( A );
	if( s != 2 )
		return false;
	const mwSize* d = mxGetDimensions( A );
	return ( d[1] == 1 );
}

bool matlabGetBoolScalar( const mxArray* arg, const char* argname )
{
	const int buflen = 1024;
	char buf[buflen];

	if( mxIsLogicalScalar( arg ) )
	{
		mxLogical* p = mxGetLogicals( arg );
		return ( *p == true );
	}

	if( mxIsNumeric( arg ) && !mxIsComplex( arg ) && matlabIsScalar( arg ) )
	{
		void* p = mxGetData( arg );

		if( mxIsInt16( arg ) )
			return ( *( (int16_t*)p ) != 0 );
		if( mxIsInt32( arg ) )
			return ( *( (int32_t*)p ) != 0 );
		if( mxIsInt64( arg ) )
			return ( *( (int64_t*)p ) != 0 );
		if( mxIsSingle( arg ) )
			return ( *( (float*)p ) != 0 );
		if( mxIsDouble( arg ) )
			return ( *( (double*)p ) != 0 );

		// Unsupported datatype
		sprintf_s( buf, buflen, "Argument '%s' could not be interpreted as a logical scalar", argname );
		VA_EXCEPT1( buf );
	}

	if( mxIsChar( arg ) && matlabIsRowVector( arg ) )
	{
		mxGetString( arg, buf, buflen );
		std::string s( buf );
		std::transform( s.begin( ), s.end( ), s.begin( ), tolower );

		if( ( s == "true" ) || ( s == "yes" ) )
			return true;
		if( ( s == "false" ) || ( s == "no" ) )
			return false;

		// Unsupported datatype
		sprintf_s( buf, buflen, "Argument '%s' could not be interpreted as a logical scalar", argname );
		VA_EXCEPT1( buf );
		return false;
	}

	sprintf_s( buf, buflen, "Argument '%s' must be a logical scalar", argname );
	VA_EXCEPT1( buf );
}

int matlabGetIntegerScalar( const mxArray* arg, const char* argname )
{
	char buf[1024];

	if( mxIsNumeric( arg ) && !mxIsComplex( arg ) && matlabIsScalar( arg ) )
	{
		void* p = mxGetData( arg );

		if( mxIsInt16( arg ) )
			return (int)( *( (int16_t*)p ) );
		if( mxIsInt32( arg ) )
			return (int)( *( (int32_t*)p ) );
		if( mxIsInt64( arg ) )
			return (int)( *( (int64_t*)p ) ); // [fwe] Possible truncation!

		if( mxIsSingle( arg ) )
		{
			float fValue = *( (float*)p );
			// Check for integer
			if( fValue == floor( fValue ) )
				return (int)fValue;
		}

		if( mxIsDouble( arg ) )
		{
			double dValue = *( (double*)p );
			// Check for integer
			if( dValue == floor( dValue ) )
				return (int)dValue; // [fwe] Possible truncation!
		}

		// Unsupported datatype
		sprintf_s( buf, 1024, "Argument '%s' could not be interpreted as an integer scalar", argname );
		VA_EXCEPT1( buf );
	}

	sprintf_s( buf, 1024, "Argument '%s' must be an integer scalar", argname );
	VA_EXCEPT1( buf );
}

double matlabGetRealScalar( const mxArray* arg, const char* argname )
{
	char buf[1024];

	bool b1 = mxIsNumeric( arg );
	bool b2 = !mxIsComplex( arg );
	bool b3 = matlabIsScalar( arg );
	if( b1 && b2 && b3 )
	{
		void* p = mxGetData( arg );

		if( mxIsInt16( arg ) )
			return (double)( *( (int16_t*)p ) );
		if( mxIsInt32( arg ) )
			return (double)( *( (int32_t*)p ) );
		if( mxIsInt64( arg ) )
			return (double)( *( (int64_t*)p ) ); // [fwe] Possible truncation!

		if( mxIsSingle( arg ) )
			return *( (float*)p );
		if( mxIsDouble( arg ) )
			return *( (double*)p );

		// Unsupported datatype
		sprintf_s( buf, 1024, "Argument '%s' could not be interpreted as a real-valued scalar", argname );
		VA_EXCEPT1( buf );
	}

	sprintf_s( buf, 1024, "Argument '%s' must be a real-valued scalar", argname );
	VA_EXCEPT1( buf );
}

void matlabGetRealVector3( const mxArray* arg, const char* argname, VAVec3& v3Pos )
{
	char buf[1024];
	int size;

	if( mxIsNumeric( arg ) && !mxIsComplex( arg ) && matlabIsVector( arg, size ) )
	{
		if( size == 3 )
		{
			if( mxIsInt16( arg ) )
			{
				int16_t* p = (int16_t*)mxGetData( arg );
				v3Pos.x    = (double)p[0];
				v3Pos.y    = (double)p[1];
				v3Pos.z    = (double)p[2];
				return;
			}

			if( mxIsInt32( arg ) )
			{
				int32_t* p = (int32_t*)mxGetData( arg );
				v3Pos.x    = (double)p[0];
				v3Pos.y    = (double)p[1];
				v3Pos.z    = (double)p[2];
				return;
			}

			if( mxIsInt64( arg ) )
			{
				int64_t* p = (int64_t*)mxGetData( arg );
				v3Pos.x    = (double)p[0];
				v3Pos.y    = (double)p[1];
				v3Pos.z    = (double)p[2];
				return;
			}

			if( mxIsSingle( arg ) )
			{
				float* p = (float*)mxGetData( arg );
				v3Pos.x  = (double)p[0];
				v3Pos.y  = (double)p[1];
				v3Pos.z  = (double)p[2];
				return;
			}

			if( mxIsDouble( arg ) )
			{
				double* p = (double*)mxGetData( arg );
				v3Pos.x   = p[0];
				v3Pos.y   = p[1];
				v3Pos.z   = p[2];
				return;
			}
		}
	}

	sprintf_s( buf, 1024, "Argument '%s' must be a real-valued vector with exactly three elements", argname );
	VA_EXCEPT1( buf );
}

void matlabGetQuaternion( const mxArray* arg, const char* argname, VAQuat& qOrient )
{
	char buf[1024];
	int size;

	if( mxIsNumeric( arg ) && !mxIsComplex( arg ) && matlabIsVector( arg, size ) )
	{
		if( size == 4 )
		{
			if( mxIsInt16( arg ) )
			{
				int16_t* p = (int16_t*)mxGetData( arg );
				qOrient.w  = (double)p[0];
				qOrient.x  = (double)p[1];
				qOrient.y  = (double)p[2];
				qOrient.z  = (double)p[3];
				return;
			}

			if( mxIsInt32( arg ) )
			{
				int32_t* p = (int32_t*)mxGetData( arg );
				qOrient.w  = (double)p[0];
				qOrient.x  = (double)p[1];
				qOrient.y  = (double)p[2];
				qOrient.z  = (double)p[3];
				return;
			}

			if( mxIsInt64( arg ) )
			{
				int64_t* p = (int64_t*)mxGetData( arg );
				qOrient.w  = (double)p[0];
				qOrient.x  = (double)p[1];
				qOrient.y  = (double)p[2];
				qOrient.z  = (double)p[3];
				return;
			}

			if( mxIsSingle( arg ) )
			{
				float* p  = (float*)mxGetData( arg );
				qOrient.w = (double)p[0];
				qOrient.x = (double)p[1];
				qOrient.y = (double)p[2];
				qOrient.z = (double)p[3];
				return;
			}

			if( mxIsDouble( arg ) )
			{
				double* p = (double*)mxGetData( arg );
				qOrient.w = (double)p[0];
				qOrient.x = (double)p[1];
				qOrient.y = (double)p[2];
				qOrient.z = (double)p[3];
				return;
			}
		}
	}

	sprintf_s( buf, 1024, "Argument '%s' must be a real-valued vector with exactly four elements (same order as Matlab quaternion: w (real), i, j, k)", argname );
	VA_EXCEPT1( buf );
}

std::string matlabGetString( const mxArray* arg, const char* argname )
{
	const int buflen = 1024;
	char buf[buflen];

	// TODO: [fwe] Diese Implementierung ist begrenzt in der Länge. Sollte man dynamisch machen...

	// Test auf Zeichenfolge
	if( mxIsChar( arg ) )
	{
		// Spezialfall: Leere Eingabe
		if( mxIsEmpty( arg ) )
			return std::string( );

		if( matlabIsRowVector( arg ) )
		{
			mxGetString( arg, buf, buflen );
			return buf;
		}
	}

	sprintf_s( buf, 1024, "Argument '%s' must be a string", argname );
	VA_EXCEPT1( buf );
}

mxArray* matlabCreateRealVector3( const VAVec3& v3Vec )
{
	mxArray* p = mxCreateDoubleMatrix( 3, 1, mxREAL );
	double* d  = mxGetPr( p );
	d[0]       = v3Vec.x;
	d[1]       = v3Vec.y;
	d[2]       = v3Vec.z;
	return p;
}

mxArray* matlabCreateQuaternion( const VAQuat& qOrient )
{
	mxArray* q = mxCreateDoubleMatrix( 4, 1, mxREAL );
	double* d  = mxGetPr( q );
	d[0]       = qOrient.w;
	d[1]       = qOrient.x;
	d[2]       = qOrient.y;
	d[3]       = qOrient.z;
	return q;
}

mxArray* matlabCreateID( int iID )
{
	mwSize d[2]               = { 1, 1 };
	mxArray* p                = mxCreateNumericArray( 2, d, mxINT32_CLASS, mxREAL );
	*( (int*)mxGetData( p ) ) = iID;
	return p;
}

mxArray* matlabCreateIDList( const std::vector<int>& viID )
{
	if( viID.empty( ) )
	{
		// Special case: Handle empty matrix as 0x0 => Matlab convenience :-}
		return mxCreateNumericMatrix( 0, 0, mxINT32_CLASS, mxREAL );
	}
	else
	{
		mxArray* a = mxCreateNumericMatrix( 1, int( viID.size( ) ), mxINT32_CLASS, mxREAL );
		int* p     = (int*)mxGetData( a );
		for( size_t i = 0; i < viID.size( ); i++ )
			p[i] = viID[i];
		return a;
	}
}

mxArray* matlabCreateDirectivityInfo( const CVADirectivityInfo& di )
{
	const mwSize nFields         = 5;
	const char* ppszFieldNames[] = { "id", "name", "class", "description", "references" };

	mxArray* pStruct = mxCreateStructMatrix( 1, 1, nFields, ppszFieldNames );
	mxSetField( pStruct, 0, ppszFieldNames[0], matlabCreateID( di.iID ) );
	mxSetField( pStruct, 0, ppszFieldNames[1], mxCreateString( di.sName.c_str( ) ) );
	mxSetField( pStruct, 0, ppszFieldNames[2], matlabCreateID( di.iClass ) );
	mxSetField( pStruct, 0, ppszFieldNames[3], mxCreateString( di.sDesc.c_str( ) ) );
	mxSetField( pStruct, 0, ppszFieldNames[4], mxCreateDoubleScalar( di.iReferences ) ); // no of references as double-value in matlab

	return pStruct;
}

mxArray* matlabCreateSignalSourceInfo( const CVASignalSourceInfo& ssi )
{
	const mwSize nFields         = 6;
	const char* ppszFieldNames[] = { "id", "type", "name", "description", "state", "references" };

	mxArray* pStruct = mxCreateStructMatrix( 1, 1, nFields, ppszFieldNames );
	mxSetField( pStruct, 0, ppszFieldNames[0], mxCreateString( ssi.sID.c_str( ) ) );
	mxSetField( pStruct, 0, ppszFieldNames[1], mxCreateDoubleScalar( ssi.iType ) );
	mxSetField( pStruct, 0, ppszFieldNames[2], mxCreateString( ssi.sName.c_str( ) ) );
	mxSetField( pStruct, 0, ppszFieldNames[3], mxCreateString( ssi.sDesc.c_str( ) ) );
	mxSetField( pStruct, 0, ppszFieldNames[4], mxCreateString( ssi.sState.c_str( ) ) );
	mxSetField( pStruct, 0, ppszFieldNames[5], mxCreateDoubleScalar( ssi.iReferences ) ); // no of references as double-value in matlab

	return pStruct;
}

mxArray* matlabCreateSceneInfo( const CVASceneInfo& sci )
{
	const mwSize nFields         = 2;
	const char* ppszFieldNames[] = { "id", "name" };

	mxArray* pStruct = mxCreateStructMatrix( 1, 1, nFields, ppszFieldNames );
	mxSetField( pStruct, 0, ppszFieldNames[0], mxCreateString( sci.sID.c_str( ) ) );
	mxSetField( pStruct, 0, ppszFieldNames[0], mxCreateString( sci.sName.c_str( ) ) );

	return pStruct;
}

mxArray* matlabCreateStruct( const CVAStruct& oStruct )
{
	std::vector<const char*> vcpFieldNames;
	CVAStruct::const_iterator it = oStruct.Begin( );
	while( it != oStruct.End( ) )
	{
		const std::string sKey( ( *it ).first );
		char* cFieldName = _strdup( sKey.c_str( ) );
		vcpFieldNames.push_back( cFieldName );
		it++;
	}

	const size_t nDims          = 1;
	const int nFields           = int( vcpFieldNames.size( ) );
	const char** ppszFieldNames = vcpFieldNames.empty( ) ? nullptr : &vcpFieldNames.at( 0 );
	mxArray* pStruct            = mxCreateStructArray( 1, &nDims, nFields, ppszFieldNames );

	for( size_t i = 0; i < nFields; i++ )
	{
		const std::string sFieldName( vcpFieldNames[i] );
		assert( oStruct.HasKey( sFieldName ) );

		const CVAStructValue* pStructValue( oStruct.GetValue( sFieldName ) );

		mxArray* p;
		int iType = pStructValue->GetDatatype( );
		switch( iType )
		{
			case CVAStructValue::BOOL:
			{
				mxSetField( pStruct, 0, sFieldName.c_str( ), mxCreateLogicalScalar( *pStructValue ) );
				break;
			}
			case CVAStructValue::INT:
			{
				size_t iVal               = 1;
				p                         = mxCreateNumericArray( 1, &iVal, mxINT32_CLASS, mxREAL );
				*( (int*)mxGetData( p ) ) = *pStructValue;
				mxSetField( pStruct, 0, sFieldName.c_str( ), p );
				break;
			}
			case CVAStructValue::DOUBLE:
			{
				mxSetField( pStruct, 0, sFieldName.c_str( ), mxCreateDoubleScalar( *pStructValue ) );
				break;
			}
			case CVAStructValue::STRING:
			{
				std::string s = *pStructValue;
				mxSetField( pStruct, 0, sFieldName.c_str( ), mxCreateString( s.c_str( ) ) );
				break;
			}
			case CVAStructValue::STRUCT:
			{
				mxSetField( pStruct, 0, sFieldName.c_str( ), matlabCreateStruct( *pStructValue ) );
				break;
			}
			case CVAStructValue::DATA:
			{
				int nSamples    = int( pStructValue->GetDataSize( ) / 4 );
				size_t nDims[2] = { 1, nSamples };
				p               = mxCreateNumericArray( 2, nDims, mxSINGLE_CLASS, mxREAL );
				mxSetField( pStruct, 0, sFieldName.c_str( ), p );
				const float* pfSrcData = (const float*)( pStructValue->GetData( ) );
				float* pfDestData      = (float*)mxGetPr( p );
				for( size_t n = 0; n < nSamples; n++ )
					pfDestData[n] = pfSrcData[n];

				break;
			}
			default:
			{
				VA_EXCEPT2( INVALID_PARAMETER, "Could not read VAStruct value at key '" + sFieldName + "'" );
			}
		}

		delete vcpFieldNames[i];
	}

	return pStruct;
}

CVAStruct matlabGetStruct( const mxArray* pa, const char* argname )
{
	if( !mxIsStruct( pa ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Argument is not a Matlab struct: " + std::string( argname ) );

	CVAStruct oRet;

	size_t nStructElements = mxGetNumberOfElements( pa );
	if( nStructElements != 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Only a single struct can be passed as argument" );

	int nNumFields = mxGetNumberOfFields( pa );
	for( size_t i = 0; i < nNumFields; i++ )
	{
		std::string sFieldName( mxGetFieldNameByNumber( pa, int( i ) ) );
		mxArray* pVal = mxGetFieldByNumber( pa, 0, int( i ) );

		if( pVal == nullptr )
			VA_EXCEPT2( INVALID_PARAMETER, "Could not resolve value of struct field '" + sFieldName + "'" );

		if( mxIsStruct( pVal ) )
		{
			oRet[sFieldName] = matlabGetStruct( pVal, sFieldName.c_str( ) );
			continue;
		}

		std::string sClassName( mxGetClassName( pVal ) );

		if( sClassName == "itaAudio" )
		{
			mxArray* pITAAudio( pVal );

			if( mxGetNumberOfElements( pITAAudio ) != 1 )
				VA_EXCEPT2( INVALID_PARAMETER, "only single itaAudio instances allowed" );

			mxArray* pITAAudioDomain = mxGetProperty( pITAAudio, 0, "domain" );
			if( pITAAudioDomain == nullptr )
				continue;

			std::string sDomain = matlabGetString( pITAAudioDomain, "domain" );
			if( sDomain != "time" )
				VA_EXCEPT2( INVALID_PARAMETER, "itaAudio objects has to be in time domain. In Matlab, do myitaaudio.' to convert from frequency domain to time domain." );

			mxArray* pITAAudioTimeData = mxGetProperty( pITAAudio, 0, "timeData" );
			if( pITAAudioTimeData == nullptr )
				VA_EXCEPT2( INVALID_PARAMETER, "itaAudio objects has no timeData." );

			size_t nChannels = size_t( mxGetN( pITAAudioTimeData ) );
			size_t nSamples  = size_t( mxGetM( pITAAudioTimeData ) );
			size_t iOffset   = 0;
			for( size_t n = 0; n < nChannels; n++ )
			{
				std::stringstream ssChannelName;
				ssChannelName << "IRDataCh" << n + 1; // Naming convention for IR data to a Renderer
				std::string sChannelName( ssChannelName.str( ) );

				mxArray* pChannelData = pITAAudioTimeData; // use offsset to access data

				iOffset += n * nSamples;

				std::vector<float> vfData( nSamples );
				if( mxIsSingle( pChannelData ) )
					for( size_t i = 0; i < nSamples; i++ )
						vfData[i] = ( (float*)mxGetData( pChannelData ) )[i + iOffset];
				if( mxIsDouble( pChannelData ) )
					for( size_t i = 0; i < nSamples; i++ )
						vfData[i] = float( ( (double*)mxGetData( pChannelData ) )[i + iOffset] );
				else
					VA_EXCEPT1( "Unsupported array data format, could not create VAStruct from this struct" );

				assert( !oRet.HasKey( sChannelName ) );
				oRet[sChannelName] = CVAStructValue( &vfData[0], int( sizeof( float ) * nSamples ) );
			}

			continue; // next field
		}

		if( mxIsNumeric( pVal ) ) // int or double
		{
			if( mxIsComplex( pVal ) )
				VA_EXCEPT2( INVALID_PARAMETER, "Complex values are not supported in VAStruct" );

			int iSize;
			if( matlabIsVector( pVal, iSize ) )
			{
				if( iSize <= 0 )
					VA_EXCEPT1( "Invalid size of matlab vector" );

				std::vector<float> vfData( iSize );
				if( mxIsSingle( pVal ) )
					for( int i = 0; i < iSize; i++ )
						vfData[i] = ( (float*)mxGetData( pVal ) )[i];
				if( mxIsDouble( pVal ) )
					for( int i = 0; i < iSize; i++ )
						vfData[i] = float( ( (double*)mxGetData( pVal ) )[i] );
				else
					VA_EXCEPT1( "Unsupported array data format, could not create VAStruct from this struct" );

				oRet[sFieldName] = CVAStructValue( &vfData[0], sizeof( float ) * iSize );
			}
			else if( mxIsInt16( pVal ) || mxIsInt32( pVal ) || mxIsInt64( pVal ) )
			{
				oRet[sFieldName] = int( matlabGetIntegerScalar( pVal, sFieldName.c_str( ) ) );
			}
			else
			{
				oRet[sFieldName] = double( matlabGetRealScalar( pVal, sFieldName.c_str( ) ) );
			}
		}
		else if( mxIsLogicalScalar( pVal ) )
		{
			mxLogical* p     = mxGetLogicals( pVal );
			oRet[sFieldName] = bool( *p );
		}
		else if( mxIsChar( pVal ) )
		{
			oRet[sFieldName] = std::string( matlabGetString( pVal, sFieldName.c_str( ) ) );
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Unkown type in field '" + sFieldName + "'" );
		}
	}

	return oRet;
}
