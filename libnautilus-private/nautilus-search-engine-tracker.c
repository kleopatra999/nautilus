/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Mr Jamie McCracken
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *
 */

#include <config.h>
#include "nautilus-search-engine-tracker.h"
#include <gmodule.h>
#include <string.h>
#include <gio/gio.h>

/* If defined, we use fts:match, this has to be enabled in Tracker to
 * work which it usually is. The alternative is to undefine it and
 * use filename matching instead. This doesn't use the content of the
 * file however.
 */
#undef FTS_MATCHING

#define MODULE_FILENAME "libtracker-sparql-0.10.so.0"

#define MODULE_MAP(a)   { #a, (gpointer *)&a }

/* Connection object */
typedef struct _TrackerSparqlConnection TrackerSparqlConnection;

#define TRACKER_SPARQL_TYPE_CONNECTION (tracker_sparql_connection_get_type ())
#define TRACKER_SPARQL_CONNECTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_SPARQL_TYPE_CONNECTION, TrackerSparqlConnection))

/* Cursor object */
typedef struct _TrackerSparqlCursor TrackerSparqlCursor;

#define TRACKER_SPARQL_TYPE_CURSOR (tracker_sparql_cursor_get_type ())
#define TRACKER_SPARQL_CURSOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_SPARQL_TYPE_CURSOR, TrackerSparqlCursor))

/* API */
static GType                     (*tracker_sparql_connection_get_type)     (void) = NULL;
static TrackerSparqlConnection * (*tracker_sparql_connection_get)          (GCancellable             *cancellable,
                                                                            GError                  **error) = NULL;
static void                      (*tracker_sparql_connection_query_async)  (TrackerSparqlConnection  *self,
                                                                            const gchar              *sparql,
                                                                            GCancellable             *cancellable,
                                                                            GAsyncReadyCallback       callback,
                                                                            gpointer                  user_data) = NULL;
static TrackerSparqlCursor *     (*tracker_sparql_connection_query_finish) (TrackerSparqlConnection  *self,
                                                                            GAsyncResult             *_res_,
                                                                            GError                  **error) = NULL;
static GType                     (*tracker_sparql_cursor_get_type)         (void) = NULL;
static void                      (*tracker_sparql_cursor_next_async)       (TrackerSparqlCursor      *self,
                                                                            GCancellable             *cancellable,
                                                                            GAsyncReadyCallback       callback,
                                                                            gpointer                  user_data) = NULL;
static gboolean                  (*tracker_sparql_cursor_next_finish)      (TrackerSparqlCursor      *self,
                                                                            GAsyncResult             *_res_,
                                                                            GError                  **error) = NULL;
static const gchar *             (*tracker_sparql_cursor_get_string)       (TrackerSparqlCursor      *self,
                                                                            gint                     *column,
                                                                            glong                    *length) = NULL;
static gchar *                   (*tracker_sparql_escape_string)           (const gchar              *literal) = NULL;

static struct TrackerFunctions
{
	const char *name;
	gpointer *pointer;
} funcs[] = {
	MODULE_MAP (tracker_sparql_connection_get_type),
	MODULE_MAP (tracker_sparql_connection_get),
	MODULE_MAP (tracker_sparql_connection_query_async),
	MODULE_MAP (tracker_sparql_connection_query_finish),
	MODULE_MAP (tracker_sparql_cursor_get_type),
	MODULE_MAP (tracker_sparql_cursor_next_async),
	MODULE_MAP (tracker_sparql_cursor_next_finish),
	MODULE_MAP (tracker_sparql_cursor_get_string),
	MODULE_MAP (tracker_sparql_escape_string)
};

static gboolean
init (void)
{
	static gboolean inited = FALSE;
	gint i;
	GModule *m;
	GModuleFlags flags;

	if (inited) {
		return TRUE;
	}

	flags = G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL;

	/* Only support 0.10 onwards */
	if ((m = g_module_open (MODULE_FILENAME, flags)) == NULL)  {
		g_debug ("No tracker backend available or it is not new enough");
		g_debug ("Only available using '%s'", MODULE_FILENAME);
		return FALSE;
	}

	inited = TRUE;

	/* Check for the symbols we need */
	for (i = 0; i < G_N_ELEMENTS (funcs); i++) {
		if (!g_module_symbol (m, funcs[i].name, funcs[i].pointer)) {
			g_warning ("Missing symbol '%s' in libtracker-sparql\n",
				   funcs[i].name);
			g_module_close (m);

			for (i = 0; i < G_N_ELEMENTS (funcs); i++)
				funcs[i].pointer = NULL;

			return FALSE;
		}
	}

	g_debug ("Loaded Tracker library and all required symbols");

	return TRUE;
}

struct NautilusSearchEngineTrackerDetails {
	TrackerSparqlConnection *connection;
	GCancellable *cancellable;

	NautilusQuery 	*query;
	gboolean 	query_pending;
};


G_DEFINE_TYPE (NautilusSearchEngineTracker,
	       nautilus_search_engine_tracker,
	       NAUTILUS_TYPE_SEARCH_ENGINE);

static void
finalize (GObject *object)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (object);

	g_clear_object (&tracker->details->query);
	g_clear_object (&tracker->details->cancellable);
	g_clear_object (&tracker->details->connection);

	G_OBJECT_CLASS (nautilus_search_engine_tracker_parent_class)->finalize (object);
}


/* stolen from tracker sources, tracker.c */
static void
sparql_append_string_literal (GString     *sparql,
                              const gchar *str)
{
	char *s;

	s = tracker_sparql_escape_string (str);

	g_string_append_c (sparql, '"');
	g_string_append (sparql, s);
	g_string_append_c (sparql, '"');

	g_free (s);
}

static void cursor_callback (GObject      *object,
			     GAsyncResult *result,
			     gpointer      user_data);

static void
cursor_next (NautilusSearchEngineTracker *tracker,
             TrackerSparqlCursor    *cursor)
{
	tracker_sparql_cursor_next_async (cursor,
	                                  tracker->details->cancellable,
	                                  cursor_callback,
	                                  tracker);
}

static void
cursor_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	NautilusSearchEngineTracker *tracker;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	GList *hits;
	gboolean success;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (user_data);

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (error) {
		nautilus_search_engine_error (NAUTILUS_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);

		if (cursor) {
			g_object_unref (cursor);
		}

		return;
	}

	if (!success) {
		nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (tracker));

		if (cursor) {
			g_object_unref (cursor);
		}

		return;
	}

	/* We iterate result by result, not n at a time. */
	hits = g_list_append (NULL, (gchar*) tracker_sparql_cursor_get_string (cursor, 0, NULL));
	nautilus_search_engine_hits_added (NAUTILUS_SEARCH_ENGINE (tracker), hits);
	g_list_free (hits);

	/* Get next */
	cursor_next (tracker, cursor);
}

static void
query_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
	NautilusSearchEngineTracker *tracker;
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (user_data);

	tracker->details->query_pending = FALSE;

	connection = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (connection,
	                                                 result,
	                                                 &error);

	if (error) {
		nautilus_search_engine_error (NAUTILUS_SEARCH_ENGINE (tracker), error->message);
		g_error_free (error);
		return;
	}

	if (!cursor) {
		nautilus_search_engine_finished (NAUTILUS_SEARCH_ENGINE (tracker));
		return;
	}

	cursor_next (tracker, cursor);
}

static void
nautilus_search_engine_tracker_start (NautilusSearchEngine *engine)
{
	NautilusSearchEngineTracker *tracker;
	gchar	*search_text, *location_uri;
	GString *sparql;
	GList *mimetypes, *l;
	gint mime_count;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);

	if (tracker->details->query_pending) {
		return;
	}

	if (tracker->details->query == NULL) {
		return;
	}

	search_text = nautilus_query_get_text (tracker->details->query);
	location_uri = nautilus_query_get_location (tracker->details->query);
	mimetypes = nautilus_query_get_mime_types (tracker->details->query);

	mime_count = g_list_length (mimetypes);

#ifdef FTS_MATCHING
	/* Using FTS: */
	sparql = g_string_new ("SELECT nie:url(?urn) "
			       "WHERE {"
			       "  ?urn a nfo:FileDataObject ;"
			       "  tracker:available true ; ");

	if (mime_count > 0) {
		g_string_append (sparql, "nie:mimeType ?mime ;");
	}

	g_string_append (sparql, "  fts:match ");
	sparql_append_string_literal (sparql, search_text);

	if (location || mime_count > 0) {
		g_string_append (sparql, " . FILTER (");
	
		if (location_uri)  {
			g_string_append (sparql, " fn:starts-with(nie:url(?urn),");
			sparql_append_string_literal (sparql, location_uri);
			g_string_append (sparql, ")");
		}

		if (mime_count > 0) {
			if (location_uri) {
				g_string_append (sparql, " && ");
			}

			g_string_append (sparql, "(");
			for (l = mimetypes; l != NULL; l = l->next) {
				if (l != mimetypes) {
					g_string_append (sparql, " || ");
				}

				g_string_append (sparql, "?mime = ");
				sparql_append_string_literal (sparql, l->data);
			}
			g_string_append (sparql, ")");
		}

		g_string_append (sparql, ")");
	}

	g_string_append (sparql, " } ORDER BY DESC(fts:rank(?urn)) ASC(nie:url(?urn))");
#else  /* FTS_MATCHING */
	/* Using filename matching: */
	sparql = g_string_new ("SELECT nie:url(?urn) "
			       "WHERE {"
			       "  ?urn a nfo:FileDataObject ;");

	if (mime_count > 0) {
		g_string_append (sparql, "nie:mimeType ?mime ;");
	}

	g_string_append (sparql, "    tracker:available true ."
			 "  FILTER (fn:contains(nfo:fileName(?urn),");

	sparql_append_string_literal (sparql, search_text);

	g_string_append (sparql, ")");

	if (mime_count > 0) {
		g_string_append (sparql, " && ");
		g_string_append (sparql, "(");
		for (l = mimetypes; l != NULL; l = l->next) {
			if (l != mimetypes) {
				g_string_append (sparql, " || ");
			}

			g_string_append (sparql, "?mime = ");
			sparql_append_string_literal (sparql, l->data);
		}
		g_string_append (sparql, ")");
	}

	g_string_append (sparql, ")");


	g_string_append (sparql, 
			 "} ORDER BY DESC(nie:url(?urn)) DESC(nfo:fileName(?urn))");
#endif /* FTS_MATCHING */

	tracker_sparql_connection_query_async (tracker->details->connection,
					       sparql->str,
					       tracker->details->cancellable,
					       query_callback,
					       tracker);
	g_string_free (sparql, TRUE);

	tracker->details->query_pending = TRUE;

	g_free (search_text);
	g_free (location_uri);

	if (mimetypes != NULL) {
		g_list_free_full (mimetypes, g_free);
	}
}

static void
nautilus_search_engine_tracker_stop (NautilusSearchEngine *engine)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);
	
	if (tracker->details->query && tracker->details->query_pending) {
		g_cancellable_cancel (tracker->details->cancellable);
		tracker->details->query_pending = FALSE;
	}
}

static gboolean
nautilus_search_engine_tracker_is_indexed (NautilusSearchEngine *engine)
{
	return TRUE;
}

static void
nautilus_search_engine_tracker_set_query (NautilusSearchEngine *engine, NautilusQuery *query)
{
	NautilusSearchEngineTracker *tracker;

	tracker = NAUTILUS_SEARCH_ENGINE_TRACKER (engine);

	if (query) {
		g_object_ref (query);
	}

	if (tracker->details->query) {
		g_object_unref (tracker->details->query);
	}

	tracker->details->query = query;
}

static void
nautilus_search_engine_tracker_class_init (NautilusSearchEngineTrackerClass *class)
{
	GObjectClass *gobject_class;
	NautilusSearchEngineClass *engine_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NAUTILUS_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nautilus_search_engine_tracker_set_query;
	engine_class->start = nautilus_search_engine_tracker_start;
	engine_class->stop = nautilus_search_engine_tracker_stop;
	engine_class->is_indexed = nautilus_search_engine_tracker_is_indexed;

	g_type_class_add_private (class, sizeof (NautilusSearchEngineTrackerDetails));
}

static void
nautilus_search_engine_tracker_init (NautilusSearchEngineTracker *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NAUTILUS_TYPE_SEARCH_ENGINE_TRACKER,
						       NautilusSearchEngineTrackerDetails);
}


NautilusSearchEngine *
nautilus_search_engine_tracker_new (void)
{
	NautilusSearchEngineTracker *engine;
	GCancellable *cancellable;
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	if (!init()) {
		return NULL;
	}

	cancellable = g_cancellable_new ();
	connection = tracker_sparql_connection_get (cancellable, &error);

	if (error) {
		g_warning ("Could not establish a connection to Tracker: %s", error->message);
		g_error_free (error);
		g_object_unref (cancellable);

		return NULL;
	} else if (!connection) {
		g_warning ("Could not establish a connection to Tracker, no TrackerSparqlConnection was returned");
		g_object_unref (cancellable);

		return NULL;
	}

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_TRACKER, NULL);

	engine->details->connection = connection;
	engine->details->cancellable = cancellable;	
	engine->details->query_pending = FALSE;

	return NAUTILUS_SEARCH_ENGINE (engine);
}
