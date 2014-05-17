/* mb_search.c - 2014/05/05 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2000-2014  Abhinav Jangda <abhijangda@hotmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mb_search.h"

/*
 * et_mb5_search_error_quark:
 *
 * To get EtMB5SearchError domain.
 *
 * Returns: GQuark for EtMB5SearchError domain
 */
GQuark
et_mb5_search_error_quark (void)
{
    return g_quark_from_static_string ("et-mb5-search-error-quark");
}

/*
 * et_musicbrainz_search_in_entity:
 * @string:
 * @child_type:
 * @parent_type:
 * @parent_mbid:
 * @root:
 *
 *
 */
gboolean
et_musicbrainz_search_in_entity (enum MB_ENTITY_TYPE child_type,
                                 enum MB_ENTITY_TYPE parent_type,
                                 gchar *parent_mbid, GNode *root,
                                 GError **error)
{
    Mb5Query query;
    Mb5Metadata metadata;
    char error_message[256];
    tQueryResult result;
    char *param_values[1];
    char *param_names[1];

    param_names [0] = "inc";
    query = mb5_query_new ("easytag", NULL, 0);

    if (child_type == MB_ENTITY_TYPE_ALBUM &&
        parent_type == MB_ENTITY_TYPE_ARTIST)
    {
        param_values [0] = "releases";
        metadata = mb5_query_query (query, "artist", parent_mbid, "", 1, param_names,
                                    param_values);
        result = mb5_query_get_lastresult (query);

        if (result == eQuery_Success)
        {
            if (metadata)
            {
                int i;
                Mb5ReleaseList list;
                Mb5Artist artist;
                artist = mb5_metadata_get_artist (metadata);
                list = mb5_artist_get_releaselist (artist);

                for (i = 0; i < mb5_release_list_size (list); i++)
                {
                    Mb5Release release;
                    release = mb5_release_list_item (list, i);
                    if (release)
                    {
                        GNode *node;
                        EtMbEntity *entity;
                        entity = g_malloc (sizeof (EtMbEntity));
                        entity->entity = release;
                        entity->type = MB_ENTITY_TYPE_ALBUM;
                        node = g_node_new (entity);
                        g_node_append (root, node);
                    }
                }
            }
        }
        else
        {
            goto err;
        }
    }
    else if (child_type == MB_ENTITY_TYPE_TRACK &&
             parent_type == MB_ENTITY_TYPE_ALBUM)
    {
        
    }

    mb5_query_delete (query);

    return TRUE;

    err:
    mb5_query_get_lasterrormessage (query, error_message,
                                    sizeof(error_message));

    switch (result)
    {
        case eQuery_ConnectionError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_CONNECTION, error_message);
            break;

        case eQuery_Timeout:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_TIMEOUT, error_message);
            break;

        case eQuery_AuthenticationError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_AUTHENTICATION, error_message);
            break;

        case eQuery_FetchError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_FETCH, error_message);
            break;
 
        case eQuery_RequestError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_REQUEST, error_message);
            break;
 
        case eQuery_ResourceNotFound:
            g_set_error (error, ET_MB5_SEARCH_ERROR, 
                         ET_MB5_SEARCH_ERROR_RESOURCE_NOT_FOUND,
                         error_message);
            break;

        default:
            break;
    }

    g_assert (error == NULL || *error != NULL);
    return FALSE;
}

/*
 * et_musicbrainz_search:
 * @string:
 * @type:
 * @root:
 *
 *
 */
gboolean
et_musicbrainz_search (gchar *string, enum MB_ENTITY_TYPE type, GNode *root,
                       GError **error)
{
    Mb5Query query;
    Mb5Metadata metadata;
    char error_message[256];
    tQueryResult result;
    char *param_values[2];
    char *param_names[2];

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    param_names [0] = "query";
    param_names [1] = "limit";
    param_values [1] = SEARCH_LIMIT_STR;
    query = mb5_query_new ("easytag", NULL, 0);

    if (type == MB_ENTITY_TYPE_ARTIST)
    {
        param_values [0] = g_strconcat ("artist:", string, NULL);
        metadata = mb5_query_query (query, "artist", "", "", 2, param_names,
                                    param_values);
        result = mb5_query_get_lastresult (query);
        if (result == eQuery_Success)
        {
            if (metadata)
            {
                int i;
                Mb5ArtistList list;
                list = mb5_metadata_get_artistlist (metadata);

                for (i = 0; i < mb5_artist_list_size (list); i++)
                {
                    Mb5Artist artist;
                    artist = mb5_artist_list_item (list, i);
                    if (artist)
                    {
                        GNode *node;
                        EtMbEntity *entity;
                        entity = g_malloc (sizeof (EtMbEntity));
                        entity->entity = mb5_artist_clone (artist);
                        entity->type = MB_ENTITY_TYPE_ARTIST;
                        node = g_node_new (entity);
                        g_node_append (root, node);
                    }
                }
            }

            g_free (param_values [0]);
            mb5_metadata_delete (metadata);
        }
        else
        {
            goto err;
        }
    }

    else if (type == MB_ENTITY_TYPE_ALBUM)
    {
        param_values [0] = g_strconcat ("release:", string, NULL);
        metadata = mb5_query_query (query, "release", "", "", 2, param_names,
                                    param_values);
        result = mb5_query_get_lastresult (query);

        if (result == eQuery_Success)
        {
            if (metadata)
            {
                int i;
                Mb5ReleaseList list;
                list = mb5_metadata_get_releaselist (metadata);

                for (i = 0; i < mb5_release_list_size (list); i++)
                {
                    Mb5Release release;
                    release = mb5_release_list_item (list, i);
                    if (release)
                    {
                        GNode *node;
                        EtMbEntity *entity;
                        entity = g_malloc (sizeof (EtMbEntity));
                        entity->entity = mb5_release_clone (release);
                        entity->type = MB_ENTITY_TYPE_ALBUM;
                        node = g_node_new (entity);
                        g_node_append (root, node);
                    }
                }
            }

            g_free (param_values [0]);
            mb5_metadata_delete (metadata);
        }
        else
        {
            goto err;
        }
    }

    else if (type == MB_ENTITY_TYPE_TRACK)
    {
    }

    mb5_query_delete (query);

    return TRUE;

    err:
    mb5_query_get_lasterrormessage (query, error_message,
                                    sizeof(error_message));

    switch (result)
    {
        case eQuery_ConnectionError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_CONNECTION, error_message);
            break;

        case eQuery_Timeout:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_TIMEOUT, error_message);
            break;

        case eQuery_AuthenticationError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_AUTHENTICATION, error_message);
            break;

        case eQuery_FetchError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_FETCH, error_message);
            break;
 
        case eQuery_RequestError:
            g_set_error (error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_REQUEST, error_message);
            break;
 
        case eQuery_ResourceNotFound:
            g_set_error (error, ET_MB5_SEARCH_ERROR, 
                         ET_MB5_SEARCH_ERROR_RESOURCE_NOT_FOUND,
                         error_message);
            break;

        default:
            break;
    }

    g_assert (error == NULL || *error != NULL);
    return FALSE;
}

/*
 * free_mb_tree:
 * @node:
 *
 *
 */
void
free_mb_tree (GNode *node)
{
    EtMbEntity *et_entity;
    GNode *child;

    et_entity = (EtMbEntity *)node->data;

    if (et_entity)
    {
        if (et_entity->type == MB_ENTITY_TYPE_ARTIST)
        {
            mb5_artist_delete ((Mb5Artist)et_entity->entity);
        }

        else if (et_entity->type == MB_ENTITY_TYPE_ALBUM)
        {
            mb5_release_delete ((Mb5Release)et_entity->entity);
        }

        else if (et_entity->type == MB_ENTITY_TYPE_TRACK)
        {
            mb5_recording_delete ((Mb5Recording)et_entity->entity);
        }
    }

    g_free (et_entity);
    g_node_unlink (node);
    child = g_node_first_child (node);

    while (child)
    {
        free_mb_tree (child);
        child = g_node_next_sibling (child);
    }

    g_node_destroy (node);
}