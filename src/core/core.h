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

#ifndef __CORE__CORE_H__
#define __CORE__CORE_H__

#include <core/CoreSlave_includes.h>
#include <core/core_resourcemanager.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

#define DIRECTFB_CORE_ABI 46

typedef struct {
     int                  magic;

     bool                 secure;

     FusionObjectPool    *graphics_state_pool;
     FusionObjectPool    *layer_context_pool;
     FusionObjectPool    *layer_region_pool;
     FusionObjectPool    *palette_pool;
     FusionObjectPool    *surface_pool;
     FusionObjectPool    *surface_allocation_pool;
     FusionObjectPool    *surface_buffer_pool;
     FusionObjectPool    *surface_client_pool;
     FusionObjectPool    *window_pool;

     FusionSHMPoolShared *shmpool;
     FusionSHMPoolShared *shmpool_data;

     FusionCall           call;
     FusionHash          *field_hash;
} CoreDFBShared;

struct __DFB_CoreDFB {
     int                        magic;

     int                        refs;

     FusionID                   fusion_id;

     FusionWorld               *world;

     CoreDFBShared             *shared;

     bool                       suspended;

     DirectLink                *cleanups;

     DirectThreadInitHandler   *init_handler;

     DirectSignalHandler       *signal_handler;

     DirectCleanupHandler      *cleanup_handler;

     CoreFontManager           *font_manager;

     struct {
          ICoreResourceManager *manager;
          DirectHash           *identities;
     } resource;

     FusionCall                 async_call;

     FusionCall                 slave_call;

     DirectLink                *memory_permissions;
     DirectMutex                memory_permissions_lock;

     int                        shutdown_tid;
     int                        shutdown_running;
};

/**********************************************************************************************************************/

typedef enum {
     DFCP_CLIPBOARD = 0x00000000,
     DFCP_COLORHASH = 0x00000001,
     DFCP_GRAPHICS  = 0x00000002,
     DFCP_INPUT     = 0x00000003,
     DFCP_LAYER     = 0x00000004,
     DFCP_SCREEN    = 0x00000005,
     DFCP_SURFACE   = 0x00000006,
     DFCP_SYSTEM    = 0x00000007,
     DFCP_WM        = 0x00000008,
} CorePartID;

typedef enum {
     CMPF_READ  = 0x00000001,
     CMPF_WRITE = 0x00000002,
} CoreMemoryPermissionFlags;

typedef void (*CoreCleanupFunc)( void *data, int emergency );

typedef void (*AsyncCallFunc)( void *ctx, void *ctx2 );

typedef struct {
     AsyncCallFunc  func;

     void          *ctx;
     void          *ctx2;
} AsyncCall;

#define CORE_TLS_IDENTITY_STACK_MAX 8

typedef struct {
     int          magic;

     FusionID     identity[CORE_TLS_IDENTITY_STACK_MAX];
     unsigned int identity_count;

     int          calling;
} CoreTLS;

/**********************************************************************************************************************/

extern CoreDFB        *core_dfb;

/*
 * Core initialization and deinitialization.
 */
DFBResult              dfb_core_create                   ( CoreDFB       **ret_core );

DFBResult              dfb_core_destroy                  ( CoreDFB        *core,
                                                           bool            emergency );

void                  *dfb_core_get_part                 ( CoreDFB        *core,
                                                           CorePartID      part_id );

DFBResult              dfb_core_initialize               ( CoreDFB        *core );

/*
 * Object creation.
 */
CoreGraphicsState     *dfb_core_create_graphics_state    ( CoreDFB                *core );
CoreLayerContext      *dfb_core_create_layer_context     ( CoreDFB                *core );
CoreLayerRegion       *dfb_core_create_layer_region      ( CoreDFB                *core );
CorePalette           *dfb_core_create_palette           ( CoreDFB                *core );
CoreSurface           *dfb_core_create_surface           ( CoreDFB                *core );
CoreSurfaceAllocation *dfb_core_create_surface_allocation( CoreDFB                *core );
CoreSurfaceBuffer     *dfb_core_create_surface_buffer    ( CoreDFB                *core );
CoreSurfaceClient     *dfb_core_create_surface_client    ( CoreDFB                *core );
CoreWindow            *dfb_core_create_window            ( CoreDFB                *core );

/*
 * Object accessor.
 */
DFBResult              dfb_core_get_graphics_state       ( CoreDFB                *core,
                                                           u32                     object_id,
                                                           CoreGraphicsState     **ret_state );

DFBResult              dfb_core_get_layer_context        ( CoreDFB                *core,
                                                           u32                     object_id,
                                                           CoreLayerContext      **ret_context );

DFBResult              dfb_core_get_layer_region         ( CoreDFB                *core,
                                                           u32                     object_id,
                                                           CoreLayerRegion       **ret_region );

DFBResult              dfb_core_get_palette              ( CoreDFB                *core,
                                                           u32                     object_id,
                                                           CorePalette           **ret_palette );

DFBResult              dfb_core_get_surface              ( CoreDFB                *core,
                                                           u32                     object_id,
                                                           CoreSurface           **ret_surface );

DFBResult              dfb_core_get_surface_allocation   ( CoreDFB                *core,
                                                           u32                     object_id,
                                                           CoreSurfaceAllocation **ret_allocation );

DFBResult              dfb_core_get_surface_buffer       ( CoreDFB                *core,
                                                           u32                     object_id,
                                                           CoreSurfaceBuffer     **ret_surface );

DFBResult              dfb_core_get_surface_client       ( CoreDFB               *core,
                                                           u32                    object_id,
                                                           CoreSurfaceClient    **ret_client );

DFBResult              dfb_core_get_window               ( CoreDFB               *core,
                                                           u32                    object_id,
                                                           CoreWindow           **ret_window );

/*
 * Debug.
 */
DirectResult           dfb_core_enum_graphics_states     ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_layer_contexts      ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_layer_regions       ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_palettes            ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_surfaces            ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_surface_allocations ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_surface_buffers     ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_surface_clients     ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

DirectResult           dfb_core_enum_windows             ( CoreDFB                *core,
                                                           FusionObjectCallback    callback,
                                                           void                   *ctx );

/*
 * Arena shared fields.
 */
DirectResult           core_arena_add_shared_field       ( CoreDFB     *core,
                                                           const char  *name,
                                                           void        *data );

DirectResult           core_arena_get_shared_field       ( CoreDFB     *core,
                                                           const char  *name,
                                                           void       **data );

/*
 * Returns true if the calling process is the master fusionee.
 */
bool                   dfb_core_is_master                ( CoreDFB *core );

/*
 * Allows other (blocking) fusionees to enter the DirectFB session.
 */
void                   dfb_core_activate                 ( CoreDFB *core );

/*
 * Returns the core's fusion world.
 */
FusionWorld           *dfb_core_world                    ( CoreDFB *core );

/*
 * Returns the shared memory pool of the core.
 */
FusionSHMPoolShared   *dfb_core_shmpool                  ( CoreDFB *core );

/*
 * Returns the shared memory pool for raw data, e.g. surface buffers.
 */
FusionSHMPoolShared   *dfb_core_shmpool_data             ( CoreDFB *core );

/*
 * Suspends all core parts, stopping input threads, closing devices...
 */
DFBResult              dfb_core_suspend                  ( CoreDFB *core );

/*
 * Resumes all core parts, reopening devices, starting input threads...
 */
DFBResult              dfb_core_resume                   ( CoreDFB *core );

/*
 * Adds a function to the cleanup stack that is called during deinitialization.
 */
CoreCleanup           *dfb_core_cleanup_add              ( CoreDFB         *core,
                                                           CoreCleanupFunc  func,
                                                           void            *data,
                                                           bool             emergency );

/*
 * Removes a function from the cleanup stack.
 */
void                   dfb_core_cleanup_remove           ( CoreDFB     *core,
                                                           CoreCleanup *cleanup );

/*
 * Returns the font manager of the core.
 */
CoreFontManager        *dfb_core_font_manager            ( CoreDFB *core );

/*
 * Memory permissions management.
 */

DFBResult              dfb_core_memory_permissions_add   ( CoreDFB                    *core,
                                                           CoreMemoryPermissionFlags   flags,
                                                           void                       *data,
                                                           size_t                      length,
                                                           CoreMemoryPermission      **ret_permission );

DFBResult              dfb_core_memory_permissions_remove( CoreDFB                    *core,
                                                           CoreMemoryPermission       *permission );

DFBResult              dfb_core_memory_permissions_check ( CoreDFB                    *core,
                                                           CoreMemoryPermissionFlags   flags,
                                                           void                       *data,
                                                           size_t                      length );

/*
 * Thread Local Storage management.
 */

void                   Core_TLS__init                    ( void );

void                   Core_TLS__deinit                  ( void );

CoreTLS               *Core_GetTLS                       ( void );

/*
 * Identity management, incoming dispatch pushes ID of caller.
 */

void                   Core_PushIdentity                 ( FusionID caller );

void                   Core_PopIdentity                  ( void );

FusionID               Core_GetIdentity                  ( void );

#if FUSION_BUILD_MULTI
void                   Core_PushCalling                  ( void );
void                   Core_PopCalling                   ( void );
int                    Core_GetCalling                   ( void );
#else /* FUSION_BUILD_MULTI */
#define                Core_PushCalling(x)
#define                Core_PopCalling(x)
#endif /* FUSION_BUILD_MULTI */

/*
 * Resource management.
 */

DFBResult              Core_Resource_CheckSurface        ( const CoreSurfaceConfig *config,
                                                           u64                      resource_id );

DFBResult              Core_Resource_CheckSurfaceUpdate  ( CoreSurface             *surface,
                                                           const CoreSurfaceConfig *config );

DFBResult              Core_Resource_AddSurface          ( CoreSurface             *surface );

DFBResult              Core_Resource_RemoveSurface       ( CoreSurface             *surface );

DFBResult              Core_Resource_UpdateSurface       ( CoreSurface             *surface,
                                                           const CoreSurfaceConfig *config );

/*
 * Client instance management.
 */

DFBResult              Core_Resource_AddIdentity         ( FusionID identity,
                                                           u32      slave_call );

void                   Core_Resource_DisposeIdentity     ( FusionID identity );

ICoreResourceClient   *Core_Resource_GetClient           ( FusionID identity );

CoreSlave             *Core_Resource_GetSlave            ( FusionID identity );

/*
 * Runs a call on the Fusion dispatch thread.
 * Used for asynchronous destruct, i.e. when a call needs to destroy itself.
 */
static __inline__ DFBResult
Core_AsyncCall( AsyncCallFunc  func,
                void          *ctx,
                void          *ctx2 )
{
     AsyncCall call;

     call.func = func;
     call.ctx  = ctx;
     call.ctx2 = ctx2;

     return fusion_call_execute2( &core_dfb->async_call, FCEF_ONEWAY | FCEF_NODIRECT, 0, &call, sizeof(call), NULL );
}

#endif
