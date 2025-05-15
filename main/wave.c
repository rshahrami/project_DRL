#include <math.h>

#define SINE_TABLE_SIZE 2000    // اندازه جدول سینوسی
#define PI 3.14159265

double A = 1.0; // دامنه موج سینوسی (بهتر است double باشد)

// جدول سینوسی از پیش محاسبه شده
double sine_lookup_table[SINE_TABLE_SIZE];

// تابع ساخت جدول سینوسی
void create_sine_lookup_table() {
    for (int i = 0; i < SINE_TABLE_SIZE; i++) {
        sine_lookup_table[i] = A * sin(2.0 * PI * (double)i / SINE_TABLE_SIZE);
    }
}