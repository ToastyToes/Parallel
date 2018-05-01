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

// O(n log n) algorithm
void mergeSort() {
	/* 
		Have multiple MPI ranks call this function
		after getting their portions of the array
	*/
}


// O(n^2) algorithm
void dijsktra() {
	/*
		
	*/
}


int main(int argc, char** argv) {
	MPI_Init(&argc,&argv);
	int rank,size;

    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    start_cycles = MPI_Wtime();
    command = argv[1];
    srand(time(NULL));

    if (argv[3]) {
    	NUM_THREADS = atoi(argv[3]);
    } else {
    	NUM_THREADS = 0;
    }

    int *array, *myarray;
    if (strcmp(command,"min")==0) {

    	if (rank == 0) {
    		printf("Finding min with MPI rank 0...\n");
    	}

    	ARRAY_SIZE = atoi(argv[2]);
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
    		array = (int*)malloc(atoi(argv[2])*sizeof(int));
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

    }
    end_cycles = MPI_Wtime();
    MPI_Barrier(MPI_COMM_WORLD);
    double time_in_secs = ((double)(end_cycles - start_cycles));
    // printf("%f %f\n", start_cycles, end_cycles);
    if (rank == 0) {
    	printf("The algorithm took %f seconds\n", end_cycles - start_cycles);
        free(array);
        free(myarray);
    }

    

    MPI_Finalize();
    return 0;
    
}