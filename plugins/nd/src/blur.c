/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2016 Quentin "Sardem FF7" Glidic
 *
 * This file is part of eventd.
 *
 * eventd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * eventd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_MATH_H
#include <math.h>
#endif /* HAVE_MATH_H */

#include <glib.h>

#include <cairo.h>

#include "blur.h"

/*
 * Box blur algorithm based on a blog post from Ivan Kuckir
 * http://blog.ivank.net/fastest-gaussian-blur.html
 * Blog post code is licenced under the MIT (Expat) licence.
 */

static void
_eventd_nd_draw_blur_box_one_dimension(guint8 *src, guint8 *dst, gint r, gint iarr, gint channel, gint dim1, gint stride1, gint dim2, gint stride2)
{
    gint i;
    for ( i = 0; i < dim1; ++i )
    {
        gint current = i * stride1 + channel;
        gint previous = current;
        gint next = current + r * stride2;
        gint j;

        guint64 val = ( r + 1 ) * src[current];
        for ( j = 0 ; j < r ; ++j )
            val += src[current + j * stride2];

        /*
         * We have <= r here because pixel r
         * still has the first pixel in its neighborhood
         * It starts to be outside at the beginning of the second loop
         */
        for ( j = 0 ; j <= r ; ++j )
        {
            val += src[next] - src[previous];
            dst[current] = (guint8) ( val / (gdouble) iarr + 0.5 );
            /* previous stays the first one */
            next += stride2;
            current += stride2;
        }
        /*
         * We have (dim2 - r - 1) here so the last pixel is "next"
         * after this loop only, and not at the last iteration
         * (which would make us overflow)
         */
        for ( /* old value is good */ ; j < dim2 - r - 1 ; ++j )
        {
            val += src[next] - src[previous];
            dst[current] = (guint8) ( val / (gdouble) iarr + 0.5 );
            previous += stride2;
            next += stride2;
            current += stride2;
        }
        for ( /* old value is good */ ; j < dim2 ; ++j )
        {
            val += src[next] - src[previous];
            dst[current] = (guint8) ( val / (gdouble) iarr + 0.5 );
            previous += stride2;
            /* next stays the last one */
            current += stride2;
        }
    }
}

static void
_eventd_nd_draw_blur_box(guint8 *src, guint8 *dst, gint r, gdouble iarr, gint width, gint height, gint stride, gint channels)
{
    gint channel;
    for ( channel = 0 ; channel < channels ; ++channel )
    {
        /* We navigate the width by channels steps and the height by stride steps */
        _eventd_nd_draw_blur_box_one_dimension(src, dst, r, iarr, channel, width, channels, height, stride);
        _eventd_nd_draw_blur_box_one_dimension(dst, src, r, iarr, channel, height, stride, width, channels);
    }
}

static void
_eventd_nd_draw_blur_gauss(guint8 *src, guint8 *dst, gint r, gint n, gint width, gint height, gint stride, gint channels)
{
    gdouble w_ideal = sqrt(( 12.0 * r * r / (gdouble) n ) + 1.0);  /* Ideal averaging filter width */
    gint wu = (gint) ( w_ideal + 1 ) | 0x1; /* Get the odd higher value */
    gint wl = wu - 2; /* And the odd lesser value */


    gdouble m_ideal = ( 12.0 * r * r - n * wl * wl - 12.0 * wl - ( 3.0 * n ) ) / ( -4.0 * ( wl + 1 ) );
    gint m = (gint) ( m_ideal + 0.5 );

    gint i;
    for ( i = 0 ; i < n ; ++i )
    {
        gint rr = ( ( i < m ? wl : wu ) - 1 ) / 2;
        gint iarr = 2 * rr + 1;
        if ( (i % 2) == 0 )
            _eventd_nd_draw_blur_box(src, dst, rr, iarr, width, height, stride, channels);
        else
            _eventd_nd_draw_blur_box(dst, src, rr, iarr, width, height, stride, channels);
    }
    if ( ( n % 2 ) == 0 )
        memcpy(src, dst, sizeof(guint8) * height * stride);
}

/*
 * If we ever support GL/D3D surfaces, we will have to
 * filter on these surface types
 * For now, everything is CPU-rendered, so map_to_image
 * will give us the best possible backend for our blur
 */
void
eventd_nd_draw_blur_surface(cairo_t *cr, gint blur)
{
    cairo_surface_t *target, *surface;

    target = cairo_get_target(cr);
    surface = cairo_surface_map_to_image(target, NULL);

    if ( cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS )
        return;

    gint width, height, stride, channels;
    guint8 *data, *tmp;

    switch ( cairo_image_surface_get_format(surface) )
    {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
        channels = 4;
    break;
    case CAIRO_FORMAT_A8:
        channels = 1;
    break;
    default:
        goto fail;
    }

    width  = cairo_image_surface_get_width(surface);
    height = cairo_image_surface_get_height(surface);

    data = cairo_image_surface_get_data(surface);
    stride = cairo_image_surface_get_stride(surface);

    tmp = g_alloca(stride * height * sizeof(guint8));
    _eventd_nd_draw_blur_gauss(data, tmp, blur, 3 /* number of passes */, width, height, stride, channels);

    cairo_surface_mark_dirty(surface);

fail:
    cairo_surface_unmap_image(target, surface);
}
