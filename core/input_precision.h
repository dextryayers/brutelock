#ifndef INPUT_PRECISION_H
#define INPUT_PRECISION_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*  Result codes */
#define PRECISION_OK      0
#define PRECISION_FAIL   -1
#define PRECISION_LOCKOUT -2
#define PRECISION_NOPIN  -3

/*  Grid preset for digit coordinate calculation */
typedef enum {
    GRID_AOSP = 0,
    GRID_SAMSUNG,
    GRID_XIAOMI,
    GRID_OPPO,
    GRID_VIVO,
    GRID_HUAWEI,
    GRID_MOTOROLA,
    GRID_SONY,
    GRID_NOKIA,
    GRID_ASUS,
    GRID_TECNO,
    GRID_INFINIX,
    GRID_ADVAN,
    GRID_HONOR,
    GRID_AUTO
} GridPreset;

/*  PIN entry method flags */
#define METHOD_KEYEVENT    (1<<0)
#define METHOD_TAP         (1<<1)
#define METHOD_TEXT        (1<<2)

/*  Digit coordinate */
typedef struct { int x, y; } DigitCoord;

/*  Initialize precision engine with screen dimensions */
void prec_init(int screen_w, int screen_h);

/*  Set global grid preset (used by all enter_pin functions) */
void prec_set_grid(GridPreset g);

/*  Get current global grid preset */
GridPreset prec_get_grid(void);

/*  Auto-detect best grid preset based on brand/model string */
GridPreset prec_detect_grid(const char *vendor, const char *model);

/*  Get digit coordinates for a given preset */
DigitCoord prec_digit_coord(char digit, GridPreset preset);

/*  Build shell command script for PIN entry using specified methods */
int prec_build_script(char *buf, int buf_sz, const char *pin,
                      GridPreset preset, int methods);

/*  Enter PIN using best-effort multi-method approach.
    Returns PRECISION_OK on unlock, PRECISION_FAIL if still locked. */
int prec_enter_pin(const char *pin);

/*  Enter PIN with fast single-method (keyevent only).
    Returns PRECISION_OK on unlock. */
int prec_enter_pin_fast(const char *pin);

/*  Enter PIN with full sakti (5 presets × 2 attempts × 3 methods).
    Returns PRECISION_OK on unlock. */
int prec_enter_pin_sakti(const char *pin);

/*  Wake screen and swipe */
void prec_wake_and_swipe(void);

/*  Get current grid preset name */
const char* prec_preset_name(GridPreset p);

/*  Expert: Detect touchscreen input device, returns fd or -1 */
int prec_detect_input_devices(void);

/*  Expert: Send raw input event to device fd */
int prec_send_raw_event(int fd, unsigned short type, unsigned short code, unsigned int value);

/*  Expert: Enter PIN via direct /dev/input/event* writes (ultra-fast) */
int prec_enter_pin_turbo(const char *pin);

/*  Expert: Multi-tap batch PIN entry (all taps queued in one transaction) */
int prec_enter_pin_multitap(const char *pin);

/*  Expert: Draw swipe pattern from coordinate array */
int prec_swipe_pattern(const int *points, int npoints);

/*  Expert: Send single digit keyevent */
int prec_keyevent_digit(int digit);

/*  Expert: Ultra-fast list brute force with zero inter-PIN delay */
int prec_brute_force_pins(const char **pins, int npins, int *found_idx);

#ifdef __cplusplus
}
#endif
#endif
