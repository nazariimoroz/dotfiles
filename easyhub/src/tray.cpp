#include "tray.hpp"

#include <gio/gio.h>

#include <algorithm>
#include <mutex>

namespace tray {

// ---------------------------------------------------------------------------
// Daemon: StatusNotifierWatcher + Host
// ---------------------------------------------------------------------------

namespace {

std::mutex g_mtx;
std::vector<std::string> g_items;  // registered item strings ("bus/path")

const char* kWatcherXml =
    "<node><interface name='org.kde.StatusNotifierWatcher'>"
    "<method name='RegisterStatusNotifierItem'>"
    "<arg type='s' direction='in'/></method>"
    "<method name='RegisterStatusNotifierHost'>"
    "<arg type='s' direction='in'/></method>"
    "<signal name='StatusNotifierItemRegistered'><arg type='s'/></signal>"
    "<signal name='StatusNotifierItemUnregistered'><arg type='s'/></signal>"
    "<signal name='StatusNotifierHostRegistered'/>"
    "<property name='RegisteredStatusNotifierItems' type='as' access='read'/>"
    "<property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
    "<property name='ProtocolVersion' type='i' access='read'/>"
    "</interface></node>";

// Turn a RegisterStatusNotifierItem argument + sender into a "bus/path" string.
std::string normalize(const std::string& svc, const char* sender) {
  if (!svc.empty() && svc[0] == '/') return std::string(sender) + svc;
  if (svc.find('/') != std::string::npos) return svc;
  if (!svc.empty() && svc[0] == ':') return svc + "/StatusNotifierItem";
  return std::string(sender) + "/StatusNotifierItem";
}

void on_method(GDBusConnection* c, const char*, const char* obj, const char* iface,
               const char* method, GVariant* params, GDBusMethodInvocation* inv,
               gpointer) {
  const char* sender = g_dbus_method_invocation_get_sender(inv);
  if (g_strcmp0(method, "RegisterStatusNotifierItem") == 0) {
    const char* svc = nullptr;
    g_variant_get(params, "(&s)", &svc);
    std::string item = normalize(svc ? svc : "", sender);
    {
      std::lock_guard<std::mutex> lk(g_mtx);
      if (std::find(g_items.begin(), g_items.end(), item) == g_items.end())
        g_items.push_back(item);
    }
    g_dbus_connection_emit_signal(c, nullptr, obj, iface,
                                  "StatusNotifierItemRegistered",
                                  g_variant_new("(s)", item.c_str()), nullptr);
    g_dbus_method_invocation_return_value(inv, nullptr);
  } else if (g_strcmp0(method, "RegisterStatusNotifierHost") == 0) {
    g_dbus_connection_emit_signal(c, nullptr, obj, iface,
                                  "StatusNotifierHostRegistered", nullptr,
                                  nullptr);
    g_dbus_method_invocation_return_value(inv, nullptr);
  } else {
    g_dbus_method_invocation_return_value(inv, nullptr);
  }
}

GVariant* on_get_prop(GDBusConnection*, const char*, const char*, const char*,
                      const char* prop, GError**, gpointer) {
  if (g_strcmp0(prop, "RegisteredStatusNotifierItems") == 0) {
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("as"));
    std::lock_guard<std::mutex> lk(g_mtx);
    for (auto& s : g_items) g_variant_builder_add(&b, "s", s.c_str());
    return g_variant_builder_end(&b);
  }
  if (g_strcmp0(prop, "IsStatusNotifierHostRegistered") == 0)
    return g_variant_new_boolean(TRUE);
  if (g_strcmp0(prop, "ProtocolVersion") == 0) return g_variant_new_int32(0);
  return nullptr;
}

const GDBusInterfaceVTable kVTable = {on_method, on_get_prop, nullptr, {nullptr}};
GDBusConnection* g_watch_conn = nullptr;

// Drop items whose owning bus name disappears.
void on_name_owner_changed(GDBusConnection* c, const char*, const char*,
                           const char*, const char*, GVariant* params, gpointer) {
  const char *name = nullptr, *old_owner = nullptr, *new_owner = nullptr;
  g_variant_get(params, "(&s&s&s)", &name, &old_owner, &new_owner);
  if (!new_owner || *new_owner) return;  // only care about name vanishing
  std::string gone;
  {
    std::lock_guard<std::mutex> lk(g_mtx);
    for (auto it = g_items.begin(); it != g_items.end();) {
      if (it->rfind(name, 0) == 0) {  // item string starts with the bus name
        gone = *it;
        it = g_items.erase(it);
      } else {
        ++it;
      }
    }
  }
  if (!gone.empty())
    g_dbus_connection_emit_signal(c, nullptr, "/StatusNotifierWatcher",
                                  "org.kde.StatusNotifierWatcher",
                                  "StatusNotifierItemUnregistered",
                                  g_variant_new("(s)", gone.c_str()), nullptr);
}

void on_bus_acquired(GDBusConnection* c, const char*, gpointer) {
  g_watch_conn = c;
  GError* err = nullptr;
  GDBusNodeInfo* node = g_dbus_node_info_new_for_xml(kWatcherXml, &err);
  if (err) {
    g_error_free(err);
    return;
  }
  g_dbus_connection_register_object(c, "/StatusNotifierWatcher",
                                    node->interfaces[0], &kVTable, nullptr,
                                    nullptr, nullptr);
  g_dbus_connection_signal_subscribe(
      c, "org.freedesktop.DBus", "org.freedesktop.DBus", "NameOwnerChanged",
      "/org/freedesktop/DBus", nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      on_name_owner_changed, nullptr, nullptr);
}

// Synchronous session-bus connection for client calls (thread-safe to share,
// but we just grab it per call for simplicity).
GVariant* call(const std::string& bus, const std::string& path,
               const char* iface, const char* method, GVariant* args,
               const GVariantType* reply_type) {
  GError* err = nullptr;
  GDBusConnection* c = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
  if (!c) {
    if (err) g_error_free(err);
    if (args) g_variant_unref(g_variant_ref_sink(args));
    return nullptr;
  }
  GVariant* r = g_dbus_connection_call_sync(c, bus.c_str(), path.c_str(), iface,
                                            method, args, reply_type,
                                            G_DBUS_CALL_FLAGS_NONE, 2000,
                                            nullptr, &err);
  g_object_unref(c);
  if (err) g_error_free(err);
  return r;
}

std::string get_str_prop(const std::string& bus, const std::string& path,
                         const char* iface, const char* prop) {
  GVariant* r = call(bus, path, "org.freedesktop.DBus.Properties", "Get",
                     g_variant_new("(ss)", iface, prop), G_VARIANT_TYPE("(v)"));
  if (!r) return "";
  GVariant* v = nullptr;
  g_variant_get(r, "(v)", &v);
  std::string out;
  if (v && (g_variant_is_of_type(v, G_VARIANT_TYPE_STRING) ||
            g_variant_is_of_type(v, G_VARIANT_TYPE_OBJECT_PATH)))
    out = g_variant_get_string(v, nullptr);
  if (v) g_variant_unref(v);
  g_variant_unref(r);
  return out;
}

void flatten(GVariant* node, std::vector<MenuEntry>& out) {
  gint32 id = 0;
  GVariant* props = nullptr;
  GVariant* children = nullptr;
  g_variant_get(node, "(i@a{sv}@av)", &id, &props, &children);

  const char* label = nullptr;
  const char* type = nullptr;
  gboolean enabled = TRUE;
  gboolean visible = TRUE;
  g_variant_lookup(props, "label", "&s", &label);
  g_variant_lookup(props, "type", "&s", &type);
  g_variant_lookup(props, "enabled", "b", &enabled);
  g_variant_lookup(props, "visible", "b", &visible);

  bool sep = type && g_strcmp0(type, "separator") == 0;
  if (visible) {
    if (sep)
      out.push_back({id, "", false, true});
    else if (label && *label)
      out.push_back({id, label, (bool)enabled, false});
  }

  GVariantIter it;
  g_variant_iter_init(&it, children);
  GVariant* boxed = nullptr;
  while ((boxed = g_variant_iter_next_value(&it))) {
    GVariant* child = g_variant_get_variant(boxed);
    flatten(child, out);
    g_variant_unref(child);
    g_variant_unref(boxed);
  }
  g_variant_unref(props);
  g_variant_unref(children);
}

}  // namespace

int run_watcher() {
  GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
  g_bus_own_name(G_BUS_TYPE_SESSION, "org.kde.StatusNotifierWatcher",
                 G_BUS_NAME_OWNER_FLAGS_REPLACE, on_bus_acquired, nullptr,
                 nullptr, nullptr, nullptr);
  g_main_loop_run(loop);
  return 0;
}

bool watcher_present() {
  GVariant* r = call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                     "org.freedesktop.DBus", "NameHasOwner",
                     g_variant_new("(s)", "org.kde.StatusNotifierWatcher"),
                     G_VARIANT_TYPE("(b)"));
  if (!r) return false;
  gboolean has = FALSE;
  g_variant_get(r, "(b)", &has);
  g_variant_unref(r);
  return has;
}

std::vector<Item> list_items() {
  std::vector<Item> items;
  GVariant* r = call("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                     "org.freedesktop.DBus.Properties", "Get",
                     g_variant_new("(ss)", "org.kde.StatusNotifierWatcher",
                                   "RegisteredStatusNotifierItems"),
                     G_VARIANT_TYPE("(v)"));
  if (!r) return items;
  GVariant* v = nullptr;
  g_variant_get(r, "(v)", &v);
  if (v) {
    GVariantIter it;
    g_variant_iter_init(&it, v);
    const char* s = nullptr;
    while (g_variant_iter_next(&it, "&s", &s)) {
      Item itm;
      itm.service = s;
      std::string str = s;
      auto slash = str.find('/');
      if (slash != std::string::npos) {
        itm.bus = str.substr(0, slash);
        itm.path = str.substr(slash);
      } else {
        itm.bus = str;
        itm.path = "/StatusNotifierItem";
      }
      itm.name = get_str_prop(itm.bus, itm.path, "org.kde.StatusNotifierItem",
                              "Title");
      if (itm.name.empty())
        itm.name =
            get_str_prop(itm.bus, itm.path, "org.kde.StatusNotifierItem", "Id");
      itm.menu_path = get_str_prop(itm.bus, itm.path,
                                   "org.kde.StatusNotifierItem", "Menu");
      // Electron tray icons report a useless Id like "chrome_status_icon_1"
      // (and an empty Title). Fall back to the menu's header label, which is
      // the real app name (e.g. "Filen").
      if (itm.name.empty() ||
          itm.name.rfind("chrome_status_icon", 0) == 0) {
        for (auto& e : menu_of(itm))
          if (!e.separator && !e.label.empty()) {
            itm.name = e.label;
            break;
          }
      }
      if (itm.name.empty()) itm.name = itm.bus;
      items.push_back(itm);
    }
    g_variant_unref(v);
  }
  g_variant_unref(r);
  return items;
}

std::vector<MenuEntry> menu_of(const Item& it) {
  std::vector<MenuEntry> out;
  if (it.menu_path.empty()) return out;
  const char* empty[] = {nullptr};
  GVariant* r = call(it.bus, it.menu_path, "com.canonical.dbusmenu",
                     "GetLayout", g_variant_new("(ii^as)", 0, -1, empty),
                     G_VARIANT_TYPE("(u(ia{sv}av))"));
  if (!r) return out;
  guint32 rev = 0;
  GVariant* root = nullptr;
  g_variant_get(r, "(u@(ia{sv}av))", &rev, &root);
  if (root) {
    flatten(root, out);
    g_variant_unref(root);
  }
  g_variant_unref(r);
  return out;
}

bool activate(const Item& it) {
  GVariant* r = call(it.bus, it.path, "org.kde.StatusNotifierItem", "Activate",
                     g_variant_new("(ii)", 0, 0), nullptr);
  if (r) g_variant_unref(r);
  return true;
}

bool trigger(const Item& it, int id) {
  if (it.menu_path.empty()) return false;
  GVariant* r = call(it.bus, it.menu_path, "com.canonical.dbusmenu", "Event",
                     g_variant_new("(isvu)", id, "clicked",
                                   g_variant_new_string(""), (guint32)0),
                     nullptr);
  if (r) g_variant_unref(r);
  return true;
}

}  // namespace tray
