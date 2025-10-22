#include <ITAStringUtils.h>
#include <VAObjectPool.h>
#include <iostream>

using namespace std;

static int iCount;

class MyItem : public CVAPoolObject
{
public:
	std::string sName;

	~MyItem( ) { cout << "Destroying object \"" << sName << "\"" << endl; }

	void hello( ) { cout << "Hello. I am object \"" << sName << "\"" << endl; }
};

class MyItemFactory : public IVAPoolObjectFactory
{
public:
	CVAPoolObject* CreatePoolObject( )
	{
		MyItem* p = new MyItem;
		p->sName  = "Item" + IntToString( ++iCount );
		cout << "Factory created object \"" << p->sName << "\"" << endl;
		return p;
	}
};

void main( )
{
	// CVAObjectPool* p = new CVAObjectPool(1,2,new MyItemFactory);
	CVAObjectPool* p = new CVAObjectPool( 1, 2, new CVAPoolObjectDefaultFactory<MyItem>, true );

	cout << "Request o1" << endl;
	MyItem* o1 = dynamic_cast<MyItem*>( p->RequestObject( ) );
	cout << "Request o2" << endl;
	MyItem* o2 = dynamic_cast<MyItem*>( p->RequestObject( ) );
	cout << "Request o3" << endl;
	MyItem* o3 = dynamic_cast<MyItem*>( p->RequestObject( ) );
	cout << "Request o4" << endl;
	MyItem* o4 = dynamic_cast<MyItem*>( p->RequestObject( ) );
	cout << "Request o5" << endl;
	MyItem* o5 = dynamic_cast<MyItem*>( p->RequestObject( ) );

	cout << "Ref count o1 = " << o1->GetNumReferences( ) << endl;
	o1->AddReference( );
	cout << "Ref count o1 = " << o1->GetNumReferences( ) << endl;
	o1->RemoveReference( );
	cout << "Ref count o1 = " << o1->GetNumReferences( ) << endl;

	cout << "Size = " << p->GetSize( ) << ", free = " << p->GetNumFree( ) << ", used = " << p->GetNumUsed( ) << endl;
	cout << "Release o1 = " << o1->RemoveReference( ) << endl;
	cout << "Size = " << p->GetSize( ) << ", free = " << p->GetNumFree( ) << ", used = " << p->GetNumUsed( ) << endl;
	cout << "Release o2 = " << o2->RemoveReference( ) << endl;
	cout << "Size = " << p->GetSize( ) << ", free = " << p->GetNumFree( ) << ", used = " << p->GetNumUsed( ) << endl;

	delete p;
}