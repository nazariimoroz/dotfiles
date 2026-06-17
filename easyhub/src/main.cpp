// easyhub — a fast TUI control hub for Hyprland/Wayland.
//
// Features:
//   * Stats   : battery, volume, brightness, Wi-Fi state, Bluetooth, CPU/mem, clock
//   * Wi-Fi   : scan + connect (nmcli)
//   * Bluetooth: scan + connect/disconnect (bluetoothctl)
//
// External tools used at runtime: nmcli, bluetoothctl, brightnessctl, wpctl.

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>

#include "tray.hpp"

using namespace ftxui;

// ---------------------------------------------------------------------------
// Small shell / fs helpers
// ---------------------------------------------------------------------------

static std::string exec(const std::string& cmd) {
  std::array<char, 256> buf{};
  std::string out;
  FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
  if (!pipe) return {};
  while (fgets(buf.data(), buf.size(), pipe)) out += buf.data();
  pclose(pipe);
  return out;
}

static std::string read_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return {};
  std::string s;
  std::getline(f, s);
  return s;
}

static std::string trim(std::string s) {
  const char* ws = " \t\r\n";
  s.erase(0, s.find_first_not_of(ws));
  auto p = s.find_last_not_of(ws);
  if (p != std::string::npos) s.erase(p + 1);
  return s;
}

static std::vector<std::string> lines_of(const std::string& s) {
  std::vector<std::string> v;
  std::stringstream ss(s);
  std::string line;
  while (std::getline(ss, line)) {
    line = trim(line);
    if (!line.empty()) v.push_back(line);
  }
  return v;
}

// Split an `nmcli -t` line on unescaped ':' separators.
static std::vector<std::string> split_nmcli(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '\\' && i + 1 < line.size()) {
      cur += line[++i];
    } else if (line[i] == ':') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += line[i];
    }
  }
  out.push_back(cur);
  return out;
}

// ---------------------------------------------------------------------------
// Stat collectors
// ---------------------------------------------------------------------------

struct Battery { int pct = -1; std::string status; };

static Battery get_battery() {
  Battery b;
  DIR* d = opendir("/sys/class/power_supply");
  if (!d) return b;
  for (dirent* e; (e = readdir(d));) {
    std::string name = e->d_name;
    if (name.rfind("BAT", 0) != 0) continue;
    std::string base = "/sys/class/power_supply/" + name + "/";
    std::string cap = read_file(base + "capacity");
    if (cap.empty()) continue;
    b.pct = std::atoi(cap.c_str());
    b.status = read_file(base + "status");
    break;
  }
  closedir(d);
  return b;
}

// returns {percent 0..100, muted}
static std::pair<int, bool> get_volume() {
  std::string out = exec("wpctl get-volume @DEFAULT_AUDIO_SINK@");
  // "Volume: 0.45 [MUTED]"
  bool muted = out.find("MUTED") != std::string::npos;
  auto pos = out.find(':');
  double v = 0;
  if (pos != std::string::npos) v = std::atof(out.c_str() + pos + 1);
  return {static_cast<int>(v * 100 + 0.5), muted};
}

static int get_brightness() {
  std::string out = trim(exec("brightnessctl -m"));  // "name,class,cur,66%,max"
  auto fields = split_nmcli(out);  // commas? no — split manually
  // brightnessctl -m is comma-separated:
  std::vector<std::string> f;
  std::stringstream ss(out);
  std::string tok;
  while (std::getline(ss, tok, ',')) f.push_back(tok);
  if (f.size() >= 4) {
    std::string p = f[3];
    if (!p.empty() && p.back() == '%') p.pop_back();
    return std::atoi(p.c_str());
  }
  return -1;
}

static std::string get_wifi_status() {
  std::string out = exec("nmcli -t -f ACTIVE,SSID,SIGNAL dev wifi");
  for (auto& l : lines_of(out)) {
    auto f = split_nmcli(l);
    if (f.size() >= 2 && f[0] == "yes")
      return f[1] + (f.size() >= 3 ? " (" + f[2] + "%)" : "");
  }
  return "disconnected";
}

static std::string get_bt_connected() {
  std::string out = exec("bluetoothctl devices Connected");
  std::vector<std::string> names;
  for (auto& l : lines_of(out)) {
    // "Device AA:BB:.. Name"
    auto sp = l.find(' ', 7);
    if (l.rfind("Device ", 0) == 0 && sp != std::string::npos)
      names.push_back(trim(l.substr(sp + 1)));
  }
  if (names.empty()) return "none";
  std::string r;
  for (size_t i = 0; i < names.size(); ++i) r += (i ? ", " : "") + names[i];
  return r;
}

// CPU usage via two /proc/stat samples.
static double cpu_sample() {
  std::string l = read_file("/proc/stat");  // "cpu  u n s i ..."
  std::stringstream ss(l);
  std::string cpu;
  long v, idle = 0, total = 0;
  ss >> cpu;
  int i = 0;
  while (ss >> v) { total += v; if (i == 3) idle = v; ++i; }
  static long prev_total = 0, prev_idle = 0;
  long dt = total - prev_total, di = idle - prev_idle;
  prev_total = total; prev_idle = idle;
  if (dt <= 0) return 0;
  return 100.0 * (dt - di) / dt;
}

static int get_mem_pct() {
  std::ifstream f("/proc/meminfo");
  long total = 0, avail = 0;
  std::string k; long v; std::string unit;
  while (f >> k >> v >> unit) {
    if (k == "MemTotal:") total = v;
    else if (k == "MemAvailable:") { avail = v; break; }
  }
  if (total <= 0) return 0;
  return static_cast<int>(100.0 * (total - avail) / total + 0.5);
}

static std::string now_clock() {
  return trim(exec("date '+%a %d %b  %H:%M:%S'"));
}

// ---------------------------------------------------------------------------
// Shared state (everything mutated off-thread funnels through here)
// ---------------------------------------------------------------------------

struct Shared {
  std::mutex m;
  // stats (written by stats thread, read in render)
  std::string battery = "...", bat_status, wifi = "...", bt = "...", clock;
  int vol = 0, bright = 0, bat_pct = 0, cpu = 0, mem = 0;
  bool muted = false;
  // staged scan results (written by worker, drained on main thread)
  bool wifi_dirty = false, bt_dirty = false;
  std::vector<std::string> staged_wifi_disp, staged_wifi_ssid;
  std::vector<std::string> staged_bt_disp, staged_bt_mac;
  std::string action = "ready";
  std::atomic<bool> busy{false};
};

static Shared g;

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

static Element bar(const std::string& label, int pct, Color c) {
  float f = pct < 0 ? 0.f : pct / 100.f;
  return hbox({
      text(label) | size(WIDTH, EQUAL, 12),
      gauge(f) | color(c) | flex,
      text("  " + (pct < 0 ? std::string("n/a") : std::to_string(pct) + "%")) |
          size(WIDTH, EQUAL, 6),
  });
}

int main(int argc, char** argv) {
  // Daemon mode: just run the tray watcher (autostarted at login).
  if (argc > 1 && std::string(argv[1]) == "--watcher") return tray::run_watcher();

  // Debug: dump tray items + their menus, then exit.
  if (argc > 1 && std::string(argv[1]) == "--list") {
    for (auto& it : tray::list_items()) {
      printf("ITEM %s  bus=%s path=%s menu=%s\n", it.name.c_str(),
             it.bus.c_str(), it.path.c_str(), it.menu_path.c_str());
      for (auto& e : tray::menu_of(it))
        printf("   [%d] %s%s\n", e.id, e.separator ? "----" : e.label.c_str(),
               e.enabled ? "" : " (disabled)");
    }
    return 0;
  }

  // Debug: `--open <substr>` triggers the "Open"-ish menu entry of the tray
  // item matching <substr> (by item name or any menu label).
  if (argc > 2 && std::string(argv[1]) == "--open") {
    std::string q = argv[2];
    for (auto& it : tray::list_items()) {
      auto m = tray::menu_of(it);
      bool match = it.name.find(q) != std::string::npos;
      for (auto& e : m)
        if (e.label.find(q) != std::string::npos) match = true;
      if (!match) continue;
      for (auto& e : m) {
        std::string lc = e.label;
        for (auto& ch : lc) ch = std::tolower(ch);
        if (e.enabled && !e.separator &&
            (lc.find("open") != std::string::npos ||
             lc.find("show") != std::string::npos)) {
          tray::trigger(it, e.id);
          printf("triggered '%s' on %s\n", e.label.c_str(), it.name.c_str());
          return 0;
        }
      }
    }
    printf("no match for '%s'\n", q.c_str());
    return 1;
  }

  // Make sure a tray watcher exists so apps export their icons. If none is
  // running yet, spawn our own detached daemon. (Apps started before any
  // watcher existed only register on their next launch.)
  if (!tray::watcher_present()) {
    std::string self = argv[0];
    exec("setsid -f '" + self + "' --watcher >/dev/null 2>&1");
  }

  auto screen = ScreenInteractive::Fullscreen();

  // ---- background stats refresh ----
  std::atomic<bool> running{true};
  std::thread stats([&] {
    cpu_sample();  // prime
    while (running) {
      auto bat = get_battery();
      auto [vol, muted] = get_volume();
      int br = get_brightness();
      std::string wifi = get_wifi_status();
      std::string bt = get_bt_connected();
      int cpu = static_cast<int>(cpu_sample() + 0.5);
      int mem = get_mem_pct();
      std::string clk = now_clock();
      {
        std::lock_guard<std::mutex> lk(g.m);
        g.bat_pct = bat.pct; g.bat_status = bat.status;
        g.battery = bat.pct < 0 ? "no battery"
                                : std::to_string(bat.pct) + "% " + bat.status;
        g.vol = vol; g.muted = muted;
        g.bright = br;
        g.wifi = wifi; g.bt = bt;
        g.cpu = cpu; g.mem = mem; g.clock = clk;
      }
      screen.PostEvent(Event::Custom);
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  });

  // ---- tab selection ----
  std::vector<std::string> tab_titles = {"  Stats  ", "  Wi-Fi  ",
                                         "  Bluetooth  ", "  Apps  ",
                                         "  Power  "};
  int tab = 0;
  auto toggle = Toggle(&tab_titles, &tab);

  // Re-read volume + brightness immediately for snappy feedback after a change.
  auto refresh_av = [&] {
    auto [v, m] = get_volume();
    int br = get_brightness();
    {
      std::lock_guard<std::mutex> lk(g.m);
      g.vol = v; g.muted = m; g.bright = br;
    }
    screen.PostEvent(Event::Custom);
  };
  auto vol_up    = [&] { exec("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+ -l 1.0"); refresh_av(); };
  auto vol_down  = [&] { exec("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-"); refresh_av(); };
  auto vol_mute  = [&] { exec("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle"); refresh_av(); };
  auto brt_up    = [&] { exec("brightnessctl set 5%+"); refresh_av(); };
  auto brt_down  = [&] { exec("brightnessctl set 5%-"); refresh_av(); };

  // ===================== STATS TAB =====================
  auto s_vdn  = Button("Vol -",  vol_down, ButtonOption::Ascii());
  auto s_vup  = Button("Vol +",  vol_up,   ButtonOption::Ascii());
  auto s_mute = Button("Mute",   vol_mute, ButtonOption::Ascii());
  auto s_bdn  = Button("Bright -", brt_down, ButtonOption::Ascii());
  auto s_bup  = Button("Bright +", brt_up,   ButtonOption::Ascii());
  auto stats_controls =
      Container::Horizontal({s_vdn, s_vup, s_mute, s_bdn, s_bup});

  auto stats_render = Renderer(stats_controls, [&] {
    Element gauges;
    {
      std::lock_guard<std::mutex> lk(g.m);
      auto bat_color = g.bat_pct < 20 ? Color::Red
                       : g.bat_pct < 50 ? Color::Yellow
                                        : Color::Green;
      gauges = vbox({
          text(g.clock) | bold | center,
          separator(),
          bar("Battery", g.bat_pct, bat_color),
          bar(g.muted ? "Volume (M)" : "Volume", g.vol,
              g.muted ? Color::GrayDark : Color::Cyan),
          bar("Brightness", g.bright, Color::Yellow),
          separator(),
          bar("CPU", g.cpu, Color::Magenta),
          bar("Memory", g.mem, Color::Blue),
          separator(),
          hbox({text(" Wi-Fi     ") | bold, text(g.wifi) | color(Color::Cyan)}),
          hbox({text(" Bluetooth ") | bold, text(g.bt) | color(Color::Blue)}),
      });
    }
    return vbox({
        gauges,
        separator(),
        hbox({s_vdn->Render(), text(" "), s_vup->Render(), text(" "),
              s_mute->Render(), text("   "), s_bdn->Render(), text(" "),
              s_bup->Render()}) |
            center,
        text("keys: -/+ volume · m mute · [ / ] brightness") | dim | center,
    });
  });

  // ===================== WIFI TAB =====================
  std::vector<std::string> wifi_disp{"(press Scan)"}, wifi_ssid;
  int wifi_sel = 0;
  std::string wifi_pass;

  auto wifi_menu = Menu(&wifi_disp, &wifi_sel);

  auto wifi_scan = [&] {
    if (g.busy.exchange(true)) return;
    { std::lock_guard<std::mutex> lk(g.m); g.action = "scanning Wi-Fi..."; }
    screen.PostEvent(Event::Custom);
    std::thread([&] {
      exec("nmcli dev wifi rescan");
      std::string out =
          exec("nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list");
      std::vector<std::string> disp, ssid;
      for (auto& l : lines_of(out)) {
        auto f = split_nmcli(l);
        if (f.size() < 4 || f[1].empty()) continue;
        std::string mark = (f[0] == "*") ? "● " : "  ";
        std::string sec = f[3].empty() ? "open" : f[3];
        char row[160];
        snprintf(row, sizeof row, "%s%-24.24s %3s%%  %s", mark.c_str(),
                 f[1].c_str(), f[2].c_str(), sec.c_str());
        disp.emplace_back(row);
        ssid.push_back(f[1]);
      }
      {
        std::lock_guard<std::mutex> lk(g.m);
        g.staged_wifi_disp = std::move(disp);
        g.staged_wifi_ssid = std::move(ssid);
        g.wifi_dirty = true;
        g.action = "found " + std::to_string(g.staged_wifi_ssid.size()) +
                   " networks";
      }
      g.busy = false;
      screen.PostEvent(Event::Custom);
    }).detach();
  };

  auto wifi_connect = [&] {
    if (g.busy.exchange(true)) return;
    if (wifi_sel < 0 || wifi_sel >= (int)wifi_ssid.size()) {
      g.busy = false;
      return;
    }
    std::string ssid = wifi_ssid[wifi_sel];
    std::string pass = wifi_pass;
    { std::lock_guard<std::mutex> lk(g.m); g.action = "connecting to " + ssid + "..."; }
    screen.PostEvent(Event::Custom);
    std::thread([&, ssid, pass] {
      std::string cmd = "nmcli dev wifi connect '" + ssid + "'";
      if (!pass.empty()) cmd += " password '" + pass + "'";
      std::string out = trim(exec(cmd));
      {
        std::lock_guard<std::mutex> lk(g.m);
        g.action = out.empty() ? "connect: done" : out;
      }
      g.busy = false;
      screen.PostEvent(Event::Custom);
    }).detach();
  };

  InputOption pass_opt;
  pass_opt.password = true;
  auto wifi_pass_in = Input(&wifi_pass, "password (blank = open / saved)", pass_opt);
  auto wifi_btn_scan = Button("Scan", wifi_scan, ButtonOption::Ascii());
  auto wifi_btn_conn = Button("Connect", wifi_connect, ButtonOption::Ascii());

  auto wifi_controls = Container::Horizontal({wifi_btn_scan, wifi_btn_conn});
  auto wifi_container =
      Container::Vertical({wifi_menu, wifi_pass_in, wifi_controls});
  auto wifi_tab = Renderer(wifi_container, [&] {
    return vbox({
        text("Wi-Fi networks") | bold,
        separator(),
        wifi_menu->Render() | frame | flex,
        separator(),
        hbox({text(" Pass ") | bold, wifi_pass_in->Render() | flex | border}),
        hbox({wifi_btn_scan->Render(), text(" "), wifi_btn_conn->Render()}),
    });
  });

  // ===================== BLUETOOTH TAB =====================
  std::vector<std::string> bt_disp{"(press Scan)"}, bt_mac;
  int bt_sel = 0;
  auto bt_menu = Menu(&bt_disp, &bt_sel);

  auto bt_scan = [&] {
    if (g.busy.exchange(true)) return;
    { std::lock_guard<std::mutex> lk(g.m); g.action = "scanning Bluetooth (8s)..."; }
    screen.PostEvent(Event::Custom);
    std::thread([&] {
      exec("bluetoothctl --timeout 8 scan on");
      std::string out = exec("bluetoothctl devices");
      std::string conn = exec("bluetoothctl devices Connected");
      std::vector<std::string> disp, mac;
      for (auto& l : lines_of(out)) {
        if (l.rfind("Device ", 0) != 0) continue;
        auto sp = l.find(' ', 7);
        if (sp == std::string::npos) continue;
        std::string m = l.substr(7, sp - 7);
        std::string name = trim(l.substr(sp + 1));
        std::string mark = conn.find(m) != std::string::npos ? "● " : "  ";
        disp.push_back(mark + name + "  [" + m + "]");
        mac.push_back(m);
      }
      {
        std::lock_guard<std::mutex> lk(g.m);
        g.staged_bt_disp = std::move(disp);
        g.staged_bt_mac = std::move(mac);
        g.bt_dirty = true;
        g.action = "found " + std::to_string(g.staged_bt_mac.size()) + " devices";
      }
      g.busy = false;
      screen.PostEvent(Event::Custom);
    }).detach();
  };

  auto bt_action = [&](bool connect) {
    if (g.busy.exchange(true)) return;
    if (bt_sel < 0 || bt_sel >= (int)bt_mac.size()) { g.busy = false; return; }
    std::string mac = bt_mac[bt_sel];
    { std::lock_guard<std::mutex> lk(g.m);
      g.action = (connect ? "connecting " : "disconnecting ") + mac + "..."; }
    screen.PostEvent(Event::Custom);
    std::thread([&, mac, connect] {
      if (connect) exec("bluetoothctl pair " + mac);
      std::string out = trim(exec("bluetoothctl " +
                                  std::string(connect ? "connect " : "disconnect ") + mac));
      { std::lock_guard<std::mutex> lk(g.m); g.action = out.empty() ? "done" : out; }
      g.busy = false;
      screen.PostEvent(Event::Custom);
    }).detach();
  };

  auto bt_btn_scan = Button("Scan", bt_scan, ButtonOption::Ascii());
  auto bt_btn_conn = Button("Connect", [&] { bt_action(true); }, ButtonOption::Ascii());
  auto bt_btn_disc = Button("Disconnect", [&] { bt_action(false); }, ButtonOption::Ascii());
  auto bt_controls = Container::Horizontal({bt_btn_scan, bt_btn_conn, bt_btn_disc});
  auto bt_container = Container::Vertical({bt_menu, bt_controls});
  auto bt_tab = Renderer(bt_container, [&] {
    return vbox({
        text("Bluetooth devices") | bold,
        separator(),
        bt_menu->Render() | frame | flex,
        separator(),
        hbox({bt_btn_scan->Render(), text(" "), bt_btn_conn->Render(), text(" "),
              bt_btn_disc->Render()}),
    });
  });

  // ===================== POWER TAB =====================
  bool confirm_open = false;
  std::string pending_label, pending_cmd;
  auto power_btn = [&](const std::string& label, const std::string& cmd) {
    return Button(
        label,
        [&, label, cmd] {
          pending_label = label;
          pending_cmd = cmd;
          confirm_open = true;
        },
        ButtonOption::Ascii());
  };
  auto p_shutdown = power_btn("Shutdown", "systemctl poweroff");
  auto p_restart  = power_btn("Restart",  "systemctl reboot");
  auto p_logout   = power_btn("Log out",  "hyprctl dispatch exit");
  auto p_sleep    = power_btn("Sleep",    "systemctl suspend");
  auto power_buttons =
      Container::Vertical({p_shutdown, p_restart, p_logout, p_sleep});
  auto power_tab = Renderer(power_buttons, [&] {
    return vbox({
        text("Power") | bold,
        separator(),
        p_shutdown->Render(),
        p_restart->Render(),
        p_logout->Render(),
        p_sleep->Render(),
    });
  });

  // Confirmation dialog shared by all power actions.
  auto confirm_yes = Button(
      "Confirm",
      [&] {
        confirm_open = false;
        std::string c = pending_cmd;
        std::thread([c] { exec(c); }).detach();
      },
      ButtonOption::Ascii());
  auto confirm_no =
      Button("Cancel", [&] { confirm_open = false; }, ButtonOption::Ascii());
  auto confirm_row = Container::Horizontal({confirm_no, confirm_yes});
  auto confirm_box = Renderer(confirm_row, [&] {
    return vbox({
               text("Really " + pending_label + "?") | bold | center,
               separator(),
               hbox({confirm_no->Render(), text("  "), confirm_yes->Render()}) |
                   center,
           }) |
           border | bgcolor(Color::Black) | size(WIDTH, GREATER_THAN, 30);
  });

  // ===================== APPS TAB =====================
  // Every open window + every system-tray app in one list. Enter on a window
  // focuses it; Enter on a tray app opens its tray menu (e.g. "Open") which
  // raises the window — exactly like clicking the tray icon in Windows/macOS.
  struct AppRow {
    bool is_window = false;
    std::string addr;  // hyprland window address
    tray::Item item;   // tray item (when !is_window)
    bool has_menu = false;
  };
  std::vector<AppRow> app_rows;
  std::vector<std::string> apps_disp{"(press Refresh)"};
  int apps_sel = 0;
  int apps_mode = 0;  // 0 = app list, 1 = a tray item's menu
  tray::Item apps_cur;
  std::vector<tray::MenuEntry> apps_menu;
  int apps_prev_tab = -1;  // for auto-refresh when entering the Apps tab

  auto apps_refresh = [&] {
    apps_mode = 0;
    app_rows.clear();
    apps_disp.clear();
    std::string out = exec(
        "hyprctl clients -j | jq -r '.[] | "
        "\"\\(.address)\\t\\(.class)\\t\\(.title)\"'");
    for (auto& l : lines_of(out)) {
      std::vector<std::string> f;
      std::stringstream ss(l);
      std::string tok;
      while (std::getline(ss, tok, '\t')) f.push_back(tok);
      if (f.size() < 2) continue;
      AppRow r;
      r.is_window = true;
      r.addr = f[0];
      std::string title = f.size() >= 3 ? f[2] : "";
      apps_disp.push_back("🪟 " + f[1] + (title.empty() ? "" : "  " + title));
      app_rows.push_back(r);
    }
    for (auto& it : tray::list_items()) {
      AppRow r;
      r.item = it;
      r.has_menu = !it.menu_path.empty();
      apps_disp.push_back("📌 " + it.name + "  (tray)");
      app_rows.push_back(r);
    }
    if (apps_disp.empty()) apps_disp.push_back("(nothing running)");
    if (apps_sel >= (int)apps_disp.size()) apps_sel = 0;
  };

  auto apps_on_enter = [&] {
    if (apps_mode == 1) {  // inside a tray item's menu
      if (apps_sel >= 0 && apps_sel < (int)apps_menu.size()) {
        auto& e = apps_menu[apps_sel];
        if (!e.separator && e.enabled) {
          tray::trigger(apps_cur, e.id);
          { std::lock_guard<std::mutex> lk(g.m); g.action = "→ " + e.label; }
          apps_refresh();
        }
      }
      return;
    }
    if (apps_sel < 0 || apps_sel >= (int)app_rows.size()) return;
    AppRow r = app_rows[apps_sel];
    if (r.is_window) {
      exec("hyprctl dispatch focuswindow address:" + r.addr);
      std::lock_guard<std::mutex> lk(g.m);
      g.action = "focused window";
    } else if (r.has_menu) {  // open the tray menu
      apps_menu = tray::menu_of(r.item);
      if (apps_menu.empty()) { tray::activate(r.item); return; }
      apps_cur = r.item;
      apps_disp.clear();
      for (auto& e : apps_menu)
        apps_disp.push_back(e.separator ? "  ────────"
                            : (e.enabled ? "  " : "· ") + e.label);
      apps_sel = 0;
      apps_mode = 1;
    } else {
      tray::activate(r.item);
      std::lock_guard<std::mutex> lk(g.m);
      g.action = "activated " + r.item.name;
    }
  };

  auto apps_menu_opt = MenuOption::Vertical();
  apps_menu_opt.on_enter = apps_on_enter;
  auto apps_list = Menu(&apps_disp, &apps_sel, apps_menu_opt);
  auto apps_btn = Button("Refresh", apps_refresh, ButtonOption::Ascii());
  auto apps_container = Container::Vertical({apps_list, apps_btn});
  auto apps_tab = Renderer(apps_container, [&] {
    return vbox({
        text(apps_mode == 1 ? "Tray menu — Enter opens · Esc back"
                            : "Apps + tray — Enter focus/open · r refresh") |
            bold,
        separator(),
        apps_list->Render() | frame | flex,
        separator(),
        apps_btn->Render(),
    });
  });

  // ===================== ASSEMBLE =====================
  auto tabs =
      Container::Tab({stats_render, wifi_tab, bt_tab, apps_tab, power_tab}, &tab);
  auto root = Container::Vertical({toggle, tabs});

  auto layout = Renderer(root, [&] {
    std::string status;
    { std::lock_guard<std::mutex> lk(g.m); status = g.action; }
    return vbox({
               text("⚡ easyhub") | bold | center | color(Color::GreenLight),
               toggle->Render() | center,
               separator(),
               tabs->Render() | flex,
               separator(),
               hbox({text(" " + status) | dim | flex,
                     text("Tab/←→ switch · q quit ") | dim}),
           }) |
           border;
  });

  // Power confirmation overlays everything.
  auto root_modal = Modal(layout, confirm_box, &confirm_open);

  // Drain staged scan results + global hotkeys on the main thread.
  auto app = CatchEvent(root_modal, [&](Event e) {
    // Apps tab: refresh on entry, 'r' to refresh, Esc to leave a tray submenu.
    if (tab == 3 && apps_prev_tab != 3) apps_refresh();
    apps_prev_tab = tab;
    if (tab == 3 && !confirm_open) {
      if (e == Event::Character('r')) { apps_refresh(); return true; }
      if (e == Event::Escape && apps_mode == 1) { apps_refresh(); return true; }
    }

    // Volume / brightness hotkeys — Stats tab only, so they don't clash with
    // the Wi-Fi password field or modal buttons.
    if (!confirm_open && tab == 0) {
      if (e == Event::Character('+') || e == Event::Character('=')) { vol_up();   return true; }
      if (e == Event::Character('-') || e == Event::Character('_')) { vol_down(); return true; }
      if (e == Event::Character('m')) { vol_mute(); return true; }
      if (e == Event::Character(']')) { brt_up();   return true; }
      if (e == Event::Character('[')) { brt_down(); return true; }
    }
    if (e == Event::Custom) {
      std::lock_guard<std::mutex> lk(g.m);
      if (g.wifi_dirty) {
        wifi_disp = g.staged_wifi_disp;
        wifi_ssid = g.staged_wifi_ssid;
        if (wifi_disp.empty()) wifi_disp = {"(none found)"};
        if (wifi_sel >= (int)wifi_disp.size()) wifi_sel = 0;
        g.wifi_dirty = false;
      }
      if (g.bt_dirty) {
        bt_disp = g.staged_bt_disp;
        bt_mac = g.staged_bt_mac;
        if (bt_disp.empty()) bt_disp = {"(none found)"};
        if (bt_sel >= (int)bt_disp.size()) bt_sel = 0;
        g.bt_dirty = false;
      }
      return true;
    }
    if (e == Event::Escape && confirm_open) {
      confirm_open = false;
      return true;
    }
    if (e == Event::Character('q') || e == Event::Escape) {
      screen.Exit();
      return true;
    }
    return false;
  });

  screen.Loop(app);

  running = false;
  if (stats.joinable()) stats.join();
  return 0;
}
