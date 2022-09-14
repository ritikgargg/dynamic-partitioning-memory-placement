#include "all_functions.h" 

int main(int argc, char *argv[]) {
    if (argc != 8) {
        printf("Usage: %s p q n m t T choice\n",argv[0]);
        printf("where, \n");
        printf("p = Total physical memory(in MB) in the simulation.\n");
        printf("q = Memory(in MB) reserved for the operating system.\n");
        printf("n = Parameter to determine the process arrival rate.\n");
        printf("m = Parameter to determine the size of the process in a request.\n");
        printf("t = Parameter to determine the duration of the process in a request.\n");
        printf("T = The time(in seconds) after which the simulation should end.\n");
        printf("choice = A number from 1, 2 or 3, denoting one of the following memory placement algorithms ::\n");
        printf("\t1. First-fit\n");
        printf("\t2. Best-fit.\n");
        printf("\t3. Next-fit.\n");
        exit(-1);
    }
    //srand(time(NULL));
    
    p = atoi(argv[1]);
    q = atoi(argv[2]);
    n = atoi(argv[3]);
    m = atoi(argv[4]);
    t = atoi(argv[5]);
    T = atoi(argv[6]);
    algo_choice = atoi(argv[7]);
    r = random_double_interval(0.1 * n, 1.2 * n);
    num_memory_cells = ((p - q)/10);    /* 1 memory cell represents 10MB of memory */
    memory = (int *) malloc(sizeof(int) * num_memory_cells);
    for(int i = 0; i < num_memory_cells; i++)
        memory[i] = 0;
    
    printf("=====================Simulation=====================\n");
    printf("Parameters for simulation :: \n");
    printf("p = %d\n", p);
    printf("q = %d\n", q);
    printf("n = %d\n", n);
    printf("m = %d\n", m);
    printf("t = %d\n", t);
    printf("T = %d\n", T);
    printf("r = %lf\n", r);
    printf("\n");

    pthread_mutex_init(&mutex, NULL);   // Initializing the mutex
    pthread_cond_init(&cond_queue, NULL);   // Initializing the conditional variable
    pthread_cond_init(&cond_memory, NULL);   // Initializing the conditional variable
    signal(SIGALRM, sig_handler); // Register signal handler for SIGALRM
    signal(SIGINT,sig_handler); // Register signal handler for SIGINT
    alarm(T);

    createThread();
    pthread_mutex_destroy(&mutex);  // Destroying the mutex
    pthread_cond_destroy(&cond_queue);    // Destroying the conditional variable
    pthread_cond_destroy(&cond_memory);    // Destroying the conditional variable
    return 0;
}