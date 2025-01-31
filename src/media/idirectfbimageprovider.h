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

#ifndef __MEDIA__IDIRECTFBIMAGEPROVIDER_H__
#define __MEDIA__IDIRECTFBIMAGEPROVIDER_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

/*
 * probing context
 */
typedef struct {
     unsigned char  header[32];
     const char    *filename;
} IDirectFBImageProvider_ProbeContext;

/**********************************************************************************************************************/

/*
 * Create (probing) the image provider.
 */
DFBResult IDirectFBImageProvider_CreateFromBuffer( IDirectFBDataBuffer     *buffer,
                                                   CoreDFB                 *core,
                                                   IDirectFB               *idirectfb,
                                                   IDirectFBImageProvider **ret_interface );

#endif
