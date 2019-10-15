/*
 * Copyright 2019 Arcus Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dbus/dbus.h>
#include <glib.h>

/* Subset of support we need from Bluez DBUS related files that we
   have crafted into a library */

#define ERROR_INTERFACE                 "org.bluez.Error"
#define GATT_MGR_IFACE                  "org.bluez.GattManager1"
#define GATT_SERVICE_IFACE              "org.bluez.GattService1"
#define GATT_CHR_IFACE                  "org.bluez.GattCharacteristic1"
#define GATT_DESCRIPTOR_IFACE           "org.bluez.GattDescriptor1"

typedef struct GDBusClient GDBusClient;
typedef struct GDBusProxy GDBusProxy;
typedef void (* GDBusProxyFunction) (GDBusProxy *proxy, void *user_data);
typedef void (* GDBusPropertyFunction) (GDBusProxy *proxy, const char *name,
                                        DBusMessageIter *iter, void *user_data);

typedef gboolean (* ValidateFunction) (const uint8_t *value, int vlen);
typedef gboolean (* UpdateFunction) (uint8_t **value, int *vlen,
                                     gboolean internal);

const char *desc_props[];

DBusConnection *g_dbus_setup_bus(DBusBusType type, const char *name,
                                 DBusError *error);

gboolean g_dbus_unregister_interface(DBusConnection *connection,
                                        const char *path, const char *name);

const char *g_dbus_proxy_get_interface(GDBusProxy *proxy);

gboolean g_dbus_attach_object_manager(DBusConnection *connection);
gboolean g_dbus_detach_object_manager(DBusConnection *connection);

GDBusClient *g_dbus_client_new(DBusConnection *connection,
                                        const char *service, const char *path);
GDBusClient *g_dbus_client_ref(GDBusClient *client);
void g_dbus_client_unref(GDBusClient *client);

GDBusProxy *g_dbus_proxy_ref(GDBusProxy *proxy);
void g_dbus_proxy_unref(GDBusProxy *proxy);

gboolean g_dbus_client_set_proxy_handlers(GDBusClient *client,
                                        GDBusProxyFunction proxy_added,
                                        GDBusProxyFunction proxy_removed,
                                        GDBusPropertyFunction property_changed,
                                        void *user_data);

gboolean register_characteristic(DBusConnection *conn,
				 const char *chr_uuid,
				 const uint8_t *value, int vlen,
				 const char **props,
				 const char *desc_uuid,
				 const uint8_t *dvalue, int dvlen,
				 const char **desc_props,
				 const char *service_path,
				 const ValidateFunction validate_cb,
				 const UpdateFunction update_cb);

char *register_service(DBusConnection *conn, const char *uuid);

void register_app(GDBusProxy *proxy);
