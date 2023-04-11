#pragma once

#define LOG_INF(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#define LOG_WRN(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)
#define LOG_ERR(format, ...) Serial.printf(format "\n", ##__VA_ARGS__)

char *url_decode(char *str);
char *url_encode(char *str);
char from_hex(char ch);
char to_hex(char code);

void doRestart(const char *reason);
void debugMemory(const char *caller);
