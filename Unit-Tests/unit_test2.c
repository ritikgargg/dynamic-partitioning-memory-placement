#include "../all_functions.h"
#include <stdio.h>

int main(){
    enQueue(&queue_front, &queue_rear, 20, 5);
    deQueue(&queue_front, &queue_rear);
    if(queue_front == NULL && queue_rear == NULL){
        printf("Test #2 passed\n");
    }else{
        printf("Test #2 failed\n");
    }
}