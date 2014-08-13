/* musicbrainz_dialog.c - 2014/05/05 */
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
 
#include "config.h"

#ifdef ENABLE_MUSICBRAINZ

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <discid/discid.h>
#include <openssl/ssl.h>

#include "easytag.h"
#include "log.h"
#include "musicbrainz_dialog.h"
#include "mbentityview.h"
#include "mb_search.h"
#include "browser.h"
#include "application_window.h"
#include "dlm.h"

#define TAG_CHOICE_DIALOG_TITLE "Select the tag you want to apply to "
#define ET_MUSICBRAINZ_DIALOG_GET_PRIVATE(obj) (obj->priv)

G_DEFINE_TYPE (EtMusicBrainzDialog, et_musicbrainz_dialog, GTK_TYPE_DIALOG)

/***************
 * Declaration *
 ***************/

/*
 * TagChoiceColumns:
 * @TAG_CHOICE_TITLE: 
 * @TAG_CHOICE_ALBUM: 
 * @TAG_CHOICE_ARTIST: 
 * @TAG_CHOICE_ALBUM_ARTIST: 
 * @TAG_CHOICE_DATE: 
 * @TAG_CHOICE_COUNTRY: 
 * @TAG_CHOICE_COLS_N:
 *
 * Tag Choice Columns
 */
enum TagChoiceColumns
{
    TAG_CHOICE_TITLE,
    TAG_CHOICE_ALBUM,
    TAG_CHOICE_ARTIST,
    TAG_CHOICE_ALBUM_ARTIST,
    TAG_CHOICE_DATE,
    TAG_CHOICE_COUNTRY,
    TAG_CHOICE_COLS_N
};

/*
 * EtMbSearchType:
 * @ET_MB_SEARCH_TYPE_MANUAL: Manual Search Type
 * @ET_MB_SEARCH_TYPE_SELECTED: Selected Search Type
 * @ET_MB_SEARCH_TYPE_AUTOMATIC: Automatic Search Type
 *
 * Represents type of Search.
 */
typedef enum
{
    ET_MB_SEARCH_TYPE_MANUAL,
    ET_MB_SEARCH_TYPE_SELECTED,
    ET_MB_SEARCH_TYPE_AUTOMATIC,
} EtMbSearchType;

/*
 * EtMbSearch:
 * @type: Type of Search
 *
 * Thread Data for storing EtMbSearch's data.
 */
typedef struct
{
    EtMbSearchType type;
} EtMbSearch;

/*
 * EtMbManualSearch:
 * @parent: Parent Struct
 * @to_search: String to search
 * @to_search_type: EtMbEntityType to search
 * @parent_node: Parent Node
 *
 * Stores information about Manual Search.
 */
typedef struct
{
    EtMbSearch parent;
    gchar *to_search;
    MbEntityKind to_search_type;
    GNode *parent_node;
} EtMbManualSearch;

/*
 * EtMbSelectedSearch:
 * @parent: Parent Struct
 * @list_iter: List of Files.
 *
 * Stores information about Selected Files Search.
 */
typedef struct
{
    EtMbSearch parent;
    GList *list_iter;
    GArray *n_nodes;
} EtMbSelectedSearch;

/*
 * EtMbAutomaticSearch:
 * @parent: Parent Struct
 *
 * Stores information about Automatic Search.
 */
typedef struct
{
    EtMbSearch parent;
} EtMbAutomaticSearch;

/*
 * MusicBrainzDialogPrivate:
 * @mb_tree_root: Root of Node Tree
 * @async_result: GSimpleAsyncResult
 * @search: EtMbSearch storing information about previous search
 * @tag_choice_store: GtkTreeModel for Tag Choice Dialog
 * @tag_choice_dialog: Tag Choice Dialog
 *
 * Private data for MusicBrainzDialog.
 */
struct _EtMusicBrainzDialogPrivate
{
    GNode *mb_tree_root;
    GSimpleAsyncResult *async_result;
    EtMbSearch *search;
    GtkTreeModel *tag_choice_store;
    GtkWidget *tag_choice_dialog;
    GtkWidget *entityView;
    gboolean exit_on_complete;
    GtkStatusbar *statusbar;
};

static GtkWidget *mbDialog;
static GtkBuilder *builder;

/*
 * ManualSearchThreadData:
 * @text_to_search: Manual Search Text
 * @type: Type of Entity
 *
 * Thread Data for storing Manual Search's data.
 */
typedef struct
{
    gchar *text_to_search;
    int type;
    GNode *node;
} ManualSearchThreadData;

/*
 * SelectedFindThreadData:
 * @text_to_search: Manual Search Text
 * @type: Type of Entity
 *
 * Thread Data for storing Manual Search's data.
 */
typedef struct
{
    GHashTable *hash_table;
    GList *list_iter;
    GNode *node;
    GArray *n_nodes;
} SelectedFindThreadData;

/*
 * DiscIDSearchThreadData:
 * @node: GNode
 *
 * Data for DiscID Search.
 */
typedef struct 
{
    GNode *node;
} DiscIDSearchThreadData;

/*
 * FreeDBIDSearchThreadData:
 * @node: GNode
 * @cddb_discid: FreeDB Discid
 *
 * Thread Data for FreeDB ID Search.
 */
typedef struct 
{
    GNode *node;
    gchar *cddb_discid;
} FreeDBIDSearchThreadData;

/**************
 * Prototypes *
 **************/
static void
get_first_selected_file (ET_File  **et_file);
static gboolean
et_apply_track_tag_to_et_file (Mb5Recording recording, EtMbEntity *album_entity, 
                               ET_File *et_file);
static void
btn_close_clicked (GtkWidget *button, gpointer data);
static void
btn_selected_find_clicked (GtkWidget *widget, gpointer user_data);
static void
btn_automatic_search_clicked (GtkWidget *button, gpointer data);
static void
et_mb_destroy_search (EtMbSearch **search);
static void
et_mb_set_search_manual (EtMbSearch **search, gchar *to_search,
                         GNode *node, MbEntityKind type);
static void
et_mb_set_selected_search (EtMbSearch **search, GArray *n_nodes,
                           GList *list_files);
static void
et_mb_set_automatic_search (EtMbSearch **search);
static void
manual_search_callback (GObject *source, GAsyncResult *res,
                        gpointer user_data);
static void
et_show_status_msg_in_idle_cb (GObject *obj, GAsyncResult *res,
                               gpointer user_data);
static void
manual_search_thread_func (GSimpleAsyncResult *res, GObject *obj,
                           GCancellable *cancellable);
static void
btn_manual_find_clicked (GtkWidget *btn, gpointer user_data);
static void
tool_btn_toggle_red_lines_clicked (GtkWidget *btn, gpointer user_data);
static void
tool_btn_up_clicked (GtkWidget *btn, gpointer user_data);
static void
tool_btn_down_clicked (GtkWidget *btn, gpointer user_data);
static void
tool_btn_invert_selection_clicked (GtkWidget *btn, gpointer user_data);
static void
tool_btn_select_all_clicked (GtkWidget *btn, gpointer user_data);
static void
tool_btn_unselect_all_clicked (GtkWidget *btn, gpointer user_data);
static void
tool_btn_refresh_clicked (GtkWidget *btn, gpointer user_data);
static void
btn_manual_stop_clicked (GtkWidget *btn, gpointer user_data);
static void
entry_tree_view_search_changed (GtkEditable *editable, gpointer user_data);
static void
selected_find_callback (GObject *source, GAsyncResult *res,
                        gpointer user_data);
static void
selected_find_thread_func (GSimpleAsyncResult *res, GObject *obj,
                           GCancellable *cancellable);
static int
get_selected_iter_list (GtkTreeView *tree_view, GList **list);
static void
btn_selected_find_clicked (GtkWidget *widget, gpointer user_data);
static void
get_first_selected_file (ET_File **et_file);
#ifdef ENABLE_lIBDISCID
static void
discid_search_callback (GObject *source, GAsyncResult *res,
                        gpointer user_data);
static void
discid_search_thread_func (GSimpleAsyncResult *res, GObject *obj,
                           GCancellable *cancellable);
#endif /* ENABLE_LIBDISCID */

static void
btn_discid_search_clicked (GtkWidget *button, gpointer data);
static void
btn_close_clicked (GtkWidget *button, gpointer data);
static void
freedbid_search_callback (GObject *source, GAsyncResult *res,
                          gpointer user_data);
static void
freedbid_search_thread_func (GSimpleAsyncResult *res, GObject *obj,
                             GCancellable *cancellable);
static void
btn_automatic_search_clicked (GtkWidget *button, gpointer data);
static void
et_set_file_tag (ET_File *et_file, gchar *title, gchar *artist,
                 gchar *album, gchar *album_artist, gchar *date,
                 gchar *country);
static void
btn_apply_changes_clicked (GtkWidget *widget, gpointer data);
static void
et_initialize_tag_choice_dialog (EtMusicBrainzDialogPrivate *mb_dialog_priv);

/*************
 * Functions *
 *************/

/*
 * et_music_brainz_dialog_set_response:
 * @response: Response of GtkDialog
 *
 * Set the Response of MusicBrainzDialog and exit it.
 */
void
et_music_brainz_dialog_set_response (GtkResponseType response)
{
    gtk_dialog_response (GTK_DIALOG (mbDialog), response);
}

/*
 * et_music_brainz_dialog_set_statusbar_message:
 * @message: Message to be displayed
 *
 * Display message in Statusbar.
 */
void
et_music_brainz_dialog_set_statusbar_message (gchar *message)
{
    gtk_statusbar_push (GTK_STATUSBAR (gtk_builder_get_object (builder,
                        "statusbar")),
                        0, message);
}

/*
 * et_mb_destroy_search:
 * @search: EtMbSearch
 *
 * Destroyes an EtMbSearch Object.
 */
static void
et_mb_destroy_search (EtMbSearch **search)
{
    g_return_if_fail (search);
    
    if (!*search)
    {
        return;
    }

    switch ((*search)->type)
    {
        case ET_MB_SEARCH_TYPE_MANUAL:
        {
            g_free (((EtMbManualSearch *)(*search))->to_search);
            g_slice_free (EtMbManualSearch, (EtMbManualSearch *)(*search));
            break;
        }
        case ET_MB_SEARCH_TYPE_SELECTED:
        {
            g_list_free_full (((EtMbSelectedSearch *)(*search))->list_iter,
                              (GDestroyNotify)gtk_tree_iter_free);
            g_array_free (((EtMbSelectedSearch *)(*search))->n_nodes, TRUE);
            g_slice_free (EtMbSelectedSearch,
                          (EtMbSelectedSearch *)(*search));
            break;
        }
        case ET_MB_SEARCH_TYPE_AUTOMATIC:
        {
            g_slice_free (EtMbAutomaticSearch,
                          (EtMbAutomaticSearch *)(*search));
            break;
        }
    }
}

/*
 * et_mb_set_search_manual:
 * @search: EtMbSearch
 * @to_search: String searched in Manual Search
 * @node: Current Parent Node
 * @type: Type of Entity
 *
 * Set the EtMbSearch as Manual Search.
 */
static void
et_mb_set_search_manual (EtMbSearch **search, gchar *to_search,
                         GNode *node, MbEntityKind type)
{
    EtMbManualSearch *manual_search;

    et_mb_destroy_search (search);
    manual_search = g_slice_new (EtMbManualSearch);
    manual_search->to_search = g_strdup (to_search);
    manual_search->parent_node = node;
    manual_search->to_search_type = type;
    *search = (EtMbSearch *)manual_search;
    (*search)->type = ET_MB_SEARCH_TYPE_MANUAL;
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_fetch_more")),
                              TRUE);
}

/*
 * et_mb_set_search_manual:
 * @search: EtMbSearch
 * @list_files: List of Files selected
 *
 * Set the EtMbSearch as Selected Search.
 */
static void
et_mb_set_selected_search (EtMbSearch **search, GArray *n_nodes,
                           GList *list_files)
{
    et_mb_destroy_search (search);
    *search = (EtMbSearch *)g_slice_new (EtMbSelectedSearch);
    (*search)->type = ET_MB_SEARCH_TYPE_SELECTED;
    ((EtMbSelectedSearch *)(*search))->list_iter = list_files;
    ((EtMbSelectedSearch *)(*search))->n_nodes = n_nodes;
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_fetch_more")),
                              TRUE);
}

/*
 * et_mb_set_search_manual:
 * @search: EtMbSearch
 *
 * Set the EtMbSearch as Automatic Search.
 */
static void
et_mb_set_automatic_search (EtMbSearch **search)
{
    et_mb_destroy_search (search);
    *search = (EtMbSearch *)g_slice_new (EtMbAutomaticSearch);
    (*search)->type = ET_MB_SEARCH_TYPE_AUTOMATIC;
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_fetch_more")),
                              FALSE);
}

/*
 * manual_search_callback:
 * @source: Source Object
 * @res: GAsyncResult
 * @user_data: User data
 *
 * Callback function for GAsyncResult for Manual Search.
 */
static void
manual_search_callback (GObject *source, GAsyncResult *res,
                        gpointer user_data)
{
    GtkComboBoxText *combo_box;
    EtMusicBrainzDialogPrivate *priv;
    EtMusicBrainzDialog *dlg;
    GSimpleAsyncResult *simple_async;
    ManualSearchThreadData *thread_data;

    simple_async = G_SIMPLE_ASYNC_RESULT (res);
    thread_data = (ManualSearchThreadData *)user_data;

    if (!g_simple_async_result_get_op_res_gboolean (simple_async))
    {
        g_object_unref (res);
        g_slice_free (ManualSearchThreadData, user_data);
        et_music_brainz_dialog_stop_set_sensitive (FALSE);
        free_mb_tree (&thread_data->node);

        if (mbDialog)
        {
            dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
            priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);

            if (priv->search)
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);
            }
            else
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
            }
        }

        return;
    }

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_entity_view_clear_all (ET_MB_ENTITY_VIEW (priv->entityView));
    free_mb_tree (&priv->mb_tree_root);
    priv->mb_tree_root = thread_data->node;
    et_mb_entity_view_set_tree_root (ET_MB_ENTITY_VIEW (priv->entityView),
                                     priv->mb_tree_root);
    gtk_statusbar_push (priv->statusbar, 0, _("Searching Completed"));
    g_object_unref (res);
    combo_box = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder,
                                                            "cb_manual_search"));
    gtk_combo_box_text_append_text (combo_box,
                                    gtk_combo_box_text_get_active_text (combo_box));
    et_music_brainz_dialog_stop_set_sensitive (FALSE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);
    priv->search = ET_MB_SEARCH_TYPE_MANUAL;
    et_mb_set_search_manual (&priv->search, thread_data->text_to_search,
                             priv->mb_tree_root, thread_data->type);
    g_slice_free (ManualSearchThreadData, thread_data);
}

/*
 * manual_search_thread_func:
 * @res: GSimpleAsyncResult
 * @obj: Source GObject
 * @cancellable: GCancellable to cancel the operation
 *
 * Thread func of GSimpleAsyncResult to do Manual Search in another thread.
 */
static void
manual_search_thread_func (GSimpleAsyncResult *res, GObject *obj,
                           GCancellable *cancellable)
{
    GError *error;
    ManualSearchThreadData *thread_data;
    gchar *status_msg;
    
    error = NULL;
    g_simple_async_result_set_op_res_gboolean (res, FALSE);

    if (g_cancellable_is_cancelled (cancellable))
    {
        g_set_error (&error, ET_MB5_SEARCH_ERROR,
                     ET_MB5_SEARCH_ERROR_CANCELLED,
                     _("Operation cancelled by user"));
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              NULL, error);
        return;
    }

    thread_data = g_async_result_get_user_data (G_ASYNC_RESULT (res));
    status_msg = g_strdup_printf (_("Searching %s"),
                                  thread_data->text_to_search);
    et_show_status_msg_in_idle (status_msg);
    g_free (status_msg);

    if (g_cancellable_is_cancelled (cancellable))
    {
        g_set_error (&error, ET_MB5_SEARCH_ERROR,
                     ET_MB5_SEARCH_ERROR_CANCELLED,
                     _("Operation cancelled by user"));
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              thread_data, error);
        return;
    }

    if (!et_musicbrainz_search (thread_data->text_to_search,
                                thread_data->type, 
                                thread_data->node,
                                g_node_n_children (thread_data->node),
                                &error, cancellable))
    {
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              thread_data, error);
        return;
    }

    if (g_cancellable_is_cancelled (cancellable))
    {
        g_set_error (&error, ET_MB5_SEARCH_ERROR,
                     ET_MB5_SEARCH_ERROR_CANCELLED,
                     _("Operation cancelled by user"));
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              thread_data, error);
        return;
    }

    g_simple_async_result_set_op_res_gboolean (res, TRUE);
}

/*
 * btn_fetch_more_clicked:
 * @btn: GtkButton
 * @user_data: Additional Data
 *
 * Callback of "clicked" signal handler for btn_fetch_more.
 */
static void
btn_fetch_more_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialog *dlg;
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    int level;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    level = et_mb_entity_view_get_current_level (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));

    if (mb_dialog_priv->search->type == ET_MB_SEARCH_TYPE_AUTOMATIC && 
        level <= 2)
    {
        et_mb_entity_view_fetch_more_at_current_level (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
        return;
    }

    if (level != 1)
    {
        return;
    }

    if (mb_dialog_priv->search->type == ET_MB_SEARCH_TYPE_MANUAL)
    {
        ManualSearchThreadData *thread_data;
        EtMbManualSearch *manual_search;

        manual_search = (EtMbManualSearch *)mb_dialog_priv->search;
        thread_data = g_slice_new (ManualSearchThreadData);
        thread_data->type = manual_search->to_search_type;
        thread_data->node = g_node_copy_deep (mb_dialog_priv->mb_tree_root, 
                                              (GCopyFunc)et_mb_entity_copy, NULL);
        thread_data->text_to_search = manual_search->to_search;
        mb5_search_cancellable = g_cancellable_new ();
        gtk_statusbar_push (mb_dialog_priv->statusbar, 0,
                            _("Starting MusicBrainz Search"));
        mb_dialog_priv->async_result = g_simple_async_result_new (NULL, 
                                                                  manual_search_callback,
                                                                  thread_data,
                                                                  btn_fetch_more_clicked);
        g_simple_async_result_run_in_thread (mb_dialog_priv->async_result,
                                             manual_search_thread_func, 0,
                                             mb5_search_cancellable);
        et_music_brainz_dialog_stop_set_sensitive (TRUE);
        et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
    }
    else if (mb_dialog_priv->search->type == ET_MB_SEARCH_TYPE_SELECTED)
    {
        GList *iter_list;
        GList *l;
        GHashTable *hash_table;
        SelectedFindThreadData *thread_data;
        EtMbSelectedSearch *selected_search;
        GtkTreeView *browser_list;
        GtkTreeSelection *selection;

        selection = et_application_window_browser_get_selection (ET_APPLICATION_WINDOW (MainWindow));
        browser_list = gtk_tree_selection_get_tree_view (selection);   
        iter_list = NULL;
        l = NULL;
    
        if (!get_selected_iter_list (GTK_TREE_VIEW (browser_list), &iter_list))
        {
            gtk_statusbar_push (mb_dialog_priv->statusbar,
                            0, _("No Files Selected"));
            return;
        }
    
        hash_table = g_hash_table_new (g_str_hash, g_str_equal);
    
        for (l = iter_list; l != NULL; l = g_list_next (l))
        {
            ET_File *etfile;
            File_Tag *file_tag;
    
            etfile = et_application_window_browser_get_et_file_from_iter (ET_APPLICATION_WINDOW (MainWindow),
                                                                          (GtkTreeIter *)l->data);
    
            file_tag = (File_Tag *)etfile->FileTag->data;
    
            if (file_tag->album != NULL)
            {
                g_hash_table_add (hash_table, file_tag->album);
            }
        }
    
        selected_search = (EtMbSelectedSearch *)mb_dialog_priv->search;
        thread_data = g_slice_new (SelectedFindThreadData);
        thread_data->hash_table = hash_table;
        thread_data->list_iter = iter_list;
        thread_data->node = g_node_copy_deep (mb_dialog_priv->mb_tree_root, 
                                              (GCopyFunc)et_mb_entity_copy, NULL);
        thread_data->n_nodes = g_array_ref (selected_search->n_nodes);
        mb5_search_cancellable = g_cancellable_new ();
        mb_dialog_priv->async_result = g_simple_async_result_new (NULL,
                                                                  selected_find_callback,
                                                                  thread_data,
                                                                  btn_fetch_more_clicked);
        g_simple_async_result_run_in_thread (mb_dialog_priv->async_result,
                                             selected_find_thread_func, 0,
                                             mb5_search_cancellable);
        gtk_statusbar_push (mb_dialog_priv->statusbar, 0,
                            _("Starting Selected Files Search"));
        et_music_brainz_dialog_stop_set_sensitive (TRUE);
        et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
    }
}

/*
 * btn_manual_find_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of btn_manual_find.
 */
static void
btn_manual_find_clicked (GtkWidget *btn, gpointer user_data)
{
    GtkWidget *cb_manual_search;
    GtkWidget *cb_manual_search_in;
    int type;
    ManualSearchThreadData *thread_data;
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    cb_manual_search_in = GTK_WIDGET (gtk_builder_get_object (builder,
                                                              "cb_manual_search_in"));
    type = gtk_combo_box_get_active (GTK_COMBO_BOX (cb_manual_search_in));

    if (type == -1)
    {
        return;
    }

    cb_manual_search = GTK_WIDGET (gtk_builder_get_object (builder,
                                                           "cb_manual_search"));
    thread_data = g_slice_new (ManualSearchThreadData);
    thread_data->type = type;
    thread_data->node = g_node_new (NULL);
    thread_data->text_to_search = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (cb_manual_search));
    mb5_search_cancellable = g_cancellable_new ();
    gtk_statusbar_push (mb_dialog_priv->statusbar, 0,
                        _("Starting MusicBrainz Search"));
    mb_dialog_priv->async_result = g_simple_async_result_new (NULL, 
                                                              manual_search_callback,
                                                              thread_data,
                                                              btn_manual_find_clicked);
    g_simple_async_result_run_in_thread (mb_dialog_priv->async_result,
                                         manual_search_thread_func, 0,
                                         mb5_search_cancellable);
    et_music_brainz_dialog_stop_set_sensitive (TRUE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of toolbtn_toggle_red_lines.
 */
static void
tool_btn_toggle_red_lines_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_entity_view_toggle_red_lines (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of toolbtn_up.
 */
static void
tool_btn_up_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;

    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (ET_MUSICBRAINZ_DIALOG (mbDialog));
    et_mb_entity_view_select_up (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of toolbtn_down.
 */
static void
tool_btn_down_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_entity_view_select_down (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of toolbtn_invert_selection.
 */
static void
tool_btn_invert_selection_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_entity_view_invert_selection (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of toolbtn_select_all.
 */
static void
tool_btn_select_all_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_entity_view_select_all (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of toolbtn_unselect_all.
 */
static void
tool_btn_unselect_all_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_entity_view_unselect_all (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of btnManualStop.
 */
static void
tool_btn_refresh_clicked (GtkWidget *btn, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);

    if (!mb_dialog_priv->search)
    {
        return;
    }

    if (et_mb_entity_view_get_current_level (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView)) >
        1)
    {
        /* Current level is more than 1, refereshing means downloading an */
        /* entity's children */
        et_mb_entity_view_refresh_current_level (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
        return;
    }

    if (mb_dialog_priv->search->type == ET_MB_SEARCH_TYPE_MANUAL)
    {
        EtMbManualSearch *manual_search;
        GtkWidget *entry;

        manual_search = (EtMbManualSearch *)mb_dialog_priv->search;
        mb_dialog_priv->mb_tree_root = g_node_new (NULL);
        entry = gtk_bin_get_child (GTK_BIN (gtk_builder_get_object (builder, "cb_manual_search")));
        gtk_entry_set_text (GTK_ENTRY (entry), manual_search->to_search);
        gtk_combo_box_set_active (GTK_COMBO_BOX (gtk_builder_get_object (builder, "cb_manual_search_in")),
                                  manual_search->to_search_type);
        btn_manual_find_clicked (NULL, NULL);
    }
    else if (mb_dialog_priv->search->type == ET_MB_SEARCH_TYPE_SELECTED)
    {
        btn_selected_find_clicked (NULL, NULL);
    }
    else if (mb_dialog_priv->search->type == ET_MB_SEARCH_TYPE_AUTOMATIC)
    {
        btn_automatic_search_clicked (NULL, NULL);
    }
}

/*
 * btn_manual_stop_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of btnManualStop.
 */
static void
btn_manual_stop_clicked (GtkWidget *btn, gpointer user_data)
{
    if (G_IS_CANCELLABLE (mb5_search_cancellable))
    {
        g_cancellable_cancel (mb5_search_cancellable);
    }
}

/*
 * entry_tree_view_search_changed:
 * @editable: GtkEditable for which handler is called
 * @user_data: User data
 *
 * Signal Handler for "changed" signal of entry_tree_view_search.
 */
static void
entry_tree_view_search_changed (GtkEditable *editable, gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_entity_view_search_in_results (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView),
                                         gtk_entry_get_text (GTK_ENTRY (gtk_builder_get_object (builder,
                                                                        "entry_tree_view_search"))));
}

/*
 * selected_find_callback:
 * @source: Source Object
 * @res: GSimpleAsyncResult
 * @user_data: User data
 *
 * Callback function for Selected Search.
 */
static void
selected_find_callback (GObject *source, GAsyncResult *res,
                        gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;
    SelectedFindThreadData  *thread_data;

    thread_data = user_data;

    if (!g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res)))
    {
        g_object_unref (res);
        g_hash_table_destroy (thread_data->hash_table);
        et_music_brainz_dialog_stop_set_sensitive (FALSE);
        g_array_unref (thread_data->n_nodes);
        g_slice_free (SelectedFindThreadData, user_data);

        if (mbDialog)
        {
            dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
            mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);

            if (mb_dialog_priv->search)
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);
            }
            else
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
            }
        }

        return;
    }

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    free_mb_tree (&mb_dialog_priv->mb_tree_root);
    mb_dialog_priv->mb_tree_root = thread_data->node;
    et_mb_entity_view_clear_all (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
    et_mb_entity_view_set_tree_root (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView),
                                     mb_dialog_priv->mb_tree_root);
    gtk_statusbar_push (mb_dialog_priv->statusbar,
                        0, _("Searching Completed"));
    g_object_unref (res);
    g_hash_table_destroy (thread_data->hash_table);
    et_music_brainz_dialog_stop_set_sensitive (FALSE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);

    if (mb_dialog_priv->exit_on_complete)
    {
        btn_close_clicked (NULL, NULL);
    }

    et_mb_set_selected_search (&mb_dialog_priv->search,
                               thread_data->n_nodes,
                               ((SelectedFindThreadData *)user_data)->list_iter);
    g_slice_free (SelectedFindThreadData, user_data);
}

/*
 * selected_find_thread_func:
 * @res: GSimpleAsyncResult
 * @obj: Source Object
 * @cancellable: GCancellable
 *
 * Thread Function for Selected Files Search.
 */
static void
selected_find_thread_func (GSimpleAsyncResult *res, GObject *obj,
                           GCancellable *cancellable)
{
    GList *list_keys;
    GList *iter;
    SelectedFindThreadData *thread_data;
    GError *error;
    int i;
    int children;

    i = 0;
    g_simple_async_result_set_op_res_gboolean (res, FALSE);
    error = NULL;
    thread_data = g_async_result_get_user_data (G_ASYNC_RESULT (res));
    list_keys = g_hash_table_get_keys (thread_data->hash_table);
    g_array_set_size (thread_data->n_nodes, g_list_length (list_keys));
    iter = g_list_first (list_keys);

    while (iter)
    {
        children = g_node_n_children (thread_data->node);

        if (!et_musicbrainz_search ((gchar *)iter->data, MB_ENTITY_KIND_ALBUM,
                                    thread_data->node, 
                                    g_array_index (thread_data->n_nodes, int, i),
                                    &error, cancellable))
        {
            g_simple_async_report_gerror_in_idle (NULL,
                                                  mb5_search_error_callback,
                                                  thread_data, error);
            g_list_free (list_keys);
            return;
        }

        g_array_index (thread_data->n_nodes, int, i) += g_node_n_children (thread_data->node) - children;
 
        if (g_cancellable_is_cancelled (cancellable))
        {
            g_set_error (&error, ET_MB5_SEARCH_ERROR,
                         ET_MB5_SEARCH_ERROR_CANCELLED,
                         _("Operation cancelled by user"));
            g_simple_async_report_gerror_in_idle (NULL,
                                                  mb5_search_error_callback,
                                                  thread_data, error);
            g_list_free (list_keys);
            return;
        }

        iter = g_list_next (iter);
        i++;
    }

    g_list_free (list_keys);
    g_simple_async_result_set_op_res_gboolean (res, TRUE);
}

/*
 * get_selected_iter_list:
 * @tree_view: GtkTreeView
 * @list:[out] GList
 *
 * Returns: Number of Elements of list.
 *
 * Get the GList of selected iterators of GtkTreeView.
 */
static int
get_selected_iter_list (GtkTreeView *tree_view, GList **list)
{
    GtkTreeModel *tree_model;
    GtkTreeSelection *selection;
    int count;
    GList *l;
    GtkTreeView *browser_list;

    selection = et_application_window_browser_get_selection (ET_APPLICATION_WINDOW (MainWindow));
    browser_list = gtk_tree_selection_get_tree_view (selection);
    tree_model = GTK_TREE_MODEL(gtk_tree_view_get_model(GTK_TREE_VIEW(browser_list)));
    count = gtk_tree_selection_count_selected_rows(selection);

    if (count > 0)
    {
        GList* list_sel_rows;

        list_sel_rows = gtk_tree_selection_get_selected_rows (selection,
                                                              &tree_model);

        for (l = list_sel_rows; l != NULL; l = g_list_next (l))
        {
            GtkTreeIter iter;

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_model),
                                        &iter,
                                        (GtkTreePath *) l->data))
            {
                *list = g_list_prepend (*list,
                                        gtk_tree_iter_copy (&iter));
            }
        }

        g_list_free_full (list_sel_rows,
                          (GDestroyNotify)gtk_tree_path_free);

    }
    else /* No rows selected, use the whole list */
    {
        GtkTreeIter current_iter;

        if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (tree_model),
                                            &current_iter))
        {
            /* No row is present, return */
            return 0;
        }

        do
        {
            *list = g_list_prepend (*list,
                                    gtk_tree_iter_copy (&current_iter));
        }
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (tree_model),
               &current_iter));

        count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL(tree_model),
                                                NULL);
    }

    *list = g_list_reverse (*list);

    return count;
}

/*
 * btn_selected_find_clicked:
 * @button: GtkButton
 * @data: User data
 *
 * Signal Handler for "clicked" signal of btnSeelctedFind.
 */
static void
btn_selected_find_clicked (GtkWidget *button, gpointer data)
{
    GList *iter_list;
    GList *l;
    GHashTable *hash_table;
    SelectedFindThreadData *thread_data;
    GtkTreeView *browser_list;
    GtkTreeSelection *selection;
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;


    selection = et_application_window_browser_get_selection (ET_APPLICATION_WINDOW (MainWindow));
    browser_list = gtk_tree_selection_get_tree_view (selection);
    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    iter_list = NULL;
    l = NULL;

    if (!get_selected_iter_list (GTK_TREE_VIEW (browser_list), &iter_list))
    {
        gtk_statusbar_push (mb_dialog_priv->statusbar,
                        0, _("No Files Selected"));
        return;
    }

    hash_table = g_hash_table_new (g_str_hash, g_str_equal);

    for (l = iter_list; l != NULL; l = g_list_next (l))
    {
        ET_File *etfile;
        File_Tag *file_tag;

        etfile = et_application_window_browser_get_et_file_from_iter (ET_APPLICATION_WINDOW (MainWindow),
                                                                      (GtkTreeIter *)l->data);
        file_tag = (File_Tag *)etfile->FileTag->data;

        if (file_tag->album != NULL)
        {
            g_hash_table_add (hash_table, file_tag->album);
        }
    }

    thread_data = g_slice_new (SelectedFindThreadData);
    thread_data->hash_table = hash_table;
    thread_data->list_iter = iter_list;
    thread_data->node = g_node_new (NULL);
    thread_data->n_nodes = g_array_new (FALSE, TRUE, sizeof (int));
    mb5_search_cancellable = g_cancellable_new ();
    mb_dialog_priv->async_result = g_simple_async_result_new (NULL,
                                                              selected_find_callback,
                                                              thread_data,
                                                              btn_selected_find_clicked);
    g_simple_async_result_run_in_thread (mb_dialog_priv->async_result,
                                         selected_find_thread_func, 0,
                                         mb5_search_cancellable);
    gtk_statusbar_push (mb_dialog_priv->statusbar, 0,
                        _("Starting Selected Files Search"));
    et_music_brainz_dialog_stop_set_sensitive (TRUE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
}

/*
 * get_first_selected_file:
 * @et_file:[out] ET_File
 *
 * Get the First Selected File from BrowserFileList.
 */
static void
get_first_selected_file (ET_File  **et_file)
{
    GtkListStore *tree_model;
    GtkTreeSelection *selection;
    int count;
    GtkTreeView *browser_list;

    selection = et_application_window_browser_get_selection (ET_APPLICATION_WINDOW (MainWindow));
    browser_list = gtk_tree_selection_get_tree_view (selection);
    tree_model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(browser_list)));
    count = gtk_tree_selection_count_selected_rows(selection);
    *et_file = NULL;

    if (count > 0)
    {
        GList* list_sel_rows;
        GtkTreeIter iter;

        list_sel_rows = gtk_tree_selection_get_selected_rows(selection, NULL);
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_model),
                                    &iter,
                                    (GtkTreePath *)list_sel_rows->data))
        {
            *et_file = et_application_window_browser_get_et_file_from_iter (ET_APPLICATION_WINDOW (MainWindow),
                                                                            &iter);
        }

        g_list_free_full (list_sel_rows,
                          (GDestroyNotify)gtk_tree_path_free);
    }
}

#ifdef ENABLE_LIBDISCID
/*
 * discid_search_callback:
 * @source: Source Object
 * @res: GAsyncResult
 * @user_data: User data
 *
 * Callback function for GAsyncResult for Discid Search.
 */
static void
discid_search_callback (GObject *source, GAsyncResult *res,
                        gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;
    DiscIDSearchThreadData *thread_data;

    thread_data = user_data;
    
    if (!g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res)))
    {
        g_object_unref (res);
        free_mb_tree (&thread_data->node);
        et_music_brainz_dialog_stop_set_sensitive (FALSE);

        if (mbDialog)
        {
            dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
            mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);

            if (mb_dialog_priv->search)
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);
            }
            else
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
            }
        }

        return;
    }

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    free_mb_tree (&mb_dialog_priv->mb_tree_root);
    mb_dialog_priv->mb_tree_root = thread_data->node;
    et_mb_entity_view_clear_all (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
    et_mb_entity_view_set_tree_root (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView),
                                     mb_dialog_priv->mb_tree_root);
    gtk_statusbar_push (mb_dialog_priv->statusbar, 0,
                        _("Searching Completed"));
    g_object_unref (res);
    g_slice_free (DiscIDSearchThreadData, thread_data);
    et_music_brainz_dialog_stop_set_sensitive (FALSE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_fetch_more")),
                              FALSE);

    if (mb_dialog_priv->exit_on_complete)
    {
        btn_close_clicked (NULL, NULL);
    }
}

/*
 * discid_search_thread_func:
 * @res: GSimpleAsyncResult
 * @obj: Source GObject
 * @cancellable: GCancellable to cancel the operation
 *
 * Thread func of GSimpleAsyncResult to do DiscID Search in another thread.
 */
static void
discid_search_thread_func (GSimpleAsyncResult *res, GObject *obj,
                           GCancellable *cancellable)
{
    GError *error;
    DiscId *disc;
    gchar *discid;
    DiscIDSearchThreadData *thread_data;

    error = NULL;
    disc = discid_new ();
    g_simple_async_result_set_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res),
                                               FALSE);

    if (!discid_read_sparse (disc, discid_get_default_device (), 0))
    {
        /* Error reading disc */
        g_set_error (&error, ET_MB5_SEARCH_ERROR,
                     ET_MB5_SEARCH_ERROR_DISCID,
                     discid_get_error_msg (disc));
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              NULL, error);
        return;
    }

    discid = discid_get_id (disc);

    if (g_cancellable_is_cancelled (cancellable))
    {
        g_set_error (&error, ET_MB5_SEARCH_ERROR,
                     ET_MB5_SEARCH_ERROR_CANCELLED,
                     _("Operation cancelled by user"));
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              NULL, error);
        discid_free (disc);
        return;
    }

    thread_data = g_async_result_get_user_data (G_ASYNC_RESULT (res));
    if (!et_musicbrainz_search (discid, MB_ENTITY_KIND_DISCID,
                                thread_data->node, 0,
                                &error, cancellable))
    {
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              NULL, error);
        discid_free (disc);
        return;
    }

    discid_free (disc);
    g_simple_async_result_set_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res),
                                               TRUE);
}

#endif /* ENABLE_LIBDISCID */

/*
 * btn_discid_search_clicked:
 * @button: GtkButton
 * @data: User data
 *
 * Signal Handler for "clicked" signal of btnDiscidSearch.
 */
static void
btn_discid_search_clicked (GtkWidget *button, gpointer data)
{
#ifdef ENABLE_LIBDISCID
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;
    DiscIDSearchThreadData *thread_data;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    thread_data = g_slice_new (DiscIDSearchThreadData);
    thread_data->node = g_node_new (NULL);
    mb5_search_cancellable = g_cancellable_new ();
    gtk_statusbar_push (mb_dialog_priv->statusbar,
                        0, _("Starting MusicBrainz Search"));
    mb_dialog_priv->async_result = g_simple_async_result_new (NULL,
                                                              discid_search_callback,
                                                              thread_data,
                                                              btn_discid_search_clicked);
    g_simple_async_result_run_in_thread (mb_dialog_priv->async_result,
                                         discid_search_thread_func, 0,
                                         mb5_search_cancellable);
    et_music_brainz_dialog_stop_set_sensitive (TRUE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
#else
    GtkWidget *msg_dialog;
    
    msg_dialog = gtk_message_dialog_new (GTK_WINDOW (mbDialog), GTK_DIALOG_MODAL, 
                                         GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                         _("libdiscid is not installed. Please install it to use DiscID Search"));
    gtk_dialog_run (GTK_DIALOG (msg_dialog));
    gtk_widget_destroy (msg_dialog);
#endif /* ENABLE_LIBDISCID */
}

/*
 * btn_close_clicked:
 * @button: GtkButton
 * @data: User data
 *
 * Signal Handler for "clicked" signal of btn_close.
 */
static void
btn_close_clicked (GtkWidget *button, gpointer data)
{
    gtk_dialog_response (GTK_DIALOG (mbDialog), GTK_RESPONSE_DELETE_EVENT);
}

/*
 * freedbid_search_callback:
 * @source: Source Object
 * @res: GAsyncResult
 * @user_data: User data
 *
 * Callback function for GAsyncResult for FreeDB Search.
 */
static void
freedbid_search_callback (GObject *source, GAsyncResult *res,
                          gpointer user_data)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;
    FreeDBIDSearchThreadData *thread_data;

    thread_data = user_data;

    if (!g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res)))
    {
        g_object_unref (res);
        free_mb_tree (&thread_data->node);
        g_free (thread_data->cddb_discid);
        g_slice_free (FreeDBIDSearchThreadData, thread_data);
        et_music_brainz_dialog_stop_set_sensitive (FALSE);

        if (mbDialog)
        {
            dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
            mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);

            if (mb_dialog_priv->search)
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);
            }
            else
            {
                et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
            }
        }

        return;
    }

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    free_mb_tree (&mb_dialog_priv->mb_tree_root);
    mb_dialog_priv->mb_tree_root = thread_data->node;
    et_mb_entity_view_clear_all (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));
    et_mb_entity_view_set_tree_root (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView),
                                     mb_dialog_priv->mb_tree_root);
    gtk_statusbar_push (mb_dialog_priv->statusbar,
                        0, _("Searching Completed"));
    g_object_unref (res);
    g_free (thread_data->cddb_discid);
    g_slice_free (FreeDBIDSearchThreadData, thread_data);
    et_music_brainz_dialog_stop_set_sensitive (FALSE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (TRUE);
    et_mb_set_automatic_search (&mb_dialog_priv->search);

    if (mb_dialog_priv->exit_on_complete)
    {
        btn_close_clicked (NULL, NULL);
    }
}

/*
 * freedbid_search_thread_func:
 * @res: GSimpleAsyncResult
 * @obj: Source GObject
 * @cancellable: GCancellable to cancel the operation
 *
 * Thread func of GSimpleAsyncResult to do FreeDB Search in another thread.
 */
static void
freedbid_search_thread_func (GSimpleAsyncResult *res, GObject *obj,
                             GCancellable *cancellable)
{
    GError *error;
    FreeDBIDSearchThreadData *thread_data;

    error = NULL;
    g_simple_async_result_set_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res),
                                               FALSE);
    thread_data = g_async_result_get_user_data (G_ASYNC_RESULT (res));

    if (g_cancellable_is_cancelled (cancellable))
    {
        g_set_error (&error, ET_MB5_SEARCH_ERROR,
                     ET_MB5_SEARCH_ERROR_CANCELLED,
                     _("Operation cancelled by user"));
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              NULL, error);
        return;
    }

    if (!et_musicbrainz_search (thread_data->cddb_discid,
                                MB_ENTITY_KIND_FREEDBID,
                                thread_data->node, 0,
                                &error, cancellable))
    {
        g_simple_async_report_gerror_in_idle (NULL,
                                              mb5_search_error_callback,
                                              NULL, error);
        return;
    }

    g_simple_async_result_set_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (res),
                                               TRUE);
}

/*
 * btn_automatic_search_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of btn_automatic_search.
 */
static void
btn_automatic_search_clicked (GtkWidget *btn, gpointer data)
{
    GtkListStore *tree_model;
    GtkTreeSelection *selection;
    int count;
    GList *iter_list;
    GList *l;
    int total_id;
    int num_tracks;
    guint total_frames;
    guint disc_length;
    gchar *cddb_discid;
    GtkTreeView *browser_list;
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;
    FreeDBIDSearchThreadData *thread_data;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    total_frames = 150;
    disc_length  = 2;
    iter_list = NULL;
    selection = et_application_window_browser_get_selection (ET_APPLICATION_WINDOW (MainWindow));
    browser_list = gtk_tree_selection_get_tree_view (selection);    
    tree_model = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(browser_list)));
    count = gtk_tree_selection_count_selected_rows(selection);

    if (count > 0)
    {
        GList* list_sel_rows;

        list_sel_rows = gtk_tree_selection_get_selected_rows(selection, NULL);

        for (l = list_sel_rows; l != NULL; l = g_list_next (l))
        {
            GtkTreeIter iter;

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_model),
                                        &iter,
                                        (GtkTreePath *) l->data))
            {
                iter_list = g_list_prepend (iter_list,
                                            gtk_tree_iter_copy (&iter));
            }
        }

        g_list_free_full (list_sel_rows,
                          (GDestroyNotify)gtk_tree_path_free);

    }
    else /* No rows selected, use the whole list */
    {
        GtkTreeIter current_iter;

        if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tree_model),
                                           &current_iter))
        {
            /* No row is present, return */
            gtk_statusbar_push (mb_dialog_priv->statusbar,
                                0, _("No file selected"));
            return;
        }

        do
        {
            iter_list = g_list_prepend (iter_list,
                                        gtk_tree_iter_copy (&current_iter));
        }
        while (gtk_tree_model_iter_next(GTK_TREE_MODEL(tree_model),
               &current_iter));

        count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(tree_model),
                                               NULL);
    }

    if (count == 0)
    {
        gtk_statusbar_push (mb_dialog_priv->statusbar,
                            0, _("No file selected"));
        return;
    }

    else if (count > 99)
    {
        gtk_statusbar_push (mb_dialog_priv->statusbar,
                            0, _("More than 99 files selected. Cannot send the request."));
        return;
    }

    else
    {
        gchar *msg;
        msg = g_strdup_printf (ngettext (_("One file selected"),
                                         _("%d files selected"), count), count);
        gtk_statusbar_push (mb_dialog_priv->statusbar,
                            0, msg);
        g_free (msg);
    }
 
    /* Compute Discid from list */
    total_id = 0;
    num_tracks = count;

    for (l = iter_list; l != NULL; l = g_list_next (l))
    {
        ET_File *etfile;
        gulong secs = 0;

        etfile = et_application_window_browser_get_et_file_from_iter (ET_APPLICATION_WINDOW (MainWindow),
                                                                      (GtkTreeIter *)l->data);
        secs = etfile->ETFileInfo->duration;
        total_frames += secs * 75;
        disc_length  += secs;

        while (secs > 0)
        {
            total_id = total_id + (secs % 10);
            secs = secs / 10;
        }
    }

    thread_data = g_slice_new (FreeDBIDSearchThreadData);
    thread_data->node = g_node_new (NULL);
    cddb_discid = g_strdup_printf ("%08x", (guint)(((total_id % 0xFF) << 24) |
                                   (disc_length << 8) | num_tracks));
    thread_data->cddb_discid = cddb_discid;
    mb5_search_cancellable = g_cancellable_new ();
    gtk_statusbar_push (mb_dialog_priv->statusbar,
                        0, _("Starting MusicBrainz Search"));
    mb_dialog_priv->async_result = g_simple_async_result_new (NULL,
                                                              freedbid_search_callback,
                                                              thread_data,
                                                              btn_automatic_search_clicked);
    g_simple_async_result_run_in_thread (mb_dialog_priv->async_result,
                                         freedbid_search_thread_func, 0,
                                         mb5_search_cancellable);
    et_music_brainz_dialog_stop_set_sensitive (TRUE);
    et_music_brainz_dialog_toolbar_buttons_set_sensitive (FALSE);
}

/*
 * et_set_file_tag:
 * @et_file: ET_File 
 * @title: Title
 * @artist: Artist
 * @album: Album
 * @album_artist: Album Artist
 * @date: Date
 * @country: Country
 *
 * Set Tags of File to the values supplied as parameters.
 */
static void
et_set_file_tag (ET_File *et_file, gchar *title, gchar *artist,
                 gchar *album, gchar *album_artist, gchar *date,
                 gchar *country)
{
    File_Tag *file_tag;

    file_tag = ET_File_Tag_Item_New();
    ET_Copy_File_Tag_Item (et_file, file_tag);
    ET_Set_Field_File_Tag_Item (&file_tag->title, title);
    ET_Set_Field_File_Tag_Item (&file_tag->artist, artist);
    ET_Set_Field_File_Tag_Item (&file_tag->album, album);
    ET_Set_Field_File_Tag_Item (&file_tag->album_artist, album_artist);
    ET_Set_Field_File_Tag_Item (&file_tag->year, date);
    ET_Manage_Changes_Of_File_Data (et_file, NULL, file_tag);
    ET_Display_File_Data_To_UI (ETCore->ETFileDisplayed);
}

/*
 * btn_apply_changes_clicked:
 * @btn: GtkButton
 * @user_data: User data
 *
 * Signal Handler for "clicked" signal of btn_apply_changes.
 */
static void
btn_apply_changes_clicked (GtkWidget *btn, gpointer data)
{
    GList *file_iter_list;
    GList *track_iter_list;
    GList *list_iter1;
    GList *list_iter2;
    GtkTreeView *browser_list;
    GtkTreeSelection *selection;
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;
    EtMbEntity *et_entity;
    EtMbEntity *album_entity;
    gchar album[NAME_MAX_SIZE];

    selection = et_application_window_browser_get_selection (ET_APPLICATION_WINDOW (MainWindow));
    browser_list = gtk_tree_selection_get_tree_view (selection);
    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    file_iter_list = NULL;
    track_iter_list = NULL;

    if (!get_selected_iter_list (GTK_TREE_VIEW (browser_list),
                                 &file_iter_list))
    {
        gtk_statusbar_push (mb_dialog_priv->statusbar,
                            0, _("No Files Selected"));
        return;
    }

    if (!et_mb_entity_view_get_selected_entity_list (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView),
                                                     &track_iter_list))
    {
        gtk_statusbar_push (mb_dialog_priv->statusbar,
                            0, _("No Track Selected"));
        g_list_free_full (file_iter_list, (GDestroyNotify)gtk_tree_iter_free);

        return;
    }

    if (((EtMbEntity *)track_iter_list->data)->type !=
        MB_ENTITY_KIND_TRACK)
    {
        gtk_statusbar_push (mb_dialog_priv->statusbar,
                            0, _("No Track Selected"));
        g_list_free_full (file_iter_list, (GDestroyNotify)gtk_tree_iter_free);
        g_list_free (track_iter_list);

        return;
    }

    album_entity = et_mb_entity_view_get_current_entity (ET_MB_ENTITY_VIEW (mb_dialog_priv->entityView));

    if (album_entity)
    {
        mb5_release_get_title (album_entity->entity, album, 
                               sizeof (album));
    }

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "chk_use_dlm"))))
    {
        for (list_iter1 = track_iter_list; list_iter1;
             list_iter1 = g_list_next (list_iter1))
        {
            ET_File *best_et_file;
            gchar *filename;
            gchar title[NAME_MAX_SIZE];
            File_Tag *file_tag;
            int min_distance;

            min_distance = 0xFFFF;
            filename = NULL;
            best_et_file = NULL;
            et_entity = list_iter1->data;
            mb5_recording_get_title (et_entity->entity, title,
                                     sizeof (title));

            for (list_iter2 = file_iter_list; list_iter2 != NULL;
                 list_iter2 = g_list_next (list_iter2))
            {
                int distance;
                ET_File *et_file;

                et_file = et_application_window_browser_get_et_file_from_iter (ET_APPLICATION_WINDOW (MainWindow),
                                                                               list_iter2->data);
                filename = ((File_Name *)et_file->FileNameCur->data)->value;
                file_tag = (File_Tag *)et_file->FileTag->data;

                if (mb_dialog_priv->search->type == 
                    ET_MB_SEARCH_TYPE_SELECTED &&
                    !file_tag->album && g_strcmp0 (file_tag->album, album))
                {
                    continue;
                }

                distance = dlm (filename, title);

                if (distance < min_distance)
                {
                    min_distance = distance;
                    best_et_file = et_file;
                }
            }

            if (best_et_file)
            {
                et_apply_track_tag_to_et_file (et_entity->entity,
                                               album_entity, 
                                               best_et_file);
            }
        }
    }
    else
    {
        for (list_iter1 = track_iter_list, list_iter2 = file_iter_list;
             list_iter1 && list_iter2; list_iter1 = g_list_next (list_iter1),
             list_iter2 = g_list_next (list_iter2))
        {
            ET_File *et_file;

            et_entity = list_iter1->data;
            et_file = et_application_window_browser_get_et_file_from_iter (ET_APPLICATION_WINDOW (MainWindow),
                                                                           list_iter2->data);

            et_apply_track_tag_to_et_file (et_entity->entity, album_entity, 
                                           et_file);
        }
    }

    g_list_free_full (file_iter_list, (GDestroyNotify)gtk_tree_iter_free);
    g_list_free (track_iter_list);
}

/*
 * et_apply_track_tag_to_et_file:
 * @recording: MusicBrainz Recording
 * @et_file: ET_File to apply tags to
 * @album_entity: Default Album Entity
 *
 * Returns: TRUE if applied and FALSE it not
 *
 * Apply Tags from Mb5Recording to ET_File.
 */
static gboolean
et_apply_track_tag_to_et_file (Mb5Recording recording, EtMbEntity *album_entity, 
                               ET_File *et_file)
{
    int size;
    Mb5ReleaseList *release_list;
    gchar title[NAME_MAX_SIZE];
    gchar *album_artist;
    gchar album[NAME_MAX_SIZE];
    gchar date[NAME_MAX_SIZE];
    gchar country[NAME_MAX_SIZE];
    gchar *artist;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GtkWidget *tag_choice_tree_view;
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;
    Mb5Release release;
    gchar *tag_choice_title;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    artist = et_mb5_recording_get_artists_names (recording);
    tag_choice_tree_view = GTK_WIDGET (gtk_builder_get_object (builder, "tag_choice_treeview"));
    release_list = mb5_recording_get_releaselist (recording);
    size = mb5_recording_get_title (recording, title,
                                    sizeof (title));
    title[size] = '\0';
    size = mb5_release_list_size (release_list);

    if (!album_entity && size > 1)
    {
        /* More than one releases show Dialog and let user decide */
 
        for (size--; size >= 0; size--)
        {
            release = mb5_release_list_item (release_list, size);
            mb5_release_get_title (release, album, sizeof (album));
            album_artist = et_mb5_release_get_artists_names (release);
            mb5_release_get_date (release, date, sizeof (date));
            mb5_release_get_country (release, country, sizeof (country));

            gtk_list_store_insert_with_values (GTK_LIST_STORE (mb_dialog_priv->tag_choice_store),
                                               &iter, -1,
                                               TAG_CHOICE_TITLE, title,
                                               TAG_CHOICE_ALBUM, album,
                                               TAG_CHOICE_ARTIST, artist,
                                               TAG_CHOICE_ALBUM_ARTIST, album_artist,
                                               TAG_CHOICE_DATE, date,
                                               TAG_CHOICE_COUNTRY, country, -1);
            g_free (album_artist);
        }

        gtk_widget_set_size_request (mb_dialog_priv->tag_choice_dialog,
                                     600, 200);
        tag_choice_title = g_strconcat (TAG_CHOICE_DIALOG_TITLE, 
                                        ((File_Name *)et_file->FileNameCur->data)->value, 
                                        NULL);
        gtk_window_set_title (GTK_WINDOW (mb_dialog_priv->tag_choice_dialog),
                              tag_choice_title);
        g_free (tag_choice_title);
        gtk_widget_show_all (mb_dialog_priv->tag_choice_dialog);

        if (gtk_dialog_run (GTK_DIALOG (mb_dialog_priv->tag_choice_dialog)) == 0)
        {
            g_free (artist);
            gtk_widget_hide (mb_dialog_priv->tag_choice_dialog);
            gtk_list_store_clear (GTK_LIST_STORE (mb_dialog_priv->tag_choice_store));
            return FALSE;
        }

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tag_choice_tree_view));

        if (gtk_tree_selection_get_selected (selection,
                                             &mb_dialog_priv->tag_choice_store,
                                             &iter))
        {
            gchar *ret_album;
            gchar *ret_album_artist;
            gchar *ret_date;
            gchar *ret_country;

            gtk_tree_model_get (GTK_TREE_MODEL (mb_dialog_priv->tag_choice_store),
                                &iter, TAG_CHOICE_ALBUM, &ret_album,
                                TAG_CHOICE_ALBUM_ARTIST,&ret_album_artist,
                                TAG_CHOICE_DATE, &ret_date,
                                TAG_CHOICE_COUNTRY, &ret_country,
                                -1);
            et_set_file_tag (et_file, title, artist,
                             ret_album, ret_album_artist,
                             ret_date, ret_country);
            g_free (ret_album);
            g_free (ret_album_artist);
            g_free (ret_date);
            g_free (ret_country);
            g_free (artist);
    
            gtk_widget_hide (mb_dialog_priv->tag_choice_dialog);
            gtk_list_store_clear (GTK_LIST_STORE (mb_dialog_priv->tag_choice_store));
    
            return TRUE;
        }
        else
        {
            g_free (artist);
    
            gtk_widget_hide (mb_dialog_priv->tag_choice_dialog);
            gtk_list_store_clear (GTK_LIST_STORE (mb_dialog_priv->tag_choice_store));
    
            return FALSE;
        }
    }

    if (!album_entity)
    {
        release = mb5_release_list_item (release_list, 0);
    }
    else
    {
        release = album_entity->entity;
    }

    mb5_release_get_title (release, album, sizeof (album));
    album_artist = et_mb5_release_get_artists_names (release);
    mb5_release_get_date (release, date, sizeof (date));
    mb5_release_get_country (release, country, sizeof (country));
    et_set_file_tag (et_file, title, artist,
                     album, album_artist,
                     date, country);
    g_free (album_artist);
    g_free (artist);

    return TRUE;
}

/*
 * et_music_brainz_dialog_clear:
 * @widget: MusicBrainz Dialog
 *
 * Clears a MusicBrainz Object and set it to NULL
 */
static void
et_music_brainz_dialog_clear (GtkWidget **widget)
{
    if (!widget || !*widget)
    {
        return;
    }

    et_music_brainz_dialog_destroy (*widget);
    *widget = NULL;
}

/*
 * et_music_brainz_dialog_destroy:
 * @widget: MusicBrainz Dialog to destroy
 *
 * Destroy the MusicBrainz Dialog.
 */
void
et_music_brainz_dialog_destroy (GtkWidget *widget)
{
    gtk_widget_destroy (widget);
    g_object_unref (G_OBJECT (builder));
}

/*
 * et_initialize_tag_choice_dialog:
 *
 * Initialize the Tag Choice Dialog.
 */
static void
et_initialize_tag_choice_dialog (EtMusicBrainzDialogPrivate *mb_dialog_priv)
{
    GtkWidget *tag_choice_list;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkListStore *list_store;
    
    mb_dialog_priv->tag_choice_dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                            "tag_choice_dialog"));
    tag_choice_list = GTK_WIDGET (gtk_builder_get_object (builder,
                                                          "tag_choice_treeview"));
    list_store = gtk_list_store_new (TAG_CHOICE_COLS_N, G_TYPE_STRING,
                                     G_TYPE_STRING, G_TYPE_STRING,
                                     G_TYPE_STRING, G_TYPE_STRING,
                                     G_TYPE_STRING);
    mb_dialog_priv->tag_choice_store = GTK_TREE_MODEL (list_store);
    gtk_tree_view_set_model (GTK_TREE_VIEW (tag_choice_list),
                             mb_dialog_priv->tag_choice_store);
    g_object_unref (list_store);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Title",
                                                       renderer, "text",
                                                       TAG_CHOICE_TITLE,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tag_choice_list),
                                 column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Album",
                                                       renderer, "text",
                                                       TAG_CHOICE_ALBUM,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tag_choice_list),
                                 column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Artist",
                                                       renderer, "text",
                                                       TAG_CHOICE_ARTIST,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tag_choice_list),
                                 column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Album Artist",
                                                       renderer, "text",
                                                       TAG_CHOICE_ALBUM_ARTIST,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tag_choice_list),
                                 column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Date",
                                                       renderer, "text",
                                                       TAG_CHOICE_DATE,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tag_choice_list),
                                 column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Country",
                                                       renderer, "text",
                                                       TAG_CHOICE_COUNTRY,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (tag_choice_list),
                                 column);
}

gboolean
et_music_brainz_get_exit_on_complete (void)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;

    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (ET_MUSICBRAINZ_DIALOG (mbDialog));

    return mb_dialog_priv->exit_on_complete;
}

/*
 * et_show_status_msg_in_idle_cb:
 * @obj: Source Object
 * @res: GAsyncResult
 * @user_data: User data
 *
 * Callback function for Displaying StatusBar Message.
 */
static void
et_show_status_msg_in_idle_cb (GObject *obj, GAsyncResult *res,
                               gpointer user_data)
{
    gtk_statusbar_push (GTK_STATUSBAR (gtk_builder_get_object (builder,
                        "statusbar")), 0, user_data);
    g_free (user_data);
    g_object_unref (res);
}

/*
 * et_show_status_msg_in_idle_cb:
 * @obj: Source Object
 * @res: GAsyncResult
 * @user_data: User data
 *
 * Function to Display StatusBar Messages in Main Thread.
 */
void
et_show_status_msg_in_idle (gchar *message)
{
    GSimpleAsyncResult *async_res;

    async_res = g_simple_async_result_new (NULL,
                                           et_show_status_msg_in_idle_cb,
                                           g_strdup (message),
                                           et_show_status_msg_in_idle);
    g_simple_async_result_complete_in_idle (async_res);
}

/*
 * et_music_brainz_dialog_stop_set_sensitive:
 * @sensitive: gboolean
 *
 * Set btn_stop, EtMbEntityView and other widgets as sensitive 
 * according to @sensitive.
 */
void
et_music_brainz_dialog_stop_set_sensitive (gboolean sensitive)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;

    if (!mbDialog)
    {
        return;
    }

    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (ET_MUSICBRAINZ_DIALOG (mbDialog));
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_stop")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_apply_changes")),
                              !sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_manual_find")),
                              !sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_selected_find")),
                              !sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_automatic_search")),
                              !sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_disc_find")),
                              !sensitive);
    gtk_widget_set_sensitive (mb_dialog_priv->entityView, !sensitive);
}

/*
 * et_music_brainz_dialog_toolbar_buttons_set_sensitive:
 * @sensitive: Widget should be sensitive or not.
 *
 * Set the Toolbar Buttons sensitive.
 */
void
et_music_brainz_dialog_toolbar_buttons_set_sensitive (gboolean sensitive)
{
    if (!mbDialog)
    {
        return;
    }

    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "entry_tree_view_search")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "toolbtn_up")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "toolbtn_down")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "toolbtn_invert_selection")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "toolbtn_select_all")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "toolbtn_unselect_all")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "toolbtn_toggle_red_lines")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "toolbtn_refresh")),
                              sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "btn_fetch_more")),
                              sensitive);
}

/*
 * mb5_search_error_callback:
 * @obj: Source Object
 * @res: GAsyncResult
 * @user_data: User data
 *
 * Callback function for displaying errors.
 */
void
mb5_search_error_callback (GObject *source, GAsyncResult *res,
                           gpointer user_data)
{
    GError *dest;
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    dest = NULL;
    g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res),
                                           &dest);
    Log_Print (LOG_ERROR,
               _("Error searching MusicBrainz Database '%s'"), dest->message);
    gtk_statusbar_push (GTK_STATUSBAR (gtk_builder_get_object (builder,
                                       "statusbar")), 0, dest->message);
    g_error_free (dest);
    et_music_brainz_dialog_stop_set_sensitive (FALSE);

    if (mb_dialog_priv->exit_on_complete)
    {
        et_music_brainz_dialog_clear (&mbDialog);
    }
}

/*
 * et_mb_entity_view_destroy:
 * @object: EtMbEntityView
 *
 * Overloaded destructor for EtMbEntityView.
 */
static void
et_musicbrainz_dialog_finalize (GObject *object)
{
    EtMusicBrainzDialogPrivate *mb_dialog_priv;
    EtMusicBrainzDialog *dlg;

    g_return_if_fail (object != NULL);
    g_return_if_fail (IS_ET_MUSICBRAINZ_DIALOG(object));
    
    dlg = ET_MUSICBRAINZ_DIALOG (mbDialog);
    mb_dialog_priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (dlg);
    et_mb_destroy_search (&mb_dialog_priv->search);
    gtk_widget_destroy (mb_dialog_priv->tag_choice_dialog);
    G_OBJECT_CLASS (et_musicbrainz_dialog_parent_class)->finalize(object);
    free_mb_tree (&mb_dialog_priv->mb_tree_root);
}

/*
 * et_mb_entity_view_class_init:
 * @klass: EtMbEntityViewClass to initialize.
 *
 * Initializes an EtMbEntityViewClass class.
 */
static void
et_musicbrainz_dialog_class_init (EtMusicBrainzDialogClass *klass)
{
    g_type_class_add_private (klass, sizeof (EtMusicBrainzDialogPrivate));
    G_OBJECT_CLASS (klass)->finalize = et_musicbrainz_dialog_finalize;
}

/*
 * et_mb_entity_view_init:
 * @entity_view: EtMbEntityView to initialize.
 *
 * Initializes an EtMbEntityView.
 */
static void
et_musicbrainz_dialog_init (EtMusicBrainzDialog *dialog)
{
    EtMusicBrainzDialogPrivate *priv;
    GtkWidget *cb_manual_search_in;
    GtkWidget *cb_search;
    GtkWidget *box;
    GError *error;
    ET_File *et_file;

    priv = dialog->priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
                                                       et_musicbrainz_dialog_get_type (),
                                                       EtMusicBrainzDialogPrivate);
    builder = gtk_builder_new ();
    error = NULL;

    if (!gtk_builder_add_from_resource (builder,
                                        "/org/gnome/EasyTAG/musicbrainz_dialog.ui",
                                        &error))
    {
        Log_Print (LOG_ERROR,
                   _("Error loading MusicBrainz Search Dialog '%s'"),
                   error->message);
        g_error_free (error);
        g_object_unref (G_OBJECT (builder));
        return;
    }

    priv->mb_tree_root = g_node_new (NULL);
    priv->search = NULL;
    priv->exit_on_complete = FALSE;
    priv->entityView = et_mb_entity_view_new ();
    priv->statusbar = GTK_STATUSBAR (gtk_builder_get_object (builder, "statusbar"));
    box = GTK_WIDGET (gtk_builder_get_object (builder, "mb_box"));
    gtk_window_set_title (GTK_WINDOW (dialog), "MusicBrainz Search");
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                        box, TRUE, TRUE, 2);
    gtk_widget_set_size_request (GTK_WIDGET (dialog), 700, 500);
    gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (builder, "central_box")),
                        priv->entityView, TRUE, TRUE, 2);
    et_initialize_tag_choice_dialog (priv);
    cb_search = GTK_WIDGET (gtk_builder_get_object (builder, "cb_manual_search"));
    g_signal_connect (gtk_bin_get_child (GTK_BIN (cb_search)), "activate",
                      G_CALLBACK (btn_manual_find_clicked), NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_manual_find"),
                      "clicked", G_CALLBACK (btn_manual_find_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "toolbtn_up"),
                      "clicked", G_CALLBACK (tool_btn_up_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "toolbtn_down"),
                      "clicked", G_CALLBACK (tool_btn_down_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "toolbtn_select_all"),
                      "clicked", G_CALLBACK (tool_btn_select_all_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "toolbtn_unselect_all"),
                      "clicked", G_CALLBACK (tool_btn_unselect_all_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "toolbtn_invert_selection"),
                      "clicked", G_CALLBACK (tool_btn_invert_selection_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "toolbtn_toggle_red_lines"),
                      "clicked", G_CALLBACK (tool_btn_toggle_red_lines_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "toolbtn_refresh"),
                      "clicked", G_CALLBACK (tool_btn_refresh_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_selected_find"),
                      "clicked", G_CALLBACK (btn_selected_find_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_disc_find"),
                      "clicked", G_CALLBACK (btn_discid_search_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_stop"),
                      "clicked", G_CALLBACK (btn_manual_stop_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_close"),
                      "clicked", G_CALLBACK (btn_close_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_apply_changes"),
                      "clicked", G_CALLBACK (btn_apply_changes_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_automatic_search"),
                      "clicked", G_CALLBACK (btn_automatic_search_clicked),
                      NULL);
    g_signal_connect (gtk_builder_get_object (builder, "btn_fetch_more"),
                      "clicked", G_CALLBACK (btn_fetch_more_clicked),
                      NULL);
    g_signal_connect_after (gtk_builder_get_object (builder, "entry_tree_view_search"),
                            "changed",
                            G_CALLBACK (entry_tree_view_search_changed),
                            NULL);

    /* Fill Values in cb_manual_search_in */
    cb_manual_search_in = GTK_WIDGET (gtk_builder_get_object (builder,
                                                              "cb_manual_search_in"));

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb_manual_search_in),
                                    _("Artist"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb_manual_search_in),
                                    _("Album"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cb_manual_search_in),
                                    _("Track"));

    gtk_combo_box_set_active (GTK_COMBO_BOX (cb_manual_search_in), 1);

    /* Set the text of cb_manual_search to the album of selected file */
    get_first_selected_file (&et_file);

    if (et_file && ((File_Tag *)et_file->FileTag->data)->album)
    {
        gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (cb_search))),
                            ((File_Tag *)et_file->FileTag->data)->album);
    }
}

/*
 * et_mb_entity_view_new:
 *
 * Creates a new EtMbEntityView.
 *
 * Returns: GtkWidget, a new EtMbEntityView.
 */
GtkWidget *
et_musicbrainz_dialog_new ()
{
    return GTK_WIDGET (g_object_new (et_musicbrainz_dialog_get_type (), NULL));
}

/*
 * et_open_musicbrainz_dialog:
 *
 * This function will open the musicbrainz dialog.
 */
void
et_open_musicbrainz_dialog ()
{
    EtMusicBrainzDialogPrivate *priv;

    mbDialog = et_musicbrainz_dialog_new ();
    priv = ET_MUSICBRAINZ_DIALOG_GET_PRIVATE (ET_MUSICBRAINZ_DIALOG (mbDialog));
    gtk_widget_show_all (mbDialog);
    gtk_dialog_run (GTK_DIALOG (mbDialog));

    if (gtk_widget_get_sensitive (GTK_WIDGET (gtk_builder_get_object (builder,
                                  "btn_stop"))))
    {
        priv->exit_on_complete = TRUE;
        btn_manual_stop_clicked (NULL, NULL);
        gtk_widget_hide (mbDialog);
    }
    else
    {
        et_music_brainz_dialog_clear (&mbDialog);
    }
}
#endif /* ENABLE_MUSICBRAINZ */