#include "utils.h"

float max(float a, float b) {
    return a > b ? a : b;
}

int min(int a, int b) {
    return a < b ? a : b;
}

int min_among_3(int a,int b,int c){
    int temp=a<b?a:b;
    return temp < c ? temp : c;
}