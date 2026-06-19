#ifndef PATTERN_AI_H
#define PATTERN_AI_H

#ifdef __cplusplus
extern "C" {
#endif

int ai_predict_pins(const char *device_json, char results[][16], int max);
int ai_generate_sequence(int digits, int count, char results[][16]);

#ifdef __cplusplus
}
#endif

#endif
