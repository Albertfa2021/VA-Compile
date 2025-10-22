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


#ifndef IW_VACORE_DATA_HISTORY_MODEL
#define IW_VACORE_DATA_HISTORY_MODEL

// VA
#include "VADataHistoryConfig.h"


// STL
#include <atomic>
#include <memory>
#include <vector>

// External
#include <tbb/concurrent_queue.h>


//! Abstract template class for a thread-safe filter property data history
/**
 * Allows to store data of a template type together with a timestamp in a buffer.
 * Thread-safety is assured if using one thread write-only and the other one read-only.
 * Write-only: Push items to queue via Push()
 * Read-only: Update buffer (pop from queue and insert into buffer) via Update() and use Estimate() to get data at a certain time stamp
 */
template<class DataType>
class CVADataHistoryModel
{
public:
	typedef std::unique_ptr<DataType> ConstDataPtr;

	CVADataHistoryModel( const CVADataHistoryConfig& oConf, CVAHistoryEstimationMethod::EMethod eMethod );
	CVADataHistoryModel( int iBufferSize, CVAHistoryEstimationMethod::EMethod eMethod, double dWindowSize = VADataHistory_DefaultWindowSize,
	                     double dWindowDelay = VADataHistory_DefaultWindowDelay );

	//! Resets the cursors of the ring buffer to make it virtually empty again
	/**
	 * Note: The data is not deleted yet but won't be used anymore
	 */
	void Reset( );

	//! Estimates the data at a given point in time. To be overloaded by derived classes.
	virtual bool Estimate( const double& dTime, DataType& oDataOut ) = 0;

	//! Returns number of history keys
	inline int GetNumHistorySamples( ) const { return m_iNumSamples; };
	//! Returns the size of the data ring-buffer
	inline int GetBufferSize( ) const { return (int)m_vpSamples.size( ); };
	//! Returns the method used for interpolating the data
	inline CVAHistoryEstimationMethod GetEstimationMethod( ) const { return m_oEstimationMethod; };
	//! Returns the time stamp of last queued sample. If no sample added yet, this returns -1.
	inline double GetLastPushedTimestamp( ) const { return m_dLastTimestamp; };
	//! Returns the last time stamp in the actual Buffer. If no sample added yet, this returns -1.
	double GetLastTimestamp( ) const;
	;

	//! Returns true if a given timestamp is newer then the latest sample and can therefore be inserted.
	bool CheckTimestampValidity( const double& dTimestamp );

	//! Queues a new sample to this history using a given time stamp. Actual insertion into buffer is then achieved calling Update().
	void Push( const double& dTimestamp, ConstDataPtr pNewData );

	/*! Inserts all queued samples into the history and deletes all samples before given timestamp except for the newest one
	 *  If time limit is negative no sample will be removed (= default)
	 */
	void Update( const double& dLowerTimeLimit = -1 );

	//@todo psc: implement this
	void SetBufferSize( int iNewSize );

protected:
	//! A sample of this history holding a timestamp and a data object
	struct CHistorySample
	{
		const double dTime = -1.0;    //!< Time [s]
		ConstDataPtr pData = nullptr; //!< Pointer to constant filter property
		double dWeight     = 0.0;     //!< Weight used for sliding window

		CHistorySample( const double& dTimestamp, ConstDataPtr pData );

		inline bool operator<( const CHistorySample& oOther ) { dTime < oOther.dTime; };
		inline bool operator>( const CHistorySample& oOther ) { dTime > oOther.dTime; };
	};
	typedef std::unique_ptr<CHistorySample> HistorySamplePtr;

	//! Actually inserts samples into the history
	void InsertSample( HistorySamplePtr );
	//! Removes all samples before given time stamp except for the newest one
	void RemoveOldSamples( const double& dLowerTimeLimit );

	/*! Gets the index N samples before the tail of the ring buffer (Lookback=0 => Index of last sample added)
	 *  Note: Shift must not exceed the buffer size!
	 */
	int GetIdxPerLookback( int iLookback ) const;
	/*! Gets the index N samples after the front of the ring buffer (Lookforward=0 => Index of oldest sample)
	 *  Note: Shift must not exceed the buffer size!
	 */
	int GetIdxPerLookforward( int iLookforward ) const;
	/*! Returns the index that is iShift elements away from idxStart. Both, positive or negative shitfs are allowed.
	 *  Note: Shift must not exceed the buffer size!
	 */
	int IterateIdx( int idxStart, int iShift ) const;
	//! Advances to the given index to the next element in the buffer
	int IncrementIdx( int idx ) const;
	//! Advances to the given index to the previous element in the buffer
	int DecrementIdx( int idx ) const;

	/*! \brief Finds the indices of the samples that surround the given timestamp
	 *  The index will be -1, if a boundary does not exist. Both -1 means, that the history is empty.
	 */
	void BoundaryIndices( const double& dTime, int& iLeft, int& iRight );

	//! \brief Sets the given data object to the closest value before the given time stamp
	//! \return true on success, false if no data is available or timestamp is before first data point
	bool SampleAndHold( const double& dTime, DataType& oDataOut );

	//! \brief Sets the given data object to the nearest neighbor of given time stamp
	//! \return true on success, false if no data is available or timestamp is before first data point
	bool NearestNeighbor( const double& dTime, DataType& oDataOut );

	/*! \brief Does a linear interpolation to the given timestamp and stores it in given data object
	 *  \return true on success, false if no data is available or timestamp if before first data point
	 *  Note: In order to use this, your template class "DataType" must have defined the following operators:
	 *  DataType = DataType; DataType += DataType; DataType -= DataType; DataType *= double
	 */
	bool LinearInterpolation( const double& dTime, DataType& oDataOut );

	/*! \brief Estimates the data for given timestamp using a sliding triangular window
	 *  \return true on success, false if no data is available or timestamp if before first data point
	 *  Note: In order to use this, your template class "DataType" must have defined the following operators:
	 *  DataType = DataType; DataType += DataType; float * DataType
	 */
	bool TriangularWindow( const double& dTime, DataType& oDataOut );

	int m_iNumSamples = 0;                     //!< Number of stored samples in the ring buffer
	int m_iFront      = 0;                     //!< Index of oldest, still available sample in the ring buffer
	int m_iTail       = -1;                    //!< Index of last inserted sample in the ring buffer
	std::vector<HistorySamplePtr> m_vpSamples; //!< Ring buffer for data samples

	CVAHistoryEstimationMethod m_oEstimationMethod; //!< Defines what algorithm is used for interpolation between data samples
	bool m_bLogInputEnabled  = false;
	bool m_bLogOutputEnabled = false;

private:
	tbb::concurrent_queue<HistorySamplePtr> m_qpNewSamples; //!< Queue for thread-safe, asynchronous adding of data

	double m_dLastTimestamp = -1;

	std::atomic<double> m_dWindowSize;
	std::atomic<double> m_dWindowDelay;

	void RemoveOldSamplesInterpolation( const double& dLowerTimeLimit );
	void RemoveOldSamplesMovingWindow( const double& dLowerTimeLimit );
	void RemoveFront( );

	//! Calculates weighting based on parameters for triangular window
	/**
	 * \param dt Time relative to window center
	 * \return Weighting factor of window (not normalized)
	 */
	double ComputeWeight( double dt ) const;

	// ---LOGGING---
	// Implement these methods in derived classes to allow for logging
protected:
	virtual void LogInputData( const double& dTime, const DataType& oData );
	virtual void LogOutputData( const double& dTime, const DataType& oData );
};

#endif // IW_VACORE_DATA_HISTORY_MODEL