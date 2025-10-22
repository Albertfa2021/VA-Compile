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

#include "VASamples.h"

#include <VAException.h>
#include <assert.h>

CVASampleBuffer::CVASampleBuffer( ) {}

CVASampleBuffer::~CVASampleBuffer( ) {}

CVASampleBuffer::CVASampleBuffer( const int iNumSamples, const bool bZeroInit )
{
	if( iNumSamples <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Sample frame requires at least one sample" );
	vfSamples.resize( iNumSamples );

	if( bZeroInit )
		SetZero( );
}

CVASampleBuffer::CVASampleBuffer( const CVASampleBuffer& oOther )
{
	this->vfSamples = oOther.vfSamples;
}

void CVASampleBuffer::SetZero( )
{
	for( size_t i = 0; i < vfSamples.size( ); i++ )
		vfSamples[i] = 0.0f;
}

CVASampleBuffer& CVASampleBuffer::operator=( const CVASampleBuffer& oOther )
{
	vfSamples = oOther.vfSamples;
	return *this;
}

float* CVASampleBuffer::GetData( )
{
	if( GetNumSamples( ) <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Sample frame requires at least one sample" );
	return &( vfSamples[0] );
}

const float* CVASampleBuffer::GetDataReadOnly( ) const
{
	if( GetNumSamples( ) <= 0 )
		VA_EXCEPT2( INVALID_PARAMETER, "Sample frame requires at least one sample" );
	return &( vfSamples[0] );
}

int CVASampleBuffer::GetNumSamples( ) const
{
	return int( vfSamples.size( ) );
}
