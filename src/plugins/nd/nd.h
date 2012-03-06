/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_ND_EVENT_H__
#define __EVENTD_ND_EVENT_H__

typedef struct {
    gboolean disable;
    gchar *title;
    gchar *message;
    gchar *icon;
    gchar *overlay_icon;
    gdouble scale;
} EventdNdEvent;

typedef struct {
    gchar *title;
    gchar *message;
    guchar *icon;
    gsize icon_length;
    const gchar *icon_format;
    guchar *overlay_icon;
    gsize overlay_icon_length;
    const gchar *overlay_icon_format;
} EventdNdNotification;

#endif /* __EVENTD_ND_EVENT_H__ */
