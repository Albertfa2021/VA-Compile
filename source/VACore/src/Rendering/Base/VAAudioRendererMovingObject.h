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

#ifndef IW_VACORE_AUDIORENDERER_MOVINGOBJECT
#define IW_VACORE_AUDIORENDERER_MOVINGOBJECT

// VA includes
#include "../../Motion/VAMotionModelBase.h"
#include "../../Motion/VASharedMotionModel.h" //Required for unique_ptr

#include <VA.h>
#include <VAPoolObject.h>

#include <memory>
#include <string>

// VA forwards
class CVASharedMotionModel;
class CVAMotionState;

/// Base class for moving objects during rendering, namely sources and receivers
class CVARendererMovingObject : public CVAPoolObject
{
private:
	const CVABasicMotionModel::Config oConf;            /// Configuration of motion model
	std::unique_ptr<CVASharedMotionModel> pMotionModel; /// Pointer to motion model
	bool bDeleted = false;                              /// Indicates whether object got deleted
	VAVec3 vPredPos;                                    /// Estimated position
	VAVec3 vPredView;                                   /// Estimated Orientation (View-Vektor)
	VAVec3 vPredUp;                                     /// Estimated Orientation (Up-Vektor)
	bool bValidTrajectoryPresent = false;               /// Estimation possible -> valid trajectory present

public:
	CVARendererMovingObject( const CVABasicMotionModel::Config& oConf_ ) : oConf( oConf_ ) { };
	/// Pool-Constructor: Initializes motion model
	virtual void PreRequest( )
	{
		//TODO: Should we rather reset the MotionModel instead of creating it every time?
		pMotionModel            = std::make_unique<CVASharedMotionModel>( new CVABasicMotionModel( oConf ), true );
		bValidTrajectoryPresent = false;
		bDeleted                = false;
	};
	/// Pool-Destructor: Deletes motion model and marks object as deleted
	virtual void PreRelease( )
	{
		pMotionModel = nullptr; //TODO: see above
	};

	/// Indicates whether the object is marked for deletion
	bool IsDeleted( ) const { return bDeleted; };
	/// Indicates whether object has a valid pose
	bool HasValidTrajectory( ) const { return bValidTrajectoryPresent; };
	/// Returns the position predicted by the motion model (set during EstimateCurrentPose())
	const VAVec3& PredictedPosition( ) const { return vPredPos; };
	/// Returns the view vector predicted by the motion model (set during EstimateCurrentPose())
	const VAVec3& PredictedViewVec( ) const { return vPredView; };
	/// Returns the up vector predicted by the motion model (set during EstimateCurrentPose())
	const VAVec3& PredictedUpVec( ) const { return vPredUp; };

	/// Marks the object for deletion (=> IsDeleted() == true)
	void MarkDeleted( ) { bDeleted = true; };
	/// Adds a new key to the motion model
	void InsertMotionKey( const CVAMotionState* pMotionState );
	/// Estimates the current position, view and up vector for given time stamp
	void EstimateCurrentPose( double dTime );
	/// Sets the name of the motion model which is used for data logging
	void SetMotionModelName( const std::string& sName );
};

#endif // IW_VACORE_AUDIORENDERER_MOVINGOBJECT
