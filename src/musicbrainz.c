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

struct _EtMusicbrainzPrivate
{
    Mb5Query query;
};

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

#endif /* ENABLE_MUSICBRAINZ */
