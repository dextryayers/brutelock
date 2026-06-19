#ifndef PIN_AI_H
#define PIN_AI_H

#ifdef __cplusplus
extern "C" {
#endif

void pin_ai_init(const char *serial, const char *imei1, const char *imei2,
                 const char *phone, const char *build_fp,
                 const char *model, int vid, int pid);

int pin_ai_next(char *pin, int max_digits);
int pin_ai_score(const char *pin, int len);
void pin_ai_reset(void);
void pin_ai_set_length(int digits);
long pin_ai_count(void);
int pin_ai_top_n(char pins[][16], int max_pins);
void pin_ai_feedback(const char *pin, int was_unlock_attempt);
void pin_ai_add_pattern(int (*gen)(char *out, int max, int len), int score);
void pin_ai_get_probs(double probs[][10], int len);

#ifdef __cplusplus
}
#endif

#endif
