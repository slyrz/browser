#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
#define true 1
#define false 0
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

const gchar *preferred_languages[] = {
  PREFERRED_LANGUAGES,
  NULL
};

enum HistoryAction {
  HISTORY_MOVE_BACK,
  HISTORY_MOVE_FORWARD,
  HISTORY_RELOAD,
};

enum SearchAction {
  SEARCH_START,
  SEARCH_NEXT,
  SEARCH_PREVIOUS,
};

enum ZoomAction {
  ZOOM_IN,
  ZOOM_OUT,
  ZOOM_SET,
};

#define USER_CACHE   (g_get_user_cache_dir())
#define USER_CONFIG  (g_get_user_config_dir())
#define USER_HOME    (g_get_home_dir())

#define PATH(location, ...) \
  (g_build_path(G_DIR_SEPARATOR_S, location, ##__VA_ARGS__, NULL))

static gchar *get_input(void);
static gboolean on_key_press_event(GtkWidget *window, GdkEventKey *event, GtkWidget *web_view);
static void do_history(GtkWidget *window, GtkWidget *web_view, gint direction);
static void do_navigation(GtkWidget *window, GtkWidget *web_view);
static void do_search(GtkWidget *window, GtkWidget *web_view, gint command);
static void do_exit(GtkWidget *window, GtkWidget *web_view);
static void do_zoom(GtkWidget *window, GtkWidget *web_view, gint action, gdouble adjust);
static void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, GtkWidget *window);
static gboolean on_decide_destination(WebKitDownload *download, gchar *suggested_filename, gpointer user_data);
static void on_received_data(WebKitDownload *download, guint64 data_length, gpointer user_data);
static void on_download_finished(WebKitDownload *download, gpointer user_data);
static void on_download_failed(WebKitDownload *download, GError *error, gpointer user_data);
static void on_download_started(WebKitWebContext *context, WebKitDownload *download, gpointer user_data);
static void on_initialize_web_extensions(WebKitWebContext *context, gpointer user_data);

static gchar *
get_input(void) {
  gchar *input_command[] = {
    INPUT_COMMAND,
    NULL
  };

  GError *error = NULL;
  gchar *line = NULL;
  gint stdin;
  gint stdout;

  g_spawn_async_with_pipes(NULL, input_command, NULL, 0, NULL, NULL, NULL, &stdin, &stdout, NULL, &error);
  g_assert_no_error(error);

  g_close(stdin, &error);
  g_assert_no_error(error);

  GIOChannel *channel = g_io_channel_unix_new(stdout);
  g_io_channel_read_line(channel, &line, NULL, NULL, &error);
  g_assert_no_error(error);
  g_io_channel_shutdown(channel, false, &error);
  g_assert_no_error(error);

  if (line != NULL)
    g_strstrip(line);
  return line;
}

static void
do_history(GtkWidget *window, GtkWidget *web_view, gint direction) {
  switch (direction) {
  case HISTORY_MOVE_BACK:
    webkit_web_view_go_back(WEBKIT_WEB_VIEW(web_view));
    break;
  case HISTORY_MOVE_FORWARD:
    webkit_web_view_go_forward(WEBKIT_WEB_VIEW(web_view));
    break;
  case HISTORY_RELOAD:
    webkit_web_view_reload(WEBKIT_WEB_VIEW(web_view));
    break;
  }
}

static void
do_navigation(GtkWidget *window, GtkWidget *web_view) {
  g_autofree gchar *input = get_input();
  g_autofree gchar *url = NULL;

  if ((input == NULL) || (strlen(input) == 0)) {
    return;
  }

  if (g_str_has_prefix(input, "http://") || g_str_has_prefix(input, "https://")) {
    url = input;
  } else {
    url = g_strdup_printf("https://%s", input);
  }

  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), url);

  if (input == url)
    input = NULL;
}

static void
do_search(GtkWidget *window, GtkWidget *web_view, gint command) {
  const WebKitFindOptions find_options = WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE | WEBKIT_FIND_OPTIONS_WRAP_AROUND;
  const guint max_match_count = 1024;

  WebKitFindController *find_controller;
  g_autofree gchar *input = NULL;

  find_controller = webkit_web_view_get_find_controller(WEBKIT_WEB_VIEW(web_view));
  switch (command) {
  case SEARCH_START:
    input = get_input();
    if ((input != NULL) && (strlen(input) > 0)) {
      webkit_find_controller_search(find_controller, input, find_options, max_match_count);
    }
    break;
  case SEARCH_NEXT:
    webkit_find_controller_search_next(find_controller);
    break;
  case SEARCH_PREVIOUS:
    webkit_find_controller_search_previous(find_controller);
    break;
  }
}

static void
do_exit(GtkWidget *window, GtkWidget *web_view) {
  gtk_main_quit();
}

static void
do_zoom(GtkWidget *window, GtkWidget *web_view, gint action, gdouble adjust) {
  const gdouble max_zoom = 4.0;
  const gdouble min_zoom = 0.25;

  gdouble level = webkit_web_view_get_zoom_level(WEBKIT_WEB_VIEW(web_view));
  switch (action) {
  case ZOOM_IN:
    level += adjust;
    break;
  case ZOOM_OUT:
    level -= adjust;
    break;
  case ZOOM_SET:
    level = adjust;
    break;
  }
  level = MIN(level, max_zoom);
  level = MAX(level, min_zoom);
  webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(web_view), level);
}

static void
on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, GtkWidget *window) {
  g_autofree gchar *title = NULL;

  switch (load_event) {
  case WEBKIT_LOAD_STARTED:
  case WEBKIT_LOAD_REDIRECTED:
  case WEBKIT_LOAD_COMMITTED:
    title = g_strdup_printf(PACKAGE " | Loading %.0f%% ...", webkit_web_view_get_estimated_load_progress(web_view) * 100.0);
    break;
  case WEBKIT_LOAD_FINISHED:
    title = g_strdup_printf(PACKAGE " | %s", webkit_web_view_get_title(web_view));
    break;
  }

  if (title != NULL) {
    gtk_window_set_title(GTK_WINDOW(window), title);
  }
}

static gboolean
on_decide_destination(WebKitDownload *download, gchar *suggested_filename, gpointer user_data) {
  g_autofree gchar *path = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *uri = NULL;
  guint iter = 0;

  do {
    g_free(name);
    g_free(path);

    if (iter == 0) {
      name = g_strdup(suggested_filename);
    } else {
      name = g_strdup_printf("%s.%u", suggested_filename, iter);
    }

    path = PATH(USER_HOME, "Downloads", name);
    iter++;
  } while (g_file_test(path, G_FILE_TEST_EXISTS));

  g_message("Saving file %s to %s", suggested_filename, uri);

  uri = g_strdup_printf("file://%s", path);
  webkit_download_set_destination(download, uri);
  return false;
}

static void
on_received_data(WebKitDownload *download, guint64 data_length, gpointer user_data) {
  g_debug("Received %" G_GUINT64_FORMAT " bytes (%.0f%% done)",
    webkit_download_get_received_data_length(download),
    webkit_download_get_estimated_progress(download) * 100.0);
}

static void
on_download_finished(WebKitDownload *download, gpointer user_data) {
  g_message("Download finished");
}

static void
on_download_failed(WebKitDownload *download, GError *error, gpointer user_data) {
  g_warning("Download failed: %s", error->message);
}

static void
on_download_started(WebKitWebContext *context, WebKitDownload *download, gpointer user_data) {
  g_message("Download started");
  g_signal_connect(G_OBJECT(download), "decide-destination", G_CALLBACK(on_decide_destination), NULL);
  g_signal_connect(G_OBJECT(download), "received-data", G_CALLBACK(on_received_data), NULL);
  g_signal_connect(G_OBJECT(download), "finished", G_CALLBACK(on_download_finished), NULL);
  g_signal_connect(G_OBJECT(download), "failed", G_CALLBACK(on_download_failed), NULL);
}

static void
on_initialize_web_extensions(WebKitWebContext *context, gpointer user_data) {
  static guint32 unique_id = 0;
  static gchar *extensions = NULL;

  if (extensions == NULL) {
    extensions = PATH(USER_CONFIG, PACKAGE, "extensions");
    g_debug ("Extensions must be placed in %s", extensions);
  }
  webkit_web_context_set_web_extensions_directory(context, extensions);
  webkit_web_context_set_web_extensions_initialization_user_data(context, g_variant_new_uint32(unique_id++));
}

static gboolean
on_key_press_event(GtkWidget *window, GdkEventKey *event, GtkWidget *web_view) {
  GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();

  if ((event->state & modifiers) == GDK_CONTROL_MASK) {
    switch (event->keyval) {
    case KEY_NAVIGATE:
      do_navigation(window, web_view);
      return true;
    case KEY_SEARCH_START:
      do_search(window, web_view, SEARCH_START);
      return true;
    case KEY_SEARCH_NEXT:
      do_search(window, web_view, SEARCH_NEXT);
      return true;
    case KEY_SEARCH_PREVIOUS:
      do_search(window, web_view, SEARCH_PREVIOUS);
      return true;
    case KEY_HISTORY_MOVE_BACK:
      do_history(window, web_view, HISTORY_MOVE_BACK);
      return true;
    case KEY_HISTORY_MOVE_FORWARD:
      do_history(window, web_view, HISTORY_MOVE_FORWARD);
      return true;
    case KEY_HISTORY_RELOAD:
      do_history(window, web_view, HISTORY_RELOAD);
      return true;
    case KEY_ZOOM_IN:
      do_zoom(window, web_view, ZOOM_IN, ZOOM_STEP);
      return true;
    case KEY_ZOOM_OUT:
      do_zoom(window, web_view, ZOOM_OUT, ZOOM_STEP);
      return true;
    case KEY_ZOOM_RESET:
      do_zoom(window, web_view, ZOOM_SET, 1.0);
      return true;
    case KEY_EXIT:
      do_exit(window, web_view);
      return true;
    }
  }
  return false;
}

static gboolean
on_delete_event(GtkWidget *window, GdkEventKey *event, GtkWidget *web_view) {
  do_exit (window, web_view);
  return false;
}

int
main(int argc, char **argv) {
  GtkWidget *window;
  GtkWidget *web_view;

  WebKitSettings *settings;
  WebKitWebContext *context;
  WebKitWebsiteDataManager *data_manager;

  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), PACKAGE);

  {
    g_autofree gchar *cache = PATH(USER_CACHE, PACKAGE, "cache");
    g_autofree gchar *data = PATH(USER_CACHE, PACKAGE, "data");

    g_debug("Cache directory in %s", cache);
    g_debug("Data directory in %s", data);
    data_manager = webkit_website_data_manager_new(
        "base-cache-directory", cache,
        "base-data-directory", data,
        NULL
    );
  }

  context = webkit_web_context_new_with_website_data_manager(data_manager);
  webkit_web_context_set_web_process_count_limit(context, 4);
  webkit_web_context_set_tls_errors_policy(context, WEBKIT_TLS_ERRORS_POLICY_FAIL);
  webkit_web_context_set_cache_model(context, WEBKIT_CACHE_MODEL_WEB_BROWSER);
  webkit_web_context_set_spell_checking_enabled(context, false);
  webkit_web_context_set_preferred_languages(context, preferred_languages);

  settings = webkit_settings_new();
  webkit_settings_set_allow_file_access_from_file_urls(settings, false);
  webkit_settings_set_allow_universal_access_from_file_urls(settings, false);
  webkit_settings_set_auto_load_images(settings, true);
  webkit_settings_set_enable_developer_extras(settings, false);
  webkit_settings_set_enable_page_cache(settings, true);
  webkit_settings_set_enable_smooth_scrolling(settings, false);
  webkit_settings_set_enable_webaudio(settings, false);
  webkit_settings_set_javascript_can_access_clipboard(settings, false);
  webkit_settings_set_javascript_can_open_windows_automatically(settings, false);

  webkit_settings_set_default_charset(settings, DEFAULT_CHARSET);
  webkit_settings_set_default_font_family(settings, DEFAULT_FONT_FAMILY);
  webkit_settings_set_default_font_size(settings, DEFAULT_FONT_SIZE);
  webkit_settings_set_default_monospace_font_size(settings, DEFAULT_MONOSPACE_FONT_SIZE);
  webkit_settings_set_minimum_font_size(settings, MINIMUM_FONT_SIZE);
  webkit_settings_set_monospace_font_family(settings, MONOSPACE_FONT_FAMILY);
  webkit_settings_set_sans_serif_font_family(settings, SANS_SERIF_FONT_FAMILY);
  webkit_settings_set_serif_font_family(settings, SERIF_FONT_FAMILY);

  web_view = webkit_web_view_new_with_context(context);
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(web_view), settings);

  gtk_container_add(GTK_CONTAINER(window), web_view);

  g_signal_connect(G_OBJECT(context), "initialize-web-extensions", G_CALLBACK(on_initialize_web_extensions), NULL);
  g_signal_connect(G_OBJECT(context), "download-started", G_CALLBACK(on_download_started), NULL);
  g_signal_connect(G_OBJECT(web_view), "load-changed", G_CALLBACK(on_load_changed), window);
  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_key_press_event), web_view);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(on_delete_event), NULL);

  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}
