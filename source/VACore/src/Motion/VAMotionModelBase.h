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

#ifndef IW_VACORE_MOTIONMODELBASE
#define IW_VACORE_MOTIONMODELBASE

#include "VAMotionModel.h"

#include <ITADataLog.h>
#include <VABase.h>
#include <VAException.h>
#include <VAStruct.h>
#include <atomic>
#include <vector>

//! Klasse zur Bewegungsschätzung
/**
 * Diese Klasse implementiert die Grundfunktionalität eines Bewegungsschätzers
 * durch Speichern vorangegangener Positionen.
 * Bewegung wird aus Gewichtung der Historie durch Dreiecksfunktion erreicht.
 *
 * \note ACHTUNG: Das Model ist nicht thread-safe!
 */
class CVABasicMotionModel : public IVAMotionModel
{
public:
	class Config
	{
	public:
		int iNumHistoryKeys; //!< Number of keys to be stored (size of ring buffer)

		bool bLogInputEnabled;           //!< Input data stream logging enabled
		bool bLogEstimatedOutputEnabled; //!< Input data stream logging enabled

		double dWindowSize;  //!< Size of weighting window [s]
		double dWindowDelay; //!< Delay of weighting window [s]

		inline void SetDefaults( )
		{
			bLogEstimatedOutputEnabled = false;
			bLogInputEnabled           = false;
			iNumHistoryKeys            = 100;
			dWindowDelay               = 0.1;
			dWindowSize                = 0.2;
		}
	};

	CVABasicMotionModel( const Config& oConf );
	~CVABasicMotionModel( );

	//! Returns number of history keys
	int GetNumHistoryKeys( ) const;

	//! Adds new history item at time of given motion state creation
	void InputMotionKey( const CVAMotionState* pNewState );

	//! Adds new history item at time of given motion state creation
	void InputMotionKey( double dTime, const VAVec3& v3Pos, const VAQuat& qOrient );

	//! Estimates motion at given time
	/**
	 * \param dTime Time of estimated position
	 * \param v3Pos Position vector
	 * \param qOrient Orientation quaternion
	 *
	 * \return True, if estimation possible, false if no data available (no motion key available)
	 *
	 * \note dTime is strictly monotously increasings
	 */
	bool EstimateMotion( double dTime, VAVec3& v3Pos, VAQuat& qOrient );

	//! Estimates a position at given time (please use EstimateMotion to get both position and orientation)
	/**
	 * \param dTime Time of estimated position
	 * \param v3Pos Position vector
	 *
	 * \return True, if estimation possible, false if no data available (no motion key available)
	 *
	 * \note dTime is strictly monotously increasings
	 */
	bool EstimatePosition( double dTime, VAVec3& v3Pos );

	//! Estimate orientation only for given time (please use EstimateMotion to get both position and orientation)
	/**
	 * \param dTime Time of estimated position
	 * \param v3View View vector
	 * \param v3Up Up vector
	 *
	 * \return True, if estimation possible, false if no data available (no motion key available)
	 *
	 * \note dTime is strictly monotously increasings
	 */
	bool EstimateOrientation( double dTime, VAVec3& v3View, VAVec3& v3Up );

	//! Estimate orientation only for given time (please use EstimateMotion to get both position and orientation)
	/**
	 * \param dTime Time of estimated position
	 * \param qOrient Orientation quaternion
	 *
	 * \return True, if estimation possible, false if no data available (no motion key available)
	 *
	 * \note dTime is strictly monotously increasings
	 */
	bool EstimateOrientation( double dTime, VAQuat& qOrient );

	void Reset( );

	//! Sets the name of the instance (used for logging file name)
	void SetName( const std::string& sNewName );

	//! Update window delay
	void UpdateWindowDelay( double dNewDelay );

	//! Update window size
	void UpdateWindowSize( double dNewSize );

protected:
	//! Data class of motion keys
	class CMotionKey
	{
	public:
		double t; //!< Time [s]

		VAVec3 p; //!< Position vector [m]
		VAVec3 v; //!< Velocity vector [m/s]
		VAVec3 a; //!< Acceleration vector [m/s²]
		VAQuat q; //!< Orientation as quaternion

		VAVec3 px; //!< Buffer for extrapolated position [m]
		double w;  //!< Buffer for weight
	};

	//! Cursor computation relative to current head element (Lookback=0 => Last key added)
	CMotionKey* GetHistoryKey( int iLookback );

	//! Calculates weighting based on window parameters
	/**
	 * \param t Time relative to window center
	 * \return Weighting factor of window (not normalized)
	 */
	double ComputeWeight( double t ) const;

	double m_dLastQueryTime; //!< Stores the last query time

	int m_iNumKeys;                  //!< Number of stored keys in the ring buffer
	int m_iTail;                     //!< Last inserted element in the ring buffer
	std::vector<CMotionKey> m_vKeys; //!< Ring buffer for keys

	//! Implementierungsklasse für Logger-Datum
	class MotionLogDataOutput : public ITALogDataBase
	{
	public:
		static std::ostream& outputDesc( std::ostream& os );
		std::ostream& outputData( std::ostream& os ) const;
		std::string sName; //!< Name um die Daten einem Objekt zuordnen zu können
		double dTime;
		VAVec3 vPos, vView, vUp;
		VAQuat qOrient;
		int iNumInvolvedKeys;
	};

	ITABufferedDataLogger<MotionLogDataOutput> m_oEstimationDataLog; //!< Logger Datum für VDL spezifische Prozess-Information

	//! Implementierungsklasse für Logger-Datum
	class MotionLogDataInput : public ITALogDataBase
	{
	public:
		static std::ostream& outputDesc( std::ostream& os );
		std::ostream& outputData( std::ostream& os ) const;
		std::string sName; //!< Name um die Daten einem Objekt zuordnen zu können
		double dTime;
		VAVec3 vPos;
		VAVec3 vVel;
		VAVec3 vView;
		VAVec3 vUp;
		VAQuat qOrient;
	};

	ITABufferedDataLogger<MotionLogDataInput> m_oInputDataLog; //!< Logger Datum für VDL spezifische Prozess-Information

private:
	const Config m_oConf;
	std::atomic<double> m_dWindowDelay;
	std::atomic<double> m_dWindowSize;
	inline CVABasicMotionModel& operator=( const CVABasicMotionModel& ) { VA_EXCEPT_NOT_IMPLEMENTED; };
};

#endif // IW_VACORE_MOTIONMODELBASE
