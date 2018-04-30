#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef BGQ
#include <hwi/include/bqc/A2_inlines.h>
#else
#define GetTimeBase MPI_Wtime
#endif

// Global variables
char *command, *p;
int ARRAY_SIZE;
int ELEMENTS_PER_RANK;
unsigned long long start_cycles = 0;
unsigned long long end_cycles = 0;
double processor_frequency = 1600000000.0;

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

void* findMin_thread(void *arg) {
	struct find_min_args args = *(struct find_min_args*)arg;
	int *inputArray = args.arr;
	int size_ = args.size;

	int min = findMin(inputArray,size_);
	int *minptr = &min;

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

    command = argv[1];
    srand(time(NULL));

    if (argv[3]) {
    	p = argv[3];
    }

    if (strcmp(command,"min")==0) {

    	
    	if (rank == 0) {
    		printf("Finding min with %d MPI ranks...\n",size);
    		start_cycles = GetTimeBase();
    	}

    	ARRAY_SIZE = atoi(argv[2]);
    	ELEMENTS_PER_RANK = ARRAY_SIZE/size;
    	int mymin;

    	if (rank == 0) {
    		int array[ARRAY_SIZE];
    		int myarray[ELEMENTS_PER_RANK];
	    	for  (int i = 0; i < ARRAY_SIZE; i++) {
	    		array[i] = rand();
	    		if (i < ELEMENTS_PER_RANK) {
	    			myarray[i] = array[i];
	    		}
	    	}

	    	int cnt = ELEMENTS_PER_RANK;
	    	pthread_t tid[atoi(argv[4])];


	    	for (int i = 1; i < size; ++i) {
	    		int subArray[ELEMENTS_PER_RANK];
	    		int subIndex = 0;

	    		// Fill subarray
	    		for (int j = 0; j < ELEMENTS_PER_RANK; ++j) {
	    			subArray[subIndex] = array[cnt];
	    			cnt++;
	    			subIndex++;
	    		}

	    		if (strcmp(p,"thread")==0) {

	    			struct find_min_args *info = (struct find_min_args*) calloc(1,sizeof(struct find_min_args));
	    			info->arr = subArray;
	    			info->size = ELEMENTS_PER_RANK;
	    			int rc = pthread_create(&tid[i],NULL,findMin_thread,(void*)info);

	    			if (rc != 0) {
	    				fprintf(stderr, "Main thread could not create child thread (%d)\n", rc);
	    				return EXIT_FAILURE;
	    			}
	    		} else {
	    			MPI_Request request;
	    			MPI_Isend(subArray,ELEMENTS_PER_RANK,MPI_INT,i,123,MPI_COMM_WORLD,&request);
	    		}
	    		
	    	}

	    	mymin = findMin(myarray,ELEMENTS_PER_RANK);
	    	printf("rank %d's min is %d\n", rank, mymin);

	    	if (strcmp(p,"thread")==0) {
	    		for (int i=1; i < size; ++i) {
		    		int *minCandidate;
		    		pthread_join(tid[i],(void**)&minCandidate);
		    		if (mymin < *minCandidate) {
		    			mymin = *minCandidate;
		    		}
		    	}
		    	printf("The minimum of the array is (using threads): %d\n", mymin);
		    	end_cycles = GetTimeBase();

		    	double time_in_secs = ((double)(end_cycles - start_cycles)) / processor_frequency;
			    if (rank == 0) {
			    	printf("The algorithm took %lf seconds\n", time_in_secs);
			    }
			    
			    MPI_Finalize();
			    return 0;
	    	}

	    	
	    	
	    } else {
	    	MPI_Status status;
	    	MPI_Request request;

	    	int array_chunk[ELEMENTS_PER_RANK];
	    	MPI_Irecv(array_chunk,ELEMENTS_PER_RANK,MPI_INT,0,123,MPI_COMM_WORLD,&request);
	    	MPI_Wait(&request,&status);

	    	mymin = findMin(array_chunk,ELEMENTS_PER_RANK);
	    	printf("rank %d's min is %d\n", rank, mymin);
    	}	

    	// Ranks send their minimum candidates to rank 0
    	if (rank != 0) {
    		MPI_Request request;
    		MPI_Isend(&mymin,1,MPI_INT,0,123,MPI_COMM_WORLD,&request);
    	}

    	MPI_Barrier(MPI_COMM_WORLD);

    	if (rank == 0) {
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
    		end_cycles = GetTimeBase();
    	}

    }

    MPI_Barrier(MPI_COMM_WORLD);
    double time_in_secs = ((double)(end_cycles - start_cycles)) / processor_frequency;
    if (rank == 0) {
    	printf("The algorithm took %lf seconds\n", time_in_secs);
    }
    
    MPI_Finalize();
    return 0;
    
}