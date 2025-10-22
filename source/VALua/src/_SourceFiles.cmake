# $Id:$

set( RelativeDir "Src" )
set( RelativeSourceGroup "Source Files" )

set( DirFiles
	VALuaCoreObject.h
	VALuaDLLMain.cpp
	VALuaShell.cpp
	VALuaShellImpl.cpp
	VALuaShellImpl.h
	luna.h
	_SourceFiles.cmake
)
set( DirFiles_SourceGroup "${RelativeSourceGroup}" )

set( LocalSourceGroupFiles  )
foreach( File ${DirFiles} )
	list( APPEND LocalSourceGroupFiles "${RelativeDir}/${File}" )
	list( APPEND ProjectSources "${RelativeDir}/${File}" )
endforeach()
source_group( ${DirFiles_SourceGroup} FILES ${LocalSourceGroupFiles} )

