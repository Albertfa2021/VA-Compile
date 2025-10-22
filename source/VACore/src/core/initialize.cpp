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

#include "core.h"

void CVACoreImpl::Initialize( )
{
	VA_NO_REENTRANCE;

	// TODO: Prüfen ob im Fehlerfall zurück in den sauberen Grundzustand [WICHTIG!]

	VA_VERBOSE( "Core", "Initializing core" );

	VA_TRY
	{
		if( m_iState == VA_CORESTATE_READY )
			VA_EXCEPT2( MODAL_ERROR, "Core already initialized." );

		if( m_iState == VA_CORESTATE_FAIL )
			VA_EXCEPT2( MODAL_ERROR, "Core corrupted, reinitialization impossible" );

		m_pCoreThread = new CVACoreThread( this );

		SetProgress( "Setting up audio hardware", "", 1 );
		InitializeAudioDriver( );

		m_pR2RPatchbay = new ITAStreamPatchbay( m_oCoreConfig.oAudioDriverConfig.dSampleRate, m_oCoreConfig.oAudioDriverConfig.iBuffersize );

		// Create output patch bay with a single output that uses all available physical audio outputs from sound card
		m_pOutputPatchbay           = new ITAStreamPatchbay( m_oCoreConfig.oAudioDriverConfig.dSampleRate, m_oCoreConfig.oAudioDriverConfig.iBuffersize );
		int iPhysicalHardwareOutput = m_pOutputPatchbay->AddOutput( m_oCoreConfig.oAudioDriverConfig.iOutputChannels );
		m_pOutputPatchbay->SetOutputGain( iPhysicalHardwareOutput, m_dOutputGain );

		m_iGlobalAuralizationMode = VA_AURAMODE_ALL;

		// Set up input stream network
		ITADatasource* pInputTail = nullptr;
		if( m_oCoreConfig.oAudioDriverConfig.iInputChannels > 0 )
		{
			pInputTail = m_pAudioDriverBackend->getInputStreamDatasource( );
			if( pInputTail )
			{
				m_pInputAmp            = new ITAStreamAmplifier( pInputTail, (float)m_dInputGain );
				m_pInputStreamDetector = new ITAStreamDetector( m_pInputAmp );
				m_pInputStreamDetector->SetProfilerEnabled( true );
				pInputTail = m_pInputStreamDetector;

				if( m_oCoreConfig.bRecordDeviceInputEnabled )
				{
					VistaFileSystemFile oRecordFile( m_oCoreConfig.sRecordDeviceInputFileName );
					VistaFileSystemDirectory oRecordBaseFolder( m_oCoreConfig.sRecordDeviceInputBaseFolder );

					if( !oRecordBaseFolder.Exists( ) )
					{
						if( oRecordBaseFolder.CreateWithParentDirectories( ) )
						{
							VA_INFO( "Core", "Created device input record base folder " << oRecordBaseFolder.GetName( ) << " with parent directories" );
						}
						else
						{
							VA_EXCEPT2( INVALID_PARAMETER, "Could not create non-existent device input record base folder '" + oRecordBaseFolder.GetName( ) + "'" );
						}
					}

					std::string sFilePath = oRecordBaseFolder.GetName( ) + "/" + oRecordFile.GetLocalName( );
					if( VistaFileSystemFile( sFilePath ).Exists( ) )
						VA_INFO( "Core", "Device input record file '" << sFilePath << "' exists, will overwrite" );

					m_pStreamProbeDeviceInput = new ITAStreamProbe( pInputTail, sFilePath );
					pInputTail                = m_pStreamProbeDeviceInput;
				}
			}
		}

		SetProgress( "Setting up resource managers", "", 2 );

		assert( m_oCoreConfig.oAudioDriverConfig.iInputChannels >= 0 );
		m_pSignalSourceManager = new CVAAudioSignalSourceManager( this, m_oCoreConfig.oAudioDriverConfig, pInputTail );
#ifdef VACORE_WITH_SAMPLER_SUPPORT
		m_pGlobalSamplePool = ITASoundSamplePool::Create( 1, m_oCoreConfig.oAudioDriverConfig.dSampleRate );
		m_pGlobalSampler = ITASoundSampler::Create( 1, m_oCoreConfig.oAudioDriverConfig.dSampleRate, m_oCoreConfig.oAudioDriverConfig.iBuffersize, m_pGlobalSamplePool );
		m_pGlobalSampler->AddMonoTrack( );
#else
		m_pGlobalSamplePool = nullptr;
		m_pGlobalSampler    = nullptr;
#endif

		m_pDirectivityManager = new CVADirectivityManager( this, m_oCoreConfig.oAudioDriverConfig.dSampleRate );
		m_pDirectivityManager->Initialize( );


		SetProgress( "Setting up scene management", "", 3 );

		m_pSceneManager = new CVASceneManager( m_pClock );
		m_pSceneManager->Initialize( );
		m_pCurSceneState = m_pSceneManager->GetHeadSceneState( );

		SetProgress( "Setting up medium environment", "", 4 );

		oHomogeneousMedium = m_oCoreConfig.oInitialHomogeneousMedium;


		SetProgress( "Initializing rendering modules", "", 5 );

		// Register all renderers and initialize
		CVAAudioRendererRegistry::GetInstance( )->RegisterInternalCoreFactoryMethods( );
		InitializeAudioRenderers( );

		if( m_voRenderers.empty( ) )
			VA_EXCEPT1( "No audio renderers created" );


		SetProgress( "Initializing reproduction modules", "", 6 );

		// Register all reproductions and initialize
		CVAAudioReproductionRegistry::GetInstance( )->RegisterInternalCoreFactoryMethods( );
		InitializeReproductionModules( );

		if( m_voReproductionModules.empty( ) )
			VA_EXCEPT1( "No audio reproduction modules created" );


		SetProgress( "Patching audio i/o of rendering and reproduction modules", "", 7 );

		// Patch renderer and reproduction modules
		PatchRendererToReproductionModules( );

		// Patch audio reproduction to output
		PatchReproductionModulesToOutput( );


		// Create output peak detector that uses patch bay output stream

		m_pOutputStreamDetector = new ITAStreamDetector( m_pOutputPatchbay->GetOutputDatasource( iPhysicalHardwareOutput ) );
		m_pOutputStreamDetector->SetProfilerEnabled( true );


		// Setup output dump (if set)
		ITADatasource* pOutputTail = m_pOutputStreamDetector;
		if( m_oCoreConfig.bRecordDeviceOutputEnabled )
		{
			VistaFileSystemFile oRecordFile( m_oCoreConfig.sRecordDeviceOutputFileName );
			VistaFileSystemDirectory oRecordBaseFolder( m_oCoreConfig.sRecordDeviceOutputBaseFolder );

			if( !oRecordBaseFolder.Exists( ) )
			{
				if( oRecordBaseFolder.CreateWithParentDirectories( ) )
				{
					VA_INFO( "Core", "Created device output record base folder " << oRecordBaseFolder.GetName( ) << " with parent directories" );
				}
				else
				{
					VA_EXCEPT2( INVALID_PARAMETER, "Could not create non-existent device output record base folder '" + oRecordBaseFolder.GetName( ) + "'" );
				}
			}

			std::string sFilePath = oRecordBaseFolder.GetName( ) + "/" + oRecordFile.GetLocalName( );
			if( VistaFileSystemFile( sFilePath ).Exists( ) )
				VA_INFO( "Core", "Device output record file '" << sFilePath << "' exists, will overwrite" );

			m_pStreamProbeDeviceOutput = new ITAStreamProbe( pOutputTail, sFilePath );
			pOutputTail                = m_pStreamProbeDeviceOutput;
		}

		// Attach the stream tracker
		m_pOutputTracker = new CVAAudiostreamTracker( pOutputTail, m_pClock, &m_fCoreClockOffset, &m_lSyncModOwner, m_pSignalSourceManager );
		pOutputTail      = m_pOutputTracker;

		// Give output stream datasource to audio driver
		m_pAudioDriverBackend->setOutputStreamDatasource( pOutputTail );

		// Core-Clock auf 0 initialisieren
		double dNow          = m_pClock->getTime( );
		m_fCoreClockOffset   = (float)dNow;
		m_dStreamClockOffset = -1;

		// Timer erzeugen und konfigurieren (wird für Peak-Events benutzt)
		m_pTicker = new VistaTicker( );
		m_pTicker->AddTrigger( new VistaTicker::TriggerContext( m_oCoreConfig.iTriggerUpdateMilliseconds, true ) );
		m_pTicker->SetAfterPulseFunctor( this );

		// Audio-Streaming starten
		SetProgress( "Starting audio streaming", "", 8 );
		m_pAudioDriverBackend->startStreaming( );

		// Timer für Peak-Events starten
		m_pTicker->StartTicker( );

		// Initialisierung erfolgreich!
		m_iState = VA_CORESTATE_READY;

		SetProgress( "Initialization finished", "", 9 );
		FinishProgress( );
	}
	VA_FINALLY
	{
		// Aufräumen und Exception weiterwerfen
		Tidyup( );
		throw;
	}
}

void CVACoreImpl::InitializeAudioDriver( )
{
#ifdef VACORE_WITH_AUDIO_BACKEND_ASIO
	if( m_oCoreConfig.oAudioDriverConfig.sDriver == "ASIO" )
		m_pAudioDriverBackend = new CVAASIOBackend( &m_oCoreConfig.oAudioDriverConfig );
#endif
#ifdef VACORE_WITH_AUDIO_BACKEND_PORTAUDIO
	if( m_oCoreConfig.oAudioDriverConfig.sDriver == "Portaudio" )
		m_pAudioDriverBackend = new CVAPortaudioBackend( &m_oCoreConfig.oAudioDriverConfig );
#endif
#ifdef VACORE_WITH_AUDIO_BACKEND_VIRTUAL
	if( m_oCoreConfig.oAudioDriverConfig.sDriver == "Virtual" )
	{
		if( m_oCoreConfig.oAudioDriverConfig.iBuffersize == -1 )
			VA_EXCEPT2( INVALID_PARAMETER, "For a virtual audio device, the buffer size has to be set (AUTO detect not possible)" );

		if( m_oCoreConfig.oAudioDriverConfig.iOutputChannels == -1 )
			VA_EXCEPT2( INVALID_PARAMETER, "For a virtual audio device, the output channel number has to be set (AUTO detect not possible)" );

		m_oCoreConfig.oAudioDriverConfig.iInputChannels = 0; // not allowed, override

		CVAVirtualAudioDriverBackend* pAudioDriverBackend = new CVAVirtualAudioDriverBackend( &m_oCoreConfig.oAudioDriverConfig );
		RegisterModule( pAudioDriverBackend );
		m_pAudioDriverBackend = pAudioDriverBackend;

		// Overwride default block pointer by manual clock
		CVAVirtualAudioDriverBackend::ManualClock* pManualClock = new CVAVirtualAudioDriverBackend::ManualClock( );
		RegisterModule( pManualClock );
		m_pClock = pManualClock;
	}
#else
#endif

	if( m_pAudioDriverBackend == nullptr )
		VA_EXCEPT2( INVALID_PARAMETER, "Unkown, uninitializable or unsupported audio driver backend '" + m_oCoreConfig.oAudioDriverConfig.sDriver + "'" );

	try
	{
		VA_INFO( "Core",
		         "Initializing audio device '" << m_pAudioDriverBackend->getDeviceName( ) << "' using '" << m_pAudioDriverBackend->getDriverName( ) << "' driver" );

		VA_TRACE( "Core", "Desired settings: sampling rate "
		                      << m_oCoreConfig.oAudioDriverConfig.dSampleRate << " Hz, "
		                      << ( m_oCoreConfig.oAudioDriverConfig.iInputChannels == CVAAudioDriverConfig::AUTO ?
                                       "all" :
                                       std::to_string( m_oCoreConfig.oAudioDriverConfig.iInputChannels ) )
		                      << " inputs, "
		                      << ( m_oCoreConfig.oAudioDriverConfig.iOutputChannels == CVAAudioDriverConfig::AUTO ?
                                       "all" :
                                       std::to_string( m_oCoreConfig.oAudioDriverConfig.iOutputChannels ) )
		                      << " outputs, "
		                      << "buffer size = "
		                      << ( m_oCoreConfig.oAudioDriverConfig.iBuffersize == 0 ? "auto" : std::to_string( m_oCoreConfig.oAudioDriverConfig.iBuffersize ) ) );

		m_pAudioDriverBackend->initialize( );

		m_oCoreConfig.oAudioDriverConfig.dSampleRate     = m_pAudioDriverBackend->getOutputStreamProperties( )->dSamplerate;
		m_oCoreConfig.oAudioDriverConfig.iInputChannels  = m_pAudioDriverBackend->getNumberOfInputs( );
		m_oCoreConfig.oAudioDriverConfig.iOutputChannels = m_pAudioDriverBackend->getOutputStreamProperties( )->uiChannels;
		m_oCoreConfig.oAudioDriverConfig.iBuffersize     = m_pAudioDriverBackend->getOutputStreamProperties( )->uiBlocklength;

		VA_INFO( "Core",
		         "Streaming at " << std::fixed << std::setw( 3 ) << std::setprecision( 1 ) << ( m_oCoreConfig.oAudioDriverConfig.dSampleRate / 1000.0f ) << " kHz on "
		                         << ( m_oCoreConfig.oAudioDriverConfig.iInputChannels == 0 ? "no" : std::to_string( m_oCoreConfig.oAudioDriverConfig.iInputChannels ) )
		                         << " inputs and "
		                         << ( m_oCoreConfig.oAudioDriverConfig.iOutputChannels == 0 ? "no" : std::to_string( m_oCoreConfig.oAudioDriverConfig.iOutputChannels ) )
		                         << " outputs with a buffer size of " << m_oCoreConfig.oAudioDriverConfig.iBuffersize << " samples." );
	}
	catch( ... )
	{
		m_pAudioDriverBackend->finalize( );

		delete m_pAudioDriverBackend;
		m_pAudioDriverBackend = nullptr;

		throw;
	}
}

void CVACoreImpl::InitializeAudioRenderers( )
{
	CVAAudioRendererRegistry* pRegistry( CVAAudioRendererRegistry::GetInstance( ) );
	const CVAStruct& oConfig( GetCoreConfig( )->GetStruct( ) );

	// Parse config for audio renderers and try to instantiate them
	CVAStruct::const_iterator cit = oConfig.Begin( );
	while( cit != oConfig.End( ) )
	{
		const std::string& sKey( cit->first );
		const CVAStruct& oArgs( cit->second );

		std::vector<std::string> vsKeyParts = splitString( sKey, ':' );
		if( vsKeyParts.size( ) == 2 && cit->second.GetDatatype( ) == CVAStructValue::STRUCT )
		{
			std::string sCategory( toUppercase( vsKeyParts[0] ) );
			std::string sID( vsKeyParts[1] );

			if( sCategory == "RENDERER" )
			{
				CVAConfigInterpreter conf( oArgs );
				conf.SetErrorPrefix( "Configuration error in section \"" + cit->first + "\"" );
				std::string sClass;
				conf.ReqString( "Class", sClass );

				bool bEnabled;
				conf.OptBool( "Enabled", bEnabled, true );
				if( !bEnabled )
				{
					cit++;
					continue; // Skip
				}

				// Initialization parameters
				CVAAudioRendererInitParams oParams;
				oParams.sID               = sID;
				oParams.sClass            = sClass;
				oParams.pCore             = this;
				oParams.pConfig           = &cit->second.GetStruct( );
				oParams.bOfflineRendering = ( m_oCoreConfig.oAudioDriverConfig.sDriver == "Virtual" ) ? true : false;

				conf.ReqStringListRegex( "Reproductions", oParams.vsReproductions, "\\s*,\\s*" );
				std::unique( oParams.vsReproductions.begin( ), oParams.vsReproductions.end( ) );

				conf.OptBool( "OutputDetectorEnabled", oParams.bOutputLevelMeterEnabled, false );


				// Set up rendering output recording
				conf.OptBool( "RecordOutputEnabled", oParams.bRecordOutputEnabled, false );
				if( oParams.bRecordOutputEnabled )
				{
					VistaFileSystemFile oRecordOutputFile( "renderer.wav" );
					VistaFileSystemDirectory oRecordOutputBaseFolder( "./" );

					std::string sFilePathRAWDeprecated;
					conf.OptString( "RecordOutputFilePath", sFilePathRAWDeprecated );
					if( !sFilePathRAWDeprecated.empty( ) )
					{
						VA_WARN( "Core",
						         "The renderer configuration key 'RecordOutputFilePath' is deprecated. Use 'RecordOutputBaseFolder' (optional) and "
						         "'RecordOutputFileName' instead." );

						std::string sDummy;
						if( conf.OptString( "RecordOutputBaseFolder", sDummy ) || conf.OptString( "RecordOutputFileName", sDummy ) )
							VA_EXCEPT2( INVALID_PARAMETER,
							            "You have combined old rendering configuration key 'RecordOutputFilePath' with one of the new keys 'RecordOutputBaseFolder' "
							            "(optional) or 'RecordOutputFileName'. Please use new key only." );

						std::string sRecordOutputFilePath = m_oCoreConfig.mMacros.SubstituteMacros( sFilePathRAWDeprecated );
						oRecordOutputFile.SetName( sRecordOutputFilePath );
						oRecordOutputBaseFolder.SetName( oRecordOutputFile.GetParentDirectory( ) );
					}
					else
					{
						std::string sFileNameRAW;
						conf.ReqString( "RecordOutputFileName", sFileNameRAW );
						std::string sFileName = m_oCoreConfig.mMacros.SubstituteMacros( sFileNameRAW );
						oRecordOutputFile.SetName( sFileName );

						std::string sBaseFolderRAW;
						conf.ReqString( "RecordOutputBaseFolder", sBaseFolderRAW );
						std::string sBaseFolder = m_oCoreConfig.mMacros.SubstituteMacros( sBaseFolderRAW );
						oRecordOutputBaseFolder.SetName( sBaseFolder );
					}

					if( !oRecordOutputBaseFolder.Exists( ) )
					{
						if( oRecordOutputBaseFolder.CreateWithParentDirectories( ) )
						{
							VA_INFO( "Core", "Created renderer record output base folder " << oRecordOutputBaseFolder.GetName( ) << " with parent directories" );
						}
						else
						{
							VA_EXCEPT2( INVALID_PARAMETER, "Could not create non-existent renderer record base folder '" + oRecordOutputBaseFolder.GetName( ) + "'" );
						}
					}

					oParams.sRecordOutputFileName   = oRecordOutputFile.GetLocalName( );
					oParams.sRecordOutputBaseFolder = oRecordOutputBaseFolder.GetName( );
				}


				// Get factory method to create requested rendering module (if available from registry)
				IVAAudioRendererFactory* pFactory = pRegistry->FindFactory( sClass );
				if( !pFactory )
					conf.Error( "Unknown class \"" + sClass + "\"" );

				IVAAudioRenderer* pRenderer = pFactory->Create( oParams );
				// TODO: Active umsetzen

				CVAAudioRendererDesc oRendererDesc( pRenderer );
				oRendererDesc.sID      = sID;
				oRendererDesc.sClass   = sClass;
				oRendererDesc.bEnabled = bEnabled;

				ITADatasource* pRendererOutputTail = pRenderer->GetOutputDatasource( );
				;
				if( oParams.bRecordOutputEnabled )
				{
					std::string sFilePath = oParams.sRecordOutputBaseFolder + "/" + oParams.sRecordOutputFileName;

					if( VistaFileSystemFile( sFilePath ).Exists( ) )
						VA_INFO( "Core", "Rendering record file '" << sFilePath << "' exists, will overwrite" );

					VistaFileSystemFile oFile( sFilePath );
					oRendererDesc.pOutputRecorder = new ITAStreamProbe( pRendererOutputTail, oFile.GetName( ) );
					pRendererOutputTail           = oRendererDesc.pOutputRecorder;
					VA_TRACE( "Core", "Rendering module will record output to file '" << oFile.GetName( ) << "'" );
				}

				if( oParams.bOutputLevelMeterEnabled )
				{
					oRendererDesc.pOutputDetector = new ITAStreamDetector( pRendererOutputTail );
					pRendererOutputTail           = oRendererDesc.pOutputDetector;
				}

				// Setup the intermediate patchbay input [temporary]
				int iInput = m_pR2RPatchbay->AddInput( pRendererOutputTail );

				// Create direct output in output patchbay for each output group [todo, not allowed yet]
				for( size_t i = 0; i < oParams.vsReproductions.size( ); i++ )
				{
					const std::string& sOutputID( oParams.vsReproductions[i] );

					const CVAHardwareOutput* pOutput = m_oCoreConfig.oHardwareSetup.GetOutput( sOutputID );
					if( pOutput )
					{
						// Output found, use direct out (no renderer)
						VA_EXCEPT2( NOT_IMPLEMENTED,
						            "Direct output to an audio hardware group is currently not supported. Use the Talkthrough reproduction module instead." );
					}
				}

				oRendererDesc.iR2RPatchBayInput = iInput;
				oRendererDesc.vsOutputs         = oParams.vsReproductions;
				m_voRenderers.push_back( oRendererDesc );
			}
		}
		cit++;
	}

	const size_t nNumRenderingModules = m_voRenderers.size( );
	if( nNumRenderingModules == 1 )
	{
		VA_INFO( "Core", "Started one rendering module" );
	}
	else
	{
		VA_INFO( "Core", "Started " << nNumRenderingModules << " rendering modules" );
	}

	for( size_t i = 0; i < nNumRenderingModules; i++ )
	{
		std::string sReproductionFeedStr = ( m_voRenderers[i].vsOutputs.size( ) == 1 ) ?
                                               "one reproduction module." :
                                               std::to_string( long( m_voRenderers[i].vsOutputs.size( ) ) ) + " reproduction modules.";
		VA_INFO( "Core", "    +    " << m_voRenderers[i].sID << " (" << m_voRenderers[i].sClass << ") feeding " << sReproductionFeedStr );
	}
}

void CVACoreImpl::InitializeReproductionModules( )
{
	CVAAudioReproductionRegistry* pRegistry( CVAAudioReproductionRegistry::GetInstance( ) );
	const CVAStruct& oConfig( GetCoreConfig( )->GetStruct( ) );

	// Parse config for reproduction modules and try to instantiate them
	CVAStruct::const_iterator cit = oConfig.Begin( );
	while( cit != oConfig.End( ) )
	{
		const std::string& sKey( cit->first );
		const CVAStruct& oArgs( cit->second );

		std::vector<std::string> vsKeyParts = splitString( sKey, ':' );
		if( vsKeyParts.size( ) == 2 && cit->second.GetDatatype( ) == CVAStructValue::STRUCT )
		{
			std::string sCategory( toUppercase( vsKeyParts[0] ) );

			if( sCategory == "REPRODUCTION" )
			{
				CVAConfigInterpreter conf( oArgs );
				conf.SetErrorPrefix( "Configuration error in section \"" + cit->first + "\"" );

				// Initialization parameters
				CVAAudioReproductionInitParams oParams;
				oParams.pCore   = this;
				oParams.pConfig = &cit->second.GetStruct( );

				oParams.sID = vsKeyParts[1];
				conf.ReqString( "Class", oParams.sClass );

				bool bEnabled;
				conf.OptBool( "Enabled", bEnabled, true );
				if( !bEnabled )
				{
					cit++;
					continue; // Skip this entry
				}

				conf.OptBool( "InputDetectorEnabled", oParams.bInputDetectorEnabled, false );
				conf.OptBool( "OutputDetectorEnabled", oParams.bOutputDetectorEnabled, false );


				// Set up reproduction output recording

				conf.OptBool( "RecordInputEnabled", oParams.bRecordInputEnabled, false );
				if( oParams.bRecordInputEnabled )
				{
					VistaFileSystemFile oRecordInputFile( "reproduction.wav" );
					VistaFileSystemDirectory oRecordInputFolder( "./" );

					std::string sFilePathRAWDeprecated;
					conf.OptString( "RecordInputFilePath", sFilePathRAWDeprecated );
					if( !sFilePathRAWDeprecated.empty( ) )
					{
						VA_WARN( "Core",
						         "The reproduction configuration key 'RecordInputFilePath' is deprecated. Use 'RecordInputBaseFolder' (optional) and "
						         "'RecordInputFileName' instead." );

						std::string sDummy;
						if( conf.OptString( "RecordInputBaseFolder", sDummy ) || conf.OptString( "RecordInputFileName", sDummy ) )
							VA_EXCEPT2( INVALID_PARAMETER,
							            "You have combined old reproduction configuration key 'RecordInputFilePath' with one of the new keys 'RecordInputBaseFolder' "
							            "(optional) or 'RecordInputFileName'. Please use new key only." );

						std::string sRecordInputFilePath = m_oCoreConfig.mMacros.SubstituteMacros( sFilePathRAWDeprecated );
						oRecordInputFile.SetName( sRecordInputFilePath );
						oRecordInputFolder.SetName( oRecordInputFile.GetParentDirectory( ) );
					}
					else
					{
						std::string sFileNameRAW;
						conf.ReqString( "RecordInputFileName", sFileNameRAW );
						std::string sFileName = m_oCoreConfig.mMacros.SubstituteMacros( sFileNameRAW );
						oRecordInputFile.SetName( sFileName );

						std::string sBaseFolderRAW;
						conf.ReqString( "RecordInputBaseFolder", sBaseFolderRAW );
						std::string sBaseFolder = m_oCoreConfig.mMacros.SubstituteMacros( sBaseFolderRAW );
						oRecordInputFolder.SetName( sBaseFolder );
					}

					if( !oRecordInputFolder.Exists( ) )
					{
						if( oRecordInputFolder.CreateWithParentDirectories( ) )
						{
							VA_INFO( "Core", "Created reproduction input record base folder " << oRecordInputFolder.GetName( ) << " with parent directories" );
						}
						else
						{
							VA_EXCEPT2( INVALID_PARAMETER,
							            "Could not create non-existent reproduction input record base folder '" + oRecordInputFolder.GetName( ) + "'" );
						}
					}

					oParams.sRecordInputFileName   = oRecordInputFile.GetLocalName( );
					oParams.sRecordInputBaseFolder = oRecordInputFolder.GetName( );
				}


				// Set up reproduction output recording

				conf.OptBool( "RecordOutputEnabled", oParams.bRecordOutputEnabled, false );
				if( oParams.bRecordOutputEnabled )
				{
					VistaFileSystemFile oRecordOutputFile( "reproduction.wav" );
					VistaFileSystemDirectory oRecordOutputBaseFolder( "./" );

					std::string sFilePathRAWDeprecated;
					conf.OptString( "RecordOutputFilePath", sFilePathRAWDeprecated );
					if( !sFilePathRAWDeprecated.empty( ) )
					{
						VA_WARN( "Core",
						         "The reproduction configuration key 'RecordOutputFilePath' is deprecated. Use 'RecordOutputBaseFolder' (optional) and "
						         "'RecordOutputFileName' instead." );

						std::string sDummy;
						if( conf.OptString( "RecordOutputBaseFolder", sDummy ) || conf.OptString( "RecordOutputFileName", sDummy ) )
							VA_EXCEPT2( INVALID_PARAMETER,
							            "You have combined old reproduction configuration key 'RecordOutputFilePath' with one of the new keys 'RecordOutputBaseFolder' "
							            "(optional) or 'RecordOutputFileName'. Please use new key only." );

						std::string sRecordOutputFilePath = m_oCoreConfig.mMacros.SubstituteMacros( sFilePathRAWDeprecated );
						oRecordOutputFile.SetName( sRecordOutputFilePath );
						oRecordOutputBaseFolder.SetName( oRecordOutputFile.GetParentDirectory( ) );
					}
					else
					{
						std::string sFileNameRAW;
						conf.ReqString( "RecordOutputFileName", sFileNameRAW );
						std::string sFileName = m_oCoreConfig.mMacros.SubstituteMacros( sFileNameRAW );
						oRecordOutputFile.SetName( sFileName );

						std::string sBaseFolderRAW;
						conf.ReqString( "RecordOutputBaseFolder", sBaseFolderRAW );
						std::string sBaseFolder = m_oCoreConfig.mMacros.SubstituteMacros( sBaseFolderRAW );
						oRecordOutputBaseFolder.SetName( sBaseFolder );
					}

					if( !oRecordOutputBaseFolder.Exists( ) )
					{
						if( oRecordOutputBaseFolder.CreateWithParentDirectories( ) )
						{
							VA_INFO( "Core", "Created reproduction output record base folder " << oRecordOutputBaseFolder.GetName( ) << " with parent directories" );
						}
						else
						{
							VA_EXCEPT2( INVALID_PARAMETER,
							            "Could not create non-existent reproduction output record base folder '" + oRecordOutputBaseFolder.GetName( ) + "'" );
						}
					}

					oParams.sRecordOutputFileName   = oRecordOutputFile.GetLocalName( );
					oParams.sRecordOutputBaseFolder = oRecordOutputBaseFolder.GetName( );
				}


				// Parse outputs
				std::vector<std::string> vsOutputs;
				conf.ReqStringListRegex( "Outputs", vsOutputs, "\\s*,\\s*" );
				std::unique( vsOutputs.begin( ), vsOutputs.end( ) ); // Uniqueness, baby!

				// Check outputs in hardware setup
				std::vector<std::string>::const_iterator cjt = vsOutputs.begin( );
				while( cjt != vsOutputs.end( ) )
				{
					const std::string& sOutput( *cjt );
					const CVAHardwareOutput* pOutput = m_oCoreConfig.oHardwareSetup.GetOutput( sOutput );
					if( pOutput == nullptr )
						conf.Error( "Referring to unknown output \"" + sOutput + "\"" );
					oParams.vpOutputs.push_back( pOutput );
					cjt++;
				}

				// Register this reproduction module
				IVAAudioReproductionFactory* pFactory = pRegistry->FindFactory( oParams.sClass );
				if( !pFactory )
					conf.Error( "Unknown class \"" + oParams.sClass + "\"" );

				// Create the module
				CVAAudioReproductionModuleDesc oDesc( pFactory->Create( oParams ) );
				oDesc.sID       = oParams.sID;
				oDesc.sClass    = oParams.sClass;
				oDesc.vpOutputs = oParams.vpOutputs;
				oDesc.bEnabled  = bEnabled;

				// Add output in Renderer-to-Reproduction patch bay
				int iReproductionModuleNumInputChannels = oDesc.pInstance->GetNumInputChannels( );
				oDesc.iR2RPatchBayOutput                = m_pR2RPatchbay->AddOutput( iReproductionModuleNumInputChannels );

				ITADatasource* pInputTail = m_pR2RPatchbay->GetOutputDatasource( oDesc.iR2RPatchBayOutput );
				if( oParams.bInputDetectorEnabled )
				{
					oDesc.pInputDetector = new ITAStreamDetector( pInputTail );
					pInputTail           = oDesc.pInputDetector;
				}
				if( oParams.bRecordInputEnabled )
				{
					std::string sFilePath = oParams.sRecordInputBaseFolder + "/" + oParams.sRecordInputFileName;
					VistaFileSystemFile oFile( sFilePath );

					if( oFile.Exists( ) )
						VA_INFO( "Core", "Reproduction input record file '" << oFile.GetName( ) << "' exists, will overwrite" );

					oDesc.pInputRecorder = new ITAStreamProbe( pInputTail, oFile.GetName( ) );
					pInputTail           = oDesc.pInputRecorder;
					VA_TRACE( "Core", "Reproduction will record input to file '" << oFile.GetName( ) << "'" );
				}

				// Assign Renderer-to-Reproduction patch bay output datasource as input for reproduction module
				oDesc.pInstance->SetInputDatasource( pInputTail );


				ITADatasource* pOutputTail = oDesc.pInstance->GetOutputDatasource( );
				if( oParams.bOutputDetectorEnabled )
				{
					oDesc.pOutputDetector = new ITAStreamDetector( pOutputTail );
					pOutputTail           = oDesc.pOutputDetector;
				}
				if( oParams.bRecordOutputEnabled )
				{
					std::string sFilePath = oParams.sRecordOutputBaseFolder + "/" + oParams.sRecordOutputFileName;
					VistaFileSystemFile oFile( sFilePath );

					if( oFile.Exists( ) )
						VA_INFO( "Core", "Reproduction output record file '" << oFile.GetName( ) << "' exists, will overwrite" );

					oDesc.pOutputRecorder = new ITAStreamProbe( pOutputTail, oFile.GetName( ) );
					pOutputTail           = oDesc.pOutputRecorder;
					VA_TRACE( "Core", "Reproduction will record output to file '" << oFile.GetName( ) << "'" );
				}

				// Add input in output patch bay and assign reproduction module datasource output to this input
				oDesc.iOutputPatchBayInput = m_pOutputPatchbay->AddInput( pOutputTail );

				// TODO: Active umsetzen ggf. in Params rein
				m_voReproductionModules.push_back( oDesc );
			}
		}
		cit++;
	}

	const size_t nNumReproductionModules = m_voReproductionModules.size( );
	if( nNumReproductionModules == 1 )
	{
		VA_INFO( "Core", "Started one reproduction module" );
	}
	else
	{
		VA_INFO( "Core", "Started " << nNumReproductionModules << " reproduction modules" );
	}

	for( size_t i = 0; i < nNumReproductionModules; i++ )
	{
		std::string sOutputGroupFeedStr = ( m_voReproductionModules[i].vpOutputs.size( ) == 1 ) ?
                                              "one output group." :
                                              std::to_string( long( m_voReproductionModules[i].vpOutputs.size( ) ) ) + " output groups.";
		VA_INFO( "Core", "    +    " << m_voReproductionModules[i].sID << " (" << m_voReproductionModules[i].sClass << ") feeding " << sOutputGroupFeedStr );
	}

	return;
}

void CVACoreImpl::PatchRendererToReproductionModules( )
{
	std::vector<CVAAudioRendererDesc>::iterator rendit = m_voRenderers.begin( );
	while( rendit != m_voRenderers.end( ) )
	{
		const CVAAudioRendererDesc& renddesc( *rendit );
		int iRendererNumOutputChannels = renddesc.pInstance->GetOutputDatasource( )->GetNumberOfChannels( );
		int iRendererR2RPatchBayInput  = renddesc.iR2RPatchBayInput;

		// Iterate over all target reproduction modules
		std::vector<std::string>::const_iterator ocit = renddesc.vsOutputs.begin( );
		while( ocit != renddesc.vsOutputs.end( ) )
		{
			const std::string& sTargetReproductionModule( *ocit );

			std::vector<CVAAudioReproductionModuleDesc>::const_iterator repcit = m_voReproductionModules.begin( );
			while( repcit != m_voReproductionModules.end( ) )
			{
				const CVAAudioReproductionModuleDesc& repdesc( *repcit );
				if( repdesc.sID == sTargetReproductionModule )
				{
					// Get R2R patch bay output of target reproduction module and patch channels from renderer input
					for( int k = 0; k < iRendererNumOutputChannels; k++ )
						m_pR2RPatchbay->ConnectChannels( iRendererR2RPatchBayInput, k, repdesc.iR2RPatchBayOutput, k );

					break; // break after target reproduction module is patched and continue with next
				}
				repcit++;
			}
			ocit++;
		}
		rendit++;
	}

	return;
}

void CVACoreImpl::PatchReproductionModulesToOutput( )
{
	int iMaxPhysicalChannelsAvailable = m_pOutputPatchbay->GetOutputNumChannels( 0 );

	std::vector<CVAAudioReproductionModuleDesc>::iterator it = m_voReproductionModules.begin( );
	while( it != m_voReproductionModules.end( ) )
	{
		CVAAudioReproductionModuleDesc& oRepDesc( *it );

		std::vector<const CVAHardwareOutput*>::iterator jt = oRepDesc.vpOutputs.begin( );
		while( jt != oRepDesc.vpOutputs.end( ) )
		{
			const CVAHardwareOutput* pOutput( *jt );

			// Only consider outputs for patching, that are set enabled via configuration
			if( pOutput->IsEnabled( ) == false )
			{
				jt++;
				continue;
			}

			for( size_t k = 0; k < pOutput->GetPhysicalOutputChannels( ).size( ); k++ )
			{
				int iPhysicalOutputChannel = pOutput->GetPhysicalOutputChannels( )[k] - 1;
				if( iPhysicalOutputChannel < iMaxPhysicalChannelsAvailable )
					m_pOutputPatchbay->ConnectChannels( oRepDesc.iOutputPatchBayInput, int( k ), 0, iPhysicalOutputChannel );
				else
					VA_EXCEPT2( INVALID_PARAMETER,
					            "VACore/ConnectRendererWithReproductionModules: Group '" + pOutput->sIdentifier + "' output channel out of range for this sound card" );
			}
			jt++;
		}
		it++;
	}

	return;
}
