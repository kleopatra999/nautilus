/*
 * nautilus-dbus-manager: nautilus DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-application.h"
#include "nautilus-dbus-manager.h"
#include "nautilus-properties-window.h"
#include "nautilus-window.h"

#include <libnautilus-private/nautilus-file-operations.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_DBUS
#include "nautilus-debug.h"

#include <gio/gio.h>

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gnome.Nautilus.FileOperations'>"
  "    <method name='CopyURIs'>"
  "      <arg type='as' name='SourceFilesURIList' direction='in'/>"
  "      <arg type='s' name='DestinationDirectoryURI' direction='in'/>"
  "    </method>"
  "    <method name='EmptyTrash'>"
  "    </method>"
  "    <method name='CopyFile'>"
  "      <arg type='s' name='SourceFileURI' direction='in'/>"
  "      <arg type='s' name='SourceDisplayName' direction='in'/>"
  "      <arg type='s' name='DestinationDirectoryURI' direction='in'/>"
  "      <arg type='s' name='DestinationDisplayName' direction='in'/>"
  "    </method>"
  "  </interface>"
  "  <interface name='org.gnome.Nautilus.FileManager1'>"
  "    <method name='ShowURIs'>"
  "      <arg type='as' name='Uris' direction='in'/>"
  "      <arg type='s' name='StartupId' direction='in'/>"
  "    </method>"
  "    <method name='SelectURIs'>"
  "      <arg type='as' name='Uris' direction='in'/>"
  "      <arg type='s' name='StartupId' direction='in'/>"
  "    </method>" 
  "    <method name='ShowProperties'>"
  "      <arg type='as' name='Uris' direction='in'/>"
  "      <arg type='s' name='StartupId' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

typedef struct _NautilusDBusManager NautilusDBusManager;
typedef struct _NautilusDBusManagerClass NautilusDBusManagerClass;

struct _NautilusDBusManager {
  GObject parent;

  GDBusConnection *connection;
  GApplication *application;

  guint owner_id;
  guint registration_id;
};

struct _NautilusDBusManagerClass {
  GObjectClass parent_class;
};

enum {
  PROP_APPLICATION = 1,
  NUM_PROPERTIES
};

#define SERVICE_TIMEOUT 5

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GType nautilus_dbus_manager_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (NautilusDBusManager, nautilus_dbus_manager, G_TYPE_OBJECT);

static NautilusDBusManager *singleton = NULL;

static void
nautilus_dbus_manager_dispose (GObject *object)
{
  NautilusDBusManager *self = (NautilusDBusManager *) object;

  if (self->registration_id != 0)
    {
      g_dbus_connection_unregister_object (self->connection, self->registration_id);
      self->registration_id = 0;
    }
  
  if (self->owner_id != 0)
    {
      g_bus_unown_name (self->owner_id);
      self->owner_id = 0;
    }

  g_clear_object (&self->connection);

  G_OBJECT_CLASS (nautilus_dbus_manager_parent_class)->dispose (object);
}

static gboolean
service_timeout_handler (gpointer user_data)
{
  NautilusDBusManager *self = user_data;

  DEBUG ("Reached the DBus service timeout");

  /* just unconditionally release here, as if an operation has been
   * called, its progress handler will hold it alive for all the task duration.
   */
  g_application_release (self->application);

  return FALSE;
}

static void
trigger_copy_file_operation (const gchar *source_uri,
                             const gchar *source_display_name,
                             const gchar *dest_dir_uri,
                             const gchar *dest_name)
{
  GFile *source_file, *target_dir;
  const gchar *target_name = NULL, *source_name = NULL;

  if (source_uri == NULL || source_uri[0] == '\0' ||
      dest_dir_uri == NULL || dest_dir_uri[0] == '\0')
    {
      DEBUG ("Called 'CopyFile' with invalid arguments, discarding");
      return;
    }

  source_file = g_file_new_for_uri (source_uri);
  target_dir = g_file_new_for_uri (dest_dir_uri);

  if (dest_name != NULL && dest_name[0] != '\0')
    target_name = dest_name;

  if (source_display_name != NULL && source_display_name[0] != '\0')
    source_name = source_display_name;

  nautilus_file_operations_copy_file (source_file, target_dir, source_name, target_name,
                                      NULL, NULL, NULL);

  g_object_unref (source_file);
  g_object_unref (target_dir);
}

static void
trigger_copy_uris_operation (const gchar **sources,
                             const gchar *destination)
{
  GList *source_files = NULL;
  GFile *dest_dir;
  gint idx;

  if (sources == NULL || sources[0] == NULL || destination == NULL)
    {
      DEBUG ("Called 'CopyURIs' with NULL arguments, discarding");
      return;
    }

  dest_dir = g_file_new_for_uri (destination);

  for (idx = 0; sources[idx] != NULL; idx++)
    source_files = g_list_prepend (source_files,
                                   g_file_new_for_uri (sources[idx]));

  nautilus_file_operations_copy (source_files, NULL,
                                 dest_dir,
                                 NULL, NULL, NULL);

  g_list_free_full (source_files, g_object_unref);
  g_object_unref (dest_dir);
}

static void
trigger_empty_trash_operation (void)
{
  nautilus_file_operations_empty_trash (NULL);
}

static void
trigger_show_uris_operation (char **uris,
                             const char *startup_id, 
                             NautilusApplication *application)
{
}

static void
trigger_select_uris_operation (char **uris,
                               const char *startup_id,
                               NautilusApplication *application)
{
}

static void
trigger_show_properties_operation (char **uris,
                                   const char *startup_id,
                                   NautilusApplication *application)
{
  GList *source_files = NULL;
  NautilusWindow *window;
  gint idx;
  
  if (uris == NULL || uris[0] == NULL || startup_id == NULL)
    {
      DEBUG ("Called 'ShowProperties' with NULL arguments, discarding");
      return;
    }

  window = nautilus_application_create_window (application,
                                               startup_id, 
                                               gdk_screen_get_default ());

  for (idx = 0; uris[idx] != NULL; idx++)
    {
      source_files = g_list_prepend (NULL,
								     nautilus_file_get_by_uri (uris[idx]));
      nautilus_properties_window_present (source_files, GTK_WIDGET (window));
    }
}

static void
handle_method_call (GDBusConnection *connection,
                    const gchar *sender,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
  NautilusDBusManager *self = user_data;
  NautilusApplication *application = (NautilusApplication *) self->application;

  DEBUG ("Handle method, sender %s, object_path %s, interface %s, method %s",
         sender, object_path, interface_name, method_name);

  if (g_strcmp0 (method_name, "CopyURIs") == 0)
    {
      const gchar **uris = NULL;
      const gchar *destination_uri = NULL;

      g_variant_get (parameters, "(^a&s&s)", &uris, &destination_uri);
      trigger_copy_uris_operation (uris, destination_uri);

      DEBUG ("Called CopyURIs with dest %s and uri %s\n", destination_uri, uris[0]);

      goto out;
    }

  if (g_strcmp0 (method_name, "EmptyTrash") == 0)
    {
      trigger_empty_trash_operation ();

      DEBUG ("Called EmptyTrash");

      goto out;
    }

  if (g_strcmp0 (method_name, "CopyFile") == 0)
    {
      const gchar *source_uri;
      const gchar *source_display_name;
      const gchar *destination_dir;
      const gchar *destination_name;

      g_variant_get (parameters, "(&s&s&s&s)", &source_uri, &source_display_name,
                     &destination_dir, &destination_name);
      trigger_copy_file_operation (source_uri, source_display_name, destination_dir, destination_name);

      DEBUG ("Called CopyFile with source %s, dest dir %s and dest name %s", source_uri, destination_dir,
             destination_name);

      goto out;
    }

  if (g_strcmp0 (method_name, "ShowURIs") == 0)
    {
      char **uris = NULL;
      const char *startup_id;

      g_variant_get (parameters, "(^a&s&s)", &uris, &startup_id);
      trigger_show_uris_operation (uris, startup_id, application);

      DEBUG ("Called ShowURIs with startup_id %s and uri %s\n", startup_id, uris[0]);

      goto out;
    }

    if (g_strcmp0 (method_name, "SelectURIs") == 0)
    {
      char **uris = NULL;
      const char *startup_id;

      g_variant_get (parameters, "(^a&s&s)", &uris, &startup_id);
      trigger_select_uris_operation (uris, startup_id, application);

      DEBUG ("Called SelectURIs with startup_id %s and uri %s\n", startup_id, uris[0]);

      goto out;
    }
	
    if (g_strcmp0 (method_name, "ShowProperties") == 0)
    {
      char **uris = NULL;
      const char *startup_id;

      g_variant_get (parameters, "(^a&s&s)", &uris, &startup_id);
      trigger_show_properties_operation (uris, startup_id, application);

      DEBUG ("Called ShowProperties with startup_id %s and uri %s\n", startup_id, uris[0]);

      goto out;
    }

 out:
  g_dbus_method_invocation_return_value (invocation, NULL);
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL,
};

static void
bus_acquired_handler_cb (GDBusConnection *conn,
                         const gchar *name,
                         gpointer user_data)
{
  NautilusDBusManager *self = user_data;
  GDBusNodeInfo *introspection_data;
  GError *error = NULL;

  DEBUG ("Bus acquired at %s", name);

  self->connection = g_object_ref (conn);
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &error);

  if (error != NULL)
    {
      g_warning ("Error parsing the FileOperations XML interface: %s", error->message);
      g_error_free (error);

      g_bus_unown_name (self->owner_id);
      self->owner_id = 0;

      g_application_release (self->application);

      return;
    }
  
  self->registration_id = g_dbus_connection_register_object (conn,
                                                             "/org/gnome/Nautilus",
                                                             introspection_data->interfaces[0],
                                                             &interface_vtable,
                                                             self,
                                                             NULL, &error);
  self->registration_id = g_dbus_connection_register_object (conn,
                                                             "/org/gnome/Nautilus",
                                                             introspection_data->interfaces[1],
                                                             &interface_vtable,
                                                             self,
                                                             NULL, &error);

  g_dbus_node_info_unref (introspection_data);

  if (error != NULL)
    {
      g_warning ("Error registering the FileOperations proxy on the bus: %s", error->message);
      g_error_free (error);

      g_bus_unown_name (self->owner_id);

      g_application_release (self->application);

      return;
    }

  g_timeout_add_seconds (SERVICE_TIMEOUT, service_timeout_handler, self);
}

static void
nautilus_dbus_manager_init (NautilusDBusManager *self)
{
  /* do nothing */
}

static void
nautilus_dbus_manager_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
  NautilusDBusManager *self = (NautilusDBusManager *) (object);

  switch (property_id)
    {
    case PROP_APPLICATION:
      self->application = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
nautilus_dbus_manager_constructed (GObject *object)
{
  NautilusDBusManager *self = (NautilusDBusManager *) (object);

  G_OBJECT_CLASS (nautilus_dbus_manager_parent_class)->constructed (object);

  g_application_hold (self->application);

  self->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                   "org.gnome.Nautilus",
                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                   bus_acquired_handler_cb,
                                   NULL,
                                   NULL,
                                   self,
                                   NULL);  
}

static void
nautilus_dbus_manager_class_init (NautilusDBusManagerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = nautilus_dbus_manager_dispose;
  oclass->constructed = nautilus_dbus_manager_constructed;
  oclass->set_property = nautilus_dbus_manager_set_property;

  properties[PROP_APPLICATION] =
    g_param_spec_object ("application",
                         "GApplication instance",
                         "The owning GApplication instance",
                         G_TYPE_APPLICATION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

void
nautilus_dbus_manager_start (GApplication *application)
{
  singleton = g_object_new (nautilus_dbus_manager_get_type (),
                            "application", application,
                            NULL);
}

void
nautilus_dbus_manager_stop (void)
{
  g_clear_object (&singleton);
}
