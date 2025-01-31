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

#ifndef __CORE__SURFACE_POOL_H__
#define __CORE__SURFACE_POOL_H__

#include <core/surface.h>

/**********************************************************************************************************************/

typedef enum {
     CSPCAPS_NONE     = 0x00000000, /* none of these */

     CSPCAPS_PHYSICAL = 0x00000001, /* pool provides physical address to buffer */
     CSPCAPS_VIRTUAL  = 0x00000002, /* pool provides virtual address to buffer */
     CSPCAPS_READ     = 0x00000004, /* pool provides Read() function (set automatically) */
     CSPCAPS_WRITE    = 0x00000008, /* pool provides Write() function (set automatically) */

     CSPCAPS_ALL      = 0x0000000F  /* all of these */
} CoreSurfacePoolCapabilities;

typedef enum {
     CSPP_DEFAULT  = 0x00000000,
     CSPP_PREFERED = 0x00000001,
     CSPP_ULTIMATE = 0x00000002
} CoreSurfacePoolPriority;

#define DFB_SURFACE_POOL_DESC_NAME_LENGTH 44

typedef struct {
     CoreSurfacePoolCapabilities caps;
     CoreSurfaceAccessFlags      access[_CSAID_NUM];
     CoreSurfaceTypeFlags        types;
     CoreSurfacePoolPriority     priority;
     char                        name[DFB_SURFACE_POOL_DESC_NAME_LENGTH];
} CoreSurfacePoolDescription;

typedef struct {
     int       (*PoolDataSize)      ( void );
     int       (*PoolLocalDataSize) ( void );
     int       (*AllocationDataSize)( void );

     /*
      * Pool init/destroy.
      */

     DFBResult (*InitPool)          ( CoreDFB                     *core,
                                      CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      void                        *system_data,
                                      CoreSurfacePoolDescription  *ret_desc );

     DFBResult (*JoinPool)          ( CoreDFB                     *core,
                                      CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      void                        *system_data );

     DFBResult (*DestroyPool)       ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local );

     DFBResult (*LeavePool)         ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local );

     DFBResult (*TestConfig)        ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceBuffer           *buffer,
                                      const CoreSurfaceConfig     *config );

     /*
      * Buffer management.
      */

     DFBResult (*AllocateBuffer)    ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceBuffer           *buffer,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data );

     DFBResult (*DeallocateBuffer)  ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceBuffer           *buffer,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data );

     /*
      * Locking.
      */

     DFBResult (*Lock)              ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data,
                                      CoreSurfaceBufferLock       *lock );

     DFBResult (*Unlock)            ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data,
                                      CoreSurfaceBufferLock       *lock );

     /*
      * Read/Write.
      */

     DFBResult (*Read)              ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data,
                                      void                        *destination,
                                      int                          pitch,
                                      const DFBRectangle          *rect );

     DFBResult (*Write)             ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data,
                                      const void                  *source,
                                      int                          pitch,
                                      const DFBRectangle          *rect );

     /*
      * Muck out.
      */
     DFBResult (*MuckOut)           ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceBuffer           *buffer );

     /*
      * Manage interlocks.
      */
     DFBResult (*PreLock)           ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data,
                                      CoreSurfaceAccessorID        accessor,
                                      CoreSurfaceAccessFlags       access );

     /*
      * Handle preallocation.
      * The surface pool checks the description and extracts/generates information for the surface configuration, to be
      * later used in the AllocateBuffer() function.
      */
     DFBResult (*PreAlloc)          ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      const DFBSurfaceDescription *description,
                                      CoreSurfaceConfig           *config );

     /*
      * Keys.
      */

     DFBResult (*CheckKey)          ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceBuffer           *buffer,
                                      const char                  *key,
                                      u64                          handle );

     DFBResult (*AllocateKey)       ( CoreSurfacePool             *pool,
                                      void                        *pool_data,
                                      void                        *pool_local,
                                      CoreSurfaceBuffer           *buffer,
                                      const char                  *key,
                                      u64                          handle,
                                      CoreSurfaceAllocation       *allocation,
                                      void                        *alloc_data );
} SurfacePoolFuncs;

struct __DFB_CoreSurfacePool {
     int                         magic;

     FusionSkirmish              lock;

     CoreSurfacePoolID           pool_id;

     CoreSurfacePoolDescription  desc;

     int                         pool_data_size;
     int                         pool_local_data_size;
     int                         alloc_data_size;

     void                       *data;

     FusionVector                allocs;

     FusionSHMPoolShared        *shmpool;

     CoreSurfacePool            *backup;
};

/**********************************************************************************************************************/

typedef DFBEnumerationResult (*CoreSurfacePoolCallback) ( CoreSurfacePool *pool, void *ctx );

typedef DFBEnumerationResult (*CoreSurfaceAllocCallback)( CoreSurfaceAllocation *allocation, void *ctx );

/**********************************************************************************************************************/

DFBResult dfb_surface_pool_initialize   ( CoreDFB                      *core,
                                          const SurfacePoolFuncs       *funcs,
                                          CoreSurfacePool             **ret_pool );

DFBResult dfb_surface_pool_initialize2  ( CoreDFB                      *core,
                                          const SurfacePoolFuncs       *funcs,
                                          void                         *ctx,
                                          CoreSurfacePool             **ret_pool );

DFBResult dfb_surface_pool_join         ( CoreDFB                      *core,
                                          CoreSurfacePool              *pool,
                                          const SurfacePoolFuncs       *funcs );

DFBResult dfb_surface_pool_join2        ( CoreDFB                      *core,
                                          CoreSurfacePool              *pool,
                                          const SurfacePoolFuncs       *funcs,
                                          void                         *ctx );

DFBResult dfb_surface_pool_destroy      ( CoreSurfacePool              *pool );

DFBResult dfb_surface_pool_leave        ( CoreSurfacePool              *pool );

DFBResult dfb_surface_pools_prealloc    ( const DFBSurfaceDescription  *description,
                                          CoreSurfaceConfig            *config );

DFBResult dfb_surface_pools_negotiate   ( CoreSurfaceBuffer            *buffer,
                                          CoreSurfaceAccessorID         accessor,
                                          CoreSurfaceAccessFlags        access,
                                          CoreSurfacePool             **ret_pools,
                                          unsigned int                  max_pools,
                                          unsigned int                 *ret_num );

DFBResult dfb_surface_pools_enumerate   ( CoreSurfacePoolCallback       callback,
                                          void                         *ctx );

DFBResult dfb_surface_pools_lookup      ( CoreSurfacePoolID             pool_id,
                                          CoreSurfacePool             **ret_pool );

DFBResult dfb_surface_pools_allocate    ( CoreSurfaceBuffer            *buffer,
                                          CoreSurfaceAccessorID         accessor,
                                          CoreSurfaceAccessFlags        access,
                                          CoreSurfaceAllocation       **ret_allocation );

DFBResult dfb_surface_pools_allocate_key( CoreSurfaceBuffer            *buffer,
                                          const char                   *key,
                                          u64                           handle,
                                          CoreSurfaceAllocation       **ret_allocation );

DFBResult dfb_surface_pool_check_key    ( CoreSurfacePool              *pool,
                                          CoreSurfaceBuffer            *buffer,
                                          const char                   *key,
                                          u64                           handle );

DFBResult dfb_surface_pool_allocate     ( CoreSurfacePool              *pool,
                                          CoreSurfaceBuffer            *buffer,
                                          const char                   *key,
                                          u64                           handle,
                                          CoreSurfaceAllocation       **ret_allocation );

DFBResult dfb_surface_pool_deallocate   ( CoreSurfacePool              *pool,
                                          CoreSurfaceAllocation        *allocation );

DFBResult dfb_surface_pool_displace     ( CoreSurfacePool              *pool,
                                          CoreSurfaceBuffer            *buffer,
                                          CoreSurfaceAllocation       **ret_allocation );

DFBResult dfb_surface_pool_prelock      ( CoreSurfacePool              *pool,
                                          CoreSurfaceAllocation        *allocation,
                                          CoreSurfaceAccessorID         accessor,
                                          CoreSurfaceAccessFlags        access );

DFBResult dfb_surface_pool_lock         ( CoreSurfacePool              *pool,
                                          CoreSurfaceAllocation        *allocation,
                                          CoreSurfaceBufferLock        *lock );

DFBResult dfb_surface_pool_unlock       ( CoreSurfacePool              *pool,
                                          CoreSurfaceAllocation        *allocation,
                                          CoreSurfaceBufferLock        *lock );

DFBResult dfb_surface_pool_read         ( CoreSurfacePool              *pool,
                                          CoreSurfaceAllocation        *allocation,
                                          void                         *data,
                                          int                           pitch,
                                          const DFBRectangle           *rect );

DFBResult dfb_surface_pool_write        ( CoreSurfacePool              *pool,
                                          CoreSurfaceAllocation        *allocation,
                                          const void                   *data,
                                          int                           pitch,
                                          const DFBRectangle           *rect );

DFBResult dfb_surface_pool_enumerate    ( CoreSurfacePool              *pool,
                                          CoreSurfaceAllocCallback      callback,
                                          void                         *ctx );

#endif
