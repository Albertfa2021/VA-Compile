#include <VAObjectContainer.h>
#include <iostream>

using namespace std;

void main( )
{
	std::string s1 = "Objekt1";
	std::string s2 = "Objekt2";
	std::string s3 = "Objekt3";

	CVAObjectContainer<std::string> c;
	int i1 = c.Add( &s1 );
	int i2 = c.Add( &s2 );
	int i3 = c.Add( &s3 );

	int i2get          = c.GetID( &s1 );
	std::string* s2get = c[i2];

	cout << "Ref count " << c.GetRefCount( i1 ) << endl;
	c.IncRefCount( i1 );
	cout << "Ref count " << c.GetRefCount( i1 ) << endl;
	c.DecRefCount( i1 );
	cout << "Ref count " << c.GetRefCount( i1 ) << endl;
	c.DecRefCount( i1 );
	cout << "Ref count " << c.GetRefCount( i1 ) << endl;


	// Iteration
	// for (CVAObjectContainer<std::string>::iterator it = c.begin(); it!=c.end(); ++cit)
	//	cout << *(*it) << endl;
}