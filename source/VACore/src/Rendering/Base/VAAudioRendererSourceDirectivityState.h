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

#ifndef IW_VACORE_RENDERERSOURCEDIRECTIVITYSTATE
#define IW_VACORE_RENDERERSOURCEDIRECTIVITYSTATE

// VA forwards
class IVADirectivity;

struct CVARendererSourceDirectivityState
{
	CVARendererSourceDirectivityState( ) : pData( nullptr ), iRecord( -1 ), bDirectivityEnabled( true ) { };

	IVADirectivity* pData; /// Directivity data, may be a nullptr
	int iRecord;           /// Directivity index
	bool bDirectivityEnabled;

	bool operator==( const CVARendererSourceDirectivityState& rhs ) const
	{
		const bool bBothEnabled     = ( bDirectivityEnabled == rhs.bDirectivityEnabled );
		const bool bSameRecordIndex = ( iRecord == rhs.iRecord );
		const bool bSameData        = ( pData == rhs.pData );

		return ( bBothEnabled && bSameRecordIndex && bSameData );
	};
	bool operator!=( const CVARendererSourceDirectivityState& rhs ) const { return !operator==( rhs ); };
};

#endif // IW_VACORE_RENDERERSOURCEDIRECTIVITYSTATE
