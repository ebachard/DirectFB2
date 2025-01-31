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
#include <core/state.h>
#include <core/palette.h>
#include <gfx/convert.h>
#include <gfx/generic/generic.h>
#include <gfx/generic/generic_util.h>
#include <gfx/util.h>

/**********************************************************************************************************************/

#if DFB_SMOOTH_SCALING

typedef struct {
     DFBRegion      clip;
     const void    *colors;
     unsigned long  protect;
     unsigned long  key;
} StretchCtx;

typedef void (*StretchHVx)( void             *dst,
                            int               dpitch,
                            const void       *src,
                            int               spitch,
                            int               width,
                            int               height,
                            int               dst_width,
                            int               dst_height,
                            const StretchCtx *ctx );

#define STRETCH_NONE           0
#define STRETCH_SRCKEY         1
#define STRETCH_PROTECT        2
#define STRETCH_SRCKEY_PROTECT 3
#define STRETCH_NUM            4

typedef struct {
     struct {
          StretchHVx up[STRETCH_NUM];
          StretchHVx down[STRETCH_NUM];
     } f[DFB_NUM_PIXELFORMATS];
} StretchFunctionTable;

/**********************************************************************************************************************/
/*** 16 bit RGB 565 scalers *******************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_RGB16
#define TABLE_NAME              stretch_hvx_RGB16
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_RGB16_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R5                5
#define SHIFT_R6                6
#define X_F81F                  0xf81f
#define X_07E0                  0x07e0
#define MASK_RGB                0xffff

#define FORMAT_RGB16
#include "stretch_up_down_16.h"
#undef FORMAT_RGB16

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0
#undef MASK_RGB

/**********************************************************************************************************************/
/*** 32 bit RGB 888 scalers *******************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_RGB32
#define TABLE_NAME              stretch_hvx_RGB32
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_RGB32_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R8                8
#define SHIFT_L8                8
#define X_00FF00FF              0x00ff00ff
#define X_FF00FF00              0x0000ff00
#define MASK_RGB                0x00ffffff

#include "stretch_up_down_32.h"

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R8
#undef SHIFT_L8
#undef X_00FF00FF
#undef X_FF00FF00
#undef MASK_RGB

/**********************************************************************************************************************/
/*** 32 bit ARGB 8888 scalers *****************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_ARGB
#define TABLE_NAME              stretch_hvx_ARGB
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_ARGB_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R8                8
#define SHIFT_L8                8
#define X_00FF00FF              0x00ff00ff
#define X_FF00FF00              0xff00ff00
#define MASK_RGB                0x00ffffff
#define HAS_ALPHA

#include "stretch_up_down_32.h"

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R8
#undef SHIFT_L8
#undef X_00FF00FF
#undef X_FF00FF00
#undef MASK_RGB
#undef HAS_ALPHA

/**********************************************************************************************************************/
/*** 16 bit ARGB 4444 scalers *****************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_ARGB4444
#define TABLE_NAME              stretch_hvx_ARGB4444
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_ARGB4444_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R5                4
#define SHIFT_R6                4
#define X_F81F                  0x0f0f
#define X_07E0                  0xf0f0
#define MASK_RGB                0x0fff
#define HAS_ALPHA

#define FORMAT_ARGB4444
#include "stretch_up_down_16.h"
#undef FORMAT_ARGB4444

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0
#undef MASK_RGB
#undef HAS_ALPHA

/**********************************************************************************************************************/
/*** 16 bit RGBA 4444 scalers *****************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_RGBA4444
#define TABLE_NAME              stretch_hvx_RGBA4444
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_RGBA4444_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R5                4
#define SHIFT_R6                4
#define X_F81F                  0x0f0f
#define X_07E0                  0xf0f0
#define MASK_RGB                0xfff0
#define HAS_ALPHA

#include "stretch_up_down_16.h"

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0
#undef MASK_RGB
#undef HAS_ALPHA

static const StretchFunctionTable *stretch_tables[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = &stretch_hvx_RGB16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = &stretch_hvx_RGB32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = &stretch_hvx_ARGB,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = &stretch_hvx_ARGB4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = &stretch_hvx_RGBA4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUV444P)]    = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
};

/**********************************************************************************************************************/
/*** NV12 / NV21 8 bit scalers ****************************************************************************************/
/**********************************************************************************************************************/

#define FUNC_NAME(UPDOWN) stretch_hvx_8_ ## UPDOWN

#include "stretch_up_down_8.h"

#undef FUNC_NAME

#define FUNC_NAME(UPDOWN) stretch_hvx_88_ ## UPDOWN

#include "stretch_up_down_88.h"

#undef FUNC_NAME

__attribute__((noinline))
static bool
stretch_hvx_planar( CardState    *state,
                    DFBRectangle *srect,
                    DFBRectangle *drect,
                    bool          down )
{
     GenefxState *gfxs;
     void        *dst;
     void        *src;
     DFBRegion    clip;

     D_ASSERT( state != NULL );
     DFB_RECTANGLE_ASSERT( srect );
     DFB_RECTANGLE_ASSERT( drect );

     gfxs = state->gfxs;

     if (state->blittingflags)
          return false;

     if (gfxs->dst_format != gfxs->src_format)
          return false;

     clip = state->clip;

     if (!dfb_region_rectangle_intersect( &clip, drect ))
          return false;

     dfb_region_translate( &clip, - drect->x, - drect->y );

     dst = gfxs->dst_org[0] + drect->y * gfxs->dst_pitch + DFB_BYTES_PER_LINE( gfxs->dst_format, drect->x );
     src = gfxs->src_org[0] + srect->y * gfxs->src_pitch + DFB_BYTES_PER_LINE( gfxs->src_format, srect->x );

     switch (gfxs->dst_format) {
          case DSPF_NV12:
          case DSPF_NV21:
               if (down)
                    stretch_hvx_8_down( dst, gfxs->dst_pitch, src, gfxs->src_pitch,
                                        srect->w, srect->h, drect->w, drect->h, &clip );
               else
                    stretch_hvx_8_up( dst, gfxs->dst_pitch, src, gfxs->src_pitch,
                                      srect->w, srect->h, drect->w, drect->h, &clip );

               clip.x1 /= 2;
               clip.x2 /= 2;
               clip.y1 /= 2;
               clip.y2 /= 2;

               dst = gfxs->dst_org[1] + drect->y/2 * gfxs->dst_pitch + DFB_BYTES_PER_LINE( gfxs->dst_format, drect->x );
               src = gfxs->src_org[1] + srect->y/2 * gfxs->src_pitch + DFB_BYTES_PER_LINE( gfxs->src_format, srect->x );

               if (down)
                    stretch_hvx_88_down( dst, gfxs->dst_pitch, src, gfxs->src_pitch,
                                         srect->w / 2, srect->h, drect->w / 2, drect->h, &clip );
               else
                    stretch_hvx_88_up( dst, gfxs->dst_pitch, src, gfxs->src_pitch,
                                       srect->w / 2, srect->h, drect->w / 2, drect->h, &clip );
               break;

          default:
               break;
     }

     return true;
}

__attribute__((noinline))
static bool
stretch_hvx( CardState    *state,
             DFBRectangle *srect,
             DFBRectangle *drect )
{
     GenefxState                *gfxs;
     const StretchFunctionTable *table;
     StretchHVx                  stretch;
     void                       *dst;
     void                       *src;
     StretchCtx                  ctx;
     u32                         colors[256];
     bool                        down = false;
     int                         idx  = STRETCH_NONE;

     D_ASSERT( state != NULL );
     DFB_RECTANGLE_ASSERT( srect );
     DFB_RECTANGLE_ASSERT( drect );

     gfxs = state->gfxs;

     if (srect->w > drect->w && srect->h > drect->h)
          down = true;

     if (down) {
          if (!(state->render_options & DSRO_SMOOTH_DOWNSCALE))
               return false;
     }
     else {
          if (!(state->render_options & DSRO_SMOOTH_UPSCALE))
               return false;
     }

     switch (gfxs->dst_format) {
          case DSPF_NV12:
          case DSPF_NV21:
               return stretch_hvx_planar( state, srect, drect, down );

          default:
               break;
     }

     if (state->blittingflags & ~(DSBLIT_COLORKEY_PROTECT | DSBLIT_SRC_COLORKEY | DSBLIT_SRC_PREMULTIPLY))
          return false;

     if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY && !DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format ))
          return false;

     if (DFB_PIXELFORMAT_INDEX(gfxs->dst_format) >= D_ARRAY_SIZE(stretch_tables))
          return false;

     if (DFB_PIXELFORMAT_INDEX(gfxs->src_format) >= D_ARRAY_SIZE((stretch_tables[0])->f))
          return false;

     table = stretch_tables[DFB_PIXELFORMAT_INDEX(gfxs->dst_format)];
     if (!table)
          return false;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          idx |= STRETCH_SRCKEY;

     if (state->blittingflags & DSBLIT_COLORKEY_PROTECT)
          idx |= STRETCH_PROTECT;

     if (down)
          stretch = table->f[DFB_PIXELFORMAT_INDEX(gfxs->src_format)].down[idx];
     else
          stretch = table->f[DFB_PIXELFORMAT_INDEX(gfxs->src_format)].up[idx];

     if (!stretch)
          return false;

     ctx.clip = state->clip;

     if (!dfb_region_rectangle_intersect( &ctx.clip, drect ))
          return false;

     dfb_region_translate( &ctx.clip, - drect->x, - drect->y );

     if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format )) {
          int             i;
          const DFBColor *entries;
          u16            *colors16 = (void*) colors;

          D_ASSERT( gfxs->Blut != NULL );

          entries = gfxs->Blut->entries;

          switch (gfxs->dst_format) {
               case DSPF_ARGB:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i = 0; i < gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors[i] = 0;
                                        break;

                                   case 255:
                                        colors[i] = PIXEL_ARGB( entries[i].a,
                                                                entries[i].r,
                                                                entries[i].g,
                                                                entries[i].b );
                                        break;

                                   default:
                                        colors[i] = PIXEL_ARGB( entries[i].a,
                                                                (alpha * entries[i].r) >> 8,
                                                                (alpha * entries[i].g) >> 8,
                                                                (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i = 0; i < gfxs->Blut->num_entries; i++)
                              colors[i] = PIXEL_ARGB( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_ABGR:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i = 0; i < gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors[i] = 0;
                                        break;

                                   case 255:
                                        colors[i] = PIXEL_ABGR( entries[i].a,
                                                                entries[i].r,
                                                                entries[i].g,
                                                                entries[i].b );
                                        break;

                                   default:
                                        colors[i] = PIXEL_ABGR( entries[i].a,
                                                                (alpha * entries[i].r) >> 8,
                                                                (alpha * entries[i].g) >> 8,
                                                                (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i = 0; i < gfxs->Blut->num_entries; i++)
                              colors[i] = PIXEL_ABGR( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_RGBAF88871:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i = 0; i < gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors[i] = 0;
                                        break;

                                   case 255:
                                        colors[i] = PIXEL_RGBAF88871( entries[i].a,
                                                                      entries[i].r,
                                                                      entries[i].g,
                                                                      entries[i].b );
                                        break;

                                   default:
                                        colors[i] = PIXEL_RGBAF88871( entries[i].a,
                                                                      (alpha * entries[i].r) >> 8,
                                                                      (alpha * entries[i].g) >> 8,
                                                                      (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i = 0; i < gfxs->Blut->num_entries; i++)
                              colors[i] = PIXEL_RGBAF88871( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_RGB32:
                    for (i = 0; i < gfxs->Blut->num_entries; i++)
                         colors[i] = PIXEL_RGB32( entries[i].r, entries[i].g, entries[i].b );
                    break;

               case DSPF_RGB16:
                    for (i = 0; i < gfxs->Blut->num_entries; i++)
                         colors16[i] = PIXEL_RGB16( entries[i].r, entries[i].g, entries[i].b );
                    break;

               case DSPF_ARGB4444:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i = 0; i < gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors16[i] = 0;
                                        break;

                                   case 255:
                                        colors16[i] = PIXEL_ARGB4444( entries[i].a,
                                                                      entries[i].r,
                                                                      entries[i].g,
                                                                      entries[i].b );
                                        break;

                                   default:
                                        colors16[i] = PIXEL_ARGB4444( entries[i].a,
                                                                      (alpha * entries[i].r) >> 8,
                                                                      (alpha * entries[i].g) >> 8,
                                                                      (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i = 0; i < gfxs->Blut->num_entries; i++)
                              colors16[i] = PIXEL_ARGB4444( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_RGBA4444:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i = 0; i < gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors16[i] = 0;
                                        break;

                                   case 255:
                                        colors16[i] = PIXEL_RGBA4444( entries[i].a,
                                                                      entries[i].r,
                                                                      entries[i].g,
                                                                      entries[i].b );
                                        break;

                                   default:
                                        colors16[i] = PIXEL_RGBA4444( entries[i].a,
                                                                      (alpha * entries[i].r) >> 8,
                                                                      (alpha * entries[i].g) >> 8,
                                                                      (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i = 0; i < gfxs->Blut->num_entries; i++)
                              colors16[i] = PIXEL_RGBA4444( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_RGB444:
                    for (i = 0; i < gfxs->Blut->num_entries; i++)
                         colors16[i] = PIXEL_RGB444( entries[i].r, entries[i].g, entries[i].b );
                    break;

               default:
                    D_UNIMPLEMENTED();
          }

          ctx.colors = colors;

          if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
               if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->dst_format ))
                    ctx.key = state->src_colorkey;
               else {
                    const DFBColor *color = &entries[state->src_colorkey % gfxs->Blut->num_entries];

                    ctx.key = dfb_color_to_pixel( gfxs->dst_format, color->r, color->g, color->b );
               }
          }
     }
     else {
          ctx.colors = NULL;

          if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
               DFBColor color;

               dfb_pixel_to_color( gfxs->src_format, state->src_colorkey, &color );

               ctx.key = dfb_color_to_pixel( gfxs->dst_format, color.r, color.g, color.b );
          }
     }

     if (state->blittingflags & DSBLIT_COLORKEY_PROTECT) {
          if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->dst_format ))
               ctx.protect = state->colorkey.index;
          else
               ctx.protect = dfb_color_to_pixel( gfxs->dst_format,
                                                 state->colorkey.r,
                                                 state->colorkey.g,
                                                 state->colorkey.b );
     }

     dst = gfxs->dst_org[0] + drect->y * gfxs->dst_pitch + DFB_BYTES_PER_LINE( gfxs->dst_format, drect->x );
     src = gfxs->src_org[0] + srect->y * gfxs->src_pitch + DFB_BYTES_PER_LINE( gfxs->src_format, srect->x );

     stretch( dst, gfxs->dst_pitch, src, gfxs->src_pitch, srect->w, srect->h, drect->w, drect->h, &ctx );

     return true;
}

#endif /* DFB_SMOOTH_SCALING */

/**********************************************************************************************************************/

typedef void (*XopAdvanceFunc)( GenefxState *gfxs );

void
gStretchBlit( CardState    *state,
              DFBRectangle *srect,
              DFBRectangle *drect )
{
     GenefxState             *gfxs;
     XopAdvanceFunc           Aop_advance;
     XopAdvanceFunc           Bop_advance;
     int                      Aop_X;
     int                      Aop_Y;
     int                      Bop_X;
     int                      Bop_Y;
     int                      fx, fy;
     int                      ix, iy;
     int                      h;
     DFBRectangle             orect = *drect;
     bool                     rotated = false;
     DFBSurfaceBlittingFlags  rotflip_blittingflags;

     D_ASSERT( state != NULL );
     D_ASSERT( state->gfxs != NULL );

     gfxs = state->gfxs;

     rotflip_blittingflags = state->blittingflags;

     dfb_simplify_blittingflags( &rotflip_blittingflags );
     rotflip_blittingflags &= (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL | DSBLIT_ROTATE90 );

     if (rotflip_blittingflags & DSBLIT_ROTATE90)
          rotated = true;

     if (dfb_config->software_warn) {
          D_WARN( "StretchBlit (%4d,%4d-%4dx%4d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x <- (%4d,%4d-%4dx%4d) %6s",
                  drect->x, drect->y, drect->w, drect->h, dfb_pixelformat_name( gfxs->dst_format ),
                  state->blittingflags, state->color.a, state->color.r, state->color.g, state->color.b,
                  srect->x, srect->y, srect->w, srect->h, dfb_pixelformat_name( gfxs->src_format ) );
     }

     CHECK_PIPELINE();

#if DFB_SMOOTH_SCALING
     if (state->render_options & (DSRO_SMOOTH_UPSCALE | DSRO_SMOOTH_DOWNSCALE) && stretch_hvx( state, srect, drect ))
          return;
#endif

     /* Clip destination rectangle. */
     if (!dfb_rectangle_intersect_by_region( drect, &state->clip ))
          return;

     /* Calculate fractions */
     if (rotated) {
          fx = (srect->h << 16) / orect.w;
          fy = (srect->w << 16) / orect.h;
     }
     else {
          fx = (srect->w << 16) / orect.w;
          fy = (srect->h << 16) / orect.h;
     }

     /* Calculate horizontal and vertical phase. */

     switch (rotflip_blittingflags & (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL | DSBLIT_ROTATE90)) {
          case DSBLIT_NOFX:
               ix = fx * (drect->x - orect.x);
               iy = fy * (drect->y - orect.y);
               break;
          case DSBLIT_FLIP_HORIZONTAL:
               ix = fx * ((orect.x + orect.w - 1) - (drect->x + drect->w - 1));
               iy = fy * (drect->y - orect.y);
               break;
          case DSBLIT_FLIP_VERTICAL:
               ix = fx * (drect->x - orect.x);
               iy = fy * ((orect.y + orect.h - 1) - (drect->y + drect->h - 1));
               break;
          case DSBLIT_ROTATE90:
               ix = fx * (drect->x - orect.x);
               iy = fy * ((orect.y + orect.h - 1) - (drect->y + drect->h - 1));
               break;
          case DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL: // ROTATE180
               ix = fx * ((orect.x + orect.w - 1) - (drect->x + drect->w - 1));
               iy = fy * ((orect.y + orect.h - 1) - (drect->y + drect->h - 1));
               break;
          case DSBLIT_ROTATE90 | DSBLIT_FLIP_VERTICAL | DSBLIT_FLIP_HORIZONTAL: // ROTATE270
               ix = fx * ((orect.x + orect.w - 1) - (drect->x + drect->w - 1));
               iy = fy * (drect->y - orect.y);
               break;
          case DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL:
               ix = fx * (drect->x - orect.x);
               iy = fy * (drect->y - orect.y);
               break;
          case DSBLIT_ROTATE90 | DSBLIT_FLIP_VERTICAL:
               ix = fx * ((orect.x + orect.w - 1) - (drect->x + drect->w - 1));
               iy = fy * ((orect.y + orect.h - 1) - (drect->y + drect->h - 1));
               break;
          default:
              ix = 0;
              iy = 0;
     }

     /* Adjust source size. */
     if (rotated) {
          srect->x += iy >> 16;
          srect->y += ix >> 16;
          ix &= 0xffff;
          iy &= 0xffff;
          srect->w = ((drect->h * fy + iy) + 0xffff) >> 16;
          srect->h = ((drect->w * fx + ix) + 0xffff) >> 16;
     }
     else {
          srect->x += ix >> 16;
          srect->y += iy >> 16;
          ix &= 0xffff;
          iy &= 0xffff;
          srect->w = ((drect->w * fx + ix) + 0xffff) >> 16;
          srect->h = ((drect->h * fy + iy) + 0xffff) >> 16;
     }

     D_ASSERT( srect->x + srect->w <= state->source->config.size.w );
     D_ASSERT( srect->y + srect->h <= state->source->config.size.h );
     D_ASSERT( drect->x + drect->w <= state->clip.x2 + 1 );
     D_ASSERT( drect->y + drect->h <= state->clip.y2 + 1 );

     if (!Genefx_ABacc_prepare( gfxs, MAX( srect->w, drect->w ) ))
          return;

     switch (gfxs->src_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               srect->x &= ~1;
               break;
          default:
               break;
     }

     switch (gfxs->dst_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               drect->x &= ~1;
               break;
          default:
               break;
     }

     if (rotated) {
          gfxs->Dlen   = drect->h;
          gfxs->SperD  = fy;
          gfxs->Xphase = iy;

          h = drect->w;
     }
     else {
          gfxs->Dlen   = drect->w;
          gfxs->SperD  = fx;
          gfxs->Xphase = ix;

          h = drect->h;
     }

     gfxs->Slen   = srect->w;
     gfxs->length = gfxs->Dlen;

     Aop_X = drect->x;
     Aop_Y = drect->y;

     Bop_X = srect->x;
     Bop_Y = srect->y;

     Bop_advance = Genefx_Bop_next;
     Aop_advance = Genefx_Aop_next;

     switch ((unsigned int) rotflip_blittingflags) {
          case DSBLIT_FLIP_HORIZONTAL:
               gfxs->Astep *= -1;
               Aop_X += (drect->w - 1);
               break;

          case DSBLIT_FLIP_VERTICAL:
               Aop_Y += (drect->h - 1);
               Aop_advance = Genefx_Aop_prev;
               break;

          case DSBLIT_ROTATE90: // 90 deg ccw
               Aop_Y = drect->y + drect->h - 1;
               gfxs->Astep *= -gfxs->dst_pitch / gfxs->dst_bpp;
               Aop_advance = Genefx_Aop_crab;
               break;

          case DSBLIT_FLIP_VERTICAL | DSBLIT_FLIP_HORIZONTAL: // 180 deg
               gfxs->Astep *= -1;
               Aop_X += (drect->w - 1);
               Aop_Y += (drect->h - 1);
               Aop_advance = Genefx_Aop_prev;
               break;

          case DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL: // 270 deg ccw
               gfxs->Astep *= gfxs->dst_pitch / gfxs->dst_bpp;
               Bop_Y = srect->y + srect->h - 1;
               Aop_advance = Genefx_Aop_crab;
               Bop_advance = Genefx_Bop_prev;
               break;

          case DSBLIT_ROTATE90 | DSBLIT_FLIP_VERTICAL:
               gfxs->Astep *= -gfxs->dst_pitch / gfxs->dst_bpp;
               Aop_X = drect->x + drect->w - 1;
               Aop_Y = drect->y + drect->h - 1;
               Aop_advance = Genefx_Aop_prev_crab;
               break;

          case DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL:
               gfxs->Astep *= gfxs->dst_pitch / gfxs->dst_bpp;
               Aop_advance = Genefx_Aop_crab;
               break;

          default:
               break;
     }

     Genefx_Aop_xy( gfxs, Aop_X, Aop_Y );
     Genefx_Bop_xy( gfxs, Bop_X, Bop_Y );

     while (h--) {
          RUN_PIPELINE();

          Aop_advance( gfxs );

          iy += rotated ? fx : fy;

          while (iy > 0xffff) {
               iy -= 0x10000;
               Bop_advance( gfxs );
          }
     }

     Genefx_ABacc_flush( gfxs );
}
