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

#include <core/layers.h>
#include <core/screens.h>

#include "drmkms_mode.h"

D_DEBUG_DOMAIN( DRMKMS_Screen, "DRMKMS/Screen", "DRM/KMS Screen" );

/**********************************************************************************************************************/

extern const DisplayLayerFuncs drmkmsPrimaryLayerFuncs;

static DFBResult
drmkmsInitScreen( CoreScreen           *screen,
                  void                 *driver_data,
                  void                 *screen_data,
                  DFBScreenDescription *description )
{
     DRMKMSData       *drmkms    = driver_data;
     DRMKMSDataShared *shared;
     drmModeRes       *resources;
     drmModeConnector *connector = NULL;
     drmModeEncoder   *encoder   = NULL;
     drmModeModeInfo  *mode;
     int               i, m;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );
     D_ASSERT( drmkms->resources != NULL );

     shared = drmkms->shared;

     resources = drmkms->resources;

     /* Set capabilities. */
     description->caps = DSCCAPS_MIXERS | DSCCAPS_ENCODERS | DSCCAPS_OUTPUTS;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "DRMKMS Screen" );

     for (i = 0; i < resources->count_connectors; i++) {
          connector = drmModeGetConnector( drmkms->fd, resources->connectors[i] );
          if (!connector)
               continue;

          if (connector->count_modes > 0) {
               if (connector->encoder_id) {
                    D_DEBUG_AT( DRMKMS_Screen, "  -> connector %u is bound to encoder %u\n",
                                connector->connector_id, connector->encoder_id );

                    encoder = drmModeGetEncoder( drmkms->fd, connector->encoder_id );
                    if (!encoder)
                         continue;
               }

               if (encoder->crtc_id) {
                    D_DEBUG_AT( DRMKMS_Screen, "  -> encoder %u is bound to ctrc %u\n",
                                connector->encoder_id, encoder->crtc_id );

                    drmkms->connector[shared->enabled_crtcs] = connector;
                    drmkms->encoder[shared->enabled_crtcs]   = encoder;

                    shared->mode[shared->enabled_crtcs] = connector->modes[0];

                    for (m = 0; m < connector->count_modes; m++)
                         D_DEBUG_AT( DRMKMS_Screen, "    => mode[%2d] is %ux%u@%uHz\n", m,
                                     connector->modes[m].hdisplay, connector->modes[m].vdisplay,
                                     connector->modes[m].vrefresh );

                    shared->enabled_crtcs++;

                    if ((!shared->mirror_outputs && !shared->multihead_outputs) || (shared->enabled_crtcs == 8))
                         break;

                    if (shared->multihead_outputs && shared->enabled_crtcs > 1) {
                         dfb_layers_register( screen, drmkms, &drmkmsPrimaryLayerFuncs );

                         DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[shared->enabled_crtcs-1],
                                                   drmkms->layer_id_next++ );
                    }
               }
          }
     }

     drmkms->crtc = drmModeGetCrtc( drmkms->fd, drmkms->encoder[0]->crtc_id );

     if (dfb_config->mode.width && dfb_config->mode.height) {
          mode = drmkms_find_mode( drmkms, 0, dfb_config->mode.width, dfb_config->mode.height, 0 );
          if (mode)
               shared->mode[0] = *mode;

          for (i = 1; i < shared->enabled_crtcs; i++)
               shared->mode[i] = shared->mode[0];
     }

     description->mixers   = shared->enabled_crtcs;
     description->encoders = shared->enabled_crtcs;
     description->outputs  = shared->enabled_crtcs;

     D_INFO( "DRMKMS/Screen: Default mode is %ux%u (%d modes in total)\n",
             shared->mode[0].hdisplay, shared->mode[0].vdisplay, drmkms->connector[0]->count_modes );

     return DFB_OK;
}

static DFBResult
drmkmsInitMixer( CoreScreen                *screen,
                 void                      *driver_data,
                 void                      *screen_data,
                 int                        mixer,
                 DFBScreenMixerDescription *description,
                 DFBScreenMixerConfig      *config )
{
     DRMKMSData *drmkms = driver_data;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     /* Set capabilities. */
     description->caps   = DSMCAPS_FULL;
     description->layers = drmkms->layer_ids[mixer];

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_MIXER_DESC_NAME_LENGTH, "DRMKMS Mixer" );

     config->flags  = DSMCONF_LAYERS;
     config->layers = description->layers;

     return DFB_OK;
}

static DFBResult
drmkmsInitEncoder( CoreScreen                  *screen,
                   void                        *driver_data,
                   void                        *screen_data,
                   int                          encoder,
                   DFBScreenEncoderDescription *description,
                   DFBScreenEncoderConfig      *config )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     /* Set capabilities. */
     description->caps = DSECAPS_RESOLUTION | DSECAPS_FREQUENCY;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_ENCODER_DESC_NAME_LENGTH, "DRMKMS Encoder" );

     config->flags = DSECONF_RESOLUTION | DSECONF_FREQUENCY | DSECONF_MIXER;
     config->mixer = encoder;

     if (drmkms->encoder[encoder]) {
          drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &config->resolution, &config->frequency );

          switch (drmkms->encoder[encoder]->encoder_type) {
               case DRM_MODE_ENCODER_DAC:
                    description->type = DSET_CRTC;
                    break;
               case DRM_MODE_ENCODER_LVDS:
               case DRM_MODE_ENCODER_TMDS:
                    description->type = DSET_DIGITAL;
                    break;
               case DRM_MODE_ENCODER_TVDAC:
                    description->type = DSET_TV;
                    break;
               default:
                    description->type = DSET_UNKNOWN;
          }

          description->all_resolutions = drmkms_modes_to_dsor_bitmask( drmkms, encoder );
     }
     else
          return DFB_INVARG;

     return DFB_OK;
}

static DFBResult
drmkmsInitOutput( CoreScreen                 *screen,
                  void                       *driver_data,
                  void                       *screen_data,
                  int                         output,
                  DFBScreenOutputDescription *description,
                  DFBScreenOutputConfig      *config )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     /* Set capabilities. */
     description->caps = DSOCAPS_RESOLUTION;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH, "DRMKMS Output" );

     config->flags   = DSOCONF_RESOLUTION | DSOCONF_ENCODER;
     config->encoder = output;

     if (drmkms->connector[output]) {
          drmkms_mode_to_dsor_dsef( &shared->mode[output], &config->resolution, NULL );

          switch (drmkms->connector[output]->connector_type) {
               case DRM_MODE_CONNECTOR_VGA:
                    description->all_connectors = DSOC_VGA;
                    description->all_signals    = DSOS_VGA;
                    break;
               case DRM_MODE_CONNECTOR_SVIDEO:
                    description->all_connectors = DSOC_YC;
                    description->all_signals    = DSOS_YC;
                    break;
               case DRM_MODE_CONNECTOR_Composite:
                    description->all_connectors = DSOC_CVBS;
                    description->all_signals    = DSOS_CVBS;
                    break;
               case DRM_MODE_CONNECTOR_Component:
                    description->all_connectors = DSOC_COMPONENT;
                    description->all_signals    = DSOS_YCBCR;
                    break;
               case DRM_MODE_CONNECTOR_HDMIA:
               case DRM_MODE_CONNECTOR_HDMIB:
                    description->all_connectors = DSOC_HDMI;
                    description->all_signals    = DSOS_HDMI;
                    break;
               default:
                    description->all_connectors = DSOC_UNKNOWN;
                    description->all_signals    = DSOC_UNKNOWN;
          }

          description->all_resolutions = drmkms_modes_to_dsor_bitmask( drmkms, output );
     }
     else
          return DFB_INVARG;

     return DFB_OK;
}

static DFBResult
drmkmsTestMixerConfig( CoreScreen                 *screen,
                       void                       *driver_data,
                       void                       *screen_data,
                       int                         mixer,
                       const DFBScreenMixerConfig *config,
                       DFBScreenMixerConfigFlags  *ret_failed )
{
     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
drmkmsSetMixerConfig( CoreScreen                 *screen,
                      void                       *driver_data,
                      void                       *screen_data,
                      int                         mixer,
                      const DFBScreenMixerConfig *config )
{
     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
drmkmsTestEncoderConfig( CoreScreen                   *screen,
                         void                         *driver_data,
                         void                         *screen_data,
                         int                           encoder,
                         const DFBScreenEncoderConfig *config,
                         DFBScreenEncoderConfigFlags  *ret_failed )
{
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & (DSECONF_FREQUENCY | DSECONF_RESOLUTION)))
          return DFB_UNSUPPORTED;

     drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &dsor, &dsef );

     if (config->flags & DSECONF_FREQUENCY)
          dsef = config->frequency;

     if (config->flags & DSECONF_RESOLUTION)
          dsor = config->resolution;

     mode = drmkms_dsor_dsef_to_mode( drmkms, encoder, dsor, dsef );
     if (!mode) {
          *ret_failed = config->flags & (DSECONF_RESOLUTION | DSECONF_FREQUENCY);
          return DFB_UNSUPPORTED;
     }

     if ((shared->primary_dimension[encoder].w && (shared->primary_dimension[encoder].w < mode->hdisplay) ) ||
         (shared->primary_dimension[encoder].h && (shared->primary_dimension[encoder].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          *ret_failed = config->flags & (DSECONF_RESOLUTION | DSECONF_FREQUENCY);
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
drmkmsSetEncoderConfig( CoreScreen                   *screen,
                        void                         *driver_data,
                        void                         *screen_data,
                        int                           encoder,
                        const DFBScreenEncoderConfig *config )
{
     DFBResult                  ret;
     int                        err;
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & (DSECONF_FREQUENCY | DSECONF_RESOLUTION)))
          return DFB_INVARG;

     drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &dsor, &dsef );

     if (config->flags & DSECONF_FREQUENCY) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> requested frequency change \n" );
          dsef = config->frequency;
     }

     if (config->flags & DSECONF_RESOLUTION) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> requested resolution change \n" );
          dsor = config->resolution;
     }

     mode = drmkms_dsor_dsef_to_mode( drmkms, encoder, dsor, dsef );
     if (!mode)
          return DFB_INVARG;

     if ((shared->primary_dimension[encoder].w && (shared->primary_dimension[encoder].w < mode->hdisplay) ) ||
         (shared->primary_dimension[encoder].h && (shared->primary_dimension[encoder].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          return DFB_INVARG;
     }

     if (shared->primary_fb) {
          err = drmModeSetCrtc( drmkms->fd, drmkms->encoder[encoder]->crtc_id,
                                shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                                &drmkms->connector[encoder]->connector_id, 1, mode );
          if (err) {
               ret = errno2result( errno );
               D_PERROR( "DRMKMS/Layer: "
                         "drmModeSetCrtc( crtc_id %u, fb_id %u, xy %d,%d, connector_id %u, mode %ux%u@%uHz )"
                         " failed for encoder %d!\n", drmkms->encoder[encoder]->crtc_id,
                         shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                         drmkms->connector[encoder]->connector_id,
                         mode->hdisplay, mode->vdisplay, mode->vrefresh, encoder);
               return ret;
          }
     }

     shared->mode[encoder] = *mode;

     return DFB_OK;
}

static DFBResult
drmkmsTestOutputConfig( CoreScreen                  *screen,
                        void                        *driver_data,
                        void                        *screen_data,
                        int                          output,
                        const DFBScreenOutputConfig *config,
                        DFBScreenOutputConfigFlags  *ret_failed )
{
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & DSOCONF_RESOLUTION))
          return DFB_UNSUPPORTED;

     drmkms_mode_to_dsor_dsef( &shared->mode[output], &dsor, &dsef );

     dsor = config->resolution;

     mode = drmkms_dsor_dsef_to_mode( drmkms, output, dsor, dsef );
     if (!mode) {
          *ret_failed = config->flags & DSOCONF_RESOLUTION;
          return DFB_UNSUPPORTED;
     }

     if ((shared->primary_dimension[output].w && (shared->primary_dimension[output].w < mode->hdisplay) ) ||
         (shared->primary_dimension[output].h && (shared->primary_dimension[output].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          *ret_failed = config->flags & DSOCONF_RESOLUTION;
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
drmkmsSetOutputConfig( CoreScreen                  *screen,
                       void                        *driver_data,
                       void                        *screen_data,
                       int                          output,
                       const DFBScreenOutputConfig *config )
{
     DFBResult                  ret;
     int                        err;
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & DSOCONF_RESOLUTION))
          return DFB_INVARG;

     drmkms_mode_to_dsor_dsef( &shared->mode[output], &dsor, &dsef );

     dsor = config->resolution;

     mode = drmkms_dsor_dsef_to_mode( drmkms, output, dsor, dsef );
     if (!mode)
          return DFB_INVARG;

     if ((shared->primary_dimension[output].w && (shared->primary_dimension[output].w < mode->hdisplay) ) ||
         (shared->primary_dimension[output].h && (shared->primary_dimension[output].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          return DFB_INVARG;
     }

     if (shared->primary_fb) {
          err = drmModeSetCrtc( drmkms->fd, drmkms->encoder[output]->crtc_id,
                                shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                                &drmkms->connector[output]->connector_id, 1, mode );
          if (err) {
               ret = errno2result( errno );
               D_PERROR( "DRMKMS/Layer: "
                         "drmModeSetCrtc( crtc_id %u, fb_id %u, xy %d,%d, connector_id %u, mode %ux%u@%uHz )"
                         " failed for output %d!\n", drmkms->encoder[output]->crtc_id,
                         shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                         drmkms->connector[output]->connector_id,
                         mode->hdisplay, mode->vdisplay, mode->vrefresh, output);
               return ret;
          }
     }

     shared->mode[output] = *mode;

     return DFB_OK;
}

static DFBResult
drmkmsGetScreenSize( CoreScreen *screen,
                     void       *driver_data,
                     void       *screen_data,
                     int        *ret_width,
                     int        *ret_height )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared;

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     *ret_width  = shared->mode[0].hdisplay;
     *ret_height = shared->mode[0].vdisplay;

     return DFB_OK;
}

const ScreenFuncs drmkmsScreenFuncs = {
     .InitScreen        = drmkmsInitScreen,
     .InitMixer         = drmkmsInitMixer,
     .InitEncoder       = drmkmsInitEncoder,
     .InitOutput        = drmkmsInitOutput,
     .TestMixerConfig   = drmkmsTestMixerConfig,
     .SetMixerConfig    = drmkmsSetMixerConfig,
     .TestEncoderConfig = drmkmsTestEncoderConfig,
     .SetEncoderConfig  = drmkmsSetEncoderConfig,
     .TestOutputConfig  = drmkmsTestOutputConfig,
     .SetOutputConfig   = drmkmsSetOutputConfig,
     .GetScreenSize     = drmkmsGetScreenSize,
};
