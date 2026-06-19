#ifndef ENGINE_BRIDGE_H
#define ENGINE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* hardware_probe bridge */
int hwprobe_run(void);

/* data_forensics bridge */
int forensics_extract(void);

/* exploit_chain bridge */
int chain_run(void);

/* neural_cracker bridge */
int neural_gen_pins(int digits, int count, char *out, int out_sz);

/* security_bypass bridge */
int secbypass_run(void);

/* auto_pwn_chain bridge */
int auto_pwn_run(void);
int auto_pwn_scan_full(char *out, int out_sz);

#ifdef __cplusplus
}
#endif

#endif
