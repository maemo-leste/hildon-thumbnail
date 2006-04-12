/*
 * This file is part of osso-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Luc Pionchon <luc.pionchon@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "osso-thumbnail-factory.h"
#include "thumbs-private.h"

#include "md5.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gfileutils.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libgnomevfs/gnome-vfs.h>

#include <gconf/gconf-client.h>

/**
 * Thumbnail factory
 */
typedef struct {
    GQueue *queue;
    GQueue *running_queue;
    
    //GList *cursor;
    guint idle_id;
    guint gconf_notify_id;

    GHashTable *mime_handlers;

    gint nprocesses, max_processes;

    gchar *thumb_base_dir;
    gchar *fail_dir;
} ThumbsFactory;

/**
 * Item in work queue
 */
typedef struct {
    gchar *uri;
    gchar *mime_type;
    guint width, height;
    OssoThumbnailFlags flags;
    OssoThumbnailFactoryFinishedCallback callback;
    gpointer user_data;
    gboolean canceled;
    
    // Data for spawned process (when necessary)
    guint thumb_width, thumb_height;
    gchar *temp_file;
    gchar *thumb_file;
    gchar *fail_file;
} ThumbsItem;

/**
 * Thumbnailing handler for a mime-type
 */
typedef struct {
    gchar *cmd;
} ThumbsHandler;

typedef struct {
    guint max_width;
    guint max_height;
    gchar *dir;
} ThumbsDirInfo;


/** Public <-> private handle conversion macros */
#define THUMBS_ITEM(handle) (ThumbsItem*)(handle)
#define THUMBS_HANDLE(item) (OssoThumbnailFactoryHandle)(item)

static gboolean process_func(gpointer data);
static void watch_func(GPid pid, gint status, gpointer data);

static void deregister_handlers();
static void register_handlers();

// Global data

static ThumbsFactory *factory = NULL;

static ThumbsDirInfo dir_info[] =
{
    { 80, 60, "osso" },
    { 128, 128, "normal" },
    { 256, 256, "large" },
    { 0, 0, NULL }
};

static GQuark app_quark = 0;

static gboolean show_debug = FALSE;

#define NR_MP3_OPTIONS 5

// Private functions

// Utility functions

static void thumb_item_free(ThumbsItem* item)
{
    g_free(item->uri);
    g_free(item->mime_type);

    if(item->temp_file) {
        g_free(item->temp_file);
    }
    if(item->thumb_file) {
        g_free(item->thumb_file);
    }
    if(item->fail_file) {
        g_free(item->fail_file);
    }

    g_free(item);
}

static ThumbsHandler *get_mime_handler(gchar *mime_type)
{
    return g_hash_table_lookup(factory->mime_handlers, mime_type);
}

static void md5_digest_to_ascii(guchar digest[16], gchar str[33])
{
    gint i;
    gchar *cursor = str;

    for(i = 0; i < 16; i++) {
        g_sprintf(cursor, "%02x", digest[i]);
        cursor += 2;
    }
}

/**
 * Get ASCII MD5 digest for a C string
 */
static void md5_c_string(const gchar *str, gchar ascii_digest[33])
{
    md5_state_t md5;
    guchar digest[16];

    md5_init(&md5);
    md5_append(&md5, (const unsigned char *)str, strlen(str));
    md5_finish(&md5, digest);
    md5_digest_to_ascii(digest, ascii_digest);
}

/*
static void md5_ascii_to_digest(gchar str[33], guchar digest[16])
{
    gint i;

    for(i = 0; i < 16; i++) {
        digest[i] = (g_ascii_xdigit_value(str[i * 2]) << 4) |
            g_ascii_xdigit_value(str[i * 2 + 1]);
    }
}
*/

// Processing functions

static gchar *get_thumb_path(char *dir)
{
    return g_build_filename(factory->thumb_base_dir, dir, NULL);
}

/**
 * Get the thumbnail category for the requested thumbnail size
 */
static gboolean get_size_info(guint width, guint height,
                              guint *thumb_width, guint *thumb_height,
                              gchar **dir)
{
    ThumbsDirInfo *info = dir_info;

    while(info->dir)  {
        if(width <= info->max_width && height <= info->max_height) {
            *thumb_width = info->max_width;
            *thumb_height =info->max_height;
            *dir = info->dir;
            return TRUE;
        }

        info++;
    }

    return FALSE;
}

static gboolean factory_can_run()
{
    return !g_queue_is_empty(factory->queue) &&
        factory->nprocesses < factory->max_processes;
}

static gboolean factory_is_running()
{
    return !g_queue_is_empty(factory->queue) ||
        factory->nprocesses > 0;
}

/**
 * To be called when queue changes or processes finish
 * Registers/unregisters idle event handler
 * Returns: TRUE if handler needed, FALSE if not
 */
static gboolean on_queue_change()
{
    if(factory_can_run()) {
        if(!factory->idle_id) {
            factory->idle_id = g_idle_add(process_func, factory);
        }

        return TRUE;
    } else {
        if(factory->idle_id) {
            //g_idle_remove_by_data(factory);
            g_source_remove(factory->idle_id);
            factory->idle_id = FALSE;
        }

        return FALSE;
    }
}

static void run_callback(ThumbsItem *item, GdkPixbuf *pixbuf, GError *error)
{
    if(!item->canceled) {
        item->callback(THUMBS_HANDLE(item), item->user_data, pixbuf, error);
    }
}

static GdkPixbuf* load_thumb_file(gchar *file, time_t *mtime, const gchar **uri,
                                  GError **error)
{
    GdkPixbuf *pixbuf;
    const gchar *uri_str, *mtime_str;

    pixbuf = gdk_pixbuf_new_from_file(file, NULL);

    if(!pixbuf) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_PIXBUF_LOAD_FAILED,
                    "Pixbuf loading failed for: %s", file);

        return NULL;
    }

    uri_str = gdk_pixbuf_get_option(pixbuf, URI_OPTION);
    mtime_str = gdk_pixbuf_get_option(pixbuf, MTIME_OPTION);

    if(!uri_str || !mtime_str) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_NO_PIXBUF_OPTIONS,
                    "Thumbnail does not contain URI or MTime tags: %s",
                    file);
        gdk_pixbuf_unref(pixbuf);
        return NULL;
    }

    if(uri) {
        *uri = uri_str;
    }
    if(mtime) {
        *mtime = atoi(mtime_str);
    }

    return pixbuf;
}

/**
 * Loads a thumbnail from cache and hands it over to callback
 *
 * Returns: TRUE if succeeded, FALSE if cache invalid
 */
static gboolean load_final_thumb(ThumbsItem *item, GError **error)
{
    GdkPixbuf *pixbuf, *pixbuf_scaled;
    const gchar *uri = NULL;
    time_t mtime = 0, cur_mtime;
    GError *load_err = NULL;

    // Can't use this since it discards pixbuf options which we needed
    //pixbuf = gdk_pixbuf_new_from_file_at_size(file,
    //  item->width, item->height, NULL);

    pixbuf = load_thumb_file(item->thumb_file, &mtime, &uri, &load_err);

    if(!pixbuf) {
        g_propagate_error(error, load_err);

        return FALSE;
    }

    cur_mtime = get_uri_mtime(uri);

    if(mtime != cur_mtime || strcmp(uri, item->uri) != 0) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_THUMB_EXPIRED,
                    "Thumbnail expired for: %s", uri);
        gdk_pixbuf_unref(pixbuf);
        return FALSE;
    }

    // Only resize when thumbnail requested is smaller
    if(item->width < gdk_pixbuf_get_width(pixbuf) ||
        item->height < gdk_pixbuf_get_height(pixbuf)) {

        pixbuf_scaled = gdk_pixbuf_scale_simple(pixbuf, item->width, item->height,
                                                GDK_INTERP_TILES);

        gdk_pixbuf_unref(pixbuf);
    } else {
        pixbuf_scaled = pixbuf;
    }

    run_callback(item, pixbuf_scaled, NULL);

    gdk_pixbuf_unref(pixbuf_scaled);
    return TRUE;
}

/**
 * Spawns a thumbnailer process for a given handler and thumbnail
 *
 * Returns: PID of spawned process or 0 if failed
 */
static GPid spawn_handler(ThumbsHandler *handler, ThumbsItem* item)
{
    gchar str_width[64], str_height[64], str_flags[64];
    gchar *args[] = { handler->cmd, item->uri,
        item->mime_type, item->temp_file,
        str_flags, str_width, str_height, NULL };

    gint pid = 0;

    g_sprintf(str_flags, "%d", item->flags);
    g_sprintf(str_width, "%d", item->thumb_width);
    g_sprintf(str_height, "%d", item->thumb_height);

    g_spawn_async(NULL, args, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                  NULL, NULL, &pid, NULL);

    return pid;
}

/**
 * Starts the thumbnail creation process
 *
 * Returns: FALSE if processing failed and item should be freed
 */
static gint process_thumb(ThumbsItem *item, GError **error)
{
    gchar *dir;
    ThumbsHandler *handler;

    gchar ascii_digest[33];

    gchar *thumb_dir;
    gchar temp_filename[128], thumb_filename[128];

    gint temp_fd;
    GPid pid;

    //g_message("Processing thumb: %s", item->uri);

    // Get thumbnail category
    if(!get_size_info(item->width, item->height,
        &item->thumb_width, &item->thumb_height, &dir)) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_ILLEGAL_SIZE,
                    "Illegal thumbnail size: (%d, %d)",
                    item->width, item->height);
        return FALSE;
    }

    // Get handler for MIME type
    handler = get_mime_handler(item->mime_type);
    if(!handler) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_NO_MIME_HANDLER,
                    "No handler for mime type: %s", item->mime_type);
        return FALSE;
    }

    thumb_dir = get_thumb_path(dir);

    if(!g_file_test(thumb_dir, G_FILE_TEST_IS_DIR)) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_NO_THUMB_DIR,
                    "Thumbnail path not a directory: %s", thumb_dir);

        g_free(thumb_dir);
        return FALSE;
    }

    // Get thumbnail filenames
    md5_c_string(item->uri, ascii_digest);

    g_sprintf(thumb_filename, "%s.png", ascii_digest);
    item->thumb_file = g_build_filename(thumb_dir, thumb_filename, NULL);

    g_sprintf(temp_filename, "tmp_%s.png.XXXXXX", ascii_digest);
    item->temp_file = g_build_filename(thumb_dir, temp_filename, NULL);

    item->fail_file = g_build_filename(factory->fail_dir, thumb_filename, NULL);

    g_free(thumb_dir);

    if(g_file_test(item->thumb_file, G_FILE_TEST_IS_REGULAR)) {
        // Load thumb
        if(show_debug)
            g_message("Loading thumb from cache: %s", item->uri);

        if(load_final_thumb(item, NULL)) {
            // Loaded from cache successfully
            thumb_item_free(item);

            return TRUE;
        }

        // Recreate when load failed
    }

    if(g_file_test(item->fail_file, G_FILE_TEST_IS_REGULAR)) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_FAILURE_CACHED,
                    "Cache indicates failure for: %s", item->uri);

        return FALSE;
    }

    temp_fd = g_mkstemp(item->temp_file);

    if(temp_fd == -1) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_TEMP_FILE_FAILED,
                    "Temporary file creation failed: %s", item->temp_file);

        return FALSE;
    }

    close(temp_fd);

    if(show_debug)
        g_message("Invoking thumbnailer: %s %s", item->uri, item->temp_file);

    if(!(pid = spawn_handler(handler, item))) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_SPAWN_FAILED,
                    "Spawning process failed: %s", handler->cmd);

        return FALSE;
    }

    if(!g_child_watch_add(pid, watch_func, item)) {
        g_set_error(error, app_quark, OSSO_THUMBNAIL_ERROR_CHILD_WATCH_FAILED,
                    "Child watch failed for pid: %d", pid);

        return FALSE;
    }

    // Go, horsey, go!
    factory->nprocesses++;
    
    g_queue_push_tail(factory->running_queue, item);

    on_queue_change();

    return TRUE;
}

/**
 * Idle handler, checks if there's work to be done
 */
static gboolean process_func(gpointer data)
{
    // Burst mode
    while(factory_can_run()) {
        GError *error = NULL;
        ThumbsItem *item = g_queue_pop_head(factory->queue);

        if(!process_thumb(item, &error)) {
            run_callback(item, NULL, error);
            thumb_item_free(item);
        }

        g_clear_error(&error);

        on_queue_change();
    }

    // on_queue_change() removes handler automagically, return TRUE
    return TRUE;
}

/**
 * Child process status notification handler, called when thumbnailer finishes
 */
static void watch_func(GPid pid, gint status, gpointer data)
{
    ThumbsItem *item = (ThumbsItem*)data;

    g_queue_remove(factory->running_queue, item);
    
    factory->nprocesses--;

    g_spawn_close_pid(pid);

    if(WIFEXITED(status)) {
        if(WEXITSTATUS(status) == 0) {
            if(rename(item->temp_file, item->thumb_file))
                g_warning("Thumbnail rename failed: %s -> %s",
                    item->temp_file, item->thumb_file);

            if(show_debug)
                g_message("Thumbnail hot off the press: %s", item->thumb_file);

            if(!item->canceled) {
                load_final_thumb(item, NULL);
            }
        } else {
            // Houston, we have a problem
            if(rename(item->temp_file, item->fail_file))
                g_warning("Thumbnail fail rename failed: %s -> %s",
                    item->temp_file, item->fail_file);

            g_warning("Thumbnailer failed for: %s", item->uri);
        }
    }

    // In case the rename failed
    unlink(item->temp_file);

    //if(!g_file_test(item->thumb_file, G_FILE_TEST_IS_REGULAR))
    //    g_warning("Thumbnail file does not exists after finishing: %s",
    //        item->thumb_file);

    thumb_item_free(item);

    on_queue_change();
}

static void add_mime_handler(const gchar *mime_type, gchar *cmd)
{
    ThumbsHandler *handler = g_new(ThumbsHandler, 1);
    handler->cmd = cmd;

    g_hash_table_insert(factory->mime_handlers, g_strdup(mime_type),
                        handler);
}

/**
 * Try to find thumbnailing program in build directory, then binary directory,
 * then path
 */
static gchar* get_handler_path(const gchar *filename)
{
    gchar *cmd;

    if(g_path_is_absolute(filename) &&
        g_file_test(filename, G_FILE_TEST_IS_EXECUTABLE)) {
        return g_strdup(filename);
    }

    cmd = g_build_filename(THUMBER_DIR, filename , NULL);
    if(g_file_test(cmd, G_FILE_TEST_IS_EXECUTABLE)) {
        return cmd;
    }
    g_free(cmd);

    cmd = g_build_filename(BIN_DIR, filename , NULL);
    if(g_file_test(cmd, G_FILE_TEST_IS_EXECUTABLE)) {
        return cmd;
    }
    g_free(cmd);

    cmd = g_find_program_in_path(filename);

    if(!cmd) {
        g_warning("Thumbnailer not found: %s", filename);
        return NULL;
    }

    return cmd;
}

/**
 * Registers all mime-types supported by GDK-Pixbuf to be handled
 * by the special included thumbnailer
 */
static void register_pixbuf_formats()
{
    static gchar *pixbuf_cmd = NULL;
    GSList *formats, *format;

    if(!pixbuf_cmd) {
        if(!(pixbuf_cmd = get_handler_path("osso-thumb-gdk-pixbuf"))) {
            return;
        }
    }

    formats = format = gdk_pixbuf_get_formats();

    while(format) {
        gchar **mime_types, **mime_type;

        mime_types = mime_type = gdk_pixbuf_format_get_mime_types(format->data);

        while(*mime_type != NULL) {
            add_mime_handler(*mime_type, pixbuf_cmd);

            mime_type++;
        }

        g_strfreev(mime_types);

        format = format->next;
    }

    g_slist_free(formats);
}

static void register_std_formats()
{
    //add_mime_handler("audio/mp3", get_handler_path("osso-thumb-libid3"));
}

// Configuration handling for GConf

static void load_mime_dir(GConfClient *client, const gchar *dirname)
{
    gchar *cmd_path = get_conf_cmd_path(dirname);
    gchar *cmd, *mime_type;

    cmd = gconf_client_get_string(client, cmd_path, NULL);

    g_free(cmd_path);

    if(!cmd) {
        g_warning("Thumbnailer does not have command: %s", dirname);
        return;
    }

    // Get directory name part
    mime_type = strrchr(dirname, '/');

    if(mime_type) {
        gchar *handler_path;

        mime_type++;

        unquote_mime_dir(mime_type);

        handler_path = get_handler_path(cmd);

        if(handler_path) {
            add_mime_handler(mime_type, handler_path);
        } else {
            g_warning("Thumbnailer not found: %s, %s", cmd, dirname);
        }
    }

    g_free(cmd);
}

static void load_all_mime_dirs(GConfClient *client)
{
    GSList *mime_dirs;
    GSList *dir;

    mime_dirs = gconf_client_all_dirs(client, THUMBS_GCONF_DIR, NULL);

    if(!mime_dirs) {
        //g_warning("No thumbnailers key in gconf registry");
        return;
    }

    for(dir = mime_dirs; dir; dir = dir->next) {
        gchar *dirname = dir->data;

        load_mime_dir(client, dirname);

        g_free(dirname);
    }

    g_slist_free(mime_dirs);
}

static void gconf_notify_func(GConfClient *client, guint cnxn_id,
                       GConfEntry *entry, gpointer user_data)
{
    // Do a complete reload of mime database
    deregister_handlers();

    register_handlers();
}

static void register_gconf_formats()
{
    GConfClient *client = gconf_client_get_default();

    g_return_if_fail(client);

    load_all_mime_dirs(client);

    // Register update notifier if necessary
    if(!factory->gconf_notify_id) {
        factory->gconf_notify_id =
            gconf_client_notify_add(client, THUMBS_GCONF_DIR,
                                    gconf_notify_func, NULL,
                                    NULL, NULL);
    }

    if(!factory->gconf_notify_id) {
        g_warning("Failed to add notifier for gconf: %s", THUMBS_GCONF_DIR);
    }

    g_object_unref(client);
}

static void thumb_handler_free(ThumbsHandler *handler)
{
    g_free(handler->cmd);
    g_free(handler);
}

static void register_handlers()
{
    factory->mime_handlers = g_hash_table_new_full(g_str_hash, g_str_equal,
                g_free, (GDestroyNotify)thumb_handler_free);

    register_pixbuf_formats();
    register_std_formats();
    register_gconf_formats();
}

static void deregister_handlers()
{
    g_hash_table_destroy(factory->mime_handlers);

    factory->mime_handlers = NULL;
}

/**
 * Initialize thumbnail paths and create cache directories
 */
static void init_thumb_dirs()
{
    ThumbsDirInfo *info = dir_info;
    int mode = 0700;
    gchar *fail_base;

    factory->thumb_base_dir =
            g_build_filename(g_get_home_dir(), ".thumbnails", NULL);
    mkdir(factory->thumb_base_dir, mode);

    fail_base = g_build_filename(factory->thumb_base_dir, "fail", NULL);
    mkdir(fail_base, mode);

    factory->fail_dir = g_build_filename(fail_base, OSSO_THUMBNAIL_APPLICATION, NULL);
    mkdir(factory->fail_dir, mode);

    g_free(fail_base);

    while(info->dir)  {
        char *path = get_thumb_path(info->dir);
        mkdir(path, mode);
        g_free(path);

        info++;
    }
}

static gboolean thumbs_init()
{
    if(!app_quark) {
        app_quark = g_quark_from_static_string(OSSO_THUMBNAIL_APPLICATION);
    }

    if(!factory) {
        g_type_init();
        gnome_vfs_init();

        factory = g_new(ThumbsFactory, 1);
        factory->queue = g_queue_new();
        factory->running_queue = g_queue_new();
                
        factory->idle_id = 0;
        factory->gconf_notify_id = 0;

        factory->nprocesses = 0;
        factory->max_processes = DEFAULT_MAX_PROCESSES;

        init_thumb_dirs();

        register_handlers();
    }

    return TRUE;
}

static void update_thumb(gchar *src_file, gchar *dest_file,
                         const gchar *uri, time_t mtime)
{
    GdkPixbuf *pixbuf;
    const gchar *mp3;
    gchar **keys, **values;

    pixbuf = load_thumb_file(src_file, NULL, NULL, NULL);
    if(!pixbuf) {
        return;
    }

    mp3 = gdk_pixbuf_get_option(pixbuf, MP3_OPTION);

    if (mp3)
    {
        /* keep MP3 options when saving */
        const gchar *noimage, *title, *artist, *album;
        gint i = 0;
        keys = g_new0(gchar *, NR_MP3_OPTIONS + 1);
        keys[i] = g_strdup (MP3_OPTION);
        values = g_new0(gchar *, NR_MP3_OPTIONS + 1);
        values[i] = g_strdup (mp3);
        i++;
        
        if ((noimage = gdk_pixbuf_get_option(pixbuf, MP3_NOIMAGE_OPTION)))
        {
            keys[i] = g_strdup (MP3_NOIMAGE_OPTION);
            values[i] = g_strdup (noimage);
            i++;
        }
        if ((title = gdk_pixbuf_get_option(pixbuf, MP3_TITLE_OPTION)))
        {
            keys[i] = g_strdup (MP3_TITLE_OPTION);
            values[i] = g_strdup (title);
            i++;
        }
        if ((artist = gdk_pixbuf_get_option(pixbuf, MP3_ARTIST_OPTION)))
        {
            keys[i] = g_strdup (MP3_ARTIST_OPTION);
            values[i] = g_strdup (artist);
            i++;
        }   
        if ((album = gdk_pixbuf_get_option(pixbuf, MP3_ALBUM_OPTION)))
        {
            keys[i] = g_strdup (MP3_ALBUM_OPTION);
            values[i] = g_strdup (album);
            i++;
        }
        
        save_thumb_file_meta(pixbuf, dest_file, mtime, uri, (const gchar**)
                             keys, (const gchar**) values);

        g_strfreev(keys);
        g_strfreev(values);
    }
    else
        save_thumb_file(pixbuf, dest_file, mtime, uri);

    gdk_pixbuf_unref(pixbuf);
}

typedef void (*update_func)(const gchar *src_uri, const gchar *dest_uri,
                            gchar *src_file, gchar *dest_file);

static void copy_update_op(const gchar *src_uri, const gchar *dest_uri,
                           gchar *src_file, gchar *dest_file)
{
    //link(src_file, dest_file);
    unlink(dest_file);

    update_thumb(src_file, dest_file, dest_uri, get_uri_mtime(dest_uri));
}

static void move_update_op(const gchar *src_uri, const gchar *dest_uri,
                           gchar *src_file, gchar *dest_file)
{
    unlink(dest_file);

    update_thumb(src_file, dest_file, dest_uri, get_uri_mtime(dest_uri));

    //g_message("unlink %s", src_file);
    unlink(src_file);
}

static void remove_update_op(const gchar *src_uri, const gchar *dest_uri,
                             gchar *src_file, gchar *dest_file)
{
    //g_message("unlink %s", src_file);
    unlink(src_file);
}

/**
 * Call the function @func with filenames set to
 * full paths of files named @src_name and @dest_name in directory @dir
 */
static void run_cache_update_dir(gchar *dir,
                                 const gchar *src_uri, const gchar *dest_uri,
                                 gchar *src_name, gchar *dest_name,
                                 update_func func)
{
    gchar *src_file, *dest_file = NULL;

    src_file = g_build_filename(dir, src_name, NULL);

    if(dest_name) {
        dest_file = g_build_filename(dir, dest_name, NULL);
    }

    //g_message("update %s (%s -> %s)", dir, src_uri, dest_uri);
    //g_message("update_file (%s -> %s)",  src_file, dest_file);

    func(src_uri, dest_uri, src_file, dest_file);

    g_free(src_file);
    if(dest_file) {
        g_free(dest_file);
    }
}

/**
 * Call the function @func for each cache directory with filenames set to
 * the cache files corresponding to @src_uri and @dest_uri
 */
static void run_cache_update(const gchar *src_uri, const gchar *dest_uri,
                      update_func func)
{
    gchar *src_name, *dest_name = NULL;
    gchar src_digest[33], dest_digest[33];

    ThumbsDirInfo *info = dir_info;

    md5_c_string(src_uri, src_digest);
    src_name = g_strconcat(src_digest, ".png", NULL);

    if(dest_uri) {
        md5_c_string(dest_uri, dest_digest);
        dest_name = g_strconcat(dest_digest, ".png", NULL);
    }

    run_cache_update_dir(factory->fail_dir, src_uri, dest_uri,
                         src_name, dest_name, func);

    while(info->dir)  {
        char *path = get_thumb_path(info->dir);

        run_cache_update_dir(path, src_uri, dest_uri,
                             src_name, dest_name, func);

        g_free(path);

        info++;
    }

    g_free(src_name);
    if(dest_name) {
        g_free(dest_name);
    }
}

// Cache cleaning functions

typedef struct {
    gchar *file;
    time_t mtime;
} ThumbsCacheFile;

static gint cache_file_compare(gconstpointer a, gconstpointer b)
{
    ThumbsCacheFile *f1 = *(ThumbsCacheFile**)a,
            *f2 = *(ThumbsCacheFile**)b;

    // Sort in descending order
    if(f2->mtime == f1->mtime) {
        return 0;
    } else if(f2->mtime < f1->mtime) {
        return -1;
    } else {
        return 1;
    }
}

static void cache_file_free(ThumbsCacheFile *item)
{
    g_free(item->file);
    g_free(item);
}

static void read_cache_dir(gchar *path, GPtrArray *files)
{
    GDir *dir;
    const gchar *file;

    dir = g_dir_open(path, 0, NULL);

    if(dir) {
        while((file = g_dir_read_name(dir)) != NULL) {
            gchar *file_path;
            ThumbsCacheFile *item;

            file_path = g_build_filename(path, file, NULL);

            if(file[0] == '.' || !g_file_test(file_path, G_FILE_TEST_IS_REGULAR)) {
                g_free(file_path);
                continue;
            }

            item = g_new(ThumbsCacheFile, 1);
            item->file = file_path;
            item->mtime = get_file_mtime(file_path);

            g_ptr_array_add(files, item);
        }

        g_dir_close(dir);
    }
}

// Public functions

void osso_thumbnail_factory_clean_cache(gint max_size, time_t min_mtime)
{
    GPtrArray *files;
    ThumbsDirInfo *info = dir_info;
    int i, size = 0;
    gboolean deleting = FALSE;

    thumbs_init();

    files = g_ptr_array_new();

    read_cache_dir(factory->fail_dir, files);

    while(info->dir)  {
        char *path = get_thumb_path(info->dir);

        read_cache_dir(path, files);

        g_free(path);

        info++;
    }

    g_ptr_array_sort(files, cache_file_compare);

    for(i = 0; i < files->len; i++) {
        ThumbsCacheFile *item = g_ptr_array_index(files, i);

        size += get_file_size(item->file);

        if((max_size >= 0 && size >= max_size) || item->mtime < min_mtime) {
            deleting = TRUE;
        }

        //g_message("Traversing %d,%s, deleting %d, size %d", item->mtime,
        //        item->file, deleting, size);

        if(deleting) {
            unlink(item->file);
        }
    }

    g_ptr_array_foreach(files, (GFunc)cache_file_free, NULL);

    g_ptr_array_free(files, TRUE);
}

GQuark osso_thumbnail_error_quark()
{
    thumbs_init();

    return app_quark;
}


OssoThumbnailFactoryHandle osso_thumbnail_factory_load_custom(
            const gchar *uri, const gchar *mime_type,
            guint width, guint height,
            OssoThumbnailFactoryFinishedCallback callback,
            gpointer user_data, OssoThumbnailFlags flags, ...)
{

    ThumbsItem *item;

    g_return_val_if_fail(uri != NULL && mime_type != NULL && callback != NULL,
                         NULL);

    thumbs_init();

    item = g_new(ThumbsItem, 1);

    item->uri = g_strdup(uri);
    item->mime_type = g_strdup(mime_type);
    item->width = width;
    item->height = height;
    item->callback = callback;
    item->user_data = user_data;
    item->flags = flags;
    item->canceled = FALSE;

    item->temp_file = item->thumb_file = item->fail_file = NULL;
    item->thumb_width = item->thumb_height = 0;

    g_queue_push_tail(factory->queue, item);

    on_queue_change();

    return THUMBS_HANDLE(item);
}

OssoThumbnailFactoryHandle osso_thumbnail_factory_load(
            const gchar *uri, const gchar *mime_type,
            guint width, guint height,
            OssoThumbnailFactoryFinishedCallback callback,
            gpointer user_data)
{
    return osso_thumbnail_factory_load_custom(uri, mime_type, width, height,
        callback, user_data, OSSO_THUMBNAIL_FLAG_CROP, -1);
}

void osso_thumbnail_factory_cancel(OssoThumbnailFactoryHandle handle)
{
    ThumbsItem *item;
    GList *lst;

    g_return_if_fail(handle);

    thumbs_init();

    item = THUMBS_ITEM(handle);

    lst = g_queue_find(factory->queue, item);

    if(lst) {
        g_queue_delete_link(factory->queue, lst);

        thumb_item_free(item);

        on_queue_change();
    } else {    
        // Not found in work queue,  maybe it is running?
        lst = g_queue_find(factory->running_queue, item);
        
        // Yes, the handle is valid
        if(lst) {
            item->canceled = TRUE;
            // Item will be freed by process notify func
        } else {
            g_warning("Thumbnail cancel on handle that doesn't exist in queue: %08X", (unsigned int)handle);
        }
    }
}

void osso_thumbnail_factory_wait()
{
    thumbs_init();

    while(factory_is_running()) {
        //process_func(factory);
        g_main_context_iteration(NULL, TRUE);
    }
}

void osso_thumbnail_factory_move(const gchar *src_uri, const gchar *dest_uri)
{
    g_return_if_fail(src_uri && dest_uri && strcmp(src_uri, dest_uri));

    thumbs_init();

    run_cache_update(src_uri, dest_uri, move_update_op);
}

void osso_thumbnail_factory_copy(const gchar *src_uri, const gchar *dest_uri)
{
    g_return_if_fail(src_uri && dest_uri && strcmp(src_uri, dest_uri));

    thumbs_init();

    run_cache_update(src_uri, dest_uri, copy_update_op);
}

void osso_thumbnail_factory_remove(const gchar *uri)
{
    g_return_if_fail(uri);

    thumbs_init();

    run_cache_update(uri, NULL, remove_update_op);
}

void osso_thumbnail_factory_move_front(OssoThumbnailFactoryHandle handle)
{
    GList *list;

    g_return_if_fail(handle);

    thumbs_init();

    list = g_queue_find(factory->queue, THUMBS_ITEM(handle));

    if(list) {
        g_queue_unlink(factory->queue, list);
        g_queue_push_head_link(factory->queue, list);
    }
}

void osso_thumbnail_factory_move_front_all_from(OssoThumbnailFactoryHandle handle)
{
    GList *list, *target;

    g_return_if_fail(handle);

    thumbs_init();

    target = g_queue_find(factory->queue, THUMBS_ITEM(handle));

    if(!target) {
        return;
    }

    while((list = g_queue_pop_head_link(factory->queue))) {
        if(list == target) {
            // Undo pop
            g_queue_push_head_link(factory->queue, list);
            break;
        } else {
            g_queue_push_tail_link(factory->queue, list);
        }
    }
}

void osso_thumbnail_factory_set_debug(gboolean debug)
{
    thumbs_init();

    show_debug = debug;
}
