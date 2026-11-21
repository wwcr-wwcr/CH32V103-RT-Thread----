#ifndef BOARD_CH422_H
#define BOARD_CH422_H

#ifdef __cplusplus
extern "C" {
#endif
/* 四位数码管，小数点附着在前一位。例 CH422_Show("%04d", 23)。 */
int CH422_Show(const char *format, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
#endif
