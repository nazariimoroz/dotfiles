// tray.hpp — StatusNotifier (system tray) support for easyhub.
//
// Two roles:
//   * Daemon  : run_watcher() owns org.kde.StatusNotifierWatcher + acts as a
//               host, so tray apps (Filen, Telegram, …) export their icons.
//               Launched via `easyhub --watcher`, autostarted at login.
//   * Client  : the TUI reads the registered items from the watcher, reads each
//               item's context menu (com.canonical.dbusmenu) and can Activate an
//               item or trigger a menu entry (e.g. "Open") — which raises the
//               app's window, exactly like clicking the tray icon.
#pragma once

#include <string>
#include <vector>

namespace tray {

// Blocks, running the StatusNotifierWatcher+Host main loop. Returns on exit.
int run_watcher();

// True if some StatusNotifierWatcher is already on the session bus.
bool watcher_present();

struct Item {
  std::string service;    // raw registration string
  std::string bus;        // unique name, e.g. ":1.530"
  std::string path;       // SNI object path, e.g. "/StatusNotifierItem"
  std::string name;       // display label (Title → Id → bus)
  std::string menu_path;  // dbusmenu object path, empty if none
};

struct MenuEntry {
  int id;
  std::string label;
  bool enabled;
  bool separator;
};

std::vector<Item> list_items();                  // from the watcher
std::vector<MenuEntry> menu_of(const Item& it);  // dbusmenu GetLayout (flattened)
bool activate(const Item& it);                   // SNI left-click
bool trigger(const Item& it, int id);            // dbusmenu "clicked" on entry

}  // namespace tray
