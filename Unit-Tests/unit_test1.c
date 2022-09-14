#include "../all_functions.h"
#include <stdio.h>

int main(){
    enQueue(&queue_front, &queue_rear, 20, 5);
    if(queue_front != NULL && queue_rear != NULL && queue_rear->size == 20 && queue_rear->duration == 5){
        printf("Test #1 passed\n");
    }else{
        printf("Test #1 failed\n");
    }
}
