/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#include <direct/direct.h>
#include <direct/log.h>
#include <direct/result.h>
#include <directfb.h>
#include <directfb_version.h>
#include <idirectfb.h>
#include <misc/conf.h>

D_DEBUG_DOMAIN( DirectFB_Main, "DirectFB/Main", "DirectFB Main Functions" );

/**********************************************************************************************************************/

IDirectFB *idirectfb_singleton = NULL;

const char *
DirectFBCheckVersion( unsigned int required_major,
                      unsigned int required_minor,
                      unsigned int required_micro )
{
     if (required_major > DIRECTFB_MAJOR_VERSION)
          return "DirectFB version too old (major mismatch)";
     if (required_major < DIRECTFB_MAJOR_VERSION)
          return "DirectFB version too new (major mismatch)";
     if (required_minor > DIRECTFB_MINOR_VERSION)
          return "DirectFB version too old (minor mismatch)";
     if (required_minor < DIRECTFB_MINOR_VERSION)
          return "DirectFB version too new (minor mismatch)";
     if (required_micro > DIRECTFB_MICRO_VERSION)
          return "DirectFB version too old (micro mismatch)";

     return NULL;
}

DFBResult
DirectFBInit( int    *argc,
              char *(*argv[]) )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Main, "%s( %p, %p )\n", __FUNCTION__, argc, argv );

     ret = dfb_config_init( argc, argv );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult
DirectFBSetOption( const char *name, const char *value )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Main, "%s( '%s', '%s' )\n", __FUNCTION__, name, value );

     if (dfb_config == NULL) {
          D_ERROR( "DirectFB/Main: DirectFBInit() has to be called before DirectFBSetOption()!\n" );
          return DFB_INIT;
     }

     if (!name)
          return DFB_INVARG;

     ret = dfb_config_set( name, value );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult
DirectFBCreate( IDirectFB **ret_interface )
{
     DFBResult  ret;
     IDirectFB *dfb;

     D_DEBUG_AT( DirectFB_Main, "%s( %p )\n", __FUNCTION__, ret_interface );

     if (!dfb_config) {
          /* Don't use D_ERROR() here, it uses dfb_config  */
          direct_log_printf( NULL, "(!) DirectFB/Main: DirectFBInit() has to be called before DirectFBCreate()!\n" );
          return DFB_INIT;
     }

     if (!ret_interface)
          return DFB_INVARG;

     if (idirectfb_singleton) {
          D_DEBUG_AT( DirectFB_Main, "  -> using singleton %p\n", idirectfb_singleton );

          idirectfb_singleton->AddRef( idirectfb_singleton );

          *ret_interface = idirectfb_singleton;

          return DFB_OK;
     }

     direct_initialize();

     if (!(direct_config->quiet & DMT_BANNER) && dfb_config->banner) {
          direct_log_printf( NULL,
                             "\n"
                             "   ~~~~~~~~~~~~~~~~~~~~~~~~~~| DirectFB %d.%d.%d %s |~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
                             "        (c) 2017-2021  DirectFB2 Open Source Project (fork of DirectFB)\n"
                             "        (c) 2012-2016  DirectFB integrated media GmbH\n"
                             "        (c) 2001-2016  The world wide DirectFB Open Source Community\n"
                             "        (c) 2000-2004  Convergence (integrated media) GmbH\n"
                             "      ----------------------------------------------------------------\n"
                             "\n",
                             DIRECTFB_MAJOR_VERSION, DIRECTFB_MINOR_VERSION, DIRECTFB_MICRO_VERSION,
                             DIRECTFB_VERSION_VENDOR );
     }

     static DirectMutex lock = DIRECT_RECURSIVE_MUTEX_INITIALIZER();

     direct_mutex_lock( &lock );

     if (idirectfb_singleton) {
          D_DEBUG_AT( DirectFB_Main, "  -> using (new) singleton %p\n", idirectfb_singleton );

          idirectfb_singleton->AddRef( idirectfb_singleton );

          *ret_interface = idirectfb_singleton;

          direct_mutex_unlock( &lock );
          return DFB_OK;
     }

     DIRECT_ALLOCATE_INTERFACE( dfb, IDirectFB );

     D_DEBUG_AT( DirectFB_Main, "  -> setting singleton to %p (was %p)\n", dfb, idirectfb_singleton );

     idirectfb_singleton = dfb;

     ret = IDirectFB_Construct( dfb );
     if (ret) {
         D_DEBUG_AT( DirectFB_Main, "  -> resetting singleton to NULL!\n" );
         idirectfb_singleton = NULL;

          direct_mutex_unlock( &lock );
          return ret;
     }

     direct_mutex_unlock( &lock );

     ret = IDirectFB_WaitInitialised( dfb );
     if (ret) {
          idirectfb_singleton = NULL;
          dfb->Release( dfb );
          return ret;
     }

     D_DEBUG_AT( DirectFB_Main, "  -> done\n" );

     *ret_interface = dfb;

     return DFB_OK;
}

DFBResult
DirectFBError( const char *msg,
               DFBResult   result )
{
     if (msg)
          direct_log_printf( NULL, "(!) DirectFBError [%s]: %s\n", msg, DirectFBErrorString( result ) );
     else
          direct_log_printf( NULL, "(!) DirectFBError: %s\n", DirectFBErrorString( result ) );

     return result;
}

const char *
DirectFBErrorString( DFBResult result )
{
     return DirectResultString( result );
}

DFBResult
DirectFBErrorFatal( const char *msg,
                    DFBResult   result )
{
     DirectFBError( msg, result );

     exit( result );
}
