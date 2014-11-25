/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifdef ENABLE_MUSICBRAINZ

#include "musicbrainz.h"

#include <glib/gi18n.h>
#include <musicbrainz5/mb5_c.h>

/* TODO: Use G_DEFINE_TYPE_WITH_PRIVATE. */
G_DEFINE_TYPE (EtMusicbrainz, et_musicbrainz, G_TYPE_OBJECT)

#define et_musicbrainz_get_instance_private(musicbrainz) (musicbrainz->priv)

#define GET(field, function, obj) \
{ \
    function (obj, buffer, sizeof (buffer)); \
 \
    if (field) \
    { \
        g_free (field); \
    } \
 \
    if (*buffer == '\0') \
    { \
        field = NULL; \
    } \
    else \
    { \
        field = g_strdup (buffer); \
    } \
}

struct _EtMusicbrainzPrivate
{
    Mb5Query query;
};

struct _EtMusicbrainzQuery
{
    /*< private >*/
    EtMusicbrainzEntity entity;
    gchar *search_string;
    gint ref_count;
};

struct _EtMusicbrainzResult
{
    /*< private >*/
    GList *list;
    gint ref_count;
};

EtMusicbrainzQuery *
et_musicbrainz_query_new (EtMusicbrainzEntity entity,
                          const gchar *search_string)
{
    EtMusicbrainzQuery *query;

    query = g_slice_new (EtMusicbrainzQuery);
    query->entity = entity;
    query->ref_count = 1;
    query->search_string = g_strdup (search_string);

    return query;
}

EtMusicbrainzQuery *
et_musicbrainz_query_ref (EtMusicbrainzQuery *query)
{
    g_return_val_if_fail (query != NULL, NULL);

    g_atomic_int_inc (&query->ref_count);

    return query;
}

void
et_musicbrainz_query_unref (EtMusicbrainzQuery *query)
{
    g_return_if_fail (query != NULL);

    if (g_atomic_int_dec_and_test (&query->ref_count))
    {
        g_free (query->search_string);
        g_slice_free (EtMusicbrainzQuery, query);
    }
}

static EtMusicbrainzResult *
et_musicbrainz_result_new (EtMusicbrainzEntity entity,
                           Mb5Metadata metadata)
{
    EtMusicbrainzResult *result;
    GList *list = NULL;
    gchar buffer[512]; /* For the GET macro. */

    result = g_slice_new (EtMusicbrainzResult);
    result->ref_count = 1;

    switch (entity)
    {
        case ET_MUSICBRAINZ_ENTITY_ARTIST:
        {
            Mb5ArtistList artist_list;
            gsize i;
            gint n_artists;

            artist_list = mb5_metadata_get_artistlist (metadata);
            n_artists = mb5_artist_list_size (artist_list);

            for (i = 0; i < n_artists; i++)
            {
                Mb5Artist artist;
                gchar *name = NULL;

                /* TODO: Come up with a full object for describing an
                 * artist. */
                artist = mb5_artist_list_item (artist_list, i);
                GET (name, mb5_artist_get_name, artist);
                list = g_list_prepend (list, name);
            }
        }
            break;
        case ET_MUSICBRAINZ_ENTITY_RELEASE_GROUP:
        {
            Mb5ReleaseGroupList release_group_list;
            gsize i;
            gint n_release_groups;

            release_group_list = mb5_metadata_get_releasegrouplist (metadata);
            n_release_groups = mb5_releasegroup_list_size (release_group_list);

            for (i = 0; i < n_release_groups; i++)
            {
                Mb5ReleaseGroup release_group;
                gchar *name = NULL;

                /* TODO: Come up with a full object for describing a release
                 * group. */
                release_group = mb5_releasegroup_list_item (release_group_list,
                                                            i);
                GET (name, mb5_releasegroup_get_title, release_group);
                list = g_list_prepend (list, name);
            }
        }
            break;
        default:
            g_assert_not_reached ();
            break;
    }

    result->list = g_list_reverse (list);

    return result;
}

EtMusicbrainzResult *
et_musicbrainz_result_ref (EtMusicbrainzResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);

    g_atomic_int_inc (&result->ref_count);

    return result;
}

void
et_musicbrainz_result_unref (EtMusicbrainzResult *result)
{
    g_return_if_fail (result != NULL);

    if (g_atomic_int_dec_and_test (&result->ref_count))
    {
        g_list_free_full (result->list, g_free);
        g_slice_free (EtMusicbrainzResult, result);
    }
}

GList *
et_musicbrainz_result_get_results (EtMusicbrainzResult *result)
{
    g_return_val_if_fail (result != NULL, NULL);

    return result->list;
}

static EtMusicbrainzResult *
query_mb_from_et_query (Mb5Query mb_query,
                        EtMusicbrainzQuery *query)
{
    const gchar *entity;
    gchar *param_names[1] = { "query" };
    gchar *param_values[1];
    Mb5Metadata metadata;
    EtMusicbrainzResult *result = NULL;

    switch (query->entity)
    {
        case ET_MUSICBRAINZ_ENTITY_ARTIST:
            entity = "artist";
            param_values[0] = g_strconcat ("artist:",
                                           query->search_string, NULL);
            break;
        case ET_MUSICBRAINZ_ENTITY_RELEASE_GROUP:
            entity = "release-group";
            param_values[0] = g_strconcat ("releasegroup:",
                                           query->search_string, NULL);
            break;
        default:
            g_assert_not_reached ();
            break;
    }

    metadata = mb5_query_query (mb_query, entity, "", "", 1, param_names,
                                param_values);

    g_free (param_values[0]);

    if (metadata != NULL)
    {
        result = et_musicbrainz_result_new (query->entity, metadata);
        mb5_metadata_delete (metadata);
    }

    return result;
}

static void
search_thread_func (GSimpleAsyncResult *result,
                    GObject *musicbrainz,
                    GCancellable *cancellable)
{
    EtMusicbrainz *self;
    EtMusicbrainzPrivate *priv;
    EtMusicbrainzQuery *query;
    EtMusicbrainzResult *mb_result;

    self = ET_MUSICBRAINZ (musicbrainz);
    priv = et_musicbrainz_get_instance_private (self);

    query = g_simple_async_result_get_op_res_gpointer (result);

    mb_result = query_mb_from_et_query (priv->query, query);

    if (mb_result == NULL)
    {
        gint code;
        gint len;
        gchar *message;

        code = mb5_query_get_lasthttpcode (priv->query);
        len = mb5_query_get_lasterrormessage (priv->query, NULL, 0);
        message = g_malloc (len);
        mb5_query_get_lasterrormessage (priv->query, message, len);

        /* TODO: Use a translatable string? */
        g_simple_async_result_set_error (result, G_IO_ERROR, G_IO_ERROR_FAILED,
                                         "MusicBrainz search query failed with error code '%d': %s",
                                         code, message);
        g_free (message);
    }
    else
    {
        g_simple_async_result_set_op_res_gpointer (result, mb_result,
                                                   (GDestroyNotify)et_musicbrainz_result_unref);
    }
}

static void
et_musicbrainz_finalize (GObject *object)
{
    EtMusicbrainz *self;
    EtMusicbrainzPrivate *priv;

    self = ET_MUSICBRAINZ (object);
    priv = et_musicbrainz_get_instance_private (self);

    if (priv->query)
    {
        mb5_query_delete (priv->query);
        priv->query = NULL;
    }

    G_OBJECT_CLASS (et_musicbrainz_parent_class)->finalize (object);
}

static void
et_musicbrainz_init (EtMusicbrainz *self)
{
    EtMusicbrainzPrivate *priv;

    priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ET_TYPE_MUSICBRAINZ,
                                                     EtMusicbrainzPrivate);

    priv->query = mb5_query_new (PACKAGE_NAME "/" PACKAGE_VERSION " ( "
                                 PACKAGE_URL " )", NULL, 0);
}

static void
et_musicbrainz_class_init (EtMusicbrainzClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = et_musicbrainz_finalize;

    g_type_class_add_private (klass, sizeof (EtMusicbrainzPrivate));
}

/*
 * et_musicbrainz_dialog_new:
 *
 * Create a new EtMusicbrainz instance.
 *
 * Returns: a new #EtMusicbrainz
 */
EtMusicbrainz *
et_musicbrainz_new (void)
{
    return g_object_new (ET_TYPE_MUSICBRAINZ, NULL);
}

/*
 * et_musicbrainz_search_async:
 * @self: the musicbrainz context from which to initiate the search
 * @query: parameters for the search query
 * @cancellable: a cancellable, or %NULL
 * @callback: a callback to trigger when the call is complete
 * @user_data: user data to be passed to @callback
 *
 * Start an asynchronous search, submitting a query according to @info and
 * delivering the result to @callback when complete.
 */
void
et_musicbrainz_search_async (EtMusicbrainz *self,
                             EtMusicbrainzQuery *query,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
    GSimpleAsyncResult *result;

    g_return_if_fail (ET_MUSICBRAINZ (self));
    g_return_if_fail (query != NULL && callback != NULL);

    result = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
                                        et_musicbrainz_search_async);

    g_simple_async_result_set_op_res_gpointer (result,
                                               et_musicbrainz_query_ref (query),
                                               (GDestroyNotify)et_musicbrainz_query_unref);
    g_simple_async_result_run_in_thread (result, search_thread_func,
                                         G_PRIORITY_DEFAULT, cancellable);
    g_object_unref (result);
}

/*
 * et_musicbrainz_search_finish:
 * @self: the musicbrainz context on which the search was initiated
 * @result: the result obtained in the ready callback from
 * et_musicbrainz_search_async()
 * @error: an error, or %NULL
 *
 * Finishes an asynchronous operation started with
 * et_musicbrainz_search_async().
 *
 * Returns: a result, or %NULL on error
 */
EtMusicbrainzResult *
et_musicbrainz_search_finish (EtMusicbrainz *self,
                              GAsyncResult *result,
                              GError **error)
{
    GSimpleAsyncResult *simple;
    EtMusicbrainzResult *mb_result;

    g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                          G_OBJECT (self),
                                                          et_musicbrainz_search_async),
                          NULL);

    simple = (GSimpleAsyncResult *)result;

    if (g_simple_async_result_propagate_error (simple, error))
    {
        return NULL;
    }

    mb_result = (EtMusicbrainzResult *)g_simple_async_result_get_op_res_gpointer (simple);

    return et_musicbrainz_result_ref (mb_result);
}

#endif /* ENABLE_MUSICBRAINZ */
