/* bar.h - 2000/05/05 */
/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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


#ifndef __BAR_H__
#define __BAR_H__

/***************
 * Declaration *
 ***************/
GtkWidget      *ProgressBar;
GtkUIManager   *UIManager;
GtkActionGroup *ActionGroup;

#define POPUP_FILE              "FilePopup"
#define POPUP_DIR               "DirPopup"
#define POPUP_SUBMENU_SCANNER   "ScannerSubpopup"
#define POPUP_DIR_RUN_AUDIO     "DirPopupRunAudio"
#define POPUP_LOG               "LogPopup"

#define AM_OPEN_OPTIONS_WINDOW      "Preferences"
#define AM_CDDB_SEARCH_FILE         "CDDBSearchFile"

#define AM_ARTIST_RUN_AUDIO_PLAYER  "ArtistRunAudio"
#define AM_ARTIST_OPEN_FILE_WITH    "ArtistOpenFile"
#define AM_ALBUM_RUN_AUDIO_PLAYER   "AlbumRunAudio"
#define AM_ALBUM_OPEN_FILE_WITH     "AlbumOpenFile"

#define AM_LOG_CLEAN                "CleanLog"

#define AM_STOP                     "Stop"

typedef struct _Action_Pair Action_Pair;
struct _Action_Pair {
    const gchar *action;
    GQuark quark;
};

/**************
 * Prototypes *
 **************/

GtkWidget *create_main_toolbar (GtkWindow *window);
GtkWidget *Create_Status_Bar   (void);
void Statusbar_Message (const gchar *message, gboolean with_timer);
GtkWidget *Create_Progress_Bar (void);

#endif /* __BAR_H__ */
