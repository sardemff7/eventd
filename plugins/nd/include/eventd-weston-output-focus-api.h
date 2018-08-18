/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011-2018 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_ND_WESTON_OUTPUT_FOCUS_API_H__
#define __EVENTD_ND_WESTON_OUTPUT_FOCUS_API_H__

#include <plugin-registry.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct weston_compositor;

#define EVENTD_WESTON_OUTPUT_FOCUS_API_NAME "eventd_weston_output_focus_api_v1"

struct eventd_weston_output_focus_api {
    void (*add_listener)(struct weston_compositor *compositor, struct wl_listener *listener);
};

static inline const struct eventd_weston_output_focus_api *
eventd_weston_output_focus_get_api(struct weston_compositor *compositor)
{
    return (const struct eventd_weston_output_focus_api *) weston_plugin_api_get(compositor, EVENTD_WESTON_OUTPUT_FOCUS_API_NAME, sizeof(struct eventd_weston_output_focus_api));
}

#ifdef  __cplusplus
}
#endif

#endif /* __EVENTD_ND_WESTON_OUTPUT_FOCUS_API_H__ */
