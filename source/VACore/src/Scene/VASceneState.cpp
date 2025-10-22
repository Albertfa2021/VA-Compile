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

#include "VASceneState.h"

#include "VAContainerState.h"
#include "VAMotionState.h"
#include "VASceneManager.h"
#include "VASceneStateBase.h"
#include "VASoundPortalState.h"
#include "VASoundReceiverState.h"
#include "VASoundSourceState.h"
#include "VASurfaceState.h"

#include <ITAASCIITable.h>
#include <ITAStringUtils.h>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <sstream>

void CVASceneState::Initialize( const int iSceneStateID, const double dModificationTime )
{
	m_iSceneStateID = iSceneStateID;
	SetModificationTime( dModificationTime );

	data.m_pSources = GetManager( )->RequestContainerState( );
	data.m_pSources->Initialize( dModificationTime );

	data.m_pListeners = GetManager( )->RequestContainerState( );
	data.m_pListeners->Initialize( dModificationTime );

	data.m_pPortals = GetManager( )->RequestContainerState( );
	data.m_pPortals->Initialize( dModificationTime );

	data.m_pSurfaces = GetManager( )->RequestContainerState( );
	data.m_pSurfaces->Initialize( dModificationTime );
}

void CVASceneState::Copy( const int iSceneStateID, const double dModificationTime, const CVASceneState* pBase )
{
	assert( pBase );
	// Zusatz-Check: Kopieren nur von fixierten Zuständen erlauben
	assert( pBase->IsFixed( ) );

	// Wichtig: Nach der Zuweisung setzen
	m_iSceneStateID = iSceneStateID;
	SetModificationTime( dModificationTime );
	SetFixed( false );

	data = pBase->data;

	// Wichtig: Referenzzähler aller referenzierten Objekte inkrementieren
	data.m_pSources->AddReference( );
	data.m_pListeners->AddReference( );
	data.m_pPortals->AddReference( );
	data.m_pSurfaces->AddReference( );

	// Selbst keine Referenzen mehr, da jetzt autonomer Zustand
	ResetReferences( );

	// Eine Referenz setzen, damit Daten erhalten bleiben. Das kommt später raus.
	AddReference( );
}

void CVASceneState::PreRelease( )
{
	// Alle Referenzen auf referenzierte Objekte entfernen
	data.m_pSources->RemoveReference( );
	data.m_pListeners->RemoveReference( );
	data.m_pPortals->RemoveReference( );
	data.m_pSurfaces->RemoveReference( );

	// Funktion der Oberklasse aufrufen
	CVASceneStateBase::PreRelease( );
}

int CVASceneState::GetID( ) const
{
	return m_iSceneStateID;
}

/*
void CVASceneState::SetConfigurationID(int iConfigID) {
assert( !m_bFinalized );
m_iSceneStateID = iConfigID;
}
*/

void CVASceneState::Fix( )
{
	// Alle enthaltenen Objekte finalisieren
	data.m_pSources->Fix( );
	data.m_pListeners->Fix( );
	data.m_pPortals->Fix( );
	data.m_pSurfaces->Fix( );

	// Finalisieren durch Oberklassen-Methode
	CVASceneStateBase::Fix( );
}

int CVASceneState::GetNumSoundSources( ) const
{
	return data.m_pSources->GetSize( );
}

void CVASceneState::GetSoundSourceIDs( std::vector<int>* pviDest ) const
{
	data.m_pSources->GetIDs( pviDest );
}

void CVASceneState::GetSoundSourceIDs( std::list<int>* pliDest ) const
{
	data.m_pSources->GetIDs( pliDest );
}

void CVASceneState::GetSoundSourceIDs( std::set<int>* psiDest ) const
{
	data.m_pSources->GetIDs( psiDest );
}

CVASoundSourceState* CVASceneState::GetSoundSourceState( const int iSourceID ) const
{
	return dynamic_cast<CVASoundSourceState*>( data.m_pSources->GetObject( iSourceID ) );
}

CVASoundSourceState* CVASceneState::AlterSoundSourceState( const int iSourceID )
{
	assert( !IsFixed( ) );

	// Schallquelle in der Szene suchen
	CVASceneStateBase* pState      = data.m_pSources->GetObject( iSourceID );
	CVASoundSourceState* pCurState = dynamic_cast<CVASoundSourceState*>( pState );
	if( !pCurState )
		return nullptr; // Ungültige ID

	// Schallquellenliste ableiten für Modifikation
	CVAContainerState* pList = AlterSoundSourceListState( );
	assert( pList );

	// Zustand der Quelle für Modifkation ableiten
	if( pCurState->IsFixed( ) )
	{
		// Autonomen Zustand ableiten
		CVASoundSourceState* pNewState = GetManager( )->RequestSoundSourceState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		pNewState->Copy( pCurState, GetModificationTime( ) );

		// Alter Zustand wird nicht mehr von der Liste referenziert
		pCurState->RemoveReference( );

		pCurState = pNewState;
	}

	// Zuständ der Quelle in der Liste abändern
	pList->SetObject( iSourceID, pCurState );

	return pCurState;
}

int CVASceneState::AddSoundSource( )
{
	assert( !IsFixed( ) );
	int iSourceID = GetManager( )->GenerateSoundSourceID( );

	// Initialer Zustand: Änderungszeit des übergeordneten Szenezustands übernehmen
	CVASoundSourceState* pInitialState = GetManager( )->RequestSoundSourceState( );
	pInitialState->Initialize( GetModificationTime( ) );

	// Liste modifizieren
	CVAContainerState* pList = AlterSoundSourceListState( );
	assert( pList );
	pList->AddObject( iSourceID, pInitialState );

	return iSourceID;
}

void CVASceneState::RemoveSoundSource( const int iSourceID )
{
	assert( !IsFixed( ) );

	// Prüfen, ob die Quelle existiert
	if( !data.m_pSources->HasObject( iSourceID ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid sound source ID" );

	// Schallquellenliste modifizieren
	CVAContainerState* pList = AlterSoundSourceListState( );
	assert( pList );
	pList->RemoveObject( iSourceID );
}

int CVASceneState::GetNumSoundReceivers( ) const
{
	return data.m_pListeners->GetSize( );
}

void CVASceneState::GetListenerIDs( std::vector<int>* pviDest ) const
{
	data.m_pListeners->GetIDs( pviDest );
}

void CVASceneState::GetListenerIDs( std::list<int>* pliDest ) const
{
	data.m_pListeners->GetIDs( pliDest );
}

void CVASceneState::GetListenerIDs( std::set<int>* psiDest ) const
{
	data.m_pListeners->GetIDs( psiDest );
}

CVAReceiverState* CVASceneState::GetReceiverState( int iListenerID ) const
{
	return dynamic_cast<CVAReceiverState*>( data.m_pListeners->GetObject( iListenerID ) );
}

CVAReceiverState* CVASceneState::AlterReceiverState( int iListenerID )
{
	assert( !IsFixed( ) );

	CVAReceiverState* pCurState = dynamic_cast<CVAReceiverState*>( data.m_pListeners->GetObject( iListenerID ) );
	if( !pCurState )
		return nullptr;

	// Hörerliste ableiten für Modifikation
	CVAContainerState* pList = AlterReceiverListState( );
	assert( pList );

	// Zustand der Quelle für Modifkation ableiten
	if( pCurState->IsFixed( ) )
	{
		// Autonomen Zustand ableiten
		CVAReceiverState* pNewState = GetManager( )->RequestSoundReceiverState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		double dModificationTime = GetModificationTime( );
		pNewState->Copy( pCurState, dModificationTime );

		// Alter Zustand wird nicht mehr von der Liste referenziert
		pCurState->RemoveReference( );

		pCurState = pNewState;
	}

	// Zuständ des Hörers in der Liste abändern
	pList->SetObject( iListenerID, pCurState );

	return pCurState;
}

int CVASceneState::AddListener( )
{
	assert( !IsFixed( ) );
	int iListenerID = GetManager( )->GenerateSoundReceiverID( );

	// Initialer Zustand: Änderungszeit des übergeordneten Szenezustands übernehmen
	CVAReceiverState* pInitialState = GetManager( )->RequestSoundReceiverState( );
	pInitialState->Initialize( GetModificationTime( ) );

	// Liste modifizieren
	CVAContainerState* pList = AlterReceiverListState( );
	assert( pList );
	pList->AddObject( iListenerID, pInitialState );

	return iListenerID;
}

void CVASceneState::RemoveReceiver( int iListenerID )
{
	assert( !IsFixed( ) );

	// Prüfen, ob der Hörer existiert
	if( !data.m_pListeners->HasObject( iListenerID ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid listener ID" );

	// Hörerliste modifizieren
	CVAContainerState* pList = AlterReceiverListState( );
	pList->RemoveObject( iListenerID );
}

void CVASceneState::GetPortalIDs( std::vector<int>* pviDest ) const
{
	data.m_pPortals->GetIDs( pviDest );
}

void CVASceneState::GetPortalIDs( std::list<int>* pliDest ) const
{
	data.m_pPortals->GetIDs( pliDest );
}

void CVASceneState::GetPortalIDs( std::set<int>* psiDest ) const
{
	data.m_pPortals->GetIDs( psiDest );
}

CVAPortalState* CVASceneState::GetPortalState( int iPortalID ) const
{
	return dynamic_cast<CVAPortalState*>( data.m_pPortals->GetObject( iPortalID ) );
}

CVAPortalState* CVASceneState::AlterPortalState( int iPortalID )
{
	assert( !IsFixed( ) );

	// Portal in der Szene suchen
	CVAPortalState* pCurState = dynamic_cast<CVAPortalState*>( data.m_pPortals->GetObject( iPortalID ) );
	if( !pCurState )
		return nullptr; // Ungültige ID

	// Portalliste ableiten für Modifikation
	CVAContainerState* pList = AlterPortalListState( );
	assert( pList );

	// Zustand des Portals für Modifkation ableiten
	if( pCurState->IsFixed( ) )
	{
		// Autonomen Zustand ableiten
		CVAPortalState* pNewState = GetManager( )->RequestPortalState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		pNewState->Copy( pCurState, GetModificationTime( ) );

		// Alter Zustand wird nicht mehr von der Liste referenziert
		pCurState->RemoveReference( );

		pCurState = pNewState;
	}

	// Zuständ des Portals in der Liste abändern
	pList->SetObject( iPortalID, pCurState );

	return pCurState;
}

int CVASceneState::AddPortal( )
{
	assert( !IsFixed( ) );
	int iPortalID = GetManager( )->GeneratePortalID( );

	// Initialer Zustand: Änderungszeit des übergeordneten Szenezustands übernehmen
	CVAPortalState* pInitialState = GetManager( )->RequestPortalState( );
	pInitialState->Initialize( GetModificationTime( ) );

	// Liste modifizieren
	CVAContainerState* pList = AlterPortalListState( );
	assert( pList );
	pList->AddObject( iPortalID, pInitialState );

	return iPortalID;
}

void CVASceneState::RemovePortal( int iPortalID )
{
	assert( !IsFixed( ) );

	// Prüfen, ob das Portal existiert
	if( !data.m_pPortals->HasObject( iPortalID ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid portal ID" );

	// Portalliste modifizieren
	CVAContainerState* pList = AlterPortalListState( );
	assert( pList );
	pList->RemoveObject( iPortalID );
}

void CVASceneState::GetSurfaceIDs( std::vector<int>* pviDest ) const
{
	data.m_pSurfaces->GetIDs( pviDest );
}

CVASurfaceState* CVASceneState::GetSurfaceState( int iSurfaceID ) const
{
	return dynamic_cast<CVASurfaceState*>( data.m_pSurfaces->GetObject( iSurfaceID ) );
}

CVASurfaceState* CVASceneState::AlterSurfaceState( int iSurfaceID, double dModificationTime )
{
	assert( !IsFixed( ) );

	// Oberfläche in der Szene suchen
	CVASurfaceState* pCurState = dynamic_cast<CVASurfaceState*>( data.m_pSurfaces->GetObject( iSurfaceID ) );

	if( !pCurState )
		return nullptr; // Ungültige ID

	// Oberflächenliste ableiten für Modifikation
	CVAContainerState* pList = AlterSurfaceListState( );
	assert( pList );

	// Zustand der Oberfläche für Modifkation ableiten
	if( pCurState->IsFixed( ) )
	{
		// Autonomen Zustand ableiten
		CVASurfaceState* pNewState = GetManager( )->RequestSurfaceState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		pNewState->Copy( pCurState, GetModificationTime( ) );

		// Alter Zustand wird nicht mehr von der Liste referenziert
		pCurState->RemoveReference( );

		pCurState = pNewState;
	}

	// Zustand der Oberfläche in der Liste abändern
	pList->SetObject( iSurfaceID, pCurState );

	return pCurState;
}

int CVASceneState::AddSurface( )
{
	assert( !IsFixed( ) );
	int iSurfaceID = GetManager( )->GenerateSurfaceID( );

	// Initialer Zustand: Änderungszeit des übergeordneten Szenezustands übernehmen
	CVASurfaceState* pInitialState = GetManager( )->RequestSurfaceState( );
	pInitialState->Initialize( GetModificationTime( ) );

	// Liste modifizieren
	CVAContainerState* pList = AlterSurfaceListState( );
	assert( pList );
	pList->AddObject( iSurfaceID, pInitialState );

	return iSurfaceID;
}

void CVASceneState::RemoveSurface( int iSurfaceID )
{
	assert( !IsFixed( ) );

	// Prüfen, ob die Oberfläche existiert
	if( !data.m_pSurfaces->HasObject( iSurfaceID ) )
		VA_EXCEPT2( INVALID_PARAMETER, "Invalid surface ID" );

	// Oberflächenliste modifizieren
	CVAContainerState* pList = AlterSurfaceListState( );
	assert( pList );
	pList->RemoveObject( iSurfaceID );
}

// Auf ID-Sets A,B berechnen: A schnitt B, A-B, B-A,
void GetIDSetIntersectionDiff( const std::set<int>& A, const std::set<int>& B, std::list<int>& A_intersection_B, std::list<int>& A_minus_B, std::list<int>& B_minus_A )
{
	/*
	 *  Algorithmus: Vergleich zweier Mengen. Bestimmung von Schnitt und Differenzen
	 *  TODO: So ist das nicht die schnellste (effizieteste) Lösung. Bitte prüfen ob schnell genug.
	 */

	A_intersection_B.clear( );
	std::back_insert_iterator<std::list<int> > bi1( A_intersection_B );
	std::set_intersection( B.begin( ), B.end( ), A.begin( ), A.end( ), bi1 );

	A_minus_B.clear( );
	std::back_insert_iterator<std::list<int> > bi2( A_minus_B );
	std::set_difference( A.begin( ), A.end( ), B.begin( ), B.end( ), bi2 );

	B_minus_A.clear( );
	std::back_insert_iterator<std::list<int> > bi3( B_minus_A );
	std::set_difference( B.begin( ), B.end( ), A.begin( ), A.end( ), bi3 );
}

void CVASceneState::Diff( const CVASceneState* pState, CVASceneStateDiff* pDiff ) const
{
	assert( pDiff );

	// [fwe] Verbesserte Variante
	// Test auf Unterschiede zunächst Anhand der Listen-Instanzen
	// (Semantik: Gleiche Zeiger auf gefixte States => Gleiche Daten)

	// Diff nach nullptr-Pointer (alles neu)
	if( pState == nullptr )
	{
		data.m_pSources->Diff( nullptr, pDiff->viNewSoundSourceIDs, pDiff->viDelSoundSourceIDs, pDiff->viComSoundSourceIDs );
		data.m_pListeners->Diff( nullptr, pDiff->viNewReceiverIDs, pDiff->viDelReceiverIDs, pDiff->viComReceiverIDs );
		data.m_pPortals->Diff( nullptr, pDiff->viNewPortalIDs, pDiff->viDelPortalIDs, pDiff->viComPortalIDs );
		data.m_pSurfaces->Diff( nullptr, pDiff->viNewSurfaceIDs, pDiff->viDelSurfaceIDs, pDiff->viComSurfaceIDs );

		return;
	}

	// Diff zu pState
	if( data.m_pSources != pState->data.m_pSources )
	{
		data.m_pSources->Diff( pState->data.m_pSources, pDiff->viNewSoundSourceIDs, pDiff->viDelSoundSourceIDs, pDiff->viComSoundSourceIDs );
	}
	else
	{
		// Erhaltene Objekt bleiben erhalten. Keine neuen oder gelöschten Objekte.
		data.m_pSources->GetIDs( &pDiff->viComSoundSourceIDs );
		pDiff->viNewSoundSourceIDs.clear( );
		pDiff->viDelSoundSourceIDs.clear( );
	}

	if( data.m_pListeners != pState->data.m_pListeners )
	{
		data.m_pListeners->Diff( pState->data.m_pListeners, pDiff->viNewReceiverIDs, pDiff->viDelReceiverIDs, pDiff->viComReceiverIDs );
	}
	else
	{
		// Erhaltene Objekt bleiben erhalten. Keine neuen oder gelöschten Objekte.
		data.m_pListeners->GetIDs( &pDiff->viComReceiverIDs );
		pDiff->viNewReceiverIDs.clear( );
		pDiff->viDelReceiverIDs.clear( );
	}

	if( data.m_pPortals != pState->data.m_pPortals )
	{
		data.m_pPortals->Diff( pState->data.m_pPortals, pDiff->viNewPortalIDs, pDiff->viDelPortalIDs, pDiff->viComPortalIDs );
	}
	else
	{
		// Erhaltene Objekt bleiben erhalten. Keine neuen oder gelöschten Objekte.
		data.m_pPortals->GetIDs( &pDiff->viComPortalIDs );
		pDiff->viNewPortalIDs.clear( );
		pDiff->viDelPortalIDs.clear( );
	}

	if( data.m_pSurfaces != pState->data.m_pSurfaces )
	{
		data.m_pSurfaces->Diff( pState->data.m_pSurfaces, pDiff->viNewSurfaceIDs, pDiff->viDelSurfaceIDs, pDiff->viComSurfaceIDs );
	}
	else
	{
		// Erhaltene Objekt bleiben erhalten. Keine neuen oder gelöschten Objekte.
		data.m_pSurfaces->GetIDs( &pDiff->viComSurfaceIDs );
		pDiff->viNewSurfaceIDs.clear( );
		pDiff->viDelSurfaceIDs.clear( );
	}
}

std::string CVASceneState::ToString( ) const
{
	std::list<int> viSoundSourceIDs;
	GetSoundSourceIDs( &viSoundSourceIDs );

	std::stringstream ss;
	ss.precision( 3 );
	ss << "Scene state " << m_iSceneStateID << " {\n";
	/* TODO:
	// Schallquellen
	ss << "  Sound sources {\n";
	for (SoundSourceMapCit cit=data.m_mSources.begin(); cit!=data.m_mSources.end(); ++cit) {
	CVASoundSourceState* p = cit->second;
	RG_Vector vPos, vView, vUp, vVelocity;
	p->GetMotionState()->GetPositionOrientationVelocityVU(vPos, vView, vUp, vVelocity);
	// TODO: YPR float fYaw, fPitch, fRoll;

	ss << "    Sound source {\n";
	ss << "      ID = " << cit->first << "\n";
	ss << "      Name = " << GetManager()->GetSoundSourceName(cit->first) << "\n";
	ss << "      PVelVU = (" << vPos[0] << ", " << vPos[1] << ", " << vPos[2] << "), ";
	ss << "(" << vVelocity[0] << ", " << vVelocity[1] << ", " << vVelocity[2] << "), ";
	ss << "(" << vView[0] << ", " << vView[1] << ", " << vView[2] << "), ";
	ss << "(" << vUp[0] << ", " << vUp[1] << ", " << vUp[2] << ")\n";
	//ss << "      YPR = (" << fYaw << "°, " << fPitch << "°, " << fRoll << "°)\n";
	ss << "      Volume = " << IVAInterface::GetVolumeStrDecibel( p->GetVolume() ) << "\n";
	ss << "      Directivity = " << p->GetDirectivityID() << "\n";
	ss << "      AuraMode = " << IVAInterface::GetAuralizationModeStr( p->GetAuralizationMode() ) << "\n";

	ss << "    }\n";
	}
	ss << "  }\n\n";

	// Hörer
	ss << "  Listeners {\n";
	for (ListenerMapCit cit=data.m_mListeners.begin(); cit!=data.m_mListeners.end(); ++cit) {
	CVAListenerState* p = cit->second;
	RG_Vector vPos, vView, vUp, vVelocity;
	p->GetMotionState()->GetPositionOrientationVelocityVU(vPos, vView, vUp, vVelocity);
	//float fYaw, fPitch, fRoll;
	//p->GetOrientationYPR(fYaw, fPitch, fRoll);

	ss << "    Listener {\n";
	ss << "      ID = " << cit->first << "\n";
	ss << "      Name = " << GetManager()->GetListenerName(cit->first) << "\n";
	ss << "      PVelVU = (" << vPos[0] << ", " << vPos[1] << ", " << vPos[2] << "), ";
	ss << "(" << vVelocity[0] << ", " << vVelocity[1] << ", " << vVelocity[2] << "), ";
	ss << "(" << vView[0] << ", " << vView[1] << ", " << vView[2] << "), ";
	ss << "(" << vUp[0] << ", " << vUp[1] << ", " << vUp[2] << ")\n";
	//ss << "      YPR = (" << fYaw << "°, " << fPitch << "°, " << fRoll << "°)\n";
	ss << "      AuraMode = " << IVAInterface::GetAuralizationModeStr( p->GetAuralizationMode() ) << "\n";

	ss << "    }\n";
	}
	ss << "  }\n\n";

	// Portale
	ss << "  Portals {\n";
	for (PortalMapCit cit=data.m_mPortals.begin(); cit!=data.m_mPortals.end(); ++cit) {
	CVAPortalState* p = cit->second;

	// TODO: Name
	ss << "    Portal { ID = " << cit->first << ", State = " << p->GetState() << " }\n";
	}
	ss << "  }\n\n";

	// Oberflächen
	ss << "  Surfaces {\n";
	for (SurfaceMapCit cit=data.m_mSurfaces.begin(); cit!=data.m_mSurfaces.end(); ++cit) {
	CVASurfaceState* p = cit->second;

	ss << "    Surface { ID = " << cit->first << ", Material = " << p->GetMaterial() << " }\n";
	}
	ss << "  }\n";

	ss << "\n}\n";
	*/


	/*
	ss << "SceneState {" << std::endl
	<< "\tID = " << m_iSceneStateID << std::endl
	<< "\tSound source IDs = " << IntVecToString( viSoundSourceIDs ) << std::endl
	<< "}" << std::endl;
	*/
	// ss << nTimes("-", 40) << std::endl
	//   << "SceneState ID=" << m_iSceneStateID << std::endl
	//   << nTimes("-", 40) << std::endl << std::endl;

	//// Sources
	// ITAASCIITable t1( (int) data.m_mSources.size(), 6);
	// t1.setColumnTitle(0, "ID");
	// t1.setColumnTitle(1, "Pos");
	// t1.setColumnTitle(2, "View");
	// t1.setColumnTitle(3, "Up");
	// t1.setColumnTitle(4, "Volume");
	// t1.setColumnTitle(5, "AMode");

	///*
	// int i=0;
	// for (SoundSourceMapCit cit=data.m_mSources.begin(); cit!=data.m_mSources.end(); ++cit) {
	//	t1.setContent(i, 0, IntToString(cit->first) );
	//	t1.setContent(i, 1, cit->second->vPos.toString());
	//	t1.setContent(i, 2, cit->second->vView.toString());
	//	t1.setContent(i, 3, cit->second->vUp.toString());
	//	t1.setContent(i, 4, FloatToString(cit->second->fVolume));
	//	t1.setContent(i, 5, BitmaskToString(cit->second->iAuraMode, 7));

	//	i++;
	//}
	//*/

	// ss << t1.toString() << std::endl;

	return ss.str( );
}


CVAContainerState* CVASceneState::AlterSoundSourceListState( )
{
	assert( !IsFixed( ) );

	// Falls finalisiert => Autonome Liste erzeugen aus bisheriger Liste
	// Falls nicht finalisiert => Bereits erzeugte autonome Liste benutzen

	CVAContainerState* pCurList = data.m_pSources;

	// Zustand der Quellenliste für Modifikation des Quellenzustands ableiten
	if( pCurList->IsFixed( ) )
	{
		// Autonome Liste ableiten
		CVAContainerState* pNewList = GetManager( )->RequestContainerState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		pNewList->Copy( pCurList, GetModificationTime( ) );

		// Alte Liste wird nicht mehr vom neuen Szenezustand referenziert
		pCurList->RemoveReference( );

		pCurList        = pNewList;
		data.m_pSources = pNewList;
	}

	return pCurList;
}

CVAContainerState* CVASceneState::AlterReceiverListState( )
{
	assert( !IsFixed( ) );

	// Falls finalisiert => Autonome Liste erzeugen aus bisheriger Liste
	// Falls nicht finalisiert => Bereits erzeugte autonome Liste benutzen

	CVAContainerState* pCurList = data.m_pListeners;

	// Zustand der Hörerliste für Modifikation des Hörerzustands ableiten
	if( pCurList->IsFixed( ) )
	{
		// Autonome Liste ableiten
		CVAContainerState* pNewList = GetManager( )->RequestContainerState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		pNewList->Copy( pCurList, GetModificationTime( ) );

		// Alte Liste wird nicht mehr vom neuen Szenezustand referenziert
		pCurList->RemoveReference( );

		pCurList          = pNewList;
		data.m_pListeners = pNewList;
	}

	return pCurList;
}

CVAContainerState* CVASceneState::AlterPortalListState( )
{
	assert( !IsFixed( ) );

	// Falls finalisiert => Autonome Liste erzeugen aus bisheriger Liste
	// Falls nicht finalisiert => Bereits erzeugte autonome Liste benutzen

	CVAContainerState* pCurList = data.m_pPortals;

	// Zustand der Portalliste für Modifikation des Portalzustands ableiten
	if( pCurList->IsFixed( ) )
	{
		// Autonome Liste ableiten
		CVAContainerState* pNewList = GetManager( )->RequestContainerState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		pNewList->Copy( pCurList, GetModificationTime( ) );

		// Alte Liste wird nicht mehr vom neuen Szenezustand referenziert
		pCurList->RemoveReference( );

		pCurList        = pNewList;
		data.m_pPortals = pNewList;
	}

	return pCurList;
}

CVAContainerState* CVASceneState::AlterSurfaceListState( )
{
	assert( !IsFixed( ) );

	// Falls finalisiert => Autonome Liste erzeugen aus bisheriger Liste
	// Falls nicht finalisiert => Bereits erzeugte autonome Liste benutzen

	CVAContainerState* pCurList = data.m_pSurfaces;

	// Zustand der Oberflächenliste für Modifikation des Oberflächenzustands ableiten
	if( pCurList->IsFixed( ) )
	{
		// Autonome Liste ableiten
		CVAContainerState* pNewList = GetManager( )->RequestContainerState( );

		// Änderungszeit des übergeordneten Szenezustands übernehmen
		pNewList->Copy( pCurList, GetModificationTime( ) );

		// Alte Liste wird nicht mehr vom neuen Szenezustand referenziert
		pCurList->RemoveReference( );

		pCurList         = pNewList;
		data.m_pSurfaces = pNewList;
	}

	return pCurList;
}
