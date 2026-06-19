#!/bin/bash
# BrutePin v1.1 - Interactive Menu
# Authorized Security Testing Only

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

source "$SCRIPT_DIR/adb.sh"
source "$SCRIPT_DIR/detector.sh"
source "$SCRIPT_DIR/engine.sh"

DELAY=3; START=""; END=""; WORDLIST=""; RESUME=0; VERBOSE=0

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

banner() {
    clear
    echo -e "${RED}"
    echo "  ██████  ██████  ██    ██ ████████ ███████  ██████  ██ ███    ██ "
    echo "  ██   ██ ██   ██ ██    ██    ██    ██      ██       ██ ████   ██ "
    echo "  ██████  ██████  ██    ██    ██    █████   ██   ███ ██ ██ ██  ██ "
    echo "  ██   ██ ██   ██ ██    ██    ██    ██      ██    ██ ██ ██  ██ ██ "
    echo "  ██████  ██   ██  ██████     ██    ███████  ██████  ██ ██   ████ "
    echo -e "${NC}"
    echo "  ==================================================================="
    echo -e "  ${GREEN}BrutePin v1.1${NC} - Interactive PIN BruteForce - By: Aniipid | $(detect_platform)"
    echo "  ==================================================================="
    echo ""
}

build_engine() {
    local eng="$1"
    case "$eng" in
        c)   make -C "$PROJECT_DIR/core" -s 2>/dev/null; echo "$PROJECT_DIR/core/brutepin" ;;
        cpp) make -C "$PROJECT_DIR/cpp" -s 2>/dev/null; echo "$PROJECT_DIR/cpp/brutepin-cpp" ;;
        rust) cd "$PROJECT_DIR/rustcore" && cargo build --release --quiet 2>/dev/null
              echo "$PROJECT_DIR/rustcore/target/release/brutepin-rust" ;;
    esac
}

run_engine() {
    local mode="$1" engine="$2" cmd
    cmd=$(build_engine "$engine")
    [[ -z "$cmd" ]] && { echo -e "${RED}[-] Engine unavailable${NC}"; return; }
    cmd="$cmd $mode"
    [[ $DELAY -ne 3 ]] && cmd+=" -d $DELAY"
    [[ -n "$START" ]] && cmd+=" -s $START"
    [[ -n "$END" ]] && cmd+=" -e $END"
    [[ $RESUME -eq 1 ]] && cmd+=" -r"
    [[ -n "$WORDLIST" ]] && cmd+=" -f $WORDLIST"
    echo -e "${CYAN}$ ${cmd}${NC}\n"
    eval "$cmd"; echo ""; read -rp "[Press Enter]"
}

menu() {
    adb_setup
    while true; do
        banner
        echo -e "${YELLOW}=== Brute Force ===${NC}"
        echo "1)  C       4-digit"
        echo "2)  C       6-digit"
        echo "3)  C       8-digit"
        echo "4)  C++     PIN"
        echo "5)  Rust    PIN"
        echo "6)  Bash    PIN (built-in)"
        echo "7)  Python  PIN (brute.py)"
        echo ""
        echo -e "${YELLOW}=== Tools ===${NC}"
        echo "w)  Wait USB"
        echo "d)  Devices"
        echo "i)  Device Info"
        echo "s)  Settings"
        echo "v)  Version"
        echo "q)  Quit"
        read -rp "> " opt
        case $opt in
            1) run_engine "4" "c" ;;
            2) run_engine "6" "c" ;;
            3) run_engine "8" "c" ;;
            4) read -rp "Mode (4/6/8): " m; run_engine "$m" "cpp" ;;
            5) read -rp "Mode (4/6/8): " m; run_engine "$m" "rust" ;;
            6) read -rp "Mode (4/6/8): " m
               if adb_check; then
                   echo; detect_device
                   read -rp "[*] Launch? (y/n): " ok
                   [[ "$ok" == "y" ]] && brute_pin "$m" "$DELAY" "${START:-0}" "${END:-0}" "$RESUME"
               else
                   echo -e "${RED}[-] No device${NC}"
               fi
               read -rp "" ;;
            7) python3 "$PROJECT_DIR/brute.py" --info; read -rp "" ;;
            w|W) adb_wait; read -rp "" ;;
            d|D) adb_list; read -rp "" ;;
            i|I) "$PROJECT_DIR/core/brutepin" --info 2>/dev/null || echo -e "${RED}[-] No device${NC}"; read -rp "" ;;
            s|S)
                clear; banner
                echo "1) Delay: $DELAY"
                echo "2) Start: ${START:-default}"
                echo "3) End: ${END:-default}"
                echo "4) Resume: $([ $RESUME -eq 1 ] && echo ON || echo OFF)"
                echo "5) Wordlist: ${WORDLIST:-not set}"
                echo "6) Generate 4-digit wordlist"
                echo "7) Generate 6-digit wordlist"
                echo "8) Generate 8-digit wordlist"
                read -rp "> " sopt
                case $sopt in
                    1) read -rp "Delay: " DELAY; DELAY=${DELAY:-3} ;;
                    2) read -rp "Start: " START ;;
                    3) read -rp "End: " END ;;
                    4) RESUME=$((1-RESUME)) ;;
                    5) read -rp "Wordlist: " WORDLIST ;;
                    6) seq -w 0 9999 > "$PROJECT_DIR/4digit.txt"; echo "Done" ;;
                    7) seq -w 0 999999 > "$PROJECT_DIR/6digit.txt"; echo "Done" ;;
                    8) seq -w 0 99999999 > "$PROJECT_DIR/8digit.txt"; echo "Done" ;;
                esac ;;
            v|V) "$PROJECT_DIR/core/brutepin" --version; read -rp "" ;;
            q|Q) exit 0 ;;
        esac
    done
}

menu
