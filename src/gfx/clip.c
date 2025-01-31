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

#include <direct/util.h>
#include <directfb_util.h>
#include <gfx/clip.h>

D_DEBUG_DOMAIN( GFX_Clipping, "GFX/Clipping", "DirectFB Graphics Clipping" );

/**********************************************************************************************************************/

#define REGION_CODE(x,y,cx1,cx2,cy1,cy2) ( ((y) > (cy2) ? 8 : 0) | \
                                           ((y) < (cy1) ? 4 : 0) | \
                                           ((x) > (cx2) ? 2 : 0) | \
                                           ((x) < (cx1) ? 1 : 0) )

DFBBoolean
dfb_clip_line( const DFBRegion *clip,
               DFBRegion       *line )
{
     unsigned char region_code1 = REGION_CODE( line->x1, line->y1, clip->x1, clip->x2, clip->y1, clip->y2 );
     unsigned char region_code2 = REGION_CODE( line->x2, line->y2, clip->x1, clip->x2, clip->y1, clip->y2 );

     D_DEBUG_AT( GFX_Clipping, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( GFX_Clipping, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     while (region_code1 | region_code2) {
          /* Line completely outside the clipping rectangle. */
          if (region_code1 & region_code2)
               return DFB_FALSE;

          if (region_code1) {
               if (region_code1 & 8) { /* divide line at bottom */
                    line->x1 = line->x1 +(line->x2-line->x1) * (clip->y2 - line->y1) / (line->y2-line->y1);
                    line->y1 = clip->y2;
               }
               else
                    if (region_code1 & 4) { /* divide line at top */
                    line->x1 = line->x1 +(line->x2-line->x1) * (clip->y1 - line->y1) / (line->y2-line->y1);
                    line->y1 = clip->y1;
               }
               else
                    if (region_code1 & 2) { /* divide line at right */
                    line->y1 = line->y1 +(line->y2-line->y1) * (clip->x2 - line->x1) / (line->x2-line->x1);
                    line->x1 = clip->x2;
               }
               else
                    if (region_code1 & 1) { /* divide line at right */
                    line->y1 = line->y1 +(line->y2-line->y1) * (clip->x1 - line->x1) / (line->x2-line->x1);
                    line->x1 = clip->x1;
               }

               region_code1 = REGION_CODE( line->x1, line->y1, clip->x1, clip->x2, clip->y1, clip->y2 );
          }
          else {
               if (region_code2 & 8) { /* divide line at bottom */
                    line->x2 = line->x1 +(line->x2-line->x1) * (clip->y2 - line->y1) / (line->y2-line->y1);
                    line->y2 = clip->y2;
               }
               else
                    if (region_code2 & 4) { /* divide line at top */
                    line->x2 = line->x1 +(line->x2-line->x1) * (clip->y1 - line->y1) / (line->y2-line->y1);
                    line->y2 = clip->y1;
               }
               else
                    if (region_code2 & 2) { /* divide line at right */
                    line->y2 = line->y1 +(line->y2-line->y1) * (clip->x2 - line->x1) / (line->x2-line->x1);
                    line->x2 = clip->x2;
               }
               else
                    if (region_code2 & 1) { /* divide line at right */
                    line->y2 = line->y1 +(line->y2-line->y1) * (clip->x1 - line->x1) / (line->x2-line->x1);
                    line->x2 = clip->x1;
               }

               region_code2 = REGION_CODE( line->x2, line->y2, clip->x1, clip->x2, clip->y1, clip->y2 );
          }
     }

     D_DEBUG_AT( GFX_Clipping, "  -> %4d,%4d-%4d,%4d\n", DFB_LINE_VALS( line ) );

     /* Successfully clipped or clipping not neccessary. */
     return DFB_TRUE;
}

DFBBoolean
dfb_clip_rectangle( const DFBRegion *clip,
                    DFBRectangle    *rect )
{
     D_DEBUG_AT( GFX_Clipping, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( GFX_Clipping, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     if ((clip->x1 >= rect->x + rect->w) || (clip->x2 < rect->x) ||
         (clip->y1 >= rect->y + rect->h) || (clip->y2 < rect->y))
          return DFB_FALSE;

     if (clip->x1 > rect->x) {
          rect->w += rect->x - clip->x1;
          rect->x  = clip->x1;
     }

     if (clip->y1 > rect->y) {
          rect->h += rect->y - clip->y1;
          rect->y  = clip->y1;
     }

     if (clip->x2 < rect->x + rect->w - 1)
          rect->w = clip->x2 - rect->x + 1;

     if (clip->y2 < rect->y + rect->h - 1)
          rect->h = clip->y2 - rect->y + 1;

     D_DEBUG_AT( GFX_Clipping, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( rect ) );

     return DFB_TRUE;
}

DFBBoolean
dfb_clip_triangle( const DFBRegion   *clip,
                   const DFBTriangle *tri,
                   DFBPoint           p[6],
                   int               *num )
{
     DFBRegion edges[3];
     int       num_edges;
     int       i, n;
     DFBPoint  p1 = { 0, 0 }, p2 = { 0, 0 };

     D_DEBUG_AT( GFX_Clipping, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( GFX_Clipping, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     /* Initialize edges. */
     edges[0].x1 = tri->x1; edges[0].y1 = tri->y1;
     edges[0].x2 = tri->x2; edges[0].y2 = tri->y2;
     edges[1].x1 = tri->x2; edges[1].y1 = tri->y2;
     edges[1].x2 = tri->x3; edges[1].y2 = tri->y3;
     edges[2].x1 = tri->x3; edges[2].y1 = tri->y3;
     edges[2].x2 = tri->x1; edges[2].y2 = tri->y1;
     num_edges = 3;

     for (i = 0; i < num_edges; i++) {
          DFBRegion *reg = &edges[i];
          DFBRegion  line;
          bool       i1, i2;

          /* Clip the edge to the clipping region. */
          line = *reg;
          if (dfb_clip_line( clip, &line )) {
               *reg = line;
               continue;
          }

          /* If the edge doesn't intersect clipping region, then intersect the edge with the diagonals of the clipping
           * rectangle.
           * If intersection point exits, add the nearest corner of the clipping region to the list of vertices. */

          /* Diagonal (x1,y1) (x2,y2). */
          line = (DFBRegion) { clip->x1, clip->y1, clip->x2, clip->y2 };
          i1 = dfb_line_segment_intersect( &line, reg, &p1.x, &p1.y );
          if (i1) {
               /* Get nearest corner. */
               if (p1.x <= clip->x1 || p1.y <= clip->y1) {
                    p1.x = clip->x1;
                    p1.y = clip->y1;
               }
               else {
                    p1.x = clip->x2;
                    p1.y = clip->y2;
               }
          }

          /* Diagonal (x2,y1) (x1,y2). */
          line = (DFBRegion) { clip->x2, clip->y1, clip->x1, clip->y2 };
          i2 = dfb_line_segment_intersect( &line, reg, &p2.x, &p2.y );
          if (i2) {
               /* Get nearest corner. */
               if (p2.x >= clip->x2 || p2.y <= clip->y1) {
                    p2.x = clip->x2;
                    p2.y = clip->y1;
               }
               else {
                    p2.x = clip->x1;
                    p2.y = clip->y2;
               }
          }

          if (i1 && i2) {
               reg->x1 = p1.x;
               reg->y1 = p1.y;
               reg->x2 = p2.x;
               reg->y2 = p2.y;
          }
          else if (i1) {
               reg->x1 = reg->x2 = p1.x;
               reg->y1 = reg->y2 = p1.y;
          }
          else if (i2) {
               reg->x1 = reg->x2 = p2.x;
               reg->y1 = reg->y2 = p2.y;
          }
          else {
               /* Redudant edge. */
               memmove( reg, &edges[i+1], (num_edges - i - 1) * sizeof(DFBRegion) );
               num_edges--;
               i--;
          }
     }

     if (num_edges < 1) {
          *num = 0;
          return DFB_FALSE;
     }

     /* Get vertices from edges. */
     p[0].x = edges[0].x1; p[0].y = edges[0].y1;
     n = 1;
     if (edges[0].x2 != edges[0].x1 || edges[0].y2 != edges[0].y1) {
          p[1].x = edges[0].x2; p[1].y = edges[0].y2;
          n++;
     }

     for (i = 1; i < num_edges; i++) {
          if (edges[i].x1 != p[n-1].x || edges[i].y1 != p[n-1].y) {
               p[n].x = edges[i].x1; p[n].y = edges[i].y1;
               n++;
          }
          if (edges[i].x2 != p[n-1].x || edges[i].y2 != p[n-1].y) {
               p[n].x = edges[i].x2; p[n].y = edges[i].y2;
               n++;
          }
     }

     if (p[n-1].x == p[0].x && p[n-1].y == p[0].y)
          n--;

     *num = n;

     D_DEBUG_AT( GFX_Clipping, "  -> %4d,%4d-%4d,%4d-%4d,%4d\n", DFB_TRIANGLE_VALS( tri ) );

     /* Actually fail if the number of vertices is below 3. */
     return (n >= 3);
}

typedef enum {
     DFEF_NONE      = 0x00000000,

     DFEF_LEFT      = 0x00000001,
     DFEF_RIGHT     = 0x00000002,
     DFEF_TOP       = 0x00000004,
     DFEF_BOTTOM    = 0x00000008,

     DFEF_ALL       = 0x0000000F
} DFBEdgeFlags;

/*
 * Clip the rectangle to the clipping region.
 * Return a flag for each edge that wasn't cut off.
 */
static DFBEdgeFlags
dfb_clip_edges( const DFBRegion *clip,
                DFBRectangle    *rect )
{
     DFBEdgeFlags flags = DFEF_ALL;

     if ((clip->x1 >= rect->x + rect->w) || (clip->x2 < rect->x) ||
         (clip->y1 >= rect->y + rect->h) || (clip->y2 < rect->y))
          return DFEF_NONE;

     if (clip->x1 > rect->x) {
          rect->w += rect->x - clip->x1;
          rect->x  = clip->x1;

          flags &= ~DFEF_LEFT;
     }

     if (clip->y1 > rect->y) {
          rect->h += rect->y - clip->y1;
          rect->y  = clip->y1;

          flags &= ~DFEF_TOP;
     }

     if (clip->x2 < rect->x + rect->w - 1) {
          rect->w = clip->x2 - rect->x + 1;

          flags &= ~DFEF_RIGHT;
     }

     if (clip->y2 < rect->y + rect->h - 1) {
          rect->h = clip->y2 - rect->y + 1;

          flags &= ~DFEF_BOTTOM;
     }

     return flags;
}

void
dfb_build_clipped_rectangle_outlines( DFBRectangle    *rect,
                                      const DFBRegion *clip,
                                      DFBRectangle    *ret_outlines,
                                      int             *ret_num )
{
     DFBEdgeFlags edges = dfb_clip_edges( clip, rect );
     int          t     = (edges & DFEF_TOP ? 1 : 0);
     int          tb    = t + (edges & DFEF_BOTTOM ? 1 : 0);
     int          num   = 0;

     D_DEBUG_AT( GFX_Clipping, "%s()\n", __FUNCTION__ );

     DFB_RECTANGLE_ASSERT( rect );

     D_ASSERT( ret_outlines != NULL );
     D_ASSERT( ret_num != NULL );

     if (edges & DFEF_TOP) {
          DFBRectangle *out = &ret_outlines[num++];

          out->x = rect->x;
          out->y = rect->y;
          out->w = rect->w;
          out->h = 1;
     }

     if (rect->h > t) {
          if (edges & DFEF_BOTTOM) {
               DFBRectangle *out = &ret_outlines[num++];

               out->x = rect->x;
               out->y = rect->y + rect->h - 1;
               out->w = rect->w;
               out->h = 1;
          }

          if (rect->h > tb) {
               if (edges & DFEF_LEFT) {
                    DFBRectangle *out = &ret_outlines[num++];

                    out->x = rect->x;
                    out->y = rect->y + t;
                    out->w = 1;
                    out->h = rect->h - tb;
               }

               if (rect->w > 1 || !(edges & DFEF_LEFT)) {
                    if (edges & DFEF_RIGHT) {
                         DFBRectangle *out = &ret_outlines[num++];

                         out->x = rect->x + rect->w - 1;
                         out->y = rect->y + t;
                         out->w = 1;
                         out->h = rect->h - tb;
                    }
               }
          }
     }

     *ret_num = num;
}

void
dfb_clip_blit( const DFBRegion *clip,
               DFBRectangle    *srect,
               int             *dx,
               int             *dy )
{
     D_DEBUG_AT( GFX_Clipping, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( GFX_Clipping, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     if (clip->x1 > *dx) {
          srect->w  = MIN( (clip->x2 - clip->x1) + 1, (*dx + srect->w) - clip->x1 );
          srect->x += clip->x1 - *dx;
          *dx = clip->x1;
     }
     else if (clip->x2 < *dx + srect->w - 1) {
          srect->w = clip->x2 - *dx + 1;
     }

     if (clip->y1 > *dy) {
          srect->h  = MIN( (clip->y2 - clip->y1) + 1, (*dy + srect->h) - clip->y1 );
          srect->y += clip->y1 - *dy;
          *dy = clip->y1;
     }
     else if (clip->y2 < *dy + srect->h - 1) {
          srect->h = clip->y2 - *dy + 1;
     }

     D_DEBUG_AT( GFX_Clipping, "  -> %4d,%4d-%4dx%4d\n -> %4d,%4d", DFB_RECTANGLE_VALS( srect ), *dx, *dy );
}

void
dfb_clip_stretchblit( const DFBRegion *clip,
                      DFBRectangle    *srect,
                      DFBRectangle    *drect )
{
     DFBRectangle orig_dst = *drect;

     D_DEBUG_AT( GFX_Clipping, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( GFX_Clipping, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     dfb_clip_rectangle( clip, drect );

     if (drect->x != orig_dst.x)
          srect->x += (drect->x - orig_dst.x) * (srect->w / (float) orig_dst.w);

     if (drect->y != orig_dst.y)
          srect->y += (drect->y - orig_dst.y) * (srect->h / (float) orig_dst.h);

     if (drect->w != orig_dst.w)
          srect->w = srect->w * (drect->w / (float) orig_dst.w);

     if (drect->h != orig_dst.h)
          srect->h = srect->h * (drect->h / (float) orig_dst.h);

     D_DEBUG_AT( GFX_Clipping, "  -> %4d,%4d-%4dx%4d -> %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS( srect ), DFB_RECTANGLE_VALS( drect ) );
}

void
dfb_clip_blit_flipped_rotated( const DFBRegion         *clip,
                               DFBRectangle            *srect,
                               DFBRectangle            *drect,
                               DFBSurfaceBlittingFlags  flags )
{

     DFBRegion dest    = DFB_REGION_INIT_FROM_RECTANGLE( drect );
     DFBRegion clipped = dest;

     D_DEBUG_AT( GFX_Clipping, "%s( 0x%08x )\n", __FUNCTION__, flags );

     D_DEBUG_AT( GFX_Clipping, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     D_ASSERT( !(flags & (DSBLIT_ROTATE270 | DSBLIT_ROTATE180)) );

     if (flags & DSBLIT_ROTATE90) {
          D_ASSERT( srect->w == drect->h );
          D_ASSERT( srect->h == drect->w );
     }
     else {
          D_ASSERT( srect->w == drect->w );
          D_ASSERT( srect->h == drect->h );
     }

     dfb_region_region_intersect( &clipped, clip );
     dfb_rectangle_from_region( drect, &clipped );

     switch (flags & (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL | DSBLIT_ROTATE90)) {
          case DSBLIT_NOFX:
               srect->x += clipped.x1 - dest.x1;
               srect->y += clipped.y1 - dest.y1;
               break;
          case DSBLIT_FLIP_HORIZONTAL:
               srect->x += dest.x2 - clipped.x2;
               srect->y += clipped.y1 - dest.y1;
               break;
          case DSBLIT_FLIP_VERTICAL:
               srect->x += clipped.x1 - dest.x1;
               srect->y += dest.y2 - clipped.y2;
               break;
          case DSBLIT_ROTATE90:
               srect->x += dest.y2 - clipped.y2;
               srect->y += clipped.x1 - dest.x1;
               break;
          case (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL): /* ROTATE180 */
               srect->x += dest.x2 - clipped.x2;
               srect->y += dest.y2 - clipped.y2;
               break;
          case (DSBLIT_ROTATE90 | DSBLIT_FLIP_VERTICAL | DSBLIT_FLIP_HORIZONTAL): /* ROTATE270 */
               srect->x += clipped.y1 - dest.y1;
               srect->y += dest.x2 - clipped.x2;
               break;
          case (DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL):
               srect->x += clipped.y1 - dest.y1;
               srect->y += clipped.x1 - dest.x1;
               break;
          case (DSBLIT_ROTATE90 | DSBLIT_FLIP_VERTICAL):
               srect->x += dest.y2 - clipped.y2;
               srect->y += dest.x2 - clipped.x2;
               break;
     }

     if (flags & DSBLIT_ROTATE90) {
          srect->w  = drect->h;
          srect->h  = drect->w;
     }
     else {
          srect->w = drect->w;
          srect->h = drect->h;
     }

     D_DEBUG_AT( GFX_Clipping, "  -> %4d,%4d-%4dx%4d -> %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS( srect ), DFB_RECTANGLE_VALS( drect ) );
}
