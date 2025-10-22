#include "../src/Scene/VAContainerState.cpp"
#include "../src/Scene/VAMotionState.cpp"
#include "../src/Scene/VAScene.h"
#include "../src/Scene/VASceneManager.cpp"
#include "../src/Scene/VASceneState.cpp"
#include "../src/Scene/VASceneStateBase.cpp"
#include "../src/Scene/VASoundPortalState.cpp"
#include "../src/Scene/VASoundReceiverState.cpp"
#include "../src/Scene/VASoundSourceState.cpp"
#include "../src/Scene/VASurfaceState.cpp"
#include "../src/Utils/VADebug.cpp"
#include "../src/Utils/VAUtils.cpp"
#include "../src/VALog.cpp"

#include <ITAClock.h>
#include <ITAStopWatch.h>
#include <ITAStringUtils.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


using namespace std;

/*
void test1() {
    CVASceneConfigSpace space;
    CVASceneVariableInt v1(&space, 1);
    VASceneConfigIndex c;
    int i;

    cout << "v1: Last config = " << v1.GetLastConfig() << ", latest value = " << v1.GetLatestValue() << endl;

    i = 2;
    c = v1.SetValue(i);
    cout << "v1: SetValue(" << i << ") => config " << c << endl;

    i = 2;
    c = v1.SetValue(i);
    cout << "v1: SetValue(" << i << ") => config " << c << endl;

    i = 3;
    c = v1.SetValue(i);
    cout << "v1: SetValue(" << i << ") => config " << c << endl;

    i = 3;
    c = v1.SetValue(i);
    cout << "v1: SetValue(" << i << ") => config " << c << endl;

    cout << endl;
    cout << "v1: Eldest config = " << v1.GetEldestConfig() <<
            ", last config = " << v1.GetLastConfig() <<
            ", last modification config = " << v1.GetLastModificationConfig() << endl;
    for (VASceneConfigIndex c=v1.GetEldestConfig(); c<=v1.GetLastConfig(); c++)
        cout << "v1: GetValue(" << c << ") = " << v1.GetValue(c) << endl;

    // Destroy
    cout << "v1: Exists? " << v1.Exists() << endl;
    cout << "v1: Destroy" << endl;
    v1.Destroy();
    cout << "v1: Exists? " << v1.Exists() << endl;

    cout << "v1: Eldest config = " << v1.GetEldestConfig() <<
            ", last config = " << v1.GetLastConfig() <<
            ", last modification config = " << v1.GetLastModificationConfig() << endl;
}
*/

/*
void test2() {
    CVASceneState s(0);

    std::vector<int> v;
    s.GetSoundSourceIDs(&v);
    cout << "Sound source IDs = ( " << IntVecToString(v) << " )" << endl << endl;

    CVASoundSourceState ss1;
    s.AddSoundSource(4711, &ss1);

    s.GetSoundSourceIDs(&v);
    cout << "Sound source IDs = ( " << IntVecToString(v) << " )" << endl << endl;

    CVASoundSourceState ss2;
    s.AddSoundSource(911, &ss2);

    CVASoundSourceState ss3;
    s.AddSoundSource(123, &ss3);

    s.GetSoundSourceIDs(&v);
    cout << "Sound source IDs = ( " << IntVecToString(v) << " )" << endl << endl;

    s.RemoveSoundSource(4711);

    s.GetSoundSourceIDs(&v);
    cout << "Sound source IDs = ( " << IntVecToString(v) << " )" << endl << endl;

    CVASceneState s2(s);
    s2.GetSoundSourceIDs(&v);
    cout << "Sound source IDs = ( " << IntVecToString(v) << " )" << endl << endl;

}
*/


void test3( )
{
	CVASceneManager sm( ITAClock::getDefaultClock( ) );
	sm.Initialize( );

	int root = sm.GetRootSceneStateID( );
	int head = sm.GetHeadSceneStateID( );

	/*
	CVASceneState* s1 = sm.CreateDerivedSceneState(root);
	int i1 = s1->AddSoundSource();
	s1->AlterSoundSourceState(i1)->SetPosition(1,2,3);
	s1->AlterSoundSourceState(i1)->SetOrientationVU(1,0,0, 0,1,0);
	s1->Fix();
	cout << s1->ToString() << endl;

	CVASceneState* s2 = sm.CreateDerivedSceneState(s1);
	s2->AlterSoundSourceState(i1)->SetPosition(4,5,6);
	s2->AlterSoundSourceState(i1)->SetOrientationYPR(1,20,300);
	int i2 = s2->AddSoundSource();
	int i3 = s2->AddSoundSource();
	int f1 = s2->AddSurface();
	s2->AlterSurfaceState(f1)->SetMaterial(4711);
	s2->Fix();
	cout << s2->ToString() << endl;

	CVASceneState* s3 = sm.CreateDerivedSceneState(s2);
	s3->RemoveSoundSource(i2);
	int l1 = s3->AddListener();
	s3->AlterListenerState(l1)->SetAuralizationMode(2);
	int p1 = s3->AddPortal();
	s3->AlterPortalState(p1)->SetState(0.123);
	s3->RemoveSurface(f1);
	s1->Fix();
	cout << s3->ToString() << endl;

	cout << endl << "----------------------------" << endl;

	cout << s1->ToString() << endl;
	cout << s2->ToString() << endl;
	cout << s3->ToString() << endl;
	*/
	sm.Finalize( );
}


std::vector<int> randomIndices( int size )
{
	// Zufällige Liste von unique IDs generieren
	std::vector<int> u( size );
	for( int i = 0; i < size; i++ )
		u[i] = i;
	std::vector<int> v;
	while( !u.empty( ) )
	{
		int i = rand( ) % u.size( );
		v.push_back( u[i] );
		u.erase( u.begin( ) + i );
	}
	return v;
}

void benchmarkContainerState( )
{
	// Initialize random seed
	srand( (unsigned int)time( NULL ) );

	CVASceneManager sm( ITAClock::getDefaultClock( ) );
	sm.Initialize( );

	// Request times
	const int cycles = 100;

	ITAStopWatch swRequest;
	swRequest.start( );
	for( int i = 0; i < cycles; i++ )
	{
		CVAContainerState* c = sm.RequestContainerState( );
	}
	swRequest.stop( );
	printf( "RequestContainerState = %s\n", timeToString( swRequest.mean( ) / cycles ).c_str( ) );

	ITAStopWatch swNew;
	swNew.start( );
	for( int i = 0; i < cycles; i++ )
	{
		CVAContainerState* c = new CVAContainerState( );
	}
	swNew.stop( );
	printf( "NewContainerState = %s\n", timeToString( swNew.mean( ) / cycles ).c_str( ) );


	ITAStopWatch sw;
	CVAContainerState* c;

	std::vector<int> v = randomIndices( 1000 );

	// Einfügen von Elementen in Container States
	c = new CVAContainerState( );
	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		c->AddObject( i, NULL );
	}
	sw.stop( );
	printf( "OrderedAddForward = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	c = new CVAContainerState( );
	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		c->AddObject( cycles - i - 1, NULL );
	}
	sw.stop( );
	printf( "OrderedAddBackward = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	c = new CVAContainerState( );
	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		c->AddObject( v[i], NULL );
	}
	sw.stop( );
	printf( "RandomAdd = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	// Löschen von Elementen
	c = new CVAContainerState( );
	for( int i = 0; i < cycles; i++ )
	{
		c->AddObject( i, NULL );
	}

	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		c->RemoveObject( i );
	}
	sw.stop( );
	printf( "OrderedRemoveForward = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	c = new CVAContainerState( );
	for( int i = 0; i < cycles; i++ )
	{
		c->AddObject( i, NULL );
	}

	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		c->RemoveObject( cycles - i - 1 );
	}
	sw.stop( );
	printf( "OrderedRemoveBackward = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	c = new CVAContainerState( );
	for( int i = 0; i < cycles; i++ )
	{
		c->AddObject( i, NULL );
	}

	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		c->RemoveObject( v[i] );
	}
	sw.stop( );
	printf( "OrderedRemoveRandom = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	// Diffen

	// Gleiche Listen
	const int elems      = 32;
	CVAContainerState* A = new CVAContainerState( );
	v                    = randomIndices( elems );
	for( int i = 0; i < elems; i++ )
	{
		A->AddObject( v[i], NULL );
	}
	CVAContainerState* B = new CVAContainerState( );
	B->Copy( A, 0 );
	CVAContainerDiff D;
	D.liCom.reserve( 100 );
	D.liNew.reserve( 100 );
	D.liDel.reserve( 100 );

	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		// B->Diff(A, &D);
	}
	sw.stop( );
	printf( "EqDiff = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	// Ungleiche Listen
	A = new CVAContainerState( );
	v = randomIndices( 100 );
	for( int i = 0; i < elems; i++ )
	{
		A->AddObject( v[i], NULL );
	}

	B = new CVAContainerState( );
	v = randomIndices( 100 );
	for( int i = 0; i < elems; i++ )
	{
		B->AddObject( v[i], NULL );
	}

	sw.reset( );
	sw.start( );
	for( int i = 0; i < cycles; i++ )
	{
		// B->Diff(A, &D);
	}
	sw.stop( );
	printf( "RandomDiff = %s\n", timeToString( sw.mean( ) / cycles ).c_str( ) );

	sm.Finalize( );


	/*
	    Fazit: Pool ist etwa 3x schneller als new

	*/
}

void testRefPtr( )
{
	CVASoundSourceState s;
	CVARefPtr<CVASoundSourceState> p;
	void* x = p;
	p       = &s;
	p->AlterMotionState( );

	p = NULL;
}

int main( int, char** )
{
	// test1();
	// test2();
	// test3();
	testRefPtr( );
	// benchmarkContainerState();
}
