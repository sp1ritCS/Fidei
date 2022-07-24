#include "fidei.h"
#include "num.h"
#include "preferences.h"
#include "utils.h"

#include <libintl.h>
#define _t(String) gettext (String)

gchar* fidei_bookitem_factory_get_title(GtkListItem*, FideiBibleBook* book) {
	if (!FIDEI_IS_BIBLEBOOK(book))
		return NULL;
	return g_strdup(fidei_biblebook_get_bname(book));
}
gchar* fidei_bookitem_factory_get_subtitle(GtkListItem*, FideiBibleBook* book) {
	if (!FIDEI_IS_BIBLEBOOK(book))
		return NULL;
	return g_strdup(fidei_biblebook_get_bsname(book));
}

gchar* fidei_chapteritem_factory_get_num(GtkListItem*, FideiListNum* num) {
	if (!FIDEI_IS_LISTNUM(num))
		return NULL;
	return g_strdup_printf("%d", fidei_listnum_get(num) + 1);
}

typedef struct {
	GListModel* bibles;
	FideiBible* active_bible;
	FideiBibleBook* active_biblebook;
	gint active_chapter;

	GSettings* settings;

	AdwWindowTitle* title;
	// parent
	GtkStack* initializer_stack;

	// bible picker
	GtkScrolledWindow* bibleselect_scroll;
	GtkListBox* bible_selector;
	GtkButton* browser_btn;
	GtkButton* bibledir_btn;

	// content-parent
	AdwLeaflet* main;
	// book/chapter select
	GtkStack* selector_stack;
	GtkScrolledWindow* booksel_scroll;
	GtkListView* book_selector;
	GtkBox* chaptersel_box;
	GtkButton* chapter_back_book_navbtn;
	GtkGridView* chapter_selector;
	// content
	GtkBox* content_actions;
	GtkButton* leaflet_back_navbtn;
	GtkButton* chapter_prev_navbtn;
	GtkButton* chapter_fwd_navbtn;
	GtkScrolledWindow* content;

	GtkTextView* current_view;
} FideiAppWindowPrivate;

struct _FideiAppWindow {
	AdwApplicationWindow parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE (FideiAppWindow, fidei_appwindow, ADW_TYPE_APPLICATION_WINDOW)

enum {
	PROP_BIBLES = 1,
	PROP_ACTIVE_BIBLE,
	N_PROPERTIES
};

static GParamSpec* obj_properties[N_PROPERTIES] = { NULL, };

static void fidei_appwindow_object_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
	FideiAppWindow* self = FIDEI_APPWINDOW(object);

	switch (prop_id) {
		case PROP_BIBLES:
			g_value_set_object(value, fidei_appwindow_get_bibles(self));
			break;
		case PROP_ACTIVE_BIBLE:
			g_value_set_object(value, fidei_appwindow_get_active_bible(self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}
static void fidei_appwindow_object_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
	FideiAppWindow* self = FIDEI_APPWINDOW(object);

	switch (prop_id) {
		case PROP_BIBLES:
			fidei_appwindow_set_bibles(self, g_value_get_object(value));
			break;
		case PROP_ACTIVE_BIBLE:
			fidei_appwindow_set_active_bible(self, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

void fidei_appwindow_class_init(FideiAppWindowClass* class) {
	GObjectClass* object_class = G_OBJECT_CLASS(class);
	GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);

	object_class->get_property = fidei_appwindow_object_get_property;
	object_class->set_property = fidei_appwindow_object_set_property;
	//object_class->dispose = fidei_appwindow_object_dispose;

	obj_properties[PROP_BIBLES] = g_param_spec_object("bibles", "Bibles", "All available bibles located on the system", G_TYPE_LIST_MODEL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
	obj_properties[PROP_ACTIVE_BIBLE] = g_param_spec_object("active-bible", "Active bible", "Currently active bible. Should reside in `bibles`", FIDEI_TYPE_BIBLE, G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);
	g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

	gtk_widget_class_set_template_from_resource(widget_class, "/arpa/sp1rit/Fidei/ui/fidei.ui");
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, title);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, initializer_stack);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, bibleselect_scroll);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, bible_selector);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, browser_btn);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, bibledir_btn);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, main);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, selector_stack);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, booksel_scroll);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, book_selector);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, chaptersel_box);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, chapter_back_book_navbtn);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, chapter_selector);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, content_actions);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, leaflet_back_navbtn);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, chapter_prev_navbtn);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, chapter_fwd_navbtn);
	gtk_widget_class_bind_template_child_private(widget_class, FideiAppWindow, content);
}

static void navigate_picker_activated(GSimpleAction*, GVariant*, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	g_settings_reset(priv->settings, "open-bible");
	gtk_stack_set_visible_child(priv->initializer_stack, GTK_WIDGET(priv->bibleselect_scroll));
}

const gchar* authors[] = {
	"Florian \"sp1rit\" <sp1rit@national.shitposting.agency>",
	NULL
};
static void open_aboutwin_activated(GSimpleAction*, GVariant*, FideiAppWindow* self) {
	GtkWidget* diag = gtk_about_dialog_new();

	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(diag), "Fidei");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(diag), _t("Take back your faith"));
	gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(diag), "arpa.sp1rit.Fidei");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(diag), _t("Copyright (c) 2022 Florian \"sp1rit\" and contributors"));
	gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(diag), GTK_LICENSE_AGPL_3_0);
#ifdef FIDEI_VERSION
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(diag), FIDEI_VERSION);
#endif
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(diag), "https://github.com/sp1ritCS/fidei");
	gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(diag), "github.com/sp1ritCS/fidei");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(diag), authors);

	gtk_window_set_transient_for(GTK_WINDOW(diag), GTK_WINDOW(self));
	gtk_window_set_modal(GTK_WINDOW(diag), TRUE);
	gtk_widget_show(diag);
}

static void open_preferences_activated(GSimpleAction*, GVariant*, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	GtkWidget* prefs = fidei_preferences_new(priv->settings);

	gtk_window_set_transient_for(GTK_WINDOW(prefs), GTK_WINDOW(self));
	gtk_window_set_modal(GTK_WINDOW(prefs), TRUE);
	gtk_widget_show(prefs);
}

static void fidei_appwindow_setup_window_size(FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	g_settings_bind(priv->settings, "window-width", self, "default-width", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(priv->settings, "window-height", self, "default-height", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(priv->settings, "is-maximized", self, "maximized", G_SETTINGS_BIND_DEFAULT);
}

static void create_chapter_content_view_apply_userfont(FideiAppWindow* self, GtkTextView* view);
static void fidei_appwindow_font_changed(GSettings*, gchar* key, FideiAppWindow* self) {
	g_return_if_fail(g_strcmp0(key, "font") == 0);
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	if (!priv->current_view)
		return;

	create_chapter_content_view_apply_userfont(self, priv->current_view);
}

static void bible_selector_clicked(GtkListBox*, GtkListBoxRow* row, FideiAppWindow* self) {
	fidei_appwindow_set_active_bible(self, g_object_get_data(G_OBJECT(row), "bible"));
}
static void open_gfile(GFile* file) {
	GError* err = NULL;
	GAppInfo* app = g_file_query_default_handler(file, NULL, &err);
	if (err) {
		g_critical(_t("Failed querying launch application: %s\n"), err->message);
		g_error_free(err);
		return;
	}

	GList* files = g_list_alloc();
	files->prev = NULL;
	files->next = NULL;
	files->data = file;

	g_app_info_launch(app, files, NULL, &err);
	if (err) {
		g_critical(_t("Failed launching application: %s\n"), err->message);
		g_error_free(err);
	}

	g_list_free(files);
	g_object_unref(app);
}
static void browser_btn_clicked(GtkButton*, gpointer) {
	GFile* download_page = g_file_new_for_uri("https://bible4u.app/download.html");

	open_gfile(download_page);

	g_object_unref(download_page);
}
static void bibledir_btn_clicked(GtkButton*, gpointer) {
	const gchar* datadir = g_get_user_data_dir();
	GFile* bibledir = g_file_new_build_filename(datadir, "arpa.sp1rit.Fidei", NULL);

	open_gfile(bibledir);

	g_object_unref(bibledir);
}


static void book_clicked(GtkListView* view, guint pos, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	GtkSelectionModel* model = gtk_list_view_get_model(view);
	GListModel* books = gtk_single_selection_get_model(GTK_SINGLE_SELECTION(model));
	FideiBibleBook* book = g_list_model_get_item(books, pos);
	priv->active_biblebook = book;

	GListStore* store = g_list_store_new(FIDEI_TYPE_LISTNUM);
	for (gint i = 0; i < fidei_biblebook_get_num_chapters(book); i++) {
		FideiListNum* num = fidei_listnum_new(i);
		g_list_store_append(store, num);
		g_object_unref(num);
	}

	GtkSingleSelection* chaptermodel = gtk_single_selection_new(G_LIST_MODEL(store));
	gtk_grid_view_set_model(priv->chapter_selector, GTK_SELECTION_MODEL(chaptermodel));
	g_object_unref(chaptermodel);

	gtk_stack_set_visible_child(priv->selector_stack, GTK_WIDGET(priv->chaptersel_box));
}

static void chapter_back_book_navbtn_clicked(GtkButton*, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	gtk_stack_set_visible_child(priv->selector_stack, GTK_WIDGET(priv->booksel_scroll));
}

static void chapter_clicked(GtkListView*, guint pos, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	fidei_appwindow_open_chapter(self, fidei_biblebook_get_booknum(priv->active_biblebook), pos);
}

static void leaflet_back_navbtn_clicked(GtkButton*, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	adw_leaflet_navigate(priv->main, ADW_NAVIGATION_DIRECTION_BACK);
}
static void chapter_prev_navbtn_clicked(GtkButton*, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	fidei_appwindow_open_chapter(self, fidei_biblebook_get_booknum(priv->active_biblebook), priv->active_chapter - 1);
}
static void chapter_fwd_navbtn_clicked(GtkButton*, FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	fidei_appwindow_open_chapter(self, fidei_biblebook_get_booknum(priv->active_biblebook), priv->active_chapter + 1);
}

void fidei_appwindow_init(FideiAppWindow* self) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	priv->bibles = NULL;
	priv->active_bible = NULL;
	priv->active_biblebook = NULL;
	priv->active_chapter = -1;

	priv->current_view = NULL;

	priv->settings = g_settings_new("arpa.sp1rit.Fidei");
	fidei_appwindow_setup_window_size(self);
	g_signal_connect(priv->settings, "changed::font", G_CALLBACK(fidei_appwindow_font_changed), self);

	GSimpleAction* navigate_picker = g_simple_action_new("nav_picker", NULL);
	GSimpleAction* open_preferences = g_simple_action_new("prefs", NULL);
	GSimpleAction* open_aboutwin = g_simple_action_new("about", NULL);
	g_signal_connect(navigate_picker, "activate", G_CALLBACK(navigate_picker_activated), self);
	g_signal_connect(open_preferences, "activate", G_CALLBACK(open_preferences_activated), self);
	g_signal_connect(open_aboutwin, "activate", G_CALLBACK(open_aboutwin_activated), self);
	g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(navigate_picker));
	g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(open_preferences));
	g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(open_aboutwin));

	gtk_widget_init_template(GTK_WIDGET(self));

	g_signal_connect(priv->bible_selector, "row-activated", G_CALLBACK(bible_selector_clicked), self);
	g_signal_connect(priv->browser_btn, "clicked", G_CALLBACK(browser_btn_clicked), NULL);
	g_signal_connect(priv->bibledir_btn, "clicked", G_CALLBACK(bibledir_btn_clicked), NULL);

	g_signal_connect(priv->book_selector, "activate", G_CALLBACK(book_clicked), self);

	g_signal_connect(priv->chapter_back_book_navbtn, "clicked", G_CALLBACK(chapter_back_book_navbtn_clicked), self);
	g_signal_connect(priv->chapter_selector, "activate", G_CALLBACK(chapter_clicked), self);

	g_signal_connect(priv->leaflet_back_navbtn, "clicked", G_CALLBACK(leaflet_back_navbtn_clicked), self);
	g_signal_connect(priv->chapter_prev_navbtn, "clicked", G_CALLBACK(chapter_prev_navbtn_clicked), self);
	g_signal_connect(priv->chapter_fwd_navbtn, "clicked", G_CALLBACK(chapter_fwd_navbtn_clicked), self);
}

GtkWidget* fidei_appwindow_new(GtkApplication* app, GListModel* bibles) {
	return g_object_new(FIDEI_TYPE_APPWINDOW, "application", app, "bibles", bibles, NULL);
}

static GtkWidget* create_picker_item(const gchar* title, const gchar* lang, const gchar* publisher) {
	GtkWidget* item = gtk_list_box_row_new();

	GtkWidget* child = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
	gtk_widget_add_css_class(child, "header");

	GtkWidget* titlebox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_valign(titlebox, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(titlebox, TRUE);
	gtk_widget_add_css_class(titlebox, "title");

	GtkWidget* titlew = gtk_label_new(title);
	gtk_widget_add_css_class(titlew, "title");
	gtk_label_set_single_line_mode(GTK_LABEL(titlew), TRUE);
	gtk_label_set_xalign(GTK_LABEL(titlew), 0.f);

	gchar* lang_ui = miso693_to_human_same(lang);
	gchar* subtitle = g_strdup_printf("%s | %s", lang_ui, publisher);
	g_free(lang_ui);
	GtkWidget* subtitlew = gtk_label_new(subtitle);
	g_free(subtitle);
	gtk_widget_add_css_class(subtitlew, "subtitle");
	gtk_label_set_single_line_mode(GTK_LABEL(subtitlew), TRUE);
	gtk_label_set_xalign(GTK_LABEL(subtitlew), 0.f);

	gtk_box_append(GTK_BOX(titlebox), titlew);
	gtk_box_append(GTK_BOX(titlebox), subtitlew);

	GtkWidget* arrow = gtk_image_new_from_icon_name("go-next-symbolic");

	gtk_box_append(GTK_BOX(child), titlebox);
	gtk_box_append(GTK_BOX(child), arrow);

	gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(item), child);

	return item;
}

GListModel* fidei_appwindow_get_bibles(FideiAppWindow* self) {
	g_return_val_if_fail(FIDEI_IS_APPWINDOW(self), NULL);
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	return priv->bibles;
}
void fidei_appwindow_set_bibles(FideiAppWindow* self, GListModel* bibles) {
	g_return_if_fail(FIDEI_IS_APPWINDOW(self));
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	if (priv->bibles) {
		g_critical(_t("Property bibles of Fidei.AppWindow can only be set once"));
		return;
	}

	priv->bibles = bibles;

	if (g_list_model_get_n_items(priv->bibles) == 0) {
		GtkWidget* empty_info = gtk_list_box_row_new();
		gtk_widget_set_sensitive(empty_info, FALSE);

		GtkWidget* child = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
		gtk_widget_add_css_class(child, "header");

		GtkWidget* label = gtk_label_new(_t("No bibles found on your system"));
		gtk_widget_add_css_class(label, "title");
		gtk_label_set_xalign(GTK_LABEL(label), 0.f);

		gtk_box_append(GTK_BOX(child), label);
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(empty_info), child);

		gtk_list_box_append(GTK_LIST_BOX(priv->bible_selector), empty_info);
	}

	gchar* last_active = g_settings_get_string(priv->settings, "open-bible");

	for (guint i = 0; i < g_list_model_get_n_items(priv->bibles); i++) {
		FideiBible* state = FIDEI_BIBLE(g_list_model_get_object(priv->bibles, i));

		if ((last_active && *last_active) && g_strcmp0(fidei_bible_get_identifier(state), last_active) == 0)
			fidei_appwindow_set_active_bible(self, state);

		GtkWidget* item = create_picker_item(
			fidei_bible_get_title(state),
			fidei_bible_get_lang(state),
			fidei_bible_get_publisher(state)
		);
		g_object_set_data(G_OBJECT(item), "bible", state);
		gtk_list_box_append(GTK_LIST_BOX(priv->bible_selector), item);
	}

	g_free(last_active);


	g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_BIBLES]);
}

FideiBible* fidei_appwindow_get_active_bible(FideiAppWindow* self) {
	g_return_val_if_fail(FIDEI_IS_APPWINDOW(self), NULL);
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	return priv->active_bible;
}
void fidei_appwindow_set_active_bible(FideiAppWindow* self, FideiBible* bible) {
	g_return_if_fail(FIDEI_IS_APPWINDOW(self));
	g_return_if_fail(FIDEI_IS_BIBLE(bible));
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	gtk_stack_set_visible_child(priv->initializer_stack, GTK_WIDGET(priv->main));

	if (priv->active_bible == bible)
		return;
	priv->active_bible = bible;

	const gchar* identifier = fidei_bible_get_identifier(priv->active_bible);
	if (identifier)
		g_settings_set_string(priv->settings, "open-bible", identifier);

	if (priv->active_bible) {
		gchar* active_book;
		gint active_chapter;

		g_settings_get(priv->settings, "active-chapter", "(si)", &active_book, &active_chapter);

		GListStore* books = fidei_bible_read_books(priv->active_bible);
		GtkSingleSelection* model = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(books)));
		gtk_list_view_set_model(priv->book_selector, GTK_SELECTION_MODEL(model));
		for (guint i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(g_object_ref(books))); i++) {
			FideiBibleBook* book = FIDEI_BIBLEBOOK(g_list_model_get_object(G_LIST_MODEL(books), i));
			if (g_strcmp0(fidei_biblebook_get_bsname(book), active_book) == 0) {
				priv->active_biblebook = book;
				fidei_appwindow_open_chapter(self, fidei_biblebook_get_booknum(book), active_chapter);
				break; // TODO: if no new chaper was opened show statuspage again
			}
		}
		g_object_unref(model);

		g_free(active_book);
	} else {
		gtk_list_view_set_model(priv->book_selector, NULL);
	}


	g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_ACTIVE_BIBLE]);
}

static void create_chapter_content_view_apply_userfont(FideiAppWindow* self, GtkTextView* view) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	gchar* selected_font = g_settings_get_string(priv->settings, "font");
	PangoFontDescription* desc = pango_font_description_from_string(selected_font);
	g_free(selected_font);

	GtkTextBuffer* buf = gtk_text_view_get_buffer(view);
	GtkTextTagTable* table = gtk_text_buffer_get_tag_table(buf);
	GtkTextTag* tag = gtk_text_tag_table_lookup(table, "user-font");

	GValue wrapped_desc = G_VALUE_INIT;
	g_value_init(&wrapped_desc, PANGO_TYPE_FONT_DESCRIPTION);
	g_value_set_boxed(&wrapped_desc, desc);
	g_object_set_property(G_OBJECT(tag), "font-desc", &wrapped_desc);

	/*GtkTextIter start,end;
	gtk_text_buffer_get_start_iter(buf, &start);
	gtk_text_buffer_get_end_iter(buf, &end);

	//gtk_text_buffer_remove_tag(GtkTextBuffer *buffer, GtkTextTag *tag, const GtkTextIter *start, const GtkTextIter *end)
	gtk_text_buffer_apply_tag_by_name(buf, "user-font", &start, &end);*/
}

static gboolean create_chapter_content_view_regex_match_eval(const GMatchInfo* match, GString* result, gpointer) {
	gchar* matched = g_match_info_fetch(match, 0);
	gsize len = g_utf8_strlen(matched, -1);
	if (len < 2) {
		result = g_string_new(matched);
		return FALSE;
	}

	g_string_append(result, "<span font_variant=\"small-caps\">");
	g_string_append_len(result, matched, 1);

	gchar* lowered = g_utf8_strdown(&matched[1], -1);
	g_string_append(result, lowered);
	g_free(lowered);

	g_string_append(result, "</span>");

	g_free(matched);
	return FALSE;
}

static GtkWidget* create_chapter_content_view(FideiAppWindow* self, const gchar* biblelang, FideiBibleVers* verses, guint n_verses) {
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);

	GtkWidget* view = gtk_text_view_new();
	gtk_widget_set_hexpand(view, TRUE);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_justification(GTK_TEXT_VIEW(view), GTK_JUSTIFY_FILL);
	gtk_widget_add_css_class(view, "content");
	gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 32);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 32);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 32);
	gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 32);
	GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	GtkTextTag* tag = gtk_text_buffer_create_tag(buf, "user-font", NULL);


	gchar* lord_regex = NULL;

	GVariantIter* iter;
	g_settings_get(priv->settings, "small-caps", "a{ss}", &iter);
	gchar *lang,*regex;
	while (g_variant_iter_loop(iter, "{ss}", &lang, &regex))
		if (g_ascii_strcasecmp(biblelang, lang) == 0) {
			lord_regex = g_strdup(regex);
			break;
		}
	g_variant_iter_free(iter);


	GRegex* compiled_regex = NULL;

	GError* err = NULL;
	compiled_regex = g_regex_new(lord_regex, 0, G_REGEX_MATCH_NOTEMPTY, &err);
	if (err) {
		g_critical("Invalid regex `%s`: %s\n", lord_regex, err->message);
		g_error_free(err);
		compiled_regex = NULL;
	}

	g_free(lord_regex);

	GtkTextIter end;
	for (gsize i = 0; i < n_verses; i++) {
		gboolean has_caption = FALSE;
		if (verses[i].caption) {
			gtk_text_buffer_get_end_iter(buf, &end);
			gchar* caption = g_strdup_printf("\n\n<span font_family=\"Cantarell\" font_size=\"large\" font_weight=\"bold\" font_variant=\"all-petite-caps\">%s</span>\n", verses[i].caption);
			gtk_text_buffer_insert_markup(buf, &end, &caption[((gsize)!i)*2], -1);
			g_free(caption);

			has_caption = TRUE;
		}

		gtk_text_buffer_get_end_iter(buf, &end);
		gchar* verslabel = g_strdup_printf(" <span font_family=\"sans-serif\" font_size=\"8pt\" rise=\"4pt\">%ld </span>", i+1);
		gtk_text_buffer_insert_markup(buf, &end, &verslabel[(gsize)((!i)|has_caption)], -1);
		g_free(verslabel);


		gtk_text_buffer_get_end_iter(buf, &end);
		if (compiled_regex) {
			GError* err = NULL;
			gchar* replaced = g_regex_replace_eval(compiled_regex, verses[i].content, -1, 0, G_REGEX_MATCH_NOTEMPTY, (GRegexEvalCallback)create_chapter_content_view_regex_match_eval, NULL, &err);
			if (err) {
				g_critical("Regex match failure `%s`: %s\n", lord_regex, err->message);
				g_error_free(err);
				gtk_text_buffer_insert(buf, &end, verses[i].content, -1);
			} else
				gtk_text_buffer_insert_markup(buf, &end, replaced, -1);
		} else
			gtk_text_buffer_insert(buf, &end, verses[i].content, -1);
	}

	GtkTextIter start;
	gtk_text_buffer_get_start_iter(buf, &start);
	gtk_text_buffer_get_end_iter(buf, &end);
	gtk_text_buffer_apply_tag(buf, tag, &start, &end);

	create_chapter_content_view_apply_userfont(self, GTK_TEXT_VIEW(view));
	return view;
}

void fidei_appwindow_open_chapter(FideiAppWindow* self, gint book, gint chapter) {
	g_return_if_fail(FIDEI_IS_APPWINDOW(self));
	g_return_if_fail((book | chapter) >= 0);
	FideiAppWindowPrivate* priv = fidei_appwindow_get_instance_private(self);
	g_return_if_fail(chapter < fidei_biblebook_get_num_chapters(priv->active_biblebook));

	FideiBibleVers* verses;
	guint num_verses = fidei_bible_read_chapter(priv->active_bible, book, chapter, &verses);
	GtkWidget* chapterview = create_chapter_content_view(self, fidei_bible_get_lang(priv->active_bible), verses, num_verses);
	for (guint i = 0; i < num_verses; i++)
		fidei_biblevers_free_inner(verses[i]);
	g_free(verses);

	g_settings_set(priv->settings, "active-chapter", "(si)", fidei_biblebook_get_bsname(priv->active_biblebook), chapter);

	gchar* title = g_strdup_printf("%s %d", fidei_biblebook_get_bname(priv->active_biblebook), chapter+1);
	adw_window_title_set_title(priv->title, title);
	adw_window_title_set_subtitle(priv->title, fidei_bible_get_title(priv->active_bible));
	g_free(title);

	gtk_widget_set_visible(GTK_WIDGET(priv->content_actions), TRUE);
	gtk_scrolled_window_set_child(priv->content, chapterview);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->chapter_prev_navbtn), (gboolean)chapter);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->chapter_fwd_navbtn), chapter+1 < fidei_biblebook_get_num_chapters(priv->active_biblebook));

	adw_leaflet_navigate(priv->main, ADW_NAVIGATION_DIRECTION_FORWARD);

	priv->active_chapter = chapter;
	priv->current_view = GTK_TEXT_VIEW(chapterview);
}
