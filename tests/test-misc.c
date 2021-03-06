/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014 David King <amigadave@amigadave.com>
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
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "misc.h"

static void
misc_convert_duration (void)
{
    gsize i;
    gchar *time;

    static const struct
    {
        const gulong seconds;
        const gchar *result;
    } times[] = 
    {
        { 0, "0:00" },
        { 10, "0:10" },
        { 100, "1:40" },
        { 1000, "16:40" },
        { 10000, "2:46:40" },
        { 100000, "27:46:40" },
        { 1000000, "277:46:40" }
        /* TODO: Add more tests. */
    };

    for (i = 0; i < G_N_ELEMENTS (times); i++)
    {
        time = Convert_Duration (times[i].seconds);
        g_assert_cmpstr (time, ==, times[i].result);
        g_free (time);
    }
}

int
main (int argc, char** argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/misc/convert-duration", misc_convert_duration);

    return g_test_run ();
}
