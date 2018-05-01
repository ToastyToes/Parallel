#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

#include "bucketSort.c"
#include "dijkstra2.c"

#ifdef BGQ
#include <hwi/include/bqc/A2_inlines.h>
#else
#define GetTimeBase MPI_Wtime
#endif

// Global variables
char *command, *p;
int ARRAY_SIZE;
int ELEMENTS_PER_RANK, ELEMENTS_PER_THREAD, NUM_THREADS;
double start_cycles = 0;
double end_cycles = 0;
double processor_frequency = 1600000000.0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct find_min_args {
    int *arr;
    int size;
};


// O(n) algorithm
// Returns min of array
int findMin(int *arr, int size) {

    /* 
        Have multiple MPI ranks call this function and 
        send the min they find to rank 0

        Rank 0 can also call this function and find its own
        min from its portion
    */

    if (size <= 0) {
        return -1;
    }

    int min = arr[0];
    for (int i = 1; i < size; ++i) {
        if (arr[i] < min) {
            min = arr[i];
        }
    }

    return min;
}

// Thread helper function for findMin
void* findMin_thread(void *arg) {
    struct find_min_args args = *(struct find_min_args*)arg;
    int *inputArray = args.arr;
    int size_ = args.size;

    int min = findMin(inputArray,size_);
    int *minptr = (int*)malloc(sizeof(int));
    *minptr = min;

    free(inputArray);

    return (void*)minptr;
}



int main(int argc, char** argv) {
    MPI_Init(&argc,&argv);
    int rank,size;

    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    start_cycles = MPI_Wtime();
    srand(time(NULL));

    if (argv[2]) {
        NUM_THREADS = atoi(argv[2]);
    } else {
        NUM_THREADS = 0;
    }

    int *array, *myarray;
    

    if (rank == 0) {
        printf("Finding min with MPI rank 0...\n");
    }

    ARRAY_SIZE = atoi(argv[1]);
    // printf("here\n");
    ELEMENTS_PER_RANK = ARRAY_SIZE/size;
    
    int mymin = INT_MAX;
    myarray = (int*)malloc(ELEMENTS_PER_RANK*sizeof(int));
    pthread_t tid[NUM_THREADS];

    if (NUM_THREADS) {
        ELEMENTS_PER_THREAD = ELEMENTS_PER_RANK/NUM_THREADS;
    } else {
        ELEMENTS_PER_THREAD = 0;
    }

    // printf("here2\n");

    if (rank == 0) {
        // printf("here3\n");
        // Create array to find minimum of 
        array = (int*)malloc(atoi(argv[1])*sizeof(int));
        if (array==NULL) {
            perror("Error: ");
            return (-1);
        }
        // printf("here3.0\n");
        for  (int i = 0; i < ARRAY_SIZE; i++) {
            // printf("%d\n", i);
            // printf("here3.01 %d %d\n",i,array == NULL);
            array[i] = rand();
            // printf("here3.02\n");
            if (i < ELEMENTS_PER_RANK) {
                // printf("here3.03\n");
                myarray[i] = array[i];
                // printf("here3.04\n");
            }
        }
        // printf("here3.1\n");
        int cnt = ELEMENTS_PER_RANK;
        int *subArray; 
        
        // Create subarray for each rank
        for (int i = 1; i < size; ++i) {
            subArray = (int*)malloc(ELEMENTS_PER_RANK*sizeof(int));

            // Fill subarray
            for (int j = 0; j < ELEMENTS_PER_RANK; ++j) {
                subArray[j] = array[cnt];
                cnt++;
            }

            // Send array chunk to other ranks
            MPI_Request request;
            MPI_Isend(subArray,ELEMENTS_PER_RANK,MPI_INT,i,123,MPI_COMM_WORLD,&request);

        }


    } else {
        MPI_Status status;
        MPI_Request request;
        // printf("here3.5\n");

        // Wait to receive array chunk from rank 0
        MPI_Irecv(myarray,ELEMENTS_PER_RANK,MPI_INT,0,123,MPI_COMM_WORLD,&request);
        MPI_Wait(&request,&status);

    }   
    MPI_Barrier(MPI_COMM_WORLD);

    if (NUM_THREADS) {
        int cnt = 0;

        // Create threads and send info to them
        for (int i = 0; i < NUM_THREADS; ++i) {
            struct find_min_args *info = (struct find_min_args*)calloc(1,sizeof(struct find_min_args));
            int *threadArray = (int*)malloc(ELEMENTS_PER_THREAD*sizeof(int));
            for (int j = 0; j < ELEMENTS_PER_THREAD; ++j) {
                threadArray[j] = myarray[cnt];
                cnt++;
            }

            info->arr = threadArray;
            info->size = ELEMENTS_PER_THREAD;

            int rc = pthread_create(&tid[i],NULL,findMin_thread,(void*)info);

            if (rc != 0) {
                fprintf(stderr, "Main thread could not create child thread (%d)\n", rc);
                return EXIT_FAILURE;
            }
        }

        // Join threads and find resulting min
        for (int i=0; i < NUM_THREADS; ++i) {
            void *status;
            int rc = pthread_join(tid[i],&status);
            if (rc != 0) {
                perror("pthread_join failed!");
                exit(EXIT_FAILURE);
            }

            // printf("minCandidate %d vs mymin %d\n", *(int*)status,mymin);
            if (*(int*)status < mymin) {
                mymin = *(int*)status;
            }

            free(status);
        }   

    } else {
        // printf("here4\n");
        mymin = findMin(myarray,ELEMENTS_PER_RANK);
    }
        
    printf("rank %d's min is %d\n", rank, mymin);

    // Ranks send their minimum candidates to rank 0
    if (rank != 0) {
        MPI_Request request;
        MPI_Isend(&mymin,1,MPI_INT,0,123,MPI_COMM_WORLD,&request);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        // printf("here5\n");
        int candidate;
        for (int i = 1; i < size; ++i) {
            MPI_Request request;
            MPI_Status status;

            MPI_Irecv(&candidate,1,MPI_INT,i,123,MPI_COMM_WORLD,&request);
            MPI_Wait(&request,&status);

            if (candidate < mymin) {
                mymin = candidate;
            }
        }

        printf("The minimum of the array (using MPI) is: %d\n", mymin);
        
    }

    
    end_cycles = MPI_Wtime();
    MPI_Barrier(MPI_COMM_WORLD);
    //double time_in_secs = ((double)(end_cycles - start_cycles));
    // printf("%f %f\n", start_cycles, end_cycles);
    if (rank == 0) {
        printf("findmin took %f seconds\n", end_cycles - start_cycles);
        free(array);
        free(myarray);
    }
    

    //===============================================================================
    // Dijkstra
    //===============================================================================
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    int *local_matrix, *local_dist, *local_pred;
    int local_n;
    MPI_Comm comm;
    MPI_Datatype block_t;
    int n = ARRAY_SIZE;
    local_n = n/size;
    local_matrix = malloc(n*local_n*sizeof(int));
    local_dist = malloc(n*local_n*sizeof(int));
    local_pred = malloc(n*local_n*sizeof(int));
    comm = MPI_COMM_WORLD;
    block_t = Build_block(n, local_n);
    
    scatter_matrix(local_matrix, n, local_n, block_t, rank, comm);
    if(rank == 0){
        printf("Scattered, starting Dijkstra\n");
        start_cycles = MPI_Wtime();

    }
    Dijkstra(local_matrix, local_dist, local_pred, n, local_n, rank, comm);
    if(rank == 0){
        end_cycles = MPI_Wtime();
        double time_in_secs = ((double)(end_cycles - start_cycles));
        printf("Finished Dijkstra in %lf seconds\n", time_in_secs);
    }
    
    //Print_dists(local_dist, n, local_n, rank, comm);
    //Print_paths(local_pred, n, local_n, rank, comm);
    
    free(local_matrix);
    free(local_dist);
    free(local_pred);

    MPI_Type_free(&block_t);
    
    // ===============================================================================
    //Bucket Sort
    // ===============================================================================
    MPI_Barrier(MPI_COMM_WORLD);
    int *arr = (int*) calloc(ARRAY_SIZE/size, sizeof(int));
    for (int n = 0; n < ARRAY_SIZE/size; ++n){
        arr[n] = rand();
    }
    start_cycles = MPI_Wtime();
    bucketSort(arr, ARRAY_SIZE/size, rank, size, NUM_THREADS);
    end_cycles = MPI_Wtime();
    if (rank == 0){
        printf("Bucket Sort took %f seconds\n", end_cycles - start_cycles);
    }

    return 0;
    
}
