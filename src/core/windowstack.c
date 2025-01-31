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

#include <config.h>
#include <core/CoreWindowStack.h>
#include <core/core.h>
#include <core/cursor.h>
#include <core/input.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/windowstack.h>
#include <core/wm.h>
#include <direct/memcpy.h>
#include <fusion/shmalloc.h>
#include <gfx/util.h>

D_DEBUG_DOMAIN( Core_WindowStack, "Core/WindowStack", "DirectFB Core WindowStack" );

/**********************************************************************************************************************/

typedef struct {
     DirectLink  link;
     void       *ctx;
} Stack_Container;

typedef struct {
     DirectLink       link;

     DFBInputDeviceID id;
     Reaction         reaction;
} StackDevice;

static DirectLink  *stack_containers      = NULL;
static DirectMutex  stack_containers_lock = DIRECT_MUTEX_INITIALIZER();

static void
stack_containers_add( CoreWindowStack *stack )
{
     Stack_Container *container;

     D_DEBUG_AT( Core_WindowStack, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &stack_containers_lock );

     container = D_CALLOC( 1, sizeof(Stack_Container) );
     if (!container) {
          D_OOM();
     }

     container->ctx = stack;

     direct_list_append( &stack_containers, &container->link );

     direct_mutex_unlock( &stack_containers_lock );
}

static void
stack_containers_remove( CoreWindowStack *stack )
{
     Stack_Container *container, *next;

     D_DEBUG_AT( Core_WindowStack, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &stack_containers_lock );

     direct_list_foreach_safe (container, next, stack_containers) {
          if ( stack == container->ctx) {
               direct_list_remove( &stack_containers, &container->link );
               D_FREE( container );
          }
     }

     direct_mutex_unlock( &stack_containers_lock );
}

static DFBEnumerationResult
stack_attach_device( CoreInputDevice *device,
                     void            *ctx )
{
     StackDevice     *dev;
     CoreWindowStack *stack = ctx;

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     dev = SHCALLOC( stack->shmpool, 1, sizeof(StackDevice) );
     if (!dev) {
          D_ERROR( "Core/WindowStack: Could not allocate "_ZU" bytes!\n", sizeof(StackDevice) );
          return DFENUM_CANCEL;
     }

     dev->id = dfb_input_device_id( device );

     direct_list_prepend( &stack->devices, &dev->link );

     dfb_input_attach( device, _dfb_windowstack_inputdevice_listener, ctx, &dev->reaction );

     return DFENUM_OK;
}

void
stack_containers_attach_device( CoreInputDevice *device )
{
     Stack_Container *stack_container;

     D_DEBUG_AT( Core_WindowStack, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &stack_containers_lock );

     direct_list_foreach (stack_container, stack_containers) {
          stack_attach_device( device, stack_container->ctx );
     }

     direct_mutex_unlock( &stack_containers_lock );
}

static DFBEnumerationResult
stack_detach_device( CoreInputDevice *device,
                     void            *ctx )
{
     DirectLink      *link;
     CoreWindowStack *stack = ctx;

     D_ASSERT( stack != NULL );
     D_ASSERT( device != NULL );

     link = stack->devices;

     while (link) {
          DirectLink  *next = link->next;
          StackDevice *dev  = (StackDevice*) link;

          if (dfb_input_device_id( device ) == dev->id) {
               direct_list_remove( &stack->devices, &dev->link );

               dfb_input_detach( device, &dev->reaction );

               SHFREE( stack->shmpool, dev );

               return DFENUM_OK;
          }

          link = next;
     }

     return DFENUM_CANCEL;
}

void
stack_containers_detach_device( CoreInputDevice *device )
{
     Stack_Container *stack_container;

     D_DEBUG_AT( Core_WindowStack, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &stack_containers_lock );

     direct_list_foreach (stack_container, stack_containers) {
          stack_detach_device( device, stack_container->ctx );
     }

     direct_mutex_unlock( &stack_containers_lock );
}

/**********************************************************************************************************************/

CoreWindowStack*
dfb_windowstack_create( CoreLayerContext *context )
{
     DFBResult               ret;
     CoreWindowStack        *stack;
     CoreLayer              *layer;
     DFBWindowSurfacePolicy  policy = DWSP_SYSTEMONLY;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, context );

     D_ASSERT( context != NULL );

     layer = dfb_layer_at( context->layer_id );

     /* Allocate window stack data (completely shared). */
     stack = SHCALLOC( context->shmpool, 1, sizeof(CoreWindowStack) );
     if (!stack) {
          D_OOSHM();
          return NULL;
     }

     stack->shmpool = context->shmpool;

     /* Store context which we belong to. */
     stack->context = context;

     /* Set default acceleration. */
     stack->cursor.numerator   = 2;
     stack->cursor.denominator = 1;
     stack->cursor.threshold   = 4;

     /* Choose cursor surface policy. */
     if (context->config.buffermode != DLBM_BACKSYSTEM) {
          CardCapabilities card_caps;

          /* Use the explicitly specified policy. */
          if (dfb_config->window_policy != -1)
               policy = dfb_config->window_policy;
          else {
               /* Examine the hardware capabilities. */
               dfb_gfxcard_get_capabilities( &card_caps );

               if (card_caps.accel & DFXL_BLIT && card_caps.blitting & DSBLIT_BLEND_ALPHACHANNEL)
                    policy = DWSP_VIDEOHIGH;
          }
     }

     stack->cursor.policy = policy;

     /* Set default background mode. */
     stack->bg.mode        = DLBM_DONTCARE;
     stack->bg.color_index = -1;

     D_MAGIC_SET( stack, CoreWindowStack );

     /* Initialize window manager */
     ret = dfb_wm_init_stack( stack );
     if (ret) {
          D_MAGIC_CLEAR( stack );
          SHFREE( context->shmpool, stack );
          return NULL;
     }

     if (dfb_config->single_window)
          fusion_vector_init( &stack->visible_windows, 23, stack->shmpool );

     /* Attach to all input devices. */
     dfb_input_enumerate_devices( stack_attach_device, stack, DICAPS_ALL );

     stack_containers_add( stack );

     CoreWindowStack_Init_Dispatch( layer->core, stack, &stack->call );

     D_DEBUG_AT( Core_WindowStack, "  -> %p\n", stack );

     return stack;
}

void
dfb_windowstack_detach_devices( CoreWindowStack *stack )
{
     DirectLink *link;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     stack_containers_remove( stack );

     link = stack->devices;

     while (link) {
          DirectLink  *next   = link->next;
          StackDevice *device = (StackDevice*) link;

          dfb_input_detach( dfb_input_device_at( device->id ), &device->reaction );

          SHFREE( stack->shmpool, device );

          link = next;
     }
}

void
dfb_windowstack_destroy( CoreWindowStack *stack )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Unlink cursor surface. */
     if (stack->cursor.surface)
          dfb_surface_unlink( &stack->cursor.surface );

     /* Shutdown window manager. */
     if (stack->flags & CWSF_INITIALIZED)
          dfb_wm_close_stack( stack );

     /* Detach listener from background surface and unlink it. */
     if (stack->bg.image) {
          dfb_surface_detach_global( stack->bg.image, &stack->bg.image_reaction );

          dfb_surface_unlink( &stack->bg.image );
     }

     CoreWindowStack_Deinit_Dispatch( &stack->call );

     /* Deallocate shared stack data. */
     if (stack->stack_data) {
          SHFREE( stack->shmpool, stack->stack_data );
          stack->stack_data = NULL;
     }

     D_MAGIC_CLEAR( stack );

     /* Free stack data. */
     SHFREE( stack->shmpool, stack );
}

void
dfb_windowstack_resize( CoreWindowStack *stack,
                        int              width,
                        int              height,
                        int              rotation )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %dx%d, %d )\n", __FUNCTION__, stack, width, height, rotation );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Store the width and height of the stack. */
     stack->width    = width;
     stack->height   = height;

     /* Store the rotation of the stack. */
     stack->rotation = rotation;

     switch (stack->rotation) {
          default:
               D_BUG( "invalid rotation %d", stack->rotation );
          case 0:
               stack->rotated_blit   = DSBLIT_NOFX;
               stack->rotated_width  = stack->width;
               stack->rotated_height = stack->height;
               break;

          case 90:
               stack->rotated_blit   = DSBLIT_ROTATE90;
               stack->rotated_width  = stack->height;
               stack->rotated_height = stack->width;
               break;

          case 180:
               stack->rotated_blit   = DSBLIT_ROTATE180;
               stack->rotated_width  = stack->width;
               stack->rotated_height = stack->height;
               break;

          case 270:
               stack->rotated_blit   = DSBLIT_ROTATE270;
               stack->rotated_width  = stack->height;
               stack->rotated_height = stack->width;
               break;
     }

     /* Setup new cursor clipping region. */
     stack->cursor.region.x1 = 0;
     stack->cursor.region.y1 = 0;
     stack->cursor.region.x2 = width - 1;
     stack->cursor.region.y2 = height - 1;

     /* Notify the window manager. */
     dfb_wm_resize_stack( stack, width, height );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

DirectResult
dfb_windowstack_lock( CoreWindowStack *stack )
{
     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->context != NULL );

     return dfb_layer_context_lock( stack->context );
}

DirectResult
dfb_windowstack_unlock( CoreWindowStack *stack )
{
     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->context != NULL );

     return dfb_layer_context_unlock( stack->context );
}

DFBResult
dfb_windowstack_repaint_all( CoreWindowStack *stack )
{
     DFBResult ret;
     DFBRegion region;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     region.x1 = 0;
     region.y1 = 0;
     region.x2 = stack->rotated_width  - 1;
     region.y2 = stack->rotated_height - 1;

     ret = dfb_wm_update_stack( stack, &region, 0 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_windowstack_set_background_mode ( CoreWindowStack               *stack,
                                      DFBDisplayLayerBackgroundMode  mode )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %u )\n", __FUNCTION__, stack, mode );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Nothing to do if mode is the same. */
     if (mode != stack->bg.mode) {
          /* A surface is required for DLBM_IMAGE and DLBM_TILE modes. */
          if ((mode == DLBM_IMAGE || mode == DLBM_TILE) && !stack->bg.image) {
               dfb_windowstack_unlock( stack );
               return DFB_MISSINGIMAGE;
          }

          /* Set new mode. */
          stack->bg.mode = mode;

          /* Force an update of the window stack. */
          if (mode != DLBM_DONTCARE)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_set_background_image( CoreWindowStack *stack,
                                      CoreSurface     *image )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p )\n", __FUNCTION__, stack, image );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( image != NULL );

     if (!(image->type & CSTF_SHARED))
          return DFB_INVARG;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Nothing to do if image is the same. */
     if (stack->bg.image != image) {
          /* Detach listener from old surface and unlink it. */
          if (stack->bg.image) {
               dfb_surface_detach_global( stack->bg.image, &stack->bg.image_reaction );

               dfb_surface_unlink( &stack->bg.image );
          }

          /* Link surface object. */
          dfb_surface_link( &stack->bg.image, image );

          /* Attach listener to new surface. */
          dfb_surface_attach_global( image, DFB_WINDOWSTACK_BACKGROUND_IMAGE_LISTENER, stack,
                                     &stack->bg.image_reaction );
     }

     /* Force an update of the window stack. */
     if (stack->bg.mode == DLBM_IMAGE || stack->bg.mode == DLBM_TILE)
          dfb_windowstack_repaint_all( stack );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_set_background_color( CoreWindowStack *stack,
                                      const DFBColor  *color )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( color != NULL );

     D_DEBUG_AT( Core_WindowStack, "  -> 0x%02x%02x%02x%02x\n", color->a, color->r, color->g, color->b );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Nothing to do if color is the same. */
     if (!DFB_COLOR_EQUAL( stack->bg.color, *color )) {
          /* Set new color. */
          stack->bg.color = *color;

          /* Force an update of the window stack. */
          if (stack->bg.mode == DLBM_COLOR)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_set_background_color_index( CoreWindowStack *stack,
                                            int              index )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %d )\n", __FUNCTION__, stack, index );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Nothing to do if color is the same. */
     if (stack->bg.color_index != index) {
          /* Set new color index. */
          stack->bg.color_index = index;

          /* Force an update of the window stack. */
          if (stack->bg.mode == DLBM_COLOR)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

static DFBResult
create_cursor_surface( CoreWindowStack *stack,
                       int              width,
                       int              height )
{
     DFBResult               ret;
     CoreSurface            *surface;
     CoreLayer              *layer;
     CoreLayerContext       *context;
     DFBSurfaceCapabilities  surface_caps = DSCAPS_PREMULTIPLIED;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %dx%d )\n", __FUNCTION__, stack, width, height );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( stack->cursor.surface == NULL );

     context = stack->context;

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );

     stack->cursor.x       = stack->width  / 2;
     stack->cursor.y       = stack->height / 2;
     stack->cursor.hot.x   = 0;
     stack->cursor.hot.y   = 0;
     stack->cursor.size.w  = width;
     stack->cursor.size.h  = height;
     stack->cursor.opacity = 0xff;

     if (context->config.buffermode == DLBM_WINDOWS)
          D_WARN( "cursor not yet visible with DLBM_WINDOWS" );

     if (dfb_config->cursor_videoonly)
          surface_caps |= DSCAPS_VIDEOONLY;

     dfb_surface_caps_apply_policy( stack->cursor.policy, &surface_caps );

     /* Create the cursor surface. */
     ret = dfb_surface_create_simple( layer->core, width, height, DSPF_ARGB, DSCS_RGB, surface_caps,
                                      CSTF_SHARED | CSTF_CURSOR, dfb_config->cursor_resource_id, NULL, &surface );
     if (ret) {
          D_ERROR( "Core/WindowStack: Failed to create surface for software cursor!\n" );
          return ret;
     }

     dfb_surface_globalize( surface );

     stack->cursor.surface = surface;

     return DFB_OK;
}

static DFBResult
load_default_cursor( CoreDFB         *core,
                     CoreWindowStack *stack )
{
     DFBResult              ret;
     int                    i, j;
     CoreSurfaceBufferLock  lock;
     void                  *data;
     u32                   *tmp_data;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     if (!stack->cursor.surface) {
          ret = create_cursor_surface( stack, 40, 40 );
          if (ret)
               return ret;
     }
     else {
          stack->cursor.hot.x  = 0;
          stack->cursor.hot.y  = 0;
          stack->cursor.size.w = 40;
          stack->cursor.size.h = 40;
     }

     /* Lock the cursor surface. */
     ret = dfb_surface_lock_buffer( stack->cursor.surface, DSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (ret) {
          D_ERROR( "Core/WindowStack: cannot lock the cursor surface!\n" );
          return ret;
     }

     data = lock.addr;

     /* Fill the cursor window surface. */
     direct_memcpy( data, cursor_data, 40 * lock.pitch );

     for (i = 0; i < 40; i++) {
#ifdef WORDS_BIGENDIAN
          j = MIN( 40, lock.pitch / 4 );
          tmp_data = data;

          while (j--) {
               *tmp_data = (*tmp_data & 0xff000000) >> 24 |
                           (*tmp_data & 0x00ff0000) >>  8 |
                           (*tmp_data & 0x0000ff00) <<  8 |
                           (*tmp_data & 0x000000ff) << 24;
               ++tmp_data;
          }
#endif
          j = MIN( 40, lock.pitch / 4 );
          tmp_data = data;

          while (j--) {
               u32 s = *tmp_data;
               u32 a = (s >> 24) + 1;

               *tmp_data = ((((s & 0x00ff00ff) * a) >> 8) & 0x00ff00ff) |
                           ((((s & 0x0000ff00) * a) >> 8) & 0x0000ff00) |
                              (s & 0xff000000);
               ++tmp_data;
          }

          data += lock.pitch;
     }

     dfb_surface_unlock_buffer( stack->cursor.surface, &lock );

     return ret;
}

DFBResult
dfb_windowstack_cursor_enable( CoreDFB         *core,
                               CoreWindowStack *stack,
                               bool             enable )
{
     DFBResult ret;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %s )\n", __FUNCTION__, stack, enable ? "enable" : "disable" );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     stack->cursor.set = true;

     if (dfb_config->no_cursor || stack->cursor.enabled == enable) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     if (enable && !stack->cursor.surface) {
          ret = load_default_cursor( core, stack );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }
     }

     /* Keep state. */
     stack->cursor.enabled = enable;

     /* Notify WM. */
     dfb_wm_update_cursor( stack, enable ? CCUF_ENABLE : CCUF_DISABLE );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_opacity( CoreWindowStack *stack,
                                    u8               opacity )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, 0x%02x )\n", __FUNCTION__, stack, opacity );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (stack->cursor.opacity != opacity) {
          /* Set new opacity. */
          stack->cursor.opacity = opacity;

          /* Notify WM. */
          if (stack->cursor.enabled)
               dfb_wm_update_cursor( stack, CCUF_OPACITY );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_shape( CoreWindowStack *stack,
                                  CoreSurface     *shape,
                                  int              hot_x,
                                  int              hot_y )
{
     DFBResult              ret;
     CoreSurface           *cursor;
     CoreCursorUpdateFlags  flags = CCUF_SHAPE;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p, hot %d, %d ) <- size %dx%d\n", __FUNCTION__,
                 stack, shape, hot_x, hot_y, shape->config.size.w, shape->config.size.h );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( shape != NULL );

     if (dfb_config->no_cursor)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     cursor = stack->cursor.surface;
     if (!cursor) {
          D_ASSUME( !stack->cursor.enabled );

          /* Create the surface for the shape. */
          ret = create_cursor_surface( stack, shape->config.size.w, shape->config.size.h );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }

          cursor = stack->cursor.surface;
     }
     else if (stack->cursor.size.w != shape->config.size.w || stack->cursor.size.h != shape->config.size.h) {
          dfb_surface_reformat( cursor, shape->config.size.w, shape->config.size.h, DSPF_ARGB );

          stack->cursor.size.w = shape->config.size.w;
          stack->cursor.size.h = shape->config.size.h;

          /* Notify about new size. */
          flags |= CCUF_SIZE;
     }

     if (stack->cursor.hot.x != hot_x || stack->cursor.hot.y != hot_y) {
          stack->cursor.hot.x = hot_x;
          stack->cursor.hot.y = hot_y;

          /* Notify about new position. */
          flags |= CCUF_POSITION;
     }

     /* Copy the content of the new shape. */
     dfb_gfx_copy_stereo( shape, DSSE_LEFT, cursor, DSSE_LEFT, NULL, 0, 0, false );

     cursor->config.caps = ((cursor->config.caps & ~DSCAPS_PREMULTIPLIED) | (shape->config.caps & DSCAPS_PREMULTIPLIED));

     /* Notify WM. */
     if (stack->cursor.enabled)
          dfb_wm_update_cursor( stack, flags );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_warp( CoreWindowStack *stack,
                             int              x,
                             int              y )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %d, %d )\n", __FUNCTION__, stack, x, y );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (x < 0)
          x = 0;
     else if (x > stack->width - 1)
          x = stack->width - 1;

     if (y < 0)
          y = 0;
     else if (y > stack->height - 1)
          y = stack->height - 1;

     if (stack->cursor.x != x || stack->cursor.y != y) {
          stack->cursor.x = x;
          stack->cursor.y = y;

          /* Notify WM. */
          if (stack->cursor.enabled)
               dfb_wm_update_cursor( stack, CCUF_POSITION );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_acceleration( CoreWindowStack *stack,
                                         int              numerator,
                                         int              denominator,
                                         int              threshold )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %d, %d, %d )\n", __FUNCTION__, stack, numerator, denominator, threshold );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     stack->cursor.numerator   = numerator;
     stack->cursor.denominator = denominator;
     stack->cursor.threshold   = threshold;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_get_cursor_position( CoreWindowStack *stack,
                                     int             *ret_x,
                                     int             *ret_y )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p, %p )\n", __FUNCTION__, stack, ret_x, ret_y );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     D_ASSUME( ret_x != NULL || ret_y != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (ret_x)
          *ret_x = stack->cursor.x;

     if (ret_y)
          *ret_y = stack->cursor.y;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

static void
WindowStack_Input_Flush( CoreWindowStack *stack )
{
     if (!stack->motion_x.type && !stack->motion_y.type)
          return;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Call the window manager to dispatch the event. */
     if (dfb_layer_context_active( stack->context )) {
          if (stack->motion_x.type && stack->motion_y.type)
               stack->motion_x.flags |= DIEF_FOLLOW;

          if (stack->motion_x.type)
               dfb_wm_process_input( stack, &stack->motion_x );

          if (stack->motion_y.type)
               dfb_wm_process_input( stack, &stack->motion_y );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     stack->motion_x.type = DIET_UNKNOWN;
     stack->motion_y.type = DIET_UNKNOWN;

     stack->motion_cleanup = NULL;
     stack->motion_ts      = 0;
}

static void
WindowStack_Input_AddAbsolute( CoreWindowStack     *stack,
                               DFBInputEvent       *target,
                               const DFBInputEvent *event )
{
     *target = *event;

     target->flags &= ~DIEF_FOLLOW;
}

static void
WindowStack_Input_AddRelative( CoreWindowStack     *stack,
                               DFBInputEvent       *target,
                               const DFBInputEvent *event )
{
     int axisrel = 0;

     if (target->type)
          axisrel = target->axisrel;

     *target = *event;

     target->axisrel += axisrel;
     target->flags   &= ~DIEF_FOLLOW;
}

static void
WindowStack_Input_Add( CoreWindowStack     *stack,
                       const DFBInputEvent *event )
{
     long long ts = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     if ((stack->motion_x.type && stack->motion_x.device_id != event->device_id) ||
         (stack->motion_y.type && stack->motion_y.device_id != event->device_id) ||
         ts - stack->motion_ts > 10000)
          WindowStack_Input_Flush( stack );

     if (!stack->motion_ts)
          stack->motion_ts = ts;

     switch (event->type) {
          case DIET_AXISMOTION:
               switch (event->axis) {
                    case DIAI_X:
                         if (event->flags & DIEF_AXISABS)
                              WindowStack_Input_AddAbsolute( stack, &stack->motion_x, event );
                         else
                              WindowStack_Input_AddRelative( stack, &stack->motion_x, event );
                         break;

                    case DIAI_Y:
                         if (event->flags & DIEF_AXISABS)
                              WindowStack_Input_AddAbsolute( stack, &stack->motion_y, event );
                         else
                              WindowStack_Input_AddRelative( stack, &stack->motion_y, event );
                         break;

                    default:
                         break;
               }
               break;

          default:
               break;
     }
}

static void
WindowStack_Input_DispatchCleanup( void *ctx )
{
     CoreWindowStack *stack = ctx;

     WindowStack_Input_Flush( stack );

     /* Decrease the layer context's reference count. */
     dfb_layer_context_unref( stack->context );
}

ReactionResult
_dfb_windowstack_inputdevice_listener( const void *msg_data,
                                       void       *ctx )
{
     DFBResult            ret;
     const DFBInputEvent *event = msg_data;
     CoreWindowStack     *stack = ctx;
     int                  num = 0;

     /* Dynamically increase/decrease the ref to the layer context when using the layer context.
        This will prevent the layer context from being destroyed when it is being used. */

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p )\n", __FUNCTION__, event, stack );

     D_ASSERT( event != NULL );
     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Make sure the layer context's reference count is non-zero. If it is, return early and indicate that the listener
        should be removed. In this scenario, this prevents the object_reference_watcher() from being called more than
        once triggered by the reference count changing from 1 to 0 again. */
     if (dfb_layer_context_ref_stat( stack->context, &num ) || num == 0)
          return RS_REMOVE;

     /* Increase the layer context's reference count. */
     if (dfb_layer_context_ref( stack->context ))
          return RS_REMOVE;

     switch (event->type) {
          case DIET_AXISMOTION:
               switch (event->axis) {
                    case DIAI_X:
                    case DIAI_Y:
                         WindowStack_Input_Add( stack, event );

                         if (!stack->motion_cleanup) {
                              ret = fusion_dispatch_cleanup_add( dfb_core_world( core_dfb ),
                                                                 WindowStack_Input_DispatchCleanup, stack,
                                                                 &stack->motion_cleanup );
                              if (ret) {
                                   D_DERROR( ret, "Core/WindowStack: Failed to add dispatch cleanup!\n" );
                                   dfb_layer_context_unref( stack->context );
                                   return RS_OK;
                              }
                         }
                         else
                              dfb_layer_context_unref( stack->context );
                         return RS_OK;

                    default:
                         break;
               }
               break;

          default:
               break;
     }

     WindowStack_Input_Flush( stack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack )) {
          dfb_layer_context_unref( stack->context );
          return RS_REMOVE;
     }

     /* Call the window manager to dispatch the event. */
     if (dfb_layer_context_active( stack->context ))
          dfb_wm_process_input( stack, event );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     /* Decrease the layer context's reference count. */
     if (!stack->motion_cleanup)
          dfb_layer_context_unref( stack->context );

     return RS_OK;
}

ReactionResult
_dfb_windowstack_background_image_listener( const void *msg_data,
                                            void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     CoreWindowStack               *stack        = ctx;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p )\n", __FUNCTION__, notification, stack );

     D_ASSERT( notification != NULL );
     D_MAGIC_ASSERT( stack, CoreWindowStack );

     if (notification->flags & CSNF_DESTROY) {
          D_ERROR( "Core/WindowStack: Surface for background vanished!\n" );
          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_FLIP | CSNF_SIZEFORMAT))
          dfb_windowstack_repaint_all( stack );

     return RS_OK;
}
