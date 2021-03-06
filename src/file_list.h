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

#ifndef ET_FILE_LIST_H_
#define ET_FILE_LIST_H_

#include <glib.h>

G_BEGIN_DECLS

#include "file.h"
#include "file_tag.h"
#include "setting.h"

GList * ET_Add_File_To_File_List (gchar *filename);
gboolean ET_Remove_File_From_File_List (ET_File *ETFile);
gboolean ET_Free_File_List (void);

gboolean ET_Create_Artist_Album_File_List (void);
gboolean ET_Free_Artist_Album_File_List (void);
gboolean ET_Remove_File_From_File_List (ET_File *ETFile);

gboolean ET_Check_If_All_Files_Are_Saved (void);

GList * ET_Displayed_File_List_First (void);
GList * ET_Displayed_File_List_Previous (void);
GList * ET_Displayed_File_List_Next (void);
GList * ET_Displayed_File_List_Last (void);
GList * ET_Displayed_File_List_By_Etfile (const ET_File *ETFile);

gboolean ET_Set_Displayed_File_List (GList *ETFileList);
gboolean ET_Free_Displayed_File_List (void);

gboolean ET_Add_File_To_History_List (ET_File *ETFile);
ET_File * ET_Undo_History_File_Data (void);
ET_File * ET_Redo_History_File_Data (void);
gboolean ET_History_File_List_Has_Undo_Data (void);
gboolean ET_History_File_List_Has_Redo_Data (void);
gboolean ET_Free_History_File_List (void);

void ET_Update_Directory_Name_Into_File_List (const gchar *last_path, const gchar *new_path);
guint ET_Get_Number_Of_Files_In_Directory (const gchar *path_utf8);

GList *ET_Sort_File_List (GList *ETFileList, EtSortMode Sorting_Type);
void ET_Sort_Displayed_File_List (EtSortMode Sorting_Type);
void ET_Sort_Displayed_File_List_And_Update_UI (EtSortMode Sorting_Type);

G_END_DECLS

#endif /* !ET_FILE_H_ */
