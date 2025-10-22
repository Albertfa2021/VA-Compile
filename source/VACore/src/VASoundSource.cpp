#include "VASoundSource.h"

#include <string>


CVASoundSource::CVASoundSource( ) {}

CVASoundSource::~CVASoundSource( ) {}

int CVASoundSource::getID( ) const
{
	return m_iID;
}

void CVASoundSource::setID( int iID )
{
	m_iID = iID;
}

std::string CVASoundSource::getName( ) const
{
	return m_sName;
}

void CVASoundSource::setName( const std::string& sName )
{
	m_sName = sName;
}
