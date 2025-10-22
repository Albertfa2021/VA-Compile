/*
 *  --------------------------------------------------------------------------------------------
 *
 *    VVV        VVV A           Virtual Acoustics (VA) | http://www.virtualacoustics.org
 *     VVV      VVV AAA          Licensed under the Apache License, Version 2.0
 *      VVV    VVV   AAA
 *       VVV  VVV     AAA        Copyright 2015-2021
 *        VVVVVV       AAA       Institute of Technical Acoustics (ITA)
 *         VVVV         AAA      RWTH Aachen University
 *
 *  --------------------------------------------------------------------------------------------
 */

// Temporarily "disable" debug to include Python.
// This is done, so that the Python debug libraries are not needed, even when building
// this module in debug mode.
#ifdef _DEBUG
#	define TMP_DEBUG
#	undef _DEBUG
#endif
#include <Python.h>
#ifdef TMP_DEBUG
#	define _DEBUG
#	undef TMP_DEBUG
#endif

PyDoc_STRVAR( module_doc,
              "connect(server, port) - connect to a VA server at given server and listening port\n"
              "disconnect() - disconnect from VA server." );

PyDoc_STRVAR( no_doc,
              "For this method no dedicated documentation is available. Please read the C++ API documentation\n"
              "of this method for further information." );

PyDoc_STRVAR( connect_doc,
              "Connect($module, /, server, port)\n"
              "--\n"
              "\n"
              "Connect to a VA server.\n"
              "\n"
              "  server\n"
              "    Remote server IP.\n"
              "  port\n"
              "    TCP/IP listening port, usually 12340." );
