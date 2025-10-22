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

#include "VAReproductionAmbisonicsBinauralMixdown.h"

#ifdef VACORE_WITH_REPRODUCTION_AMBISONICS_BINAURAL_MIXDOWN

#	include "../../Motion/VAMotionModelBase.h"
#	include "../../Scene/VAMotionState.h"
#	include "../../Scene/VASceneState.h"
#	include "../../Scene/VASoundReceiverState.h"
#	include "../../Utils/VAUtils.h"
#	include "../../VAHardwareSetup.h"
#	include "../../directivities/VADirectivityDAFFHRIR.h"

#	include <ITAConstants.h>
#	include <ITADataSourceRealization.h>
#	include <ITAFastMath.h>
#	include <ITANumericUtils.h>
#	include <ITAStreamPatchbay.h>
#	include <ITAUPConvolution.h>
#	include <ITAUPFilter.h>
#	include <ITAUPFilterPool.h>
#	include <ITAUPTrigger.h>
#	include <VistaBase/VistaQuaternion.h>
#	include <VistaBase/VistaTransformMatrix.h>
#	include <VistaBase/VistaVector3D.h>
#	include <xutility>


class CMixdownStreamFilter : public ITADatasourceRealization
{
public:
	ITADatasource* pdsInput;
	ITAUPFilterPool* pFilterPool;
	ITAUPTrigger* pTrigger;

	std::vector<ITAUPConvolution*> vpConvolver;

	CMixdownStreamFilter( int iNumLoudspeaker, double dSampleRate, int iBlockLength, int iMaxFilterLength );
	~CMixdownStreamFilter( );
	void SetIdentity( );
	void SetFilter( const int iIndex, ITAUPFilter* pFilterChL, ITAUPFilter* pFilterChR );
	void ProcessStream( const ITAStreamInfo* pStreamInfo );
	void PostIncrementBlockPointer( );
};

CVAAmbisonicsBinauralMixdownReproduction::CVAAmbisonicsBinauralMixdownReproduction( const CVAAudioReproductionInitParams& oParams )
    : m_oParams( oParams )
    , m_pdsStreamFilter( nullptr )
    , m_iListenerID( -1 )
    , m_pDefaultHRIR( nullptr )
    , m_matYinv( )
    , m_orderMatrices( )
    , m_bBFormatIsInit( false )
    , m_dTrackingDelaySeconds( 0 )
{
	CVAConfigInterpreter conf( *( m_oParams.pConfig ) );

	double dSampleRate = m_oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.dSampleRate;
	int iBlockLength   = oParams.pCore->GetCoreConfig( )->oAudioDriverConfig.iBuffersize;

	conf.ReqInteger( "TruncationOrder", m_iAmbisonicsTruncationOrder ); // Sollte aus der Zahl der Eingangskanäle berechnet werden
	conf.OptInteger( "HRIRFilterLength", m_iHRIRFilterLength, 512 );
	conf.OptInteger( "TrackedListenerID", m_iListenerID, 1 );
	conf.OptString( "RotationMode", m_sRotationMode, "BFormat" ); // Set calculation of rotations to "ViewUp" or "Quaternion" or "BFormat"


	std::string sDefaultHRIRPathRAW;
	conf.OptString( "DefaultHRIR", sDefaultHRIRPathRAW );
	std::string sReproCenterPos;
	double dTrackingDelaySeconds;
	conf.OptString( "ReproductionCenterPos", sReproCenterPos, "AUTO" );
	conf.OptString( "HRIR", sDefaultHRIRPathRAW );
	conf.OptNumber( "TrackingDelaySeconds", dTrackingDelaySeconds, 0 );
	m_dTrackingDelaySeconds = dTrackingDelaySeconds;
	if( sReproCenterPos == "AUTO" )
	{
		VA_WARN( this, "Ambisonics reproduction center set to 0 0 0" );
		m_v3ReproductionCenterPos.Set( 0, 0, 0 );
	}
	else
	{
		std::vector<std::string> vsPosComponents = splitString( sReproCenterPos, ',' );
		assert( vsPosComponents.size( ) == 3 );
		m_v3ReproductionCenterPos.Set( StringToFloat( vsPosComponents[0] ), StringToFloat( vsPosComponents[1] ), StringToFloat( vsPosComponents[2] ) );
	}


	if( !sDefaultHRIRPathRAW.empty( ) )
	{
		std::string sDefaultHRIRPath = oParams.pCore->FindFilePath( sDefaultHRIRPathRAW );
		if( sDefaultHRIRPath.empty( ) )
		{
			VA_WARN( "AmbisonicsBinauralMixdownReproduction",
			         "Could not find default HRIR '" << sDefaultHRIRPathRAW << "', you have to provide a HRIR with the tracked listener." );
			VA_WARN( "AmbisonicsBinauralMixdownReproduction", "Could not find HRIR '" << sDefaultHRIRPathRAW << "', you have to provide a HRIR for the listener." );
			VA_WARN( "AmbisonicsBinauralMixdownReproduction", "Erasing the key 'HRIR' in the VACore.ini will load the default HRIR of the virtual receiver" );
		}
		else
		{
			m_pDefaultHRIR = new CVADirectivityDAFFHRIR( sDefaultHRIRPath, "AmbisonicsBinauralMixdownReproduction_DefaultHRIR", dSampleRate );
		}
	}
	else
	{
		VA_ERROR( "AmbisonicsBinauralMixdownReproduction", "NO HRIR DATASET GIVEN" );
	}

	m_sfHRIRTemp.init( 2, m_iHRIRFilterLength, true );

	// Binaural output
	for( size_t i = 0; i < m_oParams.vpOutputs.size( ); i++ )
	{
		const CVAHardwareOutput* pTargetOutput = m_oParams.vpOutputs[i];
		if( pTargetOutput == nullptr )
			VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized output '" + pTargetOutput->sIdentifier + "' for binaural mixdown reproduction" );
		if( pTargetOutput->GetPhysicalOutputChannels( ).size( ) != 2 )
			VA_EXCEPT2( INVALID_PARAMETER, "Expecting two channels for binaural downmix target, problem with output '" + pTargetOutput->sIdentifier + "' detected" );
		m_vpTargetOutputs.push_back( pTargetOutput );
	}

	// Virtual loudspeaker output
	std::string sVirtualOutput;
	conf.ReqString( "VirtualOutput", sVirtualOutput );
	m_pVirtualOutput = m_oParams.pCore->GetCoreConfig( )->oHardwareSetup.GetOutput( sVirtualOutput );


	if( m_pVirtualOutput == nullptr )
		VA_EXCEPT2( INVALID_PARAMETER, "Unrecognized virtual output '" + sVirtualOutput + "' for binaural mixdown reproduction" );

	// Decoder input (B-Format with implicit channels)
	m_pDecoderMatrixPatchBay = new ITAStreamPatchbay( dSampleRate, iBlockLength );
	m_pDecoderMatrixPatchBay->AddInput( GetNumInputChannels( ) );

	// Decoder output (N-channel loudspeaker)
	m_pDecoderMatrixPatchBay->AddOutput( GetNumVirtualLoudspeaker( ) );
	m_pdsStreamFilter           = new CMixdownStreamFilter( GetNumVirtualLoudspeaker( ), dSampleRate, iBlockLength, m_iHRIRFilterLength );
	m_pdsStreamFilter->pdsInput = m_pDecoderMatrixPatchBay->GetOutputDatasource( 0 );

	Eigen::MatrixXd matY( m_pVirtualOutput->vpDevices.size( ), ( m_iAmbisonicsTruncationOrder + 1 ) * ( m_iAmbisonicsTruncationOrder + 1 ) );


	// TODO Center Position aus Config?
	VAVec3 va3Origin( 0, 0, 0 );
	VAVec3 va3View( 0, 0, -1 );
	VAVec3 va3Up( 0, 1, 0 );

	// Transform VAVector to VistaVector. Necessary to rotate with Quaternion
	VistaVector3D vvOrigin = VistaVector3D( va3Origin.x, va3Origin.y, va3Origin.z );
	VistaVector3D vvView   = VistaVector3D( va3View.x, va3View.y, va3View.z );
	VistaVector3D vvUp     = VistaVector3D( va3Up.x, va3Up.y, va3Up.z );


	std::vector<double> vdYParts;

	double dAzimuth, dElevation;

	// Gather matrix with loudspeaker position as (N_LS x 3);
	for( int k = 0; k < m_pVirtualOutput->vpDevices.size( ); k++ )
	{
		const CVAHardwareDevice* pDevice = m_pVirtualOutput->vpDevices[k];

		// There is no option for BFormat rotation in this application
		if( m_sRotationMode == "Quaternion" || m_sRotationMode == "BFormat" )
		{
			// Virtual Speaker position: Cast VAVec to VistaVector to build a VistaQuaternion for the rotation
			VistaVector3D vvDevicePos = VistaVector3D( pDevice->vPos.x, pDevice->vPos.y, pDevice->vPos.z );

			/*CODE SNIPPET FOR VARIABLE ROTATION
			//IF "ToDo:  Center Position aus Config" is done, uncomment following code.
			//It rotates with Quaternion with arbitrary Positions (As done in UpdateScene).
			//Has not been tested.

			//rotate ListUp to point Up in origin (0,1,0)
			VistaQuaternion qUpToOrigin(vvUp , VistaVector3D(0,1,0));

			//Rotate ListenerView and devicePosition according to displacement of ListenerUp:
			VistaVector3D vvView2 = qUpToOrigin.Rotate(vvView);
			VistaVector3D vvRotatedDevicePosFromUp = qUpToOrigin.Rotate(vvDevicePos);

			//rotate ListenerView: perpendicular to Up, Vector points straight forward (0,0,-1)
			VistaQuaternion qViewToOrigin(vvView2, VistaVector3D(0, 0, -1));

			//Rotate devicePosition according to displacement of the new ListnerView
			VistaVector3D vvRotatedDevicePos = qViewToOrigin.Rotate(vvRotatedDevicePosFromUp);


			//Final rotation: StandardListenerView to RotatedDevicePosition
			VistaQuaternion qRotationToDevice(VistaVector3D(0, 0, -1), vvRotatedDevicePos);

			//Extract Angles from Quaternion
			double roll, pitch, yaw;
			calculateYPRAnglesFromQuaternion(qRotationToDevice, roll, pitch, yaw);

			dAzimuth = yaw; //yaw --> Azimuth
			dAzimuth = ((dAzimuth < 0.0f) ? (dAzimuth + 2* ITAConstants::PI_D) : dAzimuth); //Azimuth should not be a negative value

			dElevation = pitch; //pitch --> Elevation
			*/


			// This function is enough to Rotate around STANDARD va3ORIGIN;va3VIEW;va3UP
			VistaQuaternion qRot( VistaVector3D( 0, 0, -1 ), vvDevicePos );

			// Extract Angles from Quaternion
			double roll, pitch, yaw;
			calculateYPRAnglesFromQuaternion( qRot, roll, pitch, yaw );

			dAzimuth = yaw;                                                                        // yaw --> Azimuth
			dAzimuth = ( ( dAzimuth < 0.0f ) ? ( dAzimuth + 2 * ITAConstants::PI_D ) : dAzimuth ); // Azimuth should not be a negative value

			dElevation = pitch; // pitch --> Elevation
		}

		else if( m_sRotationMode == "ViewUp" )
		{
			dAzimuth   = GetAzimuthOnTarget_DEG( va3Origin, va3View, va3Up, pDevice->vPos ) / 180 * ITAConstants::PI_D;
			dElevation = GetElevationOnTarget_DEG( va3Origin, va3Up, pDevice->vPos ) / 180 * ITAConstants::PI_D;
		}

		else
		{
			return; // RotationMode is set to a wrong String
		}


		vdYParts = SHRealvaluedBasefunctions( ITAConstants::PI_D / 2 - dElevation, dAzimuth, m_iAmbisonicsTruncationOrder );
		for( int l = 0; l < vdYParts.size( ); l++ )
		{
			matY( k, l ) = vdYParts[l];
		}
	}

	// Eigen::MatrixXd matYinv = CalculatePseudoInverse(matY);
	m_matYinv = CalculatePseudoInverse( matY );

	// std::vector<double> vdRemaxWeights = HOARemaxWeights(m_iAmbisonicsTruncationOrder);
	m_vdRemaxWeights = HOARemaxWeights( m_iAmbisonicsTruncationOrder );

	// Matrix
	for( int i = 0; i < GetNumInputChannels( ); i++ )
	{
		int iCurrentOrder = (int)floor( sqrt( i ) );
		for( int j = 0; j < GetNumVirtualLoudspeaker( ); j++ )
		{
			double dGain = m_matYinv( i, j ) * m_vdRemaxWeights[iCurrentOrder];
			m_pDecoderMatrixPatchBay->ConnectChannels( 0, i, 0, j, dGain );
		}
	}

	m_viLastHRIRIndex.resize( GetNumVirtualLoudspeaker( ) );


	// --= Motion Model =--
	CVABasicMotionModel::Config oConf;
	oConf.SetDefaults( );
	oConf.iNumHistoryKeys = 1000;
	oConf.dWindowDelay    = m_dTrackingDelaySeconds;
	oConf.dWindowSize     = 0.5;
	m_pMotionModel        = new CVABasicMotionModel( oConf );
	return;
}

CVAAmbisonicsBinauralMixdownReproduction::~CVAAmbisonicsBinauralMixdownReproduction( )
{
	delete m_pdsStreamFilter;
	m_pdsStreamFilter = NULL;

	delete m_pDecoderMatrixPatchBay;
	m_pDecoderMatrixPatchBay = NULL;

	if( m_pDefaultHRIR )
		delete m_pDefaultHRIR;
}

void CVAAmbisonicsBinauralMixdownReproduction::SetInputDatasource( ITADatasource* p )
{
	m_pDecoderMatrixPatchBay->SetInputDatasource( 0, p );
}

ITADatasource* CVAAmbisonicsBinauralMixdownReproduction::GetOutputDatasource( )
{
	return m_pdsStreamFilter;
}

int CVAAmbisonicsBinauralMixdownReproduction::GetNumVirtualLoudspeaker( ) const
{
	size_t iSize = m_pVirtualOutput->vpDevices.size( );
	return int( iSize );
}

int CVAAmbisonicsBinauralMixdownReproduction::GetNumInputChannels( ) const
{
	return ( ( m_iAmbisonicsTruncationOrder + 1 ) * ( m_iAmbisonicsTruncationOrder + 1 ) );
}

void CVAAmbisonicsBinauralMixdownReproduction::SetTrackedListener( const int iID )
{
	m_iListenerID = iID;
}


// Compare following functions to https://git.iem.at/audioplugins/IEMPluginSuite/blob/master/SceneRotator/Source/PluginProcessor.cpp
// calcRotationMatrix and helping functions ( u, v, w, p)
double P( int i, int l, int a, int b, Eigen::MatrixXd R1, Eigen::MatrixXd Rlm1 )
{
	double ri1  = R1( i + 1, 2 );
	double rim1 = R1( i + 1, 0 );
	double ri0  = R1( i + 1, 1 );

	if( b == -l )
		return ri1 * Rlm1( a + l - 1, 0 ) + rim1 * Rlm1( a + l - 1, 2 * l - 2 );
	else if( b == l )
		return ri1 * Rlm1( a + l - 1, 2 * l - 2 ) - rim1 * Rlm1( a + l - 1, 0 );
	else
		return ri0 * Rlm1( a + l - 1, b + l - 1 );
};

double U( int l, int m, int n, Eigen::MatrixXd Rone, Eigen::MatrixXd Rlm1 )
{
	return P( 0, l, m, n, Rone, Rlm1 );
}

double V( int l, int m, int n, Eigen::MatrixXd Rone, Eigen::MatrixXd Rlm1 )
{
	if( m == 0 )
	{
		auto p0 = P( 1, l, 1, n, Rone, Rlm1 );
		auto p1 = P( -1, l, -1, n, Rone, Rlm1 );
		return p0 + p1;
	}
	else if( m > 0 )
	{
		auto p0 = P( 1, l, m - 1, n, Rone, Rlm1 );
		if( m == 1 ) // d = 1;
			return p0 * sqrt( 2 );
		else // d = 0;
			return p0 - P( -1, l, 1 - m, n, Rone, Rlm1 );
	}
	else
	{
		auto p1 = P( -1, l, -m - 1, n, Rone, Rlm1 );
		if( m == -1 ) // d = 1;
			return p1 * sqrt( 2 );
		else // d = 0;
			return p1 + P( 1, l, m + 1, n, Rone, Rlm1 );
	}
}

double W( int l, int m, int n, Eigen::MatrixXd Rone, Eigen::MatrixXd Rlm1 )
{
	if( m > 0 )
	{
		auto p0 = P( 1, l, m + 1, n, Rone, Rlm1 );
		auto p1 = P( -1, l, -m - 1, n, Rone, Rlm1 );
		return p0 + p1;
	}
	else if( m < 0 )
	{
		auto p0 = P( 1, l, m - 1, n, Rone, Rlm1 );
		auto p1 = P( -1, l, 1 - m, n, Rone, Rlm1 );
		return p0 - p1;
	}

	return 0.0;
}


// Uses rotation angles in radian
void calcRotationMatrix( Eigen::MatrixXd* m_orderMatrices, const int order, double dYaw, double dPitch, double dRoll )
{
	dYaw*( dYaw <= 0.5 ? -1 : 1 );

	auto ca = std::cos( dYaw );
	auto cb = std::cos( dPitch );
	auto cy = std::cos( dRoll );

	auto sa = std::sin( dYaw );
	auto sb = std::sin( dPitch );
	auto sy = std::sin( dRoll );

	Eigen::Matrix3d rotMat;


	// ToDo: Choose a rotation Model concerning our axis rotation
	if( true ) // roll -> pitch -> yaw (extrinsic rotations)
	{
		rotMat( 0, 0 ) = ca * cb;
		rotMat( 1, 0 ) = sa * cb;
		rotMat( 2, 0 ) = -sb;

		rotMat( 0, 1 ) = ca * sb * sy - sa * cy;
		rotMat( 1, 1 ) = sa * sb * sy + ca * cy;
		rotMat( 2, 1 ) = cb * sy;

		rotMat( 0, 2 ) = ca * sb * cy + sa * sy;
		rotMat( 1, 2 ) = sa * sb * cy - ca * sy;
		rotMat( 2, 2 ) = cb * cy;
	}
	else // yaw -> pitch -> roll (extrinsic rotations)
	{
		rotMat( 0, 0 ) = ca * cb;
		rotMat( 1, 0 ) = sa * cy + ca * sb * sy;
		rotMat( 2, 0 ) = sa * sy - ca * sb * cy;

		rotMat( 0, 1 ) = -sa * cb;
		rotMat( 1, 1 ) = ca * cy - sa * sb * sy;
		rotMat( 2, 1 ) = ca * sy + sa * sb * cy;

		rotMat( 0, 2 ) = sb;
		rotMat( 1, 2 ) = -cb * sy;
		rotMat( 2, 2 ) = cb * cy;
	}


	Eigen::MatrixXd zero( 1, 1 );
	zero( 0, 0 )       = 1;
	m_orderMatrices[0] = zero;

	Eigen::MatrixXd Rl( 3, 3 );
	Rl( 0, 0 ) = rotMat( 1, 1 );
	Rl( 0, 1 ) = rotMat( 1, 2 );
	Rl( 0, 2 ) = rotMat( 1, 0 );

	Rl( 1, 0 ) = rotMat( 2, 1 );
	Rl( 1, 1 ) = rotMat( 2, 2 );
	Rl( 1, 2 ) = rotMat( 2, 0 );

	Rl( 2, 0 )         = rotMat( 0, 1 );
	Rl( 2, 1 )         = rotMat( 0, 2 );
	Rl( 2, 2 )         = rotMat( 0, 0 );
	m_orderMatrices[1] = Rl;


	for( int l = 2; l <= order; l++ )
	{
		auto Rone = m_orderMatrices[1];
		auto Rlm1 = m_orderMatrices[l - 1];
		Eigen::MatrixXd Rl( l * 2 + 1, l * 2 + 1 );

		for( int m = -l; m <= l; m++ )
		{
			for( int n = -l; n <= l; n++ )
			{
				const int d = ( m == 0 ) ? 1 : 0;
				double denom;
				if( abs( n ) == l )
					denom = ( 2 * l ) * ( 2 * l - 1 );
				else
					denom = l * l - n * n;

				double u = sqrt( ( l * l - m * m ) / denom );
				double v = sqrt( ( 1.0 + d ) * ( l + abs( m ) - 1.0 ) * ( l + abs( m ) ) / denom ) * ( 1.0 - 2.0 * d ) * 0.5;
				double w = sqrt( ( l - abs( m ) - 1.0 ) * ( l - abs( m ) ) / denom ) * ( 1.0 - d ) * ( -0.5 );

				if( u != 0.0 )
					u *= U( l, m, n, Rone, Rlm1 );
				if( v != 0.0 )
					v *= V( l, m, n, Rone, Rlm1 );
				if( w != 0.0 )
					w *= W( l, m, n, Rone, Rlm1 );
				Rl( m + l, n + l ) = u + v + w;
			}
		}

		m_orderMatrices[l] = Rl;
	}
}

// Creates a InputChannel X InputChannel Matrix out of the RotationMatrices for every Order of Ambisonics
Eigen::MatrixXd buildRotationMatrix( Eigen::MatrixXd* m_orderMatrices, int m_iAmbisonicsTruncationOrder )
{
	int iCurrentOrder;
	int boundary = pow( m_iAmbisonicsTruncationOrder + 1, 2 );
	// a rows and b columns
	Eigen::MatrixXd matRotation( boundary, boundary );
	for( int a = 0; a < boundary; a++ )
	{
		iCurrentOrder = floor( sqrt( a ) );

		for( int b = 0; b < boundary; b++ )
		{
			// prevent to reach out of bounds for rotation matrices
			if( iCurrentOrder > m_iAmbisonicsTruncationOrder )
			{
				matRotation( a, b ) = 0;
				continue;
			}
			if( a >= pow( iCurrentOrder, 2 ) && a <= 2 * iCurrentOrder + pow( iCurrentOrder, 2 ) )
			{
				if( b < pow( iCurrentOrder, 2 ) || b > 2 * iCurrentOrder + pow( iCurrentOrder, 2 ) )
				{
					matRotation( a, b ) = 0; // rows next to currentOrder Matrix are 0
					continue;
				}
				else
				{
					matRotation( a, b ) = m_orderMatrices[iCurrentOrder]( static_cast<Eigen::Index>( a - pow( iCurrentOrder, 2 ) ),
					                                                      static_cast<Eigen::Index>( b - pow( iCurrentOrder, 2 ) ) );
					continue;
				}
			}
			if( b >= pow( iCurrentOrder, 2 ) && b < 2 * iCurrentOrder + pow( iCurrentOrder, 2 ) )
			{
				if( a < pow( iCurrentOrder, 2 ) || a > 2 * iCurrentOrder + pow( iCurrentOrder, 2 ) )
				{
					matRotation( a, b ) = 0; // columns next to currentOrder Matrix are 0
					continue;
				}
				else
				{
					matRotation( a, b ) =
					    m_orderMatrices[iCurrentOrder]( static_cast<Eigen::Index>( a - pow( iCurrentOrder, 2 ) ),
					                                    static_cast<Eigen::Index>( b - pow( iCurrentOrder, 2 ) ) ); // matching zu (0,0) stelle der teil Matrizen
					continue;
				}
			}
		}
	}
	return matRotation;
}


void CVAAmbisonicsBinauralMixdownReproduction::UpdateScene( CVASceneState* pNewState )
{
	// m_iListenerID = 1;
	if( m_iListenerID == -1 )
		return;

	CVAReceiverState* pListenerState( pNewState->GetReceiverState( m_iListenerID ) );
	if( pListenerState == nullptr )
		return;

	// Motion model input
	auto pMotionState = pListenerState->GetMotionState( );
	if( !pMotionState )
		return;

	const CVADirectivityDAFFHRIR* pReceiverHRIR = (CVADirectivityDAFFHRIR*)pListenerState->GetDirectivity( );

	const CVADirectivityDAFFHRIR* pHRIR;
	if( pReceiverHRIR )
		pHRIR = pReceiverHRIR;
	else if( m_pDefaultHRIR )
		pHRIR = m_pDefaultHRIR;
	else
		return;


	double dTime            = pMotionState->GetModificationTime( );
	VAVec3 v3PosRealWorld   = pMotionState->GetRealWorldPose( ).vPos;
	VAQuat qOrientRealWorld = pMotionState->GetRealWorldPose( ).qOrient;
	m_pMotionModel->InputMotionKey( dTime, v3PosRealWorld, qOrientRealWorld );

	// Motion (orientation) estimation (uses internal window delay to emulate additional tracking delay)
	VAVec3 vListenerPos, vListenerView, vListenerUp, vListenerView2, vListenerUp2;
	VAQuat qOrientEstimated;
	m_pMotionModel->EstimateOrientation( dTime, qOrientEstimated );

	ConvertQuaternionToViewUp( qOrientEstimated, vListenerView, vListenerUp );
	ConvertQuaternionToViewUp( qOrientRealWorld, vListenerView2, vListenerUp2 );

	// keep the Listener Position fixed, since we want to keep the virtual loudspeaker array fixed around this position
	vListenerPos = m_v3ReproductionCenterPos;

	// Used as a reference point to build rotational quaternions
	// ListenerView ListenerUp are first rotated to this standard values to extract angles to the virtual speaker array
	VistaVector3D vvStandardListenerUp   = VistaVector3D( 0, 1, 0 );
	VistaVector3D vvStandardListenerView = VistaVector3D( 0, 0, -1 );

	// Listener: Cast VAVec to VistaVector to build a VistaQuaternion for the rotation
	VistaVector3D vvListenerView = VistaVector3D( vListenerView.x, vListenerView.y, vListenerView.z );
	VistaVector3D vvListenerUp   = VistaVector3D( vListenerUp.x, vListenerUp.y, vListenerUp.z );
	VistaVector3D vvListenerPos  = VistaVector3D( vListenerPos.x, vListenerPos.y, vListenerPos.z );


	if( m_sRotationMode == "BFormat" && m_bBFormatIsInit )
	{
		// Calculate Rotation from Listener to Origin
		// rotate ListenerUp to point Up in standard Up-Vector(0,1,0)
		VistaQuaternion qUpToOrigin( vvListenerUp, vvStandardListenerUp );

		// Rotate ListenerView and devicePosition according to displacement of ListenerUp:
		VistaVector3D vvRotatedListenerView = qUpToOrigin.Rotate( vvListenerView );

		// rotate ListenerView: perpendicular to Up, Vector points straight forward (0,0,-1)
		VistaQuaternion qRotatedViewToOrigin( vvRotatedListenerView, vvStandardListenerView );

		// Extract Angles from Quaternion in RAD
		double dRoll, dPitch, dYaw;
		calculateYPRAnglesFromQuaternion( qRotatedViewToOrigin, dRoll, dPitch, dYaw );

		// calculate a rotation matrix for every ambisonicsorder with calculated rotation angles roll ,pitch yaw
		calcRotationMatrix( m_orderMatrices, m_iAmbisonicsTruncationOrder, dYaw, dPitch, dRoll );
		// build ONE rotation matrix out of the several matrices for every ambisonics order
		Eigen::MatrixXd matRotation = buildRotationMatrix( m_orderMatrices, m_iAmbisonicsTruncationOrder );


		// Init diag matrix for weights
		Eigen::MatrixXd matDiagRemax( GetNumInputChannels( ), GetNumInputChannels( ) );
		matDiagRemax.diagonal( );
		matDiagRemax.setZero( );

		// add weights to diagonal
		Eigen::VectorXd vdRemaxWeights;
		for( int i = 0; i < GetNumInputChannels( ); i++ )
		{
			matDiagRemax( i, i ) = m_vdRemaxWeights[floor( sqrt( i ) )];
		}

		// Calculate the new decoding matrix with: Rotation, weights and the old decoding matrix
		Eigen::MatrixXd matDecoder = m_matYinv.transpose( ) * matDiagRemax * matRotation;


		double dGain = 0;
		// set gains to the channels according to the decoding matrix
		for( int i = 0; i < GetNumInputChannels( ); i++ )
		{
			for( int j = 0; j < GetNumVirtualLoudspeaker( ); j++ )
			{
				dGain = matDecoder( j, i );
				m_pDecoderMatrixPatchBay->ConnectChannels( 0, i, 0, j, dGain );
			}
		}
	}

	else if( ( m_sRotationMode == "ViewUp" ) || ( m_sRotationMode == "Quaternion" ) || !m_bBFormatIsInit )
	{
		double dAzimuth, dElevation;

		// iterate over all virtual loudspeakers and add rotation Quaternion to adjust position with regards to the head movement
		for( int i = 0; i < GetNumVirtualLoudspeaker( ); i++ )
		{
			// Get Pose of VirtualLoudspeaker
			const CVAHardwareDevice* pDevice( m_pVirtualOutput->vpDevices[i] );

			if( m_sRotationMode == "ViewUp" || !m_bBFormatIsInit )
			{
				dAzimuth   = GetAzimuthOnTarget_DEG( vListenerPos, vListenerView, vListenerUp, pDevice->vPos );
				dElevation = GetElevationOnTarget_DEG( vListenerPos, vListenerUp, pDevice->vPos );
			}
			else if( m_sRotationMode == "Quaternion" )
			{
				// Virtual Speaker position: Cast VAVec to VistaVector to build a VistaQuaternion for the rotation
				VistaVector3D vvDevicePos = VistaVector3D( pDevice->vPos.x, pDevice->vPos.y, pDevice->vPos.z );


				// calculate rotation quaternion for ListenerUp to point Up in origin (0,1,0)
				VistaQuaternion qUpToOrigin( vvListenerUp, vvStandardListenerUp );

				// Rotate ListenerView and devicePosition according to displacement of ListenerUp:
				VistaVector3D vvRotatedListenerView    = qUpToOrigin.Rotate( vvListenerView );
				VistaVector3D vvRotatedDevicePosFromUp = qUpToOrigin.Rotate( vvDevicePos );


				// Calculate rotation quaternion for ListenerView: perpendicular to Up, Vector points straight forward (0,0,-1)
				VistaQuaternion qViewToOrigin( vvRotatedListenerView, vvStandardListenerView );

				// Rotate devicePosition according to displacement of the new ListnerView
				VistaVector3D vvRotatedDevicePos = qViewToOrigin.Rotate( vvRotatedDevicePosFromUp );


				// Final rotation: StandardListenerView to RotatedDevicePosition
				VistaQuaternion qRotationToDevice( vvStandardListenerView, vvRotatedDevicePos );


				// Extract Angles from Quaternion
				double dRoll, dPitch, dYaw;
				calculateYPRAnglesFromQuaternion( qRotationToDevice, dRoll, dPitch, dYaw );
				// Conversion from RAD to DEG
				dAzimuth = dYaw * 180 / ITAConstants::PI_D;                            // yaw --> Azimuth
				dAzimuth = ( ( dAzimuth < 0.0f ) ? ( dAzimuth + 360.0f ) : dAzimuth ); // Azimuth should not be a negative value

				dElevation = dPitch * 180 / ITAConstants::PI_D; // pitch --> Elevation
			}


			int iIndex;
			int iIndexLeft  = 2 * i + 0;
			int iIndexRight = 2 * i + 1;


			// Re-init temp buffer to match with current HRIR length
			int iHRIRFilterLength = pHRIR->GetProperties( )->iFilterLength;
			if( m_sfHRIRTemp.length( ) != iHRIRFilterLength )
				m_sfHRIRTemp.init( 2, iHRIRFilterLength, false );

			pHRIR->GetNearestNeighbour( float( dAzimuth ), float( dElevation ), &iIndex );


			// Skip if still same HRIR
			if( iIndex == m_viLastHRIRIndex[i] )
				continue;

			pHRIR->GetHRIRByIndex( &m_sfHRIRTemp, iIndex, 0 );
			m_viLastHRIRIndex[i] = iIndex;

			ITAUPFilter* pFilterChL = m_pdsStreamFilter->pFilterPool->RequestFilter( );
			ITAUPFilter* pFilterChR = m_pdsStreamFilter->pFilterPool->RequestFilter( );

			int iLoadFilter = (std::min)( m_iHRIRFilterLength, iHRIRFilterLength );
			pFilterChL->Zeros( );
			pFilterChL->Load( m_sfHRIRTemp[0].data( ), iLoadFilter );
			pFilterChR->Zeros( );
			pFilterChR->Load( m_sfHRIRTemp[1].data( ), iLoadFilter );

			// Update
			m_pdsStreamFilter->vpConvolver[iIndexLeft]->ExchangeFilter( pFilterChL );
			m_pdsStreamFilter->vpConvolver[iIndexRight]->ExchangeFilter( pFilterChR );


			pFilterChL->Release( );
			pFilterChR->Release( );


			m_pdsStreamFilter->pTrigger->trigger( );
		}
		m_bBFormatIsInit = true;
	}

	else
	{
		return; // RotationMode is set to a wrong String
	}


	return;
}
void CVAAmbisonicsBinauralMixdownReproduction::GetCalculatedReproductionCenterPos( VAVec3& vec3CalcPos )
{
	int iNumberDevices = 0;
	VAVec3 vec3Old;
	vec3CalcPos.Set( 0, 0, 0 );
	vec3Old.Set( 0, 0, 0 );
	for( int idxTO = 0; idxTO < m_vpTargetOutputs.size( ); idxTO++ )
	{
		for( int idxDev = 0; idxDev < m_vpTargetOutputs[idxTO]->vpDevices.size( ); idxDev++ )
		{
			iNumberDevices++;
			vec3Old     = vec3CalcPos;
			vec3CalcPos = vec3Old + m_vpTargetOutputs[idxTO]->vpDevices[idxDev]->vPos;
		}
	}
	vec3Old = vec3CalcPos;
	vec3CalcPos.Set( vec3Old.x / iNumberDevices, vec3Old.y / iNumberDevices, vec3Old.z / iNumberDevices );
}

Eigen::MatrixXd CVAAmbisonicsBinauralMixdownReproduction::CalculatePseudoInverse( Eigen::MatrixXd ematIn )
{
	Eigen::JacobiSVD<Eigen::MatrixXd> svd( ematIn, Eigen::ComputeFullU | Eigen::ComputeFullV );

	double pinvtoler               = 1.e-6; // choose your tolerance wisely!
	Eigen::MatrixXd singularValues = svd.singularValues( );
	Eigen::MatrixXd singularValues_inv;
	singularValues_inv = singularValues_inv.Zero( ematIn.cols( ), ematIn.rows( ) );

	for( int i = 0; i < singularValues.size( ); ++i )
	{
		if( singularValues( i ) > pinvtoler )
			singularValues_inv( i, i ) = 1.0 / singularValues( i );
		else
			singularValues_inv( i, i ) = 0;
	}

	Eigen::MatrixXd V = svd.matrixV( );
	Eigen::MatrixXd U = svd.matrixU( );

	Eigen::MatrixXd Diag = singularValues_inv;

	Eigen::MatrixXd S1     = V * Diag;
	Eigen::MatrixXd UTrans = U.transpose( );
	Eigen::MatrixXd S2     = S1 * UTrans;

	return S2;
}

void CVAAmbisonicsBinauralMixdownReproduction::SetParameters( const CVAStruct& oParams )
{
	if( oParams.HasKey( "TrackingDelaySeconds" ) )
	{
		m_dTrackingDelaySeconds = oParams["TrackingDelaySeconds"];
		m_pMotionModel->UpdateWindowDelay( m_dTrackingDelaySeconds );
	}
}

CVAStruct CVAAmbisonicsBinauralMixdownReproduction::GetParameters( const CVAStruct& ) const
{
	CVAStruct oReturn;
	oReturn["TrackingDelaySeconds"]      = m_dTrackingDelaySeconds;
	oReturn["AmbisonicsTruncationOrder"] = m_iAmbisonicsTruncationOrder;
	oReturn["HRIRFilterLength"]          = m_iHRIRFilterLength;
	oReturn["TrackedListenerID"]         = m_iListenerID;
	// oReturn["DecoderMatrix"] = m_pDecoderMatrixPatchBay->???;
	oReturn["Name"] = m_sName;
	float fVec[3];
	fVec[0] = (float)m_v3ReproductionCenterPos.x;
	fVec[1] = (float)m_v3ReproductionCenterPos.y;
	fVec[2] = (float)m_v3ReproductionCenterPos.z;
	CVAStructValue d( fVec, 3 * sizeof( float ) );
	oReturn["ReproductionCenterPosition"] = d;
	// oReturn["TargetOutputs"] = m_vpTargetOutputs;
	oReturn["Config"] = *( m_oParams.pConfig );

	return oReturn;
}

int CVAAmbisonicsBinauralMixdownReproduction::GetAmbisonicsTruncationOrder( ) const
{
	return m_iAmbisonicsTruncationOrder;
}


// --= Stream filter =--
CMixdownStreamFilter::CMixdownStreamFilter( int iNumLoudspeaker, double dSampleRate, int iBlockLength, int iMaxFilterLength )
    : ITADatasourceRealization( 2, dSampleRate, iBlockLength )
    , pdsInput( NULL )
    , pFilterPool( NULL )
    , pTrigger( NULL )
{
	pFilterPool         = new ITAUPFilterPool( (int)m_uiBlocklength, iMaxFilterLength, 24 );
	ITAUPFilter* pDirac = pFilterPool->RequestFilter( );
	pDirac->identity( );

	pTrigger = new ITAUPTrigger;

	for( int i = 0; i < 2 * iNumLoudspeaker; i++ )
	{
		ITAUPConvolution* pConvolver = new ITAUPConvolution( (int)GetBlocklength( ), iMaxFilterLength, pFilterPool );
		pConvolver->SetFilterExchangeTrigger( pTrigger );
		pConvolver->SetFilterExchangeFadingFunction( ITABase::FadingFunction::COSINE_SQUARE );
		pConvolver->SetFilterCrossfadeLength( (int)GetBlocklength( ) );
		pConvolver->ExchangeFilter( pDirac );

		vpConvolver.push_back( pConvolver );
	}

	pDirac->Release( );

	pTrigger->trigger( );
}

CMixdownStreamFilter::~CMixdownStreamFilter( )
{
	for( size_t i = 0; i < vpConvolver.size( ); i++ )
		delete vpConvolver[i];

	delete pFilterPool;
	delete pTrigger;
}

void CMixdownStreamFilter::ProcessStream( const ITAStreamInfo* pStreamInfo )
{
	float* pfBinauralOutputDataChL = GetWritePointer( 0 );
	float* pfBinauralOutputDataChR = GetWritePointer( 1 );

	// Iterate over loudspeaker channels
	for( unsigned int i = 0; i < pdsInput->GetNumberOfChannels( ); i++ )
	{
		int iIndexLeft           = 2 * i + 0;
		int iIndexRight          = 2 * i + 1;
		const float* pfInputData = pdsInput->GetBlockPointer( i, pStreamInfo );
		int iWriteMode           = ( i == 0 ) ? ITABase::MixingMethod::OVERWRITE : ITABase::MixingMethod::ADD;
		vpConvolver[iIndexLeft]->Process( pfInputData, pfBinauralOutputDataChL, iWriteMode );
		vpConvolver[iIndexRight]->Process( pfInputData, pfBinauralOutputDataChR, iWriteMode );
	}

	IncrementWritePointer( );
}

void CMixdownStreamFilter::PostIncrementBlockPointer( )
{
	pdsInput->IncrementBlockPointer( );
}

#endif // VACORE_WITH_REPRODUCTION_AMBISONICS_BINAURAL_MIXDOWN