/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 * Copyright (C) 2014  Abhinav Jangda <abhijangda@hotmail.com>
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

#include "musicbrainz_dialog.h"

#include <glib/gi18n.h>

#include "musicbrainz.h"

/* TODO: Use G_DEFINE_TYPE_WITH_PRIVATE. */
G_DEFINE_TYPE (EtMusicbrainzDialog, et_musicbrainz_dialog, GTK_TYPE_DIALOG)

#define et_musicbrainz_dialog_get_instance_private(dialog) (dialog->priv)

static guint BOX_SPACING = 6;

struct _EtMusicbrainzDialogPrivate
{
    EtMusicbrainz *mb;
};

static void
create_musicbrainz_dialog (EtMusicbrainzDialog *self)
{
    EtMusicbrainzDialogPrivate *priv;
    GtkWidget *content_area;
    GtkBuilder *builder;
    GError *error = NULL;
    GtkWidget *grid;

    priv = et_musicbrainz_dialog_get_instance_private (self);

    gtk_window_set_title (GTK_WINDOW (self), _("MusicBrainz Search"));
    gtk_window_set_destroy_with_parent (GTK_WINDOW (self), TRUE);
    g_signal_connect (self, "delete-event",
                       G_CALLBACK (gtk_widget_hide_on_delete), NULL);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
    gtk_box_set_spacing (GTK_BOX (content_area), BOX_SPACING);
    gtk_container_set_border_width (GTK_CONTAINER (self),
                                    BOX_SPACING);

    builder = gtk_builder_new ();
    gtk_builder_add_from_resource (builder,
                                   "/org/gnome/EasyTAG/musicbrainz_dialog.ui",
                                   &error);

    if (error != NULL)
    {
        g_error ("Unable to get MusicBrainz dialog from resource: %s",
                 error->message);
    }

    grid = GTK_WIDGET (gtk_builder_get_object (builder, "musicbrainz_grid"));
    gtk_container_add (GTK_CONTAINER (content_area), grid);

    g_object_unref (builder);
}

static void
et_musicbrainz_dialog_finalize (GObject *object)
{
    EtMusicbrainzDialog *self;
    EtMusicbrainzDialogPrivate *priv;

    self = ET_MUSICBRAINZ_DIALOG (object);
    priv = et_musicbrainz_dialog_get_instance_private (self);

    g_clear_object (&priv->mb);

    G_OBJECT_CLASS (et_musicbrainz_dialog_parent_class)->finalize (object);
}

static void
et_musicbrainz_dialog_init (EtMusicbrainzDialog *self)
{
    EtMusicbrainzDialogPrivate *priv;

    priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                     ET_TYPE_MUSICBRAINZ_DIALOG,
                                                     EtMusicbrainzDialogPrivate);

    priv->mb = et_musicbrainz_new ();

    create_musicbrainz_dialog (self);
}

static void
et_musicbrainz_dialog_class_init (EtMusicbrainzDialogClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = et_musicbrainz_dialog_finalize;

    g_type_class_add_private (klass, sizeof (EtMusicbrainzDialogPrivate));
}

/*
 * et_musicbrainz_dialog_new:
 *
 * Create a new EtMusicbrainzDialog instance.
 *
 * Returns: a new #EtMusicbrainzDialog
 */
EtMusicbrainzDialog *
et_musicbrainz_dialog_new (GtkWindow *parent)
{
    g_return_val_if_fail (GTK_WINDOW (parent), NULL);

    return g_object_new (ET_TYPE_MUSICBRAINZ_DIALOG, "transient-for", parent,
                         NULL);
}

#endif /* ENABLE_MUSICBRAINZ */
