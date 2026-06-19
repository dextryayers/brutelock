#ifndef PIN_ML_H
#define PIN_ML_H

#ifdef __cplusplus
extern "C" {
#endif

int ml_predict_pins(const char *info_json, int digits, char pins[][16], int max);
int ml_score_pin(const char *pin);
int ml_sequence_to_pins(const double probs[10], int digits, char pins[][16], int max);
void ml_set_frequency_data(const int freq[10]);
int ml_predict_bootloop_pins(int digits, char pins[][16], int max);

#ifdef __cplusplus
}
#endif

#endif
