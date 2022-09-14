#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

/*Structure to store the parameters required to specify a request*/
struct node{
    int size;   
    int duration;
    struct node *next;
    int process_number;
    struct timeval arrival_time;
};

/*Structure to store the parameters to be passed to the process simulator threads*/
struct arguments{
    int duration;
    int mem_start_idx;
    int mem_size;
    int process_number;
};

int p, q, n, m, t;  /* Different input parameters required for the simulation */
int T;  /* Total execution time after which simulation should terminate. */
int algo_choice;    /* Integer to specify the choice of memory-allocation algorithm */
double r;   /* Process arrival rate */

pthread_t p_thr_id;  /* Thread ID of the request producer thread. */
pthread_t ma_thr_id;    /* Thread ID of the memory allocator thread. */
int count = 1;  /* Counter to obtain the number of the currently added request */

struct node *queue_front = NULL, *queue_rear = NULL;    /*Front and Rear of the request queue */
int *memory = NULL; /*Stores the physical memory */
int num_memory_cells;   

/* To be used in next-fit algorithm */
int next_idx_of_last_allocated = 0;  /* Index of the location just after the memory allocated for the previously allocated request */

int total_allocated_processes = 0;   /* Total number of processes which are allocated memory during the execution */
double total_turnaround_time = 0;    /* Total turnaround time for all the processes, which are allocated memory during execution */

pthread_mutex_t mutex;  /* Mutex lock */

pthread_cond_t cond_queue;    /* Conditional Variable */
pthread_cond_t cond_memory;    /* Conditional Variable */

/* Generate a random double from 0 to 1 */
double random_double(){
    return ((double)rand())/((double)RAND_MAX);
}

/* Generate a random double from a to b */
double random_double_interval(double a, double b){
    return random_double() * (b - a) + a;
}

/* Generate a random integer from a to b */
int random_integer_interval(int a, int b){
    return (rand()%(b - a)) + a;
}

void log_msg(const char *msg, bool terminate) {
    printf("%s\n", msg);
    if (terminate) exit(-1); /* failure */
}

/**
 * Function to add a request at the rear end of the queue.
 * @param front Double pointer to the front of the queue.
 * @param rear Double pointer to the rear of the queue.
 * @param s Size of the process, in the current request.
 * @param d Duration of the process, in the current request.
 */
void enQueue(struct node **front, struct node **rear, int s, int d){
    struct node *newNode = (struct node*)malloc(sizeof(struct node));
    newNode->size = s;
    newNode->duration = d;
    newNode->process_number = count;
    newNode->next =NULL;
    gettimeofday(&(newNode->arrival_time), NULL);

    if (*front == NULL && *rear == NULL) {
        *front = newNode; *rear = newNode;
    }
    else {
        (*rear)->next = newNode;
        *rear = newNode;
    }
    printf("Request is added to the queue for process %d, with size = %d and duration = %d\n", count, s, d);
    count += 1;
}

/**
 * Function to delete a request, from the front end of the queue.
 * @param front Double pointer to the front of the queue.
 * @param rear Double pointer to the rear of the queue.
 */
void deQueue(struct node **front, struct node **rear){
    if ( *front != NULL && *rear != NULL) {
        if( *front == *rear){
            free( *front);
            *front = NULL;
            *rear = NULL;
        }else{
            struct node *temp = (*front)->next;
            (*front)->next = NULL;
            free( *front );
            *front = temp;
        }
    }
}

/**
 * Function to handle signals SIGALRM and SIGINT.
 * @param signum To differentiate between the type of signal(SIGALRM or SIGINT)
 */
void sig_handler(int signum){
    pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */

    /* Calculating the % memory utilization */
    int cnt_occ = 0;
    for(int i = 0; i < num_memory_cells; i++){
        if(memory[i] == 1)
            cnt_occ += 1;
    }
    double memory_util_perc = ((cnt_occ * 10 + q) * 100.0)/p;

    /* Calculating the average turnaround time */
    double avg_turnaround_time = total_turnaround_time/total_allocated_processes;
    
    free(memory);
    while (queue_front != NULL && queue_rear != NULL)
    {
        deQueue(&queue_front, &queue_rear);    
    }
    
    if(signum == SIGALRM)
        log_msg("\nTotal allowed execution time has been reached. Program terminating...", false);
    if(signum == SIGINT)
        log_msg("\nExecution interrupted by the user. Program terminating...", false);

    printf("Memory utilization = %lf %%\n", memory_util_perc);
    printf("Average turn-around time = %lf sec\n", avg_turnaround_time);
    log_msg("Program Terminated.", true);   /* Terminating the program by passing 'true' to the log_msg() function */

    pthread_mutex_unlock(&mutex);   /* Releasing the mutex lock */
}

/**
 * Function to simulate the execution of a process request, by holding onto the allocated memory for the process duration.
 */
void* process_execution_simulator(void *parameter){
    struct arguments *para = (struct arguments*)(parameter);
    sleep(para->duration);  /* Sleep for the time duration of the process */
    pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */

    /* Releasing the memory */
    for(int i = para->mem_start_idx; i < para->mem_start_idx + para->mem_size; i++){
        memory[i] = 0;
    }
    printf("Process %d has released the memory\n", para->process_number);

    pthread_cond_broadcast(&cond_memory); /* Broadcasting a signal to all the threads waiting on the cond_memory variable */
    pthread_mutex_unlock(&mutex); /* Releasing the mutex lock */
    free(para);
    pthread_exit(NULL);
    return NULL;
}

/**
 * Function to generate and add requests to the queue.
 * @param dummy This argument is just to ensure the compatability of the defined function with the expected signature.
 */
void* req_producer_thr(void * dummy){
    int s, d; /*Process size s and process duration d */
    int rhalt_sec, rhalt_nsec;
    
    rhalt_sec = (int)(1/r);
    rhalt_nsec = (1/r - rhalt_sec) * 1000000000;
    struct timespec halt_time;
    halt_time.tv_sec = rhalt_sec;
    halt_time.tv_nsec = rhalt_nsec;

    /* This is to ensure that the process size is in between the given range and is a multiple of 10MB */
    int l_limit_size, u_limit_size, l_limit_duration, u_limit_duration;
    l_limit_size = (int)(ceil((0.5 * m)/ 10) * 10);
    u_limit_size = (int)(floor((3.0 * m)/ 10) * 10);

    /* This is to ensure that the process duration is in between the given range and is a multiple of 5 seconds */
    l_limit_duration = (int)(ceil((0.5 * t)/ 5) * 5);
    u_limit_duration = (int)(floor((6.0 * t)/ 5) * 5);
    while(true){
        s = random_integer_interval(l_limit_size/10, u_limit_size/10) * 10;    /* Size in MB */
        d = random_integer_interval(l_limit_duration/5, u_limit_duration/5) * 5;    /* Duration in seconds */
        pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */
        enQueue(&queue_front, &queue_rear, s, d);   /* Adding the request to the queue */
        pthread_cond_broadcast(&cond_queue);    /* Broadcasting a signal to all the threads waiting on the cond_queue variable */    
        pthread_mutex_unlock(&mutex); /* Releasing the mutex lock */
        nanosleep(&halt_time, NULL);                   
    }
    return NULL;
}

/**
 * Function to allocate the requested memory to the request at the front of the queue, using first-fit algorithm.
 */
void allocate_using_first_fit(){
    int mem_req = (queue_front->size)/10;   
    bool canAllocate = false;
    while(!canAllocate){
        int cur_available_mem = 0;
        int mem_start_idx = 0;
        for(int i = 0; i < num_memory_cells; i++){
            if(memory[i] == 0){ // Available memory
                cur_available_mem += 1;
            }else{
                cur_available_mem = 0;
                mem_start_idx = i + 1;
            }
            if(cur_available_mem == mem_req){
                canAllocate = true;
                break; 
            }
        }
        if(canAllocate){
            
            printf("Memory is allocated to process %d\n", queue_front->process_number);
            for(int i = mem_start_idx; i < mem_start_idx + mem_req; i++){
                memory[i] = 1;  /* Marked the memory as allocated */
            }

            /*Calculating the time between the request generation and memory allocation to it */
            double time_taken;
            struct timeval cur_time;
            gettimeofday(&cur_time, NULL);
            time_taken = (cur_time.tv_sec - (queue_front->arrival_time).tv_sec) * 1e6;
            time_taken = (time_taken + (cur_time.tv_usec - (queue_front->arrival_time).tv_usec)) * 1e-6;
            total_turnaround_time += time_taken;
            total_allocated_processes += 1;
 
            /*Create a thread which will simulate the process execution */
            pthread_t thr_id;
            struct arguments *para = (struct arguments*)malloc(sizeof(struct arguments));
            para->duration = queue_front->duration;
            para->mem_start_idx = mem_start_idx;
            para->mem_size = mem_req;
            para->process_number = queue_front->process_number;

            deQueue(&queue_front, &queue_rear);   /* Remove the request from the queue */

            int rc = pthread_create(&thr_id, NULL, process_execution_simulator, (void *) para);
            if (rc) {
                log_msg("Failed to create the process simulator thread.", true);
            }
            break;
        }else{
            pthread_cond_wait(&cond_memory, &mutex); /* Waiting on the conditional variable cond_memory */
        }
    }   
}

/**
 * Function to allocate the requested memory to the request at the front of the queue, using best-fit algorithm.
 */
void allocate_using_best_fit(){
    int mem_req = (queue_front->size)/10;
    bool canAllocate = false;
    while(!canAllocate){
        int cur_available_mem = 0;
        int mem_start_idx = 0;
        int final_mem_start_idx, final_cur_available_memory = INT_MAX;
        for(int i = 0; i < num_memory_cells; i++){
            if(memory[i] == 0){ /* Available memory */
                cur_available_mem += 1;
            }else{
                if(cur_available_mem >= mem_req){
                    canAllocate = true;
                    if(cur_available_mem < final_cur_available_memory){
                        final_cur_available_memory = cur_available_mem;
                        final_mem_start_idx = mem_start_idx;
                    }
                }
                cur_available_mem = 0;
                mem_start_idx = i + 1;
            }
        }
        if(cur_available_mem >= mem_req){
            canAllocate = true;
            if(cur_available_mem < final_cur_available_memory){
                final_cur_available_memory = cur_available_mem;
                final_mem_start_idx = mem_start_idx;
            }
        }
        if(canAllocate){
            printf("Memory is allocated to process %d\n", queue_front->process_number);
            for(int i = final_mem_start_idx; i < final_mem_start_idx + mem_req; i++){
                memory[i] = 1;  /* Marked the memory as allocated */
            } 
            
            /*Calculating the time between the request generation and memory allocation to it */
            double time_taken;
            struct timeval cur_time;
            gettimeofday(&cur_time, NULL);
            time_taken = (cur_time.tv_sec - (queue_front->arrival_time).tv_sec) * 1e6;
            time_taken = (time_taken + (cur_time.tv_usec - (queue_front->arrival_time).tv_usec)) * 1e-6;
            total_turnaround_time += time_taken;
            total_allocated_processes += 1;

            for(int i = final_mem_start_idx; i < final_mem_start_idx + mem_req; i++){
                memory[i] = 1;  /* Marked the memory as allocated */
            }           
            /*Create a thread which will simulate the process execution */
            pthread_t thr_id;
            struct arguments *para = (struct arguments*)malloc(sizeof(struct arguments));
            para->duration = queue_front->duration;
            para->mem_start_idx = final_mem_start_idx;
            para->mem_size = mem_req;
            para->process_number = queue_front->process_number;
            deQueue(&queue_front, &queue_rear);   /* Remove the request from the queue */
            int rc = pthread_create(&thr_id, NULL, process_execution_simulator, (void *) para);
            if (rc) {
                log_msg("Failed to create the process simulator thread.", true);
            }
            break;
        }else{
            pthread_cond_wait(&cond_memory, &mutex);
        }
    }
}

/**
 * Function to allocate the requested memory to the request at the front of the queue, using next-fit algorithm.
 */
void allocate_using_next_fit(){
    int mem_req = (queue_front->size)/10;
    bool canAllocate = false;
    while(!canAllocate){
        int cur_available_mem = 0;
        int mem_start_idx = next_idx_of_last_allocated;
        for(int i = next_idx_of_last_allocated; i < num_memory_cells; i++){
            if(memory[i] == 0){ /* Available memory */
                cur_available_mem += 1;
            }else{
                cur_available_mem = 0;
                mem_start_idx = i + 1;
            }
            if(cur_available_mem == mem_req){
                canAllocate = true;
                break; 
            }
        }

        if(!canAllocate){
            cur_available_mem = 0;
            mem_start_idx = 0;
            for(int i = 0; i < num_memory_cells; i++){
                if(memory[i] == 0){ /* Available memory */
                    cur_available_mem += 1;
                }else{
                    cur_available_mem = 0;
                    mem_start_idx = i + 1;
                }
                if(cur_available_mem == mem_req){
                    canAllocate = true;
                    break; 
                }
            }
        }

        if(canAllocate){
            printf("Memory is allocated to process %d\n", queue_front->process_number);
            for(int i = mem_start_idx; i < mem_start_idx + mem_req; i++){
                memory[i] = 1;  /* Marked the memory as allocated */
            }   

            /*Calculating the time between the request generation and memory allocation to it */
            double time_taken;
            struct timeval cur_time;
            gettimeofday(&cur_time, NULL);
            time_taken = (cur_time.tv_sec - (queue_front->arrival_time).tv_sec) * 1e6;
            time_taken = (time_taken + (cur_time.tv_usec - (queue_front->arrival_time).tv_usec)) * 1e-6;
            total_turnaround_time += time_taken;
            total_allocated_processes += 1;
 
            /*Create a thread which will simulate the process execution */
            pthread_t thr_id;
            struct arguments *para = (struct arguments*)malloc(sizeof(struct arguments));
            para->duration = queue_front->duration;
            para->mem_start_idx = mem_start_idx;
            para->mem_size = mem_req;
            para->process_number = queue_front->process_number;
            deQueue(&queue_front, &queue_rear);   /* Remove the request from the queue */
            next_idx_of_last_allocated = (mem_start_idx + mem_req) % num_memory_cells;
            int rc = pthread_create(&thr_id, NULL, process_execution_simulator, (void *) para);
            if (rc) {
                log_msg("Failed to create the process simulator thread.", true);
            }
            break;
        }else{
            pthread_cond_wait(&cond_memory, &mutex);
        }
    } 
}

/**
 * Function to invoke the correct memory allocation algorithm, based on user's choice. 
 */
void perform_allocation(){
    if(algo_choice == 1){
        allocate_using_first_fit();
    }else if(algo_choice == 2){
        allocate_using_best_fit();
    }else{
        allocate_using_next_fit();
    }
}

/**
 * Function to process the requests, by allocating them memory in FCFS fashion.
 * @param dummy This argument is just to ensure the compatability of the defined function with the expected signature.
 */
void* memory_allocator_thr(void *dummy){
    while(true){
        pthread_mutex_lock(&mutex); /* Acquiring the mutex lock */
        while (queue_front == NULL && queue_rear == NULL){
            /* If true, then we wait on the conditional variable cond */
            pthread_cond_wait(&cond_queue, &mutex);
        }
        perform_allocation();
        pthread_mutex_unlock(&mutex); /* Releasing the mutex lock */
    }
}

/**
 * This function creates the request generation thread and the memory allocation thread.
 */
void createThread(){
    int rc = pthread_create(&p_thr_id, NULL, req_producer_thr , NULL);
    if (rc) {
        log_msg("Failed to create the request producer thread.", true);
    }

    rc = pthread_create(&ma_thr_id, NULL, memory_allocator_thr , NULL);
    if (rc) {
        log_msg("Failed to create the memory allocator thread.", true);
    }
    pthread_join(p_thr_id, NULL);
}