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

#ifndef __DISPLAY__IDIRECTFBSURFACE_H__
#define __DISPLAY__IDIRECTFBSURFACE_H__

#include <core/CoreGraphicsStateClient.h>
#include <core/state.h>

/*
 * private data struct of IDirectFBSurface
 */
typedef struct {
     DirectLink               link;

     int                      ref;                              /* reference counter */

     DFBSurfaceCapabilities   caps;                             /* capabilities */

     struct {
         /* 'wanted' is passed to GetSubSurface(), it doesn't matter if it's too large or has negative starting
            coordinates as long as it intersects with the 'granted' rectangle of the parent.
            'wanted' should be seen as the origin for operations on that surface. Non sub surfaces have a 'wanted'
            rectangle of '{ 0, 0, width, height }'. 'wanted' is calculated just once during surface creation. */
         DFBRectangle         wanted;
         /* 'granted' is the intersection of the 'wanted' rectangle and the 'granted' one of the parent. If they do not
            intersect, DFB_INVAREA is returned. For non sub surfaces it's the same as the 'wanted' rectangle, because it
            is the rectangle describing the whole surface. 'granted' is calculated just once during surface creation. */
         DFBRectangle         granted;
         /* 'current' is the intersection of the 'granted' rectangle and the surface extents. SetClip() and many other
            functions are limited by this. This way sub surface area information is preserved during surface resizing,
            e.g. when resizing a window. Calling SetClip() with NULL causes the clipping region to exactly cover the
            'current' rectangle, also the flag 'clip_set' is cleared causing the clipping region to be set to the new
            'current' after resizing. If SetClip() is called with a clipping region specified, an intersection is done
            with the 'wanted' rectangle that is then stored in 'clip_wanted' and 'clip_set' is set. However, if there
            is no intersection, DFB_INVARG is returned, otherwise another intersection is made with the 'current'
            rectangle and gets applied to the surface's state.
            Each resize, after the 'current' rectangle is updated, the clipping region is set to NULL or 'clip_wanted'
            depending on 'clip_set'. This way even clipping regions are restored or extended automatically. It's now
            possible to create a fullscreen primary and call SetVideoMode() with different resolutions or pixelformats
            several times without the need for updating the primary surface by recreating it. */
         DFBRectangle         current;
         /* 'insets' is actually set by the window manager. */
         DFBInsets            insets;
     } area;

     bool                     limit_set;                        /* granted rectangle set */

     bool                     clip_set;                         /* fixed clip set, SetClip() called with clip != NULL */
     DFBRegion                clip_wanted;                      /* last region passed to SetClip() intersected by
                                                                   wanted area, only valid if clip_set != 0 */

     CoreSurface             *surface;                          /* buffer to show */
     bool                     locked;                           /* which buffer is locked */
     CoreSurfaceBufferLock    lock;                             /* lock for allocation */

     IDirectFBFont           *font;                             /* font to use */
     CardState                state;                            /* render state to use */
     DFBTextEncodingID        encoding;                         /* text encoding */

     struct {
          u8                  r;                                /* red component */
          u8                  g;                                /* green component */
          u8                  b;                                /* blue component */
          u32                 value;                            /* r/g/b in surface's format */
     } src_key;

     struct {
          u8                  r;                                /* red component */
          u8                  g;                                /* green component */
          u8                  b;                                /* blue component */
          u32                 value;                            /* r/g/b in surface's format */
     } dst_key;

     Reaction                 reaction;                         /* surface reaction */
     Reaction                 reaction_frame;                   /* frame reaction for CSCH_FRAME */

     CoreDFB                 *core;
     IDirectFB               *idirectfb;

     IDirectFBSurface        *thiz;
     IDirectFBSurface        *parent;
     DirectLink              *children_data;
     DirectLink              *children_free;
     DirectMutex              children_lock;

     CoreGraphicsStateClient  state_client;

     CoreMemoryPermission    *memory_permissions[3];
     unsigned int             memory_permissions_count;

     DirectWaitQueue          back_buffer_wq;
     DirectMutex              back_buffer_lock;

     unsigned int             frame_ack;

     CoreSurfaceClient       *surface_client;
     unsigned int             surface_client_flip_count;
     DirectMutex              surface_client_lock;

     DFBSurfaceStereoEye      src_eye;

     long long                current_frame_time;

     DFBFrameTimeConfig       frametime_config;

     unsigned int             local_flip_count;
     unsigned int             local_buffer_count;

     CoreSurfaceAllocation   *allocations[MAX_SURFACE_BUFFERS];
} IDirectFBSurface_data;

/*
 * initializes interface struct and private data
 */
DFBResult IDirectFBSurface_Construct        ( IDirectFBSurface       *thiz,
                                              IDirectFBSurface       *parent,
                                              DFBRectangle           *req_rect,
                                              DFBRectangle           *clip_rect,
                                              DFBInsets              *insets,
                                              CoreSurface            *surface,
                                              DFBSurfaceCapabilities  caps,
                                              CoreDFB                *core,
                                              IDirectFB              *idirectfb );

/*
 * destroys surface(s) and frees private data
 */
void      IDirectFBSurface_Destruct         ( IDirectFBSurface       *thiz );

/*
 * flips surface buffers
 */
DFBResult IDirectFBSurface_Flip             ( IDirectFBSurface       *thiz,
                                              const DFBRegion        *region,
                                              DFBSurfaceFlipFlags     flags );

/*
 * flips left and right buffers
 */
DFBResult IDirectFBSurface_FlipStereo       ( IDirectFBSurface       *thiz,
                                              const DFBRegion        *left_region,
                                              const DFBRegion        *right_region,
                                              DFBSurfaceFlipFlags     flags );

/*
 * stops all drawing
 */
void      IDirectFBSurface_StopAll          ( IDirectFBSurface_data  *data );

/*
 * waits for the back buffer
 */
void      IDirectFBSurface_WaitForBackBuffer( IDirectFBSurface_data  *data );

#endif
