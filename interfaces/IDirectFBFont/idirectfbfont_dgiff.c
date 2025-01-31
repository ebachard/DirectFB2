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

#include <core/fonts.h>
#include <core/surface.h>
#include <dgiff.h>
#include <direct/filesystem.h>
#include <direct/hash.h>
#include <directfb_util.h>
#include <media/idirectfbfont.h>

D_DEBUG_DOMAIN( Font_DGIFF, "Font/DGIFF", "DGIFF Font Provider" );

static DFBResult Probe    ( IDirectFBFont_ProbeContext *ctx );

static DFBResult Construct( IDirectFBFont              *thiz,
                            CoreDFB                    *core,
                            IDirectFBFont_ProbeContext *ctx,
                            DFBFontDescription         *desc );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBFont, DGIFF )

/**********************************************************************************************************************/

typedef struct {
     void         *map;      /* memory map of the font file */
     int           size;     /* size of the memory map */

     CoreSurface **rows;     /* bitmaps of loaded glyphs */
     int           num_rows;
} DGIFFImplData;

/**********************************************************************************************************************/

static void
IDirectFBFont_DGIFF_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data      = thiz->priv;
     DGIFFImplData      *impl_data = data->font->impl_data;

     D_DEBUG_AT( Font_DGIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (impl_data->rows) {
          int i;

          for (i = 0; i < impl_data->num_rows; i++) {
               if (impl_data->rows[i])
                    dfb_surface_unref( impl_data->rows[i] );
          }

          D_FREE( impl_data->rows );
     }

     direct_file_unmap( impl_data->map, impl_data->size );

     D_FREE( impl_data );

     IDirectFBFont_Destruct( thiz );
}

static DirectResult
IDirectFBFont_DGIFF_Release( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font_DGIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBFont_DGIFF_Destruct( thiz );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBFont_ProbeContext *ctx )
{
     DFBResult   ret = DFB_OK;
     DirectFile  fd;
     DGIFFHeader header;
     size_t      bytes;

     if (!ctx->filename)
          return DFB_UNSUPPORTED;

     /* Open the file. */
     ret = direct_file_open( &fd, ctx->filename, O_RDONLY, 0 );
     if (ret) {
          D_DERROR( ret, "Font/DGIFF: Failed to open '%s'!\n", ctx->filename );
          return ret;
     }

     /* Read the header. */
     ret = direct_file_read( &fd, &header, sizeof(header), &bytes );
     if (bytes != sizeof(header)) {
          D_DERROR( ret, "Font/DGIFF: Failure reading "_ZU" bytes from '%s'!\n", sizeof(header), ctx->filename );
          goto out;
     }

     /* Check the magic. */
     if (strncmp( (const char*) header.magic, "DGIFF", 5 ))
          ret = DFB_UNSUPPORTED;

out:
     direct_file_close( &fd );

     return ret;
}

static DFBResult
Construct( IDirectFBFont              *thiz,
           CoreDFB                    *core,
           IDirectFBFont_ProbeContext *ctx,
           DFBFontDescription         *desc )
{
     DFBResult        ret;
     int              i;
     DirectFile       fd;
     DirectFileInfo   info;
     DGIFFFaceHeader *face;
     DGIFFGlyphInfo  *glyphs;
     DGIFFGlyphRow   *row;
     void            *ptr  = NULL;
     CoreFont        *font = NULL;
     DGIFFImplData   *data = NULL;
     DGIFFHeader     *header;

     D_DEBUG_AT( Font_DGIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (desc->flags & DFDESC_ROTATION) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_UNSUPPORTED;
     }

     /* Open the file. */
     ret = direct_file_open( &fd, ctx->filename, O_RDONLY, 0 );
     if (ret) {
          D_DERROR( ret, "Font/DGIFF: Failed to open '%s'!\n", ctx->filename );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Query file size. */
     ret = direct_file_get_info( &fd, &info );
     if (ret) {
          D_DERROR( ret, "Font/DGIFF: Failed during get_info() of '%s'!\n", ctx->filename );
          goto error;
     }

     /* Memory-mapped file. */
     ret = direct_file_map( &fd, NULL, 0, info.size, DFP_READ, &ptr );
     if (ret) {
          D_DERROR( ret, "Font/DGIFF: Failed during mmap() of '%s'!\n", ctx->filename );
          goto error;
     }

     direct_file_close( &fd );

     header = ptr;
     face   = ptr + sizeof(DGIFFHeader);

     /* Lookup requested face, otherwise use first if nothing requested. */
     if (desc->flags & DFDESC_HEIGHT) {
          for (i = 0; i < header->num_faces; i++) {
               if (face->size == desc->height)
                    break;

               face = (void*) face + face->next_face;
          }

          if (i == header->num_faces) {
               ret = DFB_UNSUPPORTED;
               D_ERROR( "Font/DGIFF: Requested size %d not found in '%s'!\n", desc->height, ctx->filename );
               goto error;
          }
     }

     glyphs = (void*) (face + 1);
     row    = (void*) (glyphs + face->num_glyphs);

     /* Create the font object. */
     ret = dfb_font_create( core, desc, ctx->filename, &font );
     if (ret)
          goto error;

     /* Fill font information. */
     if (face->blittingflags)
          font->blittingflags = face->blittingflags;

     font->pixel_format = face->pixelformat;
     font->surface_caps = DSCAPS_NONE;
     font->ascender     = face->ascender;
     font->descender    = face->descender;
     font->height       = face->height;
     font->maxadvance   = face->max_advance;
     font->up_unit_x    =  0.0;
     font->up_unit_y    = -1.0;
     font->flags        = CFF_SUBPIXEL_ADVANCE;

     CORE_FONT_DEBUG_AT( Font_DGIFF, font );

     /* Allocate implementation data. */
     data = D_CALLOC( 1, sizeof(DGIFFImplData) );
     if (!data) {
          ret = D_OOM();
          goto error;
     }

     data->map  = ptr;
     data->size = info.size;

     data->num_rows = face->num_rows;

     /* Allocate array for glyph cache rows. */
     data->rows = D_CALLOC( data->num_rows, sizeof(void*) );
     if (!data->rows) {
          ret = D_OOM();
          goto error;
     }

     /* Build glyph cache rows. */
     for (i = 0; i < data->num_rows; i++) {
          ret = dfb_surface_create_simple( core, row->width, row->height, face->pixelformat,
                                           DFB_COLORSPACE_DEFAULT( face->pixelformat ), DSCAPS_NONE, CSTF_NONE, 0, NULL,
                                           &data->rows[i] );
          if (ret) {
               D_DERROR( ret, "DGIFF/Font: Could not create %s %dx%d glyph row surface!\n",
                         dfb_pixelformat_name( face->pixelformat ), row->width, row->height );
               goto error;
          }

          dfb_surface_write_buffer( data->rows[i], DSBR_BACK, row + 1, row->pitch, NULL );

          /* Jump to next row. */
          row = (void*) (row + 1) + row->pitch * row->height;
     }

     /* Build glyph info. */
     for (i = 0; i < face->num_glyphs; i++) {
          CoreGlyphData  *glyph_data;
          DGIFFGlyphInfo *glyph = &glyphs[i];

          glyph_data = D_CALLOC( 1, sizeof(CoreGlyphData) );
          if (!glyph_data) {
               ret = D_OOM();
               goto error;
          }

          glyph_data->surface  = data->rows[glyph->row];
          glyph_data->start    = glyph->offset;
          glyph_data->width    = glyph->width;
          glyph_data->height   = glyph->height;
          glyph_data->left     = glyph->left;
          glyph_data->top      = glyph->top;
          glyph_data->xadvance = glyph->advance << 8;
          glyph_data->yadvance = 0;

          D_MAGIC_SET( glyph_data, CoreGlyphData );

          direct_hash_insert( font->layers[0].glyph_hash, glyph->unicode, glyph_data );

          if (glyph->unicode < 128)
               font->layers[0].glyph_data[glyph->unicode] = glyph_data;
     }

     font->impl_data = data;

     IDirectFBFont_Construct( thiz, font );

     thiz->Release = IDirectFBFont_DGIFF_Release;

     return DFB_OK;

error:
     if (font) {
          if (data) {
               if (data->rows) {
                    for (i = 0; i < data->num_rows; i++) {
                         if (data->rows[i])
                              dfb_surface_unref( data->rows[i] );
                    }

                    D_FREE( data->rows );
               }

               D_FREE( data );
          }

          dfb_font_destroy( font );
     }

     if (ptr)
          direct_file_unmap( ptr, info.size );

     direct_file_close( &fd );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}
