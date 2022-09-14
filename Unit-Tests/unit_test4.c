#include "../all_functions.h"
#include <stdio.h>

int main(){
    int arr1[10] = {1,1,1,1,1,1,1,1,0,0};
    int arr2[10] = {1,1,1,1,1,1,1,1,1,1};
    memory = (int *)malloc(sizeof(int) * 10);
    for(int i = 0; i < 10; i++){
        memory[i] = arr1[i];
    }
    num_memory_cells = 10;
    enQueue(&queue_front, &queue_rear, 20, 10000);
    allocate_using_first_fit();
    bool flag = true;
    for(int i = 0; i < 10; i++){
        if(memory[i] != arr2[i]){
            flag = false;
            break;
        }
    }
    if(flag){
        printf("Test #4 passed\n");
    }else{
        printf("Test #4 failed\n");
    }
}
