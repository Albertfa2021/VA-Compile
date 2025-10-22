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

#include "VAMotionModelBase.h"

#include "../Scene/VAMotionState.h"
#include "../Utils/VAUtils.h"
#include "../VALog.h"

#include <VAException.h>
#include <assert.h>
#include <iostream>

CVABasicMotionModel::CVABasicMotionModel( const Config& oConf ) : m_iNumKeys( 0 ), m_iTail( -1 ), m_dLastQueryTime( 0 ), m_oConf( oConf )
{
	if( m_oConf.iNumHistoryKeys < 1 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model history needs to be greater than zero" );

	m_vKeys.resize( m_oConf.iNumHistoryKeys );

	m_dWindowDelay = oConf.dWindowDelay;
	m_dWindowSize  = oConf.dWindowSize;

	if( m_dWindowDelay < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window delay cannot be negative" );
	if( m_dWindowSize <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window size cannot be negative or zero" );
}

CVABasicMotionModel::~CVABasicMotionModel( )
{
	try
	{
		Reset( ); // Store motion log, then deactivate (would try to store again on deletion)
		m_oEstimationDataLog.setOutputFile( "" );
		m_oInputDataLog.setOutputFile( "" );
	}
	catch( ITAException& e )
	{
		VA_ERROR( "BasicMotionModel", "Could not reset motion model instance: " << e.sReason );
	}
}

int CVABasicMotionModel::GetNumHistoryKeys( ) const
{
	return m_iNumKeys;
}

void CVABasicMotionModel::InputMotionKey( const CVAMotionState* pNewState )
{
	double dTime  = pNewState->GetModificationTime( );
	VAVec3 v3Pos  = pNewState->GetPosition( );
	VAVec3 v3View = pNewState->GetView( );
	VAVec3 v3Up   = pNewState->GetUp( );

	VAQuat qOrient;
	ConvertViewUpToQuaternion( v3View, v3Up, qOrient );
	InputMotionKey( dTime, v3Pos, qOrient );
}

void CVABasicMotionModel::InputMotionKey( double dTime, const VAVec3& vPos, const VAQuat& qOrient )
{
	/*
	VAQuat qOrient = pNewState->GetOrientation();
	VAQuat qOrientT;
	ConvertViewUpToQuaternion( vView, vUp, qOrientT );
	assert( qOrientT == qOrient );
	*/
	VAVec3 vView, vUp;
	ConvertQuaternionToViewUp( qOrient, vView, vUp );

	if( m_oConf.bLogInputEnabled )
	{
		MotionLogDataInput oLogItem;
		oLogItem.dTime   = dTime;
		oLogItem.vPos    = vPos;
		oLogItem.vView   = vView;
		oLogItem.vUp     = vUp;
		oLogItem.qOrient = qOrient;
		m_oInputDataLog.log( oLogItem );
	}

	// Store new element
	m_iTail             = ( m_iTail + 1 ) % m_oConf.iNumHistoryKeys;
	CMotionKey& oNewKey = m_vKeys[m_iTail];
	oNewKey.t           = dTime;
	oNewKey.p           = vPos;
	oNewKey.q           = qOrient;
	// oNewKey.v = vVel; // Get rid of velocity, this is not set by user anymore

	// Update number of elements
	if( m_iNumKeys < m_oConf.iNumHistoryKeys )
		m_iNumKeys++;

	return;
}

bool CVABasicMotionModel::EstimateMotion( double dTime, VAVec3& vPos, VAQuat& qOrient )
{
	// Ensure monotonous growing query times - Maybe helpful with the model
	if( m_iNumKeys == 0 )
		return false;

	assert( dTime >= m_dLastQueryTime );
	m_dLastQueryTime = dTime;

	MotionLogDataOutput oLogDataItem;
	if( m_oConf.bLogEstimatedOutputEnabled )
	{
		oLogDataItem.dTime = dTime;
	}

	int n       = 0; // Number of considered history elements
	double wsum = 0; // Sum of weights of considered history elements

	// Debug: iNumKeys = 1;
	while( n < m_iNumKeys )
	{
		CMotionKey* k = GetHistoryKey( n ); // Look n-times back into history
		if( !k )
		{
			assert( false );
			break;
		}

		// Important: Estimation time must lie behind the latest key's time
		double dt = (double)( (long double)dTime - (long double)k->t );
		// assert( dt > 0 );

		// Compute weight
		k->w = ComputeWeight( dt );

		// Keyframe out of focus... We do not need to continue
		bool bOutOfFocus = ( m_dWindowSize * 0.5f - m_dWindowDelay ) > 0;
		if( k->w <= 0 && bOutOfFocus )
			break;

		// Perform linear extrapolation with this key
		k->px.x = k->p.x + (float)dt * k->v.x;
		k->px.y = k->p.y + (float)dt * k->v.y;
		k->px.z = k->p.z + (float)dt * k->v.z;

		wsum += k->w;
		n++;
	}

	VAVec3 vVel;
	vVel.Set( 0, 0, 0 );
	vPos.Set( 0, 0, 0 );
	// There are history keys, but all are out of the window range => Last value counts
	if( ( n == 0 ) || ( wsum == 0 ) )
	{
		CMotionKey* k = GetHistoryKey( 0 );
		if( k )
		{
			double dt = (double)( (long double)dTime - (long double)k->t );
			vPos.x    = k->p.x + (float)dt * k->v.x;
			vPos.y    = k->p.y + (float)dt * k->v.y;
			vPos.z    = k->p.z + (float)dt * k->v.z;
			vVel.x    = k->v.x;
			vVel.y    = k->v.y;
			vVel.z    = k->v.z;

			qOrient = k->q;
			qOrient.Norm( );
		}

		if( m_oConf.bLogEstimatedOutputEnabled )
		{
			oLogDataItem.vPos    = vPos;
			oLogDataItem.qOrient = qOrient;
			ConvertQuaternionToViewUp( qOrient, oLogDataItem.vView, oLogDataItem.vUp );
			oLogDataItem.iNumInvolvedKeys = 0;
			m_oEstimationDataLog.log( oLogDataItem );
		}

		return true;
	}

	// Superposition of all extrapolated positions according to their weight
	double max_weight = -1.0f;
	for( int i = 0; i < n; i++ )
	{
		CMotionKey* k = GetHistoryKey( i );
		float s       = (float)( k->w / wsum );

		vPos.x += s * k->px.x;
		vPos.y += s * k->px.y;
		vPos.z += s * k->px.z;
		vVel.x += k->v.x;
		vVel.y += k->v.y;
		vVel.z += k->v.z;

		/* @todo use Slerp instead of linear weighting
		VistaQuaternion qStart( k->q.x, k->q.y, k->q.z, k->q.w );
		VistaQuaternion qEnd = qStart.Slerp( );
		*/

		// For now, just use window center (peak value of weights)
		if( max_weight < k->w )
		{
			qOrient.x  = k->q.x;
			qOrient.y  = k->q.y;
			qOrient.z  = k->q.z;
			qOrient.w  = k->q.w;
			max_weight = k->w;
		}
	}

	qOrient.Norm( ); // Important, if linear accumulation & weighting is used!

	if( m_oConf.bLogEstimatedOutputEnabled )
	{
		oLogDataItem.vPos             = vPos;
		oLogDataItem.iNumInvolvedKeys = n;
		oLogDataItem.qOrient          = qOrient;
		ConvertQuaternionToViewUp( qOrient, oLogDataItem.vView, oLogDataItem.vUp );
		m_oEstimationDataLog.log( oLogDataItem );
	}

	return true;
}

bool CVABasicMotionModel::EstimatePosition( double dTime, VAVec3& v3Pos )
{
	VAQuat qOrient;
	return EstimateMotion( dTime, v3Pos, qOrient );
}

bool CVABasicMotionModel::EstimateOrientation( double dTime, VAQuat& qOrient )
{
	VAVec3 v3Pos;
	return EstimateMotion( dTime, v3Pos, qOrient );
}

bool CVABasicMotionModel::EstimateOrientation( double dTime, VAVec3& vView, VAVec3& vUp )
{
	VAQuat qOrient;
	auto bRetVal = EstimateOrientation( dTime, qOrient );
	if( bRetVal )
	{
		ConvertQuaternionToViewUp( qOrient, vView, vUp );
		vView.Norm( );
		vUp.Norm( );
	}
	return bRetVal;
}

CVABasicMotionModel::CMotionKey* CVABasicMotionModel::GetHistoryKey( int iLookback )
{
	if( !( iLookback < m_iNumKeys ) )
	{
		assert( false );
		return nullptr;
	}

	assert( ( iLookback >= 0 ) && ( iLookback < m_oConf.iNumHistoryKeys ) );
	int iIndex = ( m_iTail + m_oConf.iNumHistoryKeys - iLookback ) % m_oConf.iNumHistoryKeys;
	assert( ( iIndex >= 0 ) && ( iIndex < (int)m_vKeys.size( ) ) );
	return &m_vKeys[iIndex];
}

double CVABasicMotionModel::ComputeWeight( double t ) const
{
	// Dreiecks-Funktion mit vorgegebener Fensterbreite
	double w = 1 - abs( t - m_dWindowDelay ) / m_dWindowSize;
	return ( w < 0 ? 0 : w );
}

void CVABasicMotionModel::Reset( )
{
	if( m_oConf.bLogEstimatedOutputEnabled )
	{
		std::string sFileName = m_oEstimationDataLog.GetOutputFileName( );
		try
		{
			m_oEstimationDataLog.store( sFileName );
		}
		catch( ITAException& e )
		{
			VA_ERROR( "BasicMotionModel", "Could not store motion data log: " << e.sReason );
		}
	}

	if( m_oConf.bLogInputEnabled )
	{
		std::string sFileName = m_oInputDataLog.GetOutputFileName( );
		try
		{
			m_oInputDataLog.store( sFileName );
		}
		catch( ITAException& e )
		{
			VA_ERROR( "BasicMotionModel", "Could not store motion data log: " << e.sReason );
		}
	}

	// No need to clear the key vector, will be overwritten anyway
	m_iNumKeys = 0;

	return;
}

void CVABasicMotionModel::SetName( const std::string& sNewName )
{
	// If no input file is set, logger will not export to a file.
	if( m_oConf.bLogInputEnabled )
		m_oInputDataLog.setOutputFile( sNewName + "_Input.log" );
	if( m_oConf.bLogEstimatedOutputEnabled )
		m_oEstimationDataLog.setOutputFile( sNewName + "_Estimated.log" );
}

void CVABasicMotionModel::UpdateWindowDelay( double dNewDelay )
{
	if( dNewDelay < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window delay cannot be negative" );

	m_dWindowDelay = dNewDelay;
}

void CVABasicMotionModel::UpdateWindowSize( double dNewSize )
{
	if( dNewSize < 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Basic motion model window size cannot be negative or zero" );

	m_dWindowSize = dNewSize;
}

std::ostream& CVABasicMotionModel::MotionLogDataOutput::outputDesc( std::ostream& os )
{
	os << "Estimation time"
	   << "\t"
	   << "Estimated Position [x]"
	   << "\t"
	   << "Estimated Position [y]"
	   << "\t"
	   << "Estimated Position [z]"
	   << "\t"
	   << "Estimated View [x]"
	   << "\t"
	   << "Estimated View [y]"
	   << "\t"
	   << "Estimated View [z]"
	   << "\t"
	   << "Estimated Up [x]"
	   << "\t"
	   << "Estimated Up [y]"
	   << "\t"
	   << "Estimated Up [z]"
	   << "\t"
	   << "Input Orientation [x]"
	   << "\t"
	   << "Input Orientation [y]"
	   << "\t"
	   << "Input Orientation [z]"
	   << "\t"
	   << "Input Orientation [q]"
	   << "\t"
	   << "Num involved keys [n]" << std::endl;
	return os;
}

std::ostream& CVABasicMotionModel::MotionLogDataOutput::outputData( std::ostream& os ) const
{
	os.precision( 12 );
	os << dTime << "\t";
	os << vPos.x << "\t" << vPos.y << "\t" << vPos.z << "\t";
	os << vView.x << "\t" << vView.y << "\t" << vView.z << "\t";
	os << vUp.x << "\t" << vUp.y << "\t" << vUp.z << "\t";
	os << qOrient.x << "\t" << qOrient.y << "\t" << qOrient.z << "\t" << qOrient.w << "\t";
	os << iNumInvolvedKeys << std::endl;
	return os;
}

std::ostream& CVABasicMotionModel::MotionLogDataInput::outputDesc( std::ostream& os )
{
	os << "Input time"
	   << "\t"
	   << "Input Position [x]"
	   << "\t"
	   << "Input Position [y]"
	   << "\t"
	   << "Input Position [z]"
	   << "\t"
	   << "Input View [x]"
	   << "\t"
	   << "Input View [y]"
	   << "\t"
	   << "Input View [z]"
	   << "\t"
	   << "Input Up [x]"
	   << "\t"
	   << "Input Up [y]"
	   << "\t"
	   << "Input Up [z]"
	   << "\t"
	   << "Input Orientation [x]"
	   << "\t"
	   << "Input Orientation [y]"
	   << "\t"
	   << "Input Orientation [z]"
	   << "\t"
	   << "Input Orientation [q]"
	   << "\t"
	   << "Placeholder" << std::endl;
	return os;
}

std::ostream& CVABasicMotionModel::MotionLogDataInput::outputData( std::ostream& os ) const
{
	os.precision( 12 );
	os << dTime << "\t";
	os << vPos.x << "\t" << vPos.y << "\t" << vPos.z << "\t";
	os << vVel.x << "\t" << vVel.y << "\t" << vVel.z << "\t";
	os << vView.x << "\t" << vView.y << "\t" << vView.z << "\t";
	os << vUp.x << "\t" << vUp.y << "\t" << vUp.z << "\t";
	os << qOrient.x << "\t" << qOrient.y << "\t" << qOrient.z << "\t" << qOrient.w << "\t";
	os << 0.0f << std::endl;
	return os;
}
