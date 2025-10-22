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

#include "VAReproductionHeadphones.h"

#ifdef VACORE_WITH_REPRODUCTION_HEADPHONES

#	include "../../Utils/VAUtils.h"
#	include "../../VACoreConfig.h"
#	include "../../VALog.h"

#	include <ITAFileSystemUtils.h>
#	include <ITANumericUtils.h>
#	include <ITASampleFrame.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>

// Streaming class
class HPEQStreamFilter : public ITADatasourceRealization
{
public:
	ITADatasource* pdsInput;

	float fGain;

	ITAUPConvolution* pConvolverLeft;
	ITAUPConvolution* pConvolverRight;

	HPEQStreamFilter( double dSampleRate, int iBlockLength, const ITASampleFrame& sfHpIRinv );
	~HPEQStreamFilter( );

	void ProcessStream( const ITAStreamInfo* pStreamInfo );
	void PostIncrementBlockPointer( );

	// Exchanges the filter IR. Filter has to be of same length as initial filter
	/**
	 * \param sfHpIRinv Impulse response of the inverse HP filter
	 * \return True on success, false if filter length mismatch occured
	 */
	bool ExchangeFilter( const ITASampleFrame& sfHpIRinv );
};

CVAHeadphonesReproduction::CVAHeadphonesReproduction( const CVAAudioReproductionInitParams& oParams )
    : CVAObject( oParams.sClass + ":" + oParams.sID )
    , m_oParams( oParams )
    , m_pHPEQStreamFilter( NULL )
{
	m_oParams.pCore->RegisterModule( this );

	CVAConfigInterpreter conf( *( m_oParams.pConfig ) );

	// Gains
	if( oParams.pConfig->HasKey( "HpIRInvCalibrationGain" ) && oParams.pConfig->HasKey( "HpIRInvCalibrationGainDecibel" ) )
		VA_EXCEPT2( INVALID_PARAMETER, "HPIRInvCalibrationGain was included in the configuration both as factor and decibel and I can't decide which one is correct." );

	double dEqualizationCalibrationGain;
	if( oParams.pConfig->HasKey( "HpIRInvCalibrationGainDecibel" ) )
	{
		double dEqualizationCalibrationGainDecibel;
		conf.ReqNumber( "HpIRInvCalibrationGainDecibel", dEqualizationCalibrationGainDecibel );
		dEqualizationCalibrationGain = db20_to_ratio( dEqualizationCalibrationGainDecibel );
	}
	else if( oParams.pConfig->HasKey( "HpIRInvCalibrationGain" ) )
	{
		conf.ReqNumber( "HpIRInvCalibrationGain", dEqualizationCalibrationGain );
	}
	else
	{
		dEqualizationCalibrationGain = 1.0f;
	}

	// Inverse IR file
	std::string sFilePathRaw;
	conf.ReqString( "HpIRInvFile", sFilePathRaw );

	std::string sFilePath = m_oParams.pCore->GetCoreConfig( )->mMacros.SubstituteMacros( sFilePathRaw );
	;

	std::string sFinalFilePath = oParams.pCore->FindFilePath( sFilePath );
	if( !doesPathExist( sFinalFilePath ) )
		VA_EXCEPT2( INVALID_PARAMETER,
		            "Headphone EQ impulse response file '" + sFilePath + "' could not be found. Please make sure the directory is added as a search path." );

	m_sfHpIRInv.Load( sFinalFilePath ); // sets length to given number of samples

	if( m_sfHpIRInv.channels( ) != 2 )
		VA_EXCEPT2( INVALID_PARAMETER, "Headphone EQ impulse response '" + sFilePath + "' requires two channels" );

	int iHpIRInvFilterLength;
	conf.OptInteger( "HpIRInvFilterLength", iHpIRInvFilterLength, m_sfHpIRInv.length( ) );

	if( iHpIRInvFilterLength < m_sfHpIRInv.length( ) )
		VA_WARN( "HPEQ reproduction module", "Filter length of given inverse HPIR file is longer than configured length. Cropping." );

	// Calibrate ( HPTF * HPTFinv == 0dB )
	m_sfHpIRInv.mul_scalar( float( dEqualizationCalibrationGain ) );

	double dSampleRate  = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength    = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;
	m_pHPEQStreamFilter = new HPEQStreamFilter( dSampleRate, iBlockLength, m_sfHpIRInv );
}

CVAHeadphonesReproduction::~CVAHeadphonesReproduction( )
{
	delete m_pHPEQStreamFilter;
}

CVAObjectInfo CVAHeadphonesReproduction::GetObjectInfo( ) const
{
	CVAObjectInfo oInfo;
	oInfo.iID   = CVAObject::GetObjectID( );
	oInfo.sName = CVAObject::GetObjectName( );
	oInfo.sDesc = "Headphones Audio Reproduction";

	return oInfo;
}

CVAStruct CVAHeadphonesReproduction::CallObject( const CVAStruct& oArgs )
{
	SetParameters( oArgs );
	return GetParameters( oArgs );
}

void CVAHeadphonesReproduction::SetParameters( const CVAStruct& oArgs )
{
	const CVAStructValue* pStruct;

	if( ( pStruct = oArgs.GetValue( "HpIRInvFile" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "File path value must be a string" );

		std::string sSetKey                     = toUppercase( *pStruct );
		const CVAStructValue* pHpIRinvPathValue = pStruct;
		if( pHpIRinvPathValue != nullptr )
		{
			if( pHpIRinvPathValue->IsString( ) == false )
				VA_EXCEPT2( INVALID_PARAMETER, "Path to inverse impulse response file must be a string" );

			std::string sFilePathRaw = std::string( *pHpIRinvPathValue );
			std::string sFilePath    = m_oParams.pCore->FindFilePath( sFilePathRaw );

			if( !doesPathExist( sFilePath ) )
				VA_ERROR( GetObjectName( ), "Path '" << sFilePath << "' does not exist" );

			ITASampleFrame sfNewHpIRinv( sFilePath );

			if( sfNewHpIRinv.channels( ) != 2 )
				VA_EXCEPT2( INVALID_PARAMETER, "Headphone EQ impulse response '" + sFilePath + "' requires two channels" );

			if( sfNewHpIRinv.length( ) > m_sfHpIRInv.length( ) )
			{
				VA_WARN( GetObjectName( ), "New IR filter is longer than convolution instance, cropping remaining samples" );
				m_sfHpIRInv.write( sfNewHpIRinv, m_sfHpIRInv.length( ) );
			}
			else
			{
				m_sfHpIRInv.zero( );
				m_sfHpIRInv.write( sfNewHpIRinv, sfNewHpIRinv.length( ) );
			}

			if( !m_pHPEQStreamFilter->ExchangeFilter( m_sfHpIRInv ) )
				VA_ERROR( GetObjectName( ), "Could not exchange filter impulse response, length mismatch" );

			if( oArgs.HasKey( "HPIRInvCalibrationGain" ) && oArgs.HasKey( "HPIRInvCalibrationGainDecibel" ) )
				VA_EXCEPT2( INVALID_PARAMETER, "HPIRInvCalibrationGain was passed both as factor and decibel and I can't decide which one is correct." );

			if( oArgs.HasKey( "HPIRInvCalibrationGain" ) )
			{
				const CVAStructValue* pGainValue = oArgs.GetValue( "HPIRInvCalibrationGain" );
				if( pGainValue != nullptr )
				{
					if( pGainValue->IsNumeric( ) == false )
						VA_EXCEPT2( INVALID_PARAMETER, "Gain value must be numerical" );

					double dGain               = double( *pGainValue );
					m_pHPEQStreamFilter->fGain = float( dGain );
				}
			}

			if( oArgs.HasKey( "HPIRInvCalibrationGainDebicel" ) )
			{
				const CVAStructValue* pGainValue = oArgs.GetValue( "HPIRInvCalibrationGainDebicel" );
				if( pGainValue != nullptr )
				{
					if( pGainValue->IsNumeric( ) == false )
						VA_EXCEPT2( INVALID_PARAMETER, "Gain value must be numerical" );

					double dGainDecibel        = double( *pGainValue );
					m_pHPEQStreamFilter->fGain = float( db20_to_ratio( dGainDecibel ) );
				}
				else
				{
					VA_EXCEPT2( INVALID_PARAMETER, "Gain value missing" );
				}
			}
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized SET value, use 'print' 'help' for further information" );
		}
	}
}

CVAStruct CVAHeadphonesReproduction::GetParameters( const CVAStruct& oArgs ) const
{
	CVAStruct oReturn;
	const CVAStructValue* pStruct;

	if( ( pStruct = oArgs.GetValue( "PRINT" ) ) != nullptr )
	{
		VA_PRINT( "Available commands for " + CVAObject::GetObjectName( ) + ": print, set, get" );
		VA_PRINT( "PRINT:" );
		VA_PRINT( "\t'print', 'help'" );
		VA_PRINT( "GET:" );
		VA_PRINT( "\t'get', 'gain'" );
		VA_PRINT( "SET:" );
		VA_PRINT( "\t'set', 'gain', 'value', <number>" );
		VA_PRINT( "\t'set', 'hpirinv', 'value', <path> (use something like '$(VADataDir)/path/to/ir.wav' )" );

		oReturn["Return"] = true; // dummy return value, otherwise Problem with MATLAB
		return oReturn;
	}

	if( ( pStruct = oArgs.GetValue( "GET" ) ) != nullptr )
	{
		if( pStruct->GetDatatype( ) != CVAStructValue::STRING )
			VA_EXCEPT2( INVALID_PARAMETER, "GET command must be a string" );
		std::string sGetCommand = toUppercase( *pStruct );

		if( sGetCommand == "GAIN" )
		{
			oReturn["RETURN"] = m_pHPEQStreamFilter->fGain;
			return oReturn;
		}
		else
		{
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized GET command " + sGetCommand );
		}
	}

	return oReturn;
}

void CVAHeadphonesReproduction::SetInputDatasource( ITADatasource* p )
{
	m_pHPEQStreamFilter->pdsInput = p;
}

ITADatasource* CVAHeadphonesReproduction::GetOutputDatasource( )
{
	return m_pHPEQStreamFilter;
}

int CVAHeadphonesReproduction::GetNumInputChannels( ) const
{
	return m_pHPEQStreamFilter->GetNumberOfChannels( );
}


// --= Streaming class =--

HPEQStreamFilter::HPEQStreamFilter( double dSampleRate, int iBlockLength, const ITASampleFrame& sfHpIRinv )
    : ITADatasourceRealization( 2, dSampleRate, iBlockLength )
    , pdsInput( NULL )
    , fGain( 1.0f )
{
	pConvolverLeft = new ITAUPConvolution( GetBlocklength( ), sfHpIRinv.length( ) );
	pConvolverLeft->SetFilterExchangeFadingFunction( ITABase::FadingFunction::SWITCH );

	pConvolverRight = new ITAUPConvolution( GetBlocklength( ), sfHpIRinv.length( ) );
	pConvolverRight->SetFilterExchangeFadingFunction( ITABase::FadingFunction::SWITCH );

	ExchangeFilter( sfHpIRinv );
}

bool HPEQStreamFilter::ExchangeFilter( const ITASampleFrame& sfHpIRinv )
{
	if( sfHpIRinv.length( ) > pConvolverLeft->GetMaxFilterlength( ) )
		return false;

	// Left
	ITAUPFilter* pHPEQLeft = pConvolverLeft->RequestFilter( );
	pHPEQLeft->Load( ( &sfHpIRinv[0] )->data( ), sfHpIRinv.length( ) );
	pConvolverLeft->ExchangeFilter( pHPEQLeft );
	pConvolverLeft->ReleaseFilter( pHPEQLeft );

	// Right
	ITAUPFilter* pHPEQRight = pConvolverLeft->RequestFilter( );
	pHPEQRight->Load( ( &sfHpIRinv[1] )->data( ), sfHpIRinv.length( ) );
	pConvolverRight->ExchangeFilter( pHPEQRight );
	pConvolverLeft->ReleaseFilter( pHPEQRight );

	return true;
}

HPEQStreamFilter::~HPEQStreamFilter( )
{
	delete pConvolverLeft, pConvolverRight;
}

void HPEQStreamFilter::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	float* pfBinauralOutputDataChL = GetWritePointer( 0 );
	float* pfBinauralOutputDataChR = GetWritePointer( 1 );

	const float* pfInputDataLeft  = pdsInput->GetBlockPointer( 0, pStreamInfo );
	const float* pfInputDataRight = pdsInput->GetBlockPointer( 1, pStreamInfo );

	pConvolverLeft->Process( pfInputDataLeft, pfBinauralOutputDataChL, ITABase::MixingMethod::OVERWRITE );
	pConvolverRight->Process( pfInputDataRight, pfBinauralOutputDataChR, ITABase::MixingMethod::OVERWRITE );

	pConvolverLeft->SetGain( fGain );
	pConvolverRight->SetGain( fGain );

	IncrementWritePointer( );
}

void HPEQStreamFilter::PostIncrementBlockPointer( )
{
	pdsInput->IncrementBlockPointer( );
}

#endif // VACORE_WITH_REPRODUCTION_HEADPHONES
