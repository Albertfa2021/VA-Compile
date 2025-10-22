#include <VASceneControlCommand.h>

CVACommand::CVACommand( ) : iCommand( -1 ) { };
CVACommand ~CVACommand( ) { };

void CVACommand::Exec( IVASceneControlInterface* pServiceProvider )
{
	switch( iCommand )
	{
		case VA_COMMAND_SETSOUNDSOURCEAURALIZATIONMODE:
			pServiceProvider->SetSoundSourceAuralizationMode( iObjectID, iParam );
			break;

		case VA_COMMAND_SETSOUNDSOURCEVOLUME:
			pServiceProvider->SetSoundSourceVolume( iObjectID, dParam );
			break;

		case VA_COMMAND_SETSOUNDSOURCEMUTED:
			pServiceProvider->SetSoundSourceMuted( iObjectID, bParam );
	};
}

void CVACommand::SetSoundSourceAuralizationMode( int iSoundSourceID, int iAuralizationMode )
{
	iCommand = VA_COMMAND_SETSOUNDSOURCEAURALIZATIONMODE;
	iObject  = iSoundSourceID;
	iParam   = iAuralizationMode;
}

void CVACommand::SetSoundSourceVolume( int iSoundSourceID, double dGain )
{
	iCommand = VA_COMMAND_SETSOUNDSOURCEVOLUME;
	iObject  = iSoundSourceID;
	dParam   = dGain;
}

void CVACommand::SetSoundSourceMuted( int iSoundSourceID, bool bMuted )
{
	iCommand = VA_COMMAND_SETSOUNDSOURCEMUTED;
	iObject  = iSoundSourceID;
	bParam   = bMuted;
}