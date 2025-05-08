#include <math.h>

#define PI 3.14159265

double wave(double *angle, double step){
    double out = sin(*angle);
    // printf("%f\n", sin(angle));
    *angle += step; 
    // printf("%f\n", *angle);
    if (*angle >= 2 * PI) { 
        *angle -= 2 * PI; 
    }
    return out;
}