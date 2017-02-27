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

const gchar *supported_protocols[] = {
  SUPPORTED_PROTOCOLS,
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

#define USER_CACHE   g_get_user_cache_dir()
#define USER_CONFIG  g_get_user_config_dir()
#define USER_HOME    g_get_home_dir()

#define PATH(location, ...) \
  (g_build_path(G_DIR_SEPARATOR_S, location, ##__VA_ARGS__, NULL))

static GtkWidget* create_web_view(void);
static GtkWidget* create_window(GtkWidget *web_view);
static gchar *get_input(const gchar *suggestion);
static void do_history(GtkWidget *window, GtkWidget *web_view, gint action);
static void do_navigation(GtkWidget *window, GtkWidget *web_view);
static void do_search(GtkWidget *window, GtkWidget *web_view, gint action);
static void do_exit(GtkWidget *window, GtkWidget *web_view);
static void do_stop_loading(GtkWidget *window, GtkWidget *web_view);
static void do_zoom(GtkWidget *window, GtkWidget *web_view, gint action, gdouble adjust);
static void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, GtkWidget *window);
static void on_mouse_target_changed(WebKitWebView *web_view, WebKitHitTestResult *hit_test_result, guint modifiers, GtkWidget *window);
static void on_ready_to_show(WebKitWebView *web_view, GtkWidget *window);
static gboolean on_decide_destination(WebKitDownload *download, gchar *suggested_filename, gpointer user_data);
static void on_received_data(WebKitDownload *download, guint64 data_length, gpointer user_data);
static void on_download_finished(WebKitDownload *download, gpointer user_data);
static void on_download_failed(WebKitDownload *download, GError *error, gpointer user_data);
static void on_download_started(WebKitWebContext *context, WebKitDownload *download, gpointer user_data);
static void on_initialize_web_extensions(WebKitWebContext *context, gpointer user_data);
static gboolean on_key_press_event(GtkWidget *window, GdkEventKey *event, GtkWidget *web_view);
static gboolean on_delete_event(GtkWidget *window, GdkEventKey *event, GtkWidget *web_view);
static GtkWidget* on_create(WebKitWebView *web_view, WebKitNavigationAction *navigation_action, gpointer user_data);

static struct {
  guint windows;
  WebKitWebContext *context;
  struct {
    gchar *extensions;
    gchar *cache;
    gchar *data;
    gchar *cookies;
  } path;
} global = {0};

static GtkWidget *
create_web_view(void) {
  GtkWidget *web_view;
  WebKitSettings *settings;

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

  web_view = webkit_web_view_new_with_context(global.context);
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(web_view), settings);

  g_signal_connect(G_OBJECT(web_view), "create", G_CALLBACK(on_create), NULL);

  return web_view;
}

static GtkWidget *
create_window(GtkWidget *web_view) {
  GtkWidget *window;

  global.windows++;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_add(GTK_CONTAINER(window), web_view);
  g_signal_connect(G_OBJECT(web_view), "load-changed", G_CALLBACK(on_load_changed), window);
  g_signal_connect(G_OBJECT(web_view), "ready-to-show", G_CALLBACK(on_ready_to_show), window);
  g_signal_connect(G_OBJECT(web_view), "mouse-target-changed", G_CALLBACK(on_mouse_target_changed), window);
  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_key_press_event), web_view);
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(on_delete_event), web_view);
  return window;
}

static gchar *
get_input(const gchar *suggestion) {
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

  GIOChannel *input = g_io_channel_unix_new(stdin);
  if (suggestion) {
    g_io_channel_write_chars(input, suggestion, -1, NULL, &error);
    g_assert_no_error(error);
    g_io_channel_write_unichar (input, '\n', &error);
    g_assert_no_error(error);
  }
  g_io_channel_shutdown(input, true, &error);
  g_assert_no_error(error);

  GIOChannel *output = g_io_channel_unix_new(stdout);
  g_io_channel_read_line(output, &line, NULL, NULL, &error);
  g_assert_no_error(error);
  g_io_channel_shutdown(output, false, &error);
  g_assert_no_error(error);

  if (line) {
    g_strstrip(line);
  }
  return line;
}

static void
do_history(GtkWidget *window, GtkWidget *web_view, gint action) {
  switch (action) {
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
  const gchar *suggestion = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(web_view));
  g_autofree gchar *input = get_input(suggestion);
  g_autofree gchar *url = NULL;

  if ((!input) || (strlen(input) == 0)) {
    return;
  }

  guint i = 0;
  while ((!url) && (supported_protocols[i])) {
    if (g_str_has_prefix(input, supported_protocols[i])) {
      if (g_str_has_prefix(input + strlen(supported_protocols[i]), "://")) {
        url = input;
      }
    }
    i++;
  }
  if (!url) {
    url = g_strdup_printf(DEFAULT_PROTOCOL "://%s", input);
  }
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), url);

  if (input == url) {
    input = NULL;
  }
}

static void
do_search(GtkWidget *window, GtkWidget *web_view, gint action) {
  const WebKitFindOptions find_options = WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE | WEBKIT_FIND_OPTIONS_WRAP_AROUND;
  const guint max_match_count = 1024;
  const gchar *suggestion = NULL;

  WebKitFindController *find_controller;
  g_autofree gchar *input = NULL;

  find_controller = webkit_web_view_get_find_controller(WEBKIT_WEB_VIEW(web_view));
  switch (action) {
  case SEARCH_START:
    suggestion = webkit_find_controller_get_search_text(find_controller);
    input = get_input(suggestion);
    if ((input) && (strlen(input) > 0)) {
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
  g_assert(global.windows > 0);

  global.windows--;
  if (global.windows == 0)
    gtk_main_quit();
}

static void
do_stop_loading(GtkWidget *window, GtkWidget *web_view) {
  webkit_web_view_stop_loading(WEBKIT_WEB_VIEW(web_view));
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

  if (title) {
    gtk_window_set_title(GTK_WINDOW(window), title);
  }
}

static void
on_mouse_target_changed(WebKitWebView *web_view, WebKitHitTestResult *hit_test_result, guint modifiers, GtkWidget *window) {
  static gchar *saved = NULL;
  g_autofree gchar *title = NULL;

  if (webkit_hit_test_result_context_is_link(hit_test_result)) {
    if (!saved) {
      saved = g_strdup(gtk_window_get_title(GTK_WINDOW(window)));
    }
    title = g_strdup_printf(PACKAGE " | %s", webkit_hit_test_result_get_link_uri(hit_test_result));
  } else {
    title = saved;
    saved = NULL;
  }

  if (title) {
    gtk_window_set_title(GTK_WINDOW(window), title);
  }
}

static void
on_ready_to_show(WebKitWebView *web_view, GtkWidget *window) {
  g_message("WebView is ready to show");
  gtk_widget_show_all(window);
}

static gboolean
on_decide_destination(WebKitDownload *download, gchar *suggested_filename, gpointer user_data) {
  g_autofree gchar *file = NULL;
  g_autofree gchar *name = NULL;
  g_autofree gchar *uri = NULL;
  guint iter = 0;

  do {
    g_free(name);
    g_free(file);

    if (iter == 0) {
      name = g_strdup(suggested_filename);
    } else {
      name = g_strdup_printf("%s.%u", suggested_filename, iter);
    }

    file = PATH(USER_HOME, "Downloads", name);
    iter++;
  } while (g_file_test(file, G_FILE_TEST_EXISTS));

  g_message("Saving file %s to %s", suggested_filename, uri);

  uri = g_strdup_printf("file://%s", file);
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

  webkit_web_context_set_web_extensions_directory(context, global.path.extensions);
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
    case KEY_STOP_LOADING:
      do_stop_loading(window, web_view);
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
  do_exit(window, web_view);
  return false;
}

static GtkWidget*
on_create(WebKitWebView *parent, WebKitNavigationAction *navigation_action, gpointer user_data) {
  const gchar *url = webkit_uri_request_get_uri(webkit_navigation_action_get_request(navigation_action));

  GtkWidget *window;
  GtkWidget *web_view;

  g_message("Opening new window for %s", url);
  web_view = create_web_view();
  window = create_window(web_view);
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), url);
  return web_view;
}

int
main(int argc, char **argv) {
  GtkWidget *window;

  WebKitCookieManager *cookie_manager;
  WebKitWebsiteDataManager *data_manager;
  WebKitSettings *settings;

  gtk_init(&argc, &argv);

  global.path.extensions = PATH(USER_CONFIG, PACKAGE, "extensions");
  global.path.cache = PATH(USER_CACHE, PACKAGE, "cache");
  global.path.data = PATH(USER_CACHE, PACKAGE, "data");
  global.path.cookies = PATH(USER_CACHE, PACKAGE, "cookies.txt");

  g_debug(
    "This browser uses the following paths:\n"
    "  Extensions: %s\n"
    "  Cache:      %s\n"
    "  Data:       %s\n"
    "  Cookies:    %s\n",
    global.path.extensions,
    global.path.cache,
    global.path.data,
    global.path.cookies
  );

  data_manager = webkit_website_data_manager_new(
    "base-cache-directory", global.path.cache,
    "base-data-directory", global.path.data,
    NULL
  );

  global.context = webkit_web_context_new_with_website_data_manager(data_manager);
  webkit_web_context_set_web_process_count_limit(global.context, 4);
  webkit_web_context_set_tls_errors_policy(global.context, WEBKIT_TLS_ERRORS_POLICY_FAIL);
  webkit_web_context_set_cache_model(global.context, WEBKIT_CACHE_MODEL_WEB_BROWSER);
  webkit_web_context_set_spell_checking_enabled(global.context, false);
  webkit_web_context_set_preferred_languages(global.context, preferred_languages);

  g_signal_connect(G_OBJECT(global.context), "initialize-web-extensions", G_CALLBACK(on_initialize_web_extensions), NULL);
  g_signal_connect(G_OBJECT(global.context), "download-started", G_CALLBACK(on_download_started), NULL);

  cookie_manager = webkit_web_context_get_cookie_manager(global.context);
  webkit_cookie_manager_set_persistent_storage(cookie_manager, global.path.cookies, WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT);
  webkit_cookie_manager_set_accept_policy(cookie_manager, WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);

  window = create_window(create_web_view());
  gtk_widget_show_all(window);
  gtk_main();
  return 0;
}
