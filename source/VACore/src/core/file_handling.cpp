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

#ifdef WIN32
// Trick um DLL-Pfad zu ermitteln
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#endif

std::string VACore::GetCoreLibFilePath( )
{
#ifdef WIN32
	CHAR pszPath[MAX_PATH + 1] = { 0 };
	GetModuleFileNameA( (HINSTANCE)&__ImageBase, pszPath, _countof( pszPath ) );
	return std::string( pszPath );
#else
	VA_EXCEPT2( NOT_IMPLEMENTED, "This function is not implemented for your platform. Sorry." );
	return "";
#endif // WIN32
}

CVAStruct CVACoreImpl::GetSearchPaths( ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	CVAStruct oSearchPaths;
	for( size_t i = 0; i < m_oCoreConfig.vsSearchPaths.size( ); i++ )
		oSearchPaths["path_" + std::to_string( long( i ) )] = m_oCoreConfig.vsSearchPaths[i];

	return oSearchPaths;
}

CVAStruct CVACoreImpl::GetFileList( const bool bRecursive, const std::string& sFileSuffixFilter ) const
{
	VA_NO_REENTRANCE;
	VA_CHECK_INITIALIZED;

	CVAStruct oFileList;
	for( size_t i = 0; i < m_oCoreConfig.vsSearchPaths.size( ); i++ )
	{
		if( bRecursive )
		{
			RecursiveFileList( m_oCoreConfig.vsSearchPaths[i], oFileList, sFileSuffixFilter );
		}
		else
		{
			std::vector<std::string> vsFileList;
			FileList( m_oCoreConfig.vsSearchPaths[i], vsFileList, sFileSuffixFilter );

			CVAStruct oMoreFiles;
			for( size_t j = 0; j < vsFileList.size( ); j++ )
				oMoreFiles[std::to_string( long( j ) )] = vsFileList[j];

			oFileList[m_oCoreConfig.vsSearchPaths[i]] = oMoreFiles;
		}
	}
	return oFileList;
}

std::string CVACoreImpl::FindFilePath( const std::string& sRelativeFilePath ) const
{
	VA_TRY
	{
		std::string sRelativeFilePathSubstituted = SubstituteMacros( sRelativeFilePath );
		VistaFileSystemFile oFile( sRelativeFilePathSubstituted );
		if( oFile.Exists( ) && oFile.IsFile( ) )
			return sRelativeFilePathSubstituted;

		// Search with paths list
		std::string sFinalPathWithSearch;
		for( size_t i = 0; i < m_oCoreConfig.vsSearchPaths.size( ); i++ )
		{
			const std::string& sSearchPath( m_oCoreConfig.vsSearchPaths[i] );
			std::string sCombinedFilePath = correctPath( sSearchPath + PATH_SEPARATOR + sRelativeFilePathSubstituted );
			VistaFileSystemFile oFile( sCombinedFilePath );
			VA_TRACE( "Core:FindFilePath", "Searching in directory '" + sSearchPath + "' for file '" + sRelativeFilePathSubstituted + "'" );
			if( oFile.Exists( ) && oFile.IsFile( ) )
			{
				if( sFinalPathWithSearch.empty( ) )
				{
					sFinalPathWithSearch = sCombinedFilePath;
				}
				else
				{
					VA_WARN( "Core", "Found ambiguous file path '" + sCombinedFilePath + "' (skipped), using first path '" + sFinalPathWithSearch + "'" );
				}
			}
		}
		return sFinalPathWithSearch;
	}
	VA_RETHROW;
}

void CVACoreImpl::RecursiveFileList( const std::string& sBasePath, CVAStruct& oFileList, const std::string sFileSuffixMask ) const
{
	std::vector<std::string> vsFileList;
	FileList( sBasePath, vsFileList, sFileSuffixMask );

	CVAStruct oMoreFiles;
	for( size_t j = 0; j < vsFileList.size( ); j++ )
		oMoreFiles[std::to_string( long( j ) )] = vsFileList[j];

	oFileList[sBasePath] = oMoreFiles;

	std::vector<std::string> vsFolderList;
	FolderList( sBasePath, vsFolderList );

	for( size_t j = 0; j < vsFolderList.size( ); j++ )
		RecursiveFileList( vsFolderList[j], oFileList, sFileSuffixMask );
}

void CVACoreImpl::FolderList( const std::string& sBasePath, std::vector<std::string>& vsFolderList ) const
{
	vsFolderList.clear( );

	VistaFileSystemDirectory oFolder( sBasePath );
	if( !oFolder.Exists( ) || !oFolder.IsDirectory( ) )
		return;

	VistaFileSystemDirectory::const_iterator cit = oFolder.begin( );
	while( cit != oFolder.end( ) )
	{
		VistaFileSystemNode* pNode( *cit++ );
		if( pNode->IsDirectory( ) && pNode->GetLocalName( ) != "." && pNode->GetLocalName( ) != ".." )
			vsFolderList.push_back( pNode->GetName( ) );
	}
}

void CVACoreImpl::FileList( const std::string& sBasePath, std::vector<std::string>& vsFileList, const std::string& sFileSuffixMask ) const
{
	vsFileList.clear( );

	VistaFileSystemDirectory oFolder( sBasePath );
	if( !oFolder.Exists( ) || !oFolder.IsDirectory( ) )
		return;

	VistaFileSystemDirectory::const_iterator cit = oFolder.begin( );
	while( cit != oFolder.end( ) )
	{
		VistaFileSystemNode* pNode( *cit++ );
		if( pNode->IsFile( ) )
		{
			const std::string sFileName = pNode->GetLocalName( );
			if( sFileSuffixMask != "*" && !sFileSuffixMask.empty( ) )
			{
				if( sFileName.substr( sFileName.find_last_of( "." ) + 1 ).compare( sFileSuffixMask ) )
					vsFileList.push_back( pNode->GetName( ) );
			}
			else
				vsFileList.push_back( pNode->GetName( ) );
		}
	}
}

std::string CVACoreImpl::SubstituteMacros( const std::string& sStr ) const
{
	VA_TRY { return m_oCoreConfig.mMacros.SubstituteMacros( sStr ); }
	VA_RETHROW;
}
