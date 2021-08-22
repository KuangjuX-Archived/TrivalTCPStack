#include "utils.h"

float max(float a, float b) {
    return a > b ? a : b;
}

int push(int *arr,int const len,int value){
    int i;
    for(i=0;i<len;i++){
        if(arr[i]==-1) {
            arr[i]=value;
            break;
        }
    }
    return i!=len;
}

int pop(int *arr,int const len){
    int i;
    int return_val=arr[0];
    for(i=0;arr[i]!=-1&&i<len;i++){
        arr[i]=arr[i+1];
    }
    return return_val;
}