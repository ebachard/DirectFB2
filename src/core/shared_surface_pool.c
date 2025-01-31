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

#include <core/core.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <core/system.h>
#include <fusion/conf.h>
#include <fusion/shmalloc.h>
#include <fusion/shm/pool.h>

D_DEBUG_DOMAIN( Core_Shared, "Core/Shared", "DirectFB Core Shared Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
     FusionSHMPoolShared *shmpool;
} SharedPoolData;

typedef struct {
     CoreDFB     *core;
     FusionWorld *world;
} SharedPoolLocalData;

typedef struct {
     void *addr;
     void *aligned_addr;
     int   pitch;
     int   size;
} SharedAllocationData;

/**********************************************************************************************************************/

static int
sharedPoolDataSize()
{
     return sizeof(SharedPoolData);
}

static int
sharedPoolLocalDataSize()
{
     return sizeof(SharedPoolLocalData);
}

static int
sharedAllocationDataSize()
{
     return sizeof(SharedAllocationData);
}

static DFBResult
sharedInitPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data,
                CoreSurfacePoolDescription *ret_desc )
{
     DFBResult            ret;
     SharedPoolData      *data  = pool_data;
     SharedPoolLocalData *local = pool_local;

     D_DEBUG_AT( Core_Shared, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     local->core  = core;
     local->world = dfb_core_world( core );

     ret = fusion_shm_pool_create( local->world, "Surface Memory Pool", dfb_config->surface_shmpool_size,
                                   fusion_config->debugshm, &data->shmpool );
     if (ret)
          return ret;

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_INTERNAL;
     ret_desc->priority          = (dfb_system_caps() & CSCAPS_PREFER_SHM) ? CSPP_PREFERED : CSPP_DEFAULT;

     if (dfb_system_caps() & CSCAPS_SYSMEM_EXTERNAL)
          ret_desc->types |= CSTF_EXTERNAL;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Shared Memory" );

     return DFB_OK;
}

static DFBResult
sharedDestroyPool( CoreSurfacePool *pool,
                   void            *pool_data,
                   void            *pool_local )
{
     SharedPoolData      *data  = pool_data;
     SharedPoolLocalData *local = pool_local;

     D_DEBUG_AT( Core_Shared, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     fusion_shm_pool_destroy( local->world, data->shmpool );

     return DFB_OK;
}

static DFBResult
sharedAllocateBuffer( CoreSurfacePool       *pool,
                      void                  *pool_data,
                      void                  *pool_local,
                      CoreSurfaceBuffer     *buffer,
                      CoreSurfaceAllocation *allocation,
                      void                  *alloc_data )
{
     CoreSurface          *surface;
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_Shared, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     surface = buffer->surface;

     /* Create aligned shared system surface buffer if both base address and pitch are non-zero. */
     if (dfb_config->system_surface_align_base && dfb_config->system_surface_align_pitch) {
          /* Make sure base address and pitch are a positive power of two. */
          D_ASSERT( dfb_config->system_surface_align_base >= 2 );
          D_ASSERT( !(dfb_config->system_surface_align_base & (dfb_config->system_surface_align_base - 1)) );
          D_ASSERT( dfb_config->system_surface_align_pitch >= 2 );
          D_ASSERT( !(dfb_config->system_surface_align_pitch & (dfb_config->system_surface_align_pitch - 1)) );

          dfb_surface_calc_buffer_size( surface, dfb_config->system_surface_align_pitch, 0,
                                        &alloc->pitch, &alloc->size );

          alloc->addr = SHMALLOC( data->shmpool, alloc->size + dfb_config->system_surface_align_base );
          if (!alloc->addr)
               return D_OOSHM();

          /* Calculate the aligned address. */
          unsigned long addr           = (unsigned long) alloc->addr;
          unsigned long aligned_offset = dfb_config->system_surface_align_base -
                                         (addr % dfb_config->system_surface_align_base );

          alloc->aligned_addr = (void*) (addr + aligned_offset);
     }
     /* Create un-aligned shared system surface buffer. */
     else {
          dfb_surface_calc_buffer_size( surface, 8, 0, &alloc->pitch, &alloc->size );

          alloc->addr = SHMALLOC( data->shmpool, alloc->size );
          if (!alloc->addr)
               return D_OOSHM();

          alloc->aligned_addr = NULL;
     }

     allocation->flags = CSALF_VOLATILE;
     allocation->size  = alloc->size;

     return DFB_OK;
}

static DFBResult
sharedDeallocateBuffer( CoreSurfacePool       *pool,
                        void                  *pool_data,
                        void                  *pool_local,
                        CoreSurfaceBuffer     *buffer,
                        CoreSurfaceAllocation *allocation,
                        void                  *alloc_data )
{
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_Shared, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     SHFREE( data->shmpool, alloc->addr );

     return DFB_OK;
}

static DFBResult
sharedLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     SharedAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_Shared, "%s() <- size %d\n", __FUNCTION__, alloc->size );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     /* Provide aligned address if one's available, otherwise the un-aligned one. */
     if (alloc->aligned_addr)
          lock->addr = alloc->aligned_addr;
     else
          lock->addr = alloc->addr;

     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
sharedUnlock( CoreSurfacePool       *pool,
              void                  *pool_data,
              void                  *pool_local,
              CoreSurfaceAllocation *allocation,
              void                  *alloc_data,
              CoreSurfaceBufferLock *lock )
{
     D_DEBUG_AT( Core_Shared, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     return DFB_OK;
}

const SurfacePoolFuncs sharedSurfacePoolFuncs = {
     .PoolDataSize       = sharedPoolDataSize,
     .PoolLocalDataSize  = sharedPoolLocalDataSize,
     .AllocationDataSize = sharedAllocationDataSize,
     .InitPool           = sharedInitPool,
     .DestroyPool        = sharedDestroyPool,
     .AllocateBuffer     = sharedAllocateBuffer,
     .DeallocateBuffer   = sharedDeallocateBuffer,
     .Lock               = sharedLock,
     .Unlock             = sharedUnlock,
};
