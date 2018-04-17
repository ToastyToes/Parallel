/***************************************************************************/
/* Template for Asssignment 4/5 ********************************************/
/* Jon Harris, Chris George, Ben Gruber  ********************************************/
/***************************************************************************/

/***************************************************************************/
/* Includes ****************************************************************/
/***************************************************************************/
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>

#include "clcg4.c"


/***************************************************************************/
/* Defines *****************************************************************/
/***************************************************************************/

#define ALIVE 1
#define DEAD  0

/***************************************************************************/
/* Global Vars *************************************************************/
/***************************************************************************/

// You define these
double start_time,finish_time;
int ROWS_PER_RANK;
int NUM_RANKS, num_rows, num_ghost_rows;
int NUM_THREADS, THRESHOLD;
int GRID_SIZE = 16384;

/***************************************************************************/
/* Function Decs ***********************************************************/
/***************************************************************************/

// You define these
void* game_of_life(void *arg);

/***************************************************************************/
/* Function: Main **********************************************************/
/***************************************************************************/

int main(int argc, char *argv[])
{
//    int i = 0;
    int mpi_myrank;
    int mpi_commsize;
// Example MPI startup and using CLCG4 RNG
    MPI_Init( &argc, &argv);
    MPI_Comm_size( MPI_COMM_WORLD, &mpi_commsize);
    MPI_Comm_rank( MPI_COMM_WORLD, &mpi_myrank);

    if (mpi_myrank == 0) {
        start_time = MPI_Wtime();
    }

    NUM_THREADS = atoi(argv[1]);
    THRESHOLD = atoi(argv[2]);
    NUM_RANKS = mpi_commsize;
    ROWS_PER_RANK = GRID_SIZE/NUM_RANKS;
    num_ghost_rows = NUM_RANKS - 1;

    int *row = (int*)malloc(sizeof(int)*GRID_SIZE);
    int **universe = (int**)malloc(sizeof(row)*GRID_SIZE);
    
// Init 16,384 RNG streams - each rank has an independent stream
    InitDefault();
    
// Note, used the mpi_myrank to select which RNG stream to use.
// You must replace mpi_myrank with the right row being used.
// This just show you how to call the RNG.    
    // printf("Rank %d of %d has been started and a first Random Value of %lf\n", 
    //    mpi_myrank, mpi_commsize, GenVal(mpi_myrank));
    MPI_Barrier( MPI_COMM_WORLD );
    
// Insert your code
    if (NUM_THREADS) {
        // Create thread array
        pthread_t tid[GRID_SIZE];

        // Create each thread and call thread function game_of_life
        for (int i = 0; i < NUM_THREADS; ++i) {
            void *info;
            int rc = pthread_create(&tid[i],NULL,game_of_life,info);

            if (rc != 0) {
                fprintf(stderr,"Main thread could not create child thread (%d)\n",rc);
                return EXIT_FAILURE;
            }
        }

        // Join child threads with main thread
        for (int i = 0; i < NUM_THREADS; ++i) {
            void **thread_info;
            pthread_join(tid[i],thread_info);
            free(thread_info);
        }

    } else {
        // Corner case without pthreads
    }

// END -Perform a barrier and then leave MPI
    MPI_Barrier( MPI_COMM_WORLD );
    MPI_Finalize();
    return 0;
}

/***************************************************************************/
/* Other Functions - You write as part of the assignment********************/
/***************************************************************************/
void* game_of_life(void *arg) {

}