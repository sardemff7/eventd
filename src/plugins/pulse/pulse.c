/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#include <pulse/pulseaudio.h>
#include <sndfile.h>

#include <glib.h>

#include <eventd-plugin.h>

typedef struct {
    gchar *sample;
    gboolean created;
} EventdPulseEvent;

static pa_threaded_mainloop *pa_loop = NULL;
static pa_context *sound = NULL;

static GHashTable *events = NULL;


static void
pa_context_state_callback(pa_context *c, void *userdata)
{
    pa_context_state_t state = pa_context_get_state(c);
    switch ( state )
    {
        case PA_CONTEXT_READY:
            pa_threaded_mainloop_signal(pa_loop, 0);
        default:
        break;
    }
}

static void
pa_context_notify_callback(pa_context *s, void *userdata)
{
    pa_threaded_mainloop_signal(pa_loop, 0);
}

static void
pa_context_success_callback(pa_context *s, int success, void *userdata)
{
    pa_threaded_mainloop_signal(pa_loop, 0);
}

static void
pa_sample_state_callback(pa_stream *sample, void *userdata)
{
    pa_stream_state_t state = pa_stream_get_state(sample);
    switch  ( state )
    {
        case PA_STREAM_TERMINATED:
            pa_stream_unref(sample);
        break;
        case PA_STREAM_READY:
            pa_threaded_mainloop_signal(pa_loop, 0);
        default:
        break;
    }
}

static void
eventd_pulse_start()
{
    pa_loop = pa_threaded_mainloop_new();
    pa_threaded_mainloop_start(pa_loop);

    sound = pa_context_new(pa_threaded_mainloop_get_api(pa_loop), PACKAGE_NAME);
    if ( ! sound )
        g_error("Can't open sound system");
    pa_context_get_state(sound);
    pa_context_set_state_callback(sound, pa_context_state_callback, NULL);

    pa_threaded_mainloop_lock(pa_loop);
    pa_context_connect(sound, NULL, 0, NULL);
    pa_threaded_mainloop_wait(pa_loop);
    pa_threaded_mainloop_unlock(pa_loop);
}

static void
eventd_pulse_stop()
{
    pa_operation* op = pa_context_drain(sound, pa_context_notify_callback, NULL);
    if ( op )
    {
        pa_threaded_mainloop_lock(pa_loop);
        pa_threaded_mainloop_wait(pa_loop);
        pa_operation_unref(op);
        pa_threaded_mainloop_unlock(pa_loop);
    }
    pa_context_disconnect(sound);
    pa_context_unref(sound);
    pa_threaded_mainloop_stop(pa_loop);
    pa_threaded_mainloop_free(pa_loop);
}


static const pa_sample_spec sound_spec = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};

typedef sf_count_t (*sndfile_readf_t)(SNDFILE *sndfile, void *ptr, sf_count_t frames);

static gboolean
eventd_pulse_create_sample(const gchar *sample_name, const gchar *filename)
{
    int retval = 0;

    SF_INFO sfi;
    SNDFILE* f = NULL;
    if ( ( f = sf_open(filename, SFM_READ, &sfi) ) == NULL )
    {
        g_warning("Can't open sound file");
        goto out;
    }

    pa_sample_spec sample_spec;
    switch (sfi.format & SF_FORMAT_SUBMASK)
    {
    case SF_FORMAT_PCM_16:
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_S8:
        sample_spec.format = PA_SAMPLE_S16NE;
    break;
    case SF_FORMAT_PCM_24:
        sample_spec.format = PA_SAMPLE_S24NE;
    break;
    case SF_FORMAT_PCM_32:
        sample_spec.format = PA_SAMPLE_S32NE;
    break;
    case SF_FORMAT_ULAW:
        sample_spec.format = PA_SAMPLE_ULAW;
    break;
    case SF_FORMAT_ALAW:
        sample_spec.format = PA_SAMPLE_ALAW;
    break;
    case SF_FORMAT_FLOAT:
    case SF_FORMAT_DOUBLE:
    default:
        sample_spec.format = PA_SAMPLE_FLOAT32NE;
    break;
    }

    sample_spec.rate = (uint32_t)sfi.samplerate;
    sample_spec.channels = (uint8_t)sfi.channels;

    if ( ! pa_sample_spec_valid(&sample_spec) )
    {
        g_warning("Invalid spec");
        goto out;
    }

    sndfile_readf_t readf_function = NULL;
    switch ( sample_spec.format )
    {
    case PA_SAMPLE_S16NE:
        readf_function = (sndfile_readf_t) sf_readf_short;
    break;
    case PA_SAMPLE_S32NE:
    case PA_SAMPLE_S24_32NE:
        readf_function = (sndfile_readf_t) sf_readf_int;
    break;
    case PA_SAMPLE_FLOAT32NE:
        readf_function = (sndfile_readf_t) sf_readf_float;
    break;
    case PA_SAMPLE_ULAW:
    case PA_SAMPLE_ALAW:
    default:
        g_warning("Can't handle this one");
        goto out;
    break;
    }

    size_t sample_length = (size_t) sfi.frames *pa_frame_size(&sample_spec);
    pa_stream *sample = pa_stream_new(sound, sample_name, &sample_spec, NULL);
    pa_stream_set_state_callback(sample, pa_sample_state_callback, NULL);
    pa_stream_connect_upload(sample, sample_length);

    pa_threaded_mainloop_lock(pa_loop);
    pa_threaded_mainloop_wait(pa_loop);

    size_t frame_size = pa_frame_size(&sample_spec);
    void *data = NULL;
    size_t nbytes = -1;
    sf_count_t r = 0;
    while ( pa_stream_begin_write(sample, &data, &nbytes) > -1 )
    {
        if ( ( r = readf_function(f, data, (sf_count_t) (nbytes/frame_size)) ) > 0 )
        {
            r *= (sf_count_t)frame_size;
            pa_stream_write(sample, data, r, NULL, 0, PA_SEEK_RELATIVE);
        }
        else
        {
            pa_stream_cancel_write(sample);
            break;
        }
        r = 0;
        nbytes = -1;
    }
    if ( r  < 0 )
        g_warning("Error while reading sound file");
    else
        retval = 1;
    /*
     * TODO: load the file then write it to the PA stream
     */

    pa_stream_finish_upload(sample);
    //pa_stream_unref(sample);

    pa_threaded_mainloop_unlock(pa_loop);
out:
    sf_close(f);
    return retval;
}

static void
eventd_pulse_event_action(const gchar *client_type, const gchar *client_name, const gchar *event_type, const gchar *event_name, const gchar *event_data)
{
    gchar *name;
    EventdPulseEvent *event = NULL;
    pa_operation *op;

    name = g_strdup_printf("%s-%s", client_type, event_type);

    event = g_hash_table_lookup(events, name);
    if ( event == NULL )
        goto fail;

    pa_threaded_mainloop_lock(pa_loop);
    op = pa_context_play_sample(sound, name, NULL, PA_VOLUME_NORM, NULL, NULL);
    if ( op )
        pa_operation_unref(op);
    else
        g_warning("Can't play sample %s", name);
    pa_threaded_mainloop_unlock(pa_loop);

fail:
    g_free(name);
}

static void
eventd_pulse_remove_sample(const char *name)
{
    pa_threaded_mainloop_lock(pa_loop);
    pa_context_remove_sample(sound, name, pa_context_success_callback, NULL);
    pa_threaded_mainloop_wait(pa_loop);
    pa_threaded_mainloop_unlock(pa_loop);
}

static EventdPulseEvent *
eventd_pulse_event_new(const gchar *sample, const gchar *filename)
{
    EventdPulseEvent *event = NULL;
    gchar *real_filename = NULL;

    if ( filename )
    {
        if ( filename[0] != '/')
            real_filename = g_strdup_printf("%s/"PACKAGE_NAME"/sounds/%s", g_get_user_data_dir(), filename);
        else
            real_filename = g_strdup(filename);

        if ( ! eventd_pulse_create_sample(sample, real_filename) )
            goto fail;
    }

    event = g_new0(EventdPulseEvent, 1);

    event->sample = g_strdup(sample);
    event->created = (filename != NULL);

fail:
    g_free(real_filename);
    return event;
}

static void
eventd_pulse_event_parse(const gchar *type, const gchar *event, GKeyFile *config_file, GKeyFile *defaults_config_file)
{
    gchar *sample = NULL;
    gchar *filename = NULL;
    EventdPulseEvent *pulse_event = NULL;

    if ( ! g_key_file_has_group(config_file, "sound") )
        return;

    if ( eventd_config_key_file_get_string(config_file, "sound", "sample", event, type, &sample) < 0 )
        goto skip;
    if ( eventd_config_key_file_get_string(config_file, "sound", "file", event, type, &filename) < 0 )
        goto skip;

    /* Check defaults */
    if ( ( defaults_config_file ) && ( ! sample ) && ( ! filename ) && g_key_file_has_group(defaults_config_file, "sound") )
    {
        eventd_config_key_file_get_string(defaults_config_file, "sound", "sample", "defaults", type, &sample);
        eventd_config_key_file_get_string(defaults_config_file, "sound", "file", "defaults", type, &filename);
    }

    if ( ( filename ) || ( sample ) )
    {
        gchar *name;

        name = g_strdup_printf("%s-%s", type, event);

        if ( ! sample )
            sample = g_strdup(name);

        pulse_event = eventd_pulse_event_new(sample, filename);
        if ( pulse_event )
            g_hash_table_insert(events, name, pulse_event);
        else
            g_free(name);
    }

skip:
    g_free(filename);
    g_free(sample);
}

static void
eventd_pulse_event_free(EventdPulseEvent *event)
{
    if ( event->created )
        eventd_pulse_remove_sample(event->sample);

    g_free(event->sample);
    g_free(event);
}

static void
eventd_pulse_config_init()
{
    events = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)eventd_pulse_event_free);
}

static void
eventd_pulse_config_clean()
{
    g_hash_table_unref(events);
}

void
eventd_plugin_get_info(EventdPlugin *plugin)
{
    plugin->start = eventd_pulse_start;
    plugin->stop = eventd_pulse_stop;

    plugin->config_init = eventd_pulse_config_init;
    plugin->config_clean = eventd_pulse_config_clean;

    plugin->event_parse = eventd_pulse_event_parse;
    plugin->event_action = eventd_pulse_event_action;
}
