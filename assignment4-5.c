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
int mpi_myrank;
int mpi_commsize;
int ROWS_PER_RANK, ROWS_PER_THREAD;
int NUM_RANKS, num_rows, num_ghost_rows;
int NUM_THREADS, THRESHOLD;
int GRID_SIZE = 16384;

struct thread_args{
    short (*universe)[];
    short start_row;
    short *top_ghost_row;
    short *bot_ghost_row;
};

/***************************************************************************/
/* Function Decs ***********************************************************/
/***************************************************************************/

// You define these
void* game_of_life(void *arg);
void tick();

/***************************************************************************/
/* Function: Main **********************************************************/
/***************************************************************************/

int main(int argc, char *argv[])
{
//    int i = 0;
    double start_time, finish_time;
    
// Example MPI startup and using CLCG4 RNG
    MPI_Init( &argc, &argv);
    MPI_Comm_size( MPI_COMM_WORLD, &mpi_commsize);
    MPI_Comm_rank( MPI_COMM_WORLD, &mpi_myrank);
    MPI_Request request, request2;
    MPI_Status status, status2;

    if (mpi_myrank == 0) {
        start_time = MPI_Wtime();
    }

    NUM_THREADS = atoi(argv[1]);
    THRESHOLD = atoi(argv[2]);
    NUM_RANKS = mpi_commsize;
    ROWS_PER_RANK = GRID_SIZE/NUM_RANKS;
    ROWS_PER_THREAD = ROWS_PER_RANK/NUM_THREADS;
    num_ghost_rows = NUM_RANKS - 1;
    
    // This rank's rows of the grid
    short (*universe)[GRID_SIZE] = malloc(ROWS_PER_RANK*GRID_SIZE*sizeof(short)); 

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

        //Initialize grid to random states
        for (int i = 0; i < ROWS_PER_RANK; ++i){
            for (int j = 0; j < GRID_SIZE; ++j){
                universe[i][j] = (GenVal(mpi_myrank) > .5) ? 1 : 0;
            }
        }
        // Create thread array
        pthread_t tid[NUM_THREADS];

        // Create each thread and call thread function game_of_life
        for (int i = 1; i < NUM_THREADS; ++i) {
            struct thread_args *info = (struct thread_args*) calloc(1, sizeof(struct thread_args));//possible memory issue from multiple of same variable?
            info->universe = universe;
            info->start_row = i*ROWS_PER_THREAD;
            info->top_ghost_row = NULL;
            if (i == NUM_THREADS-1 && mpi_myrank != mpi_commsize-1){
                // TODO: Figure out ghost rows
                MPI_Isend(universe, GRID_SIZE, MPI_SHORT,mpi_myrank-1, mpi_myrank-1, MPI_COMM_WORLD, &request);
                MPI_Irecv(top_ghost_row, GRID_SIZE, MPI_SHORT,mpi_myrank, mpi_myrank+1, MPI_COMM_WORLD, &request2);
                MPI_Wait(&request2, status2);
            }
            else 
                info->bot_ghost_row = NULL;
            if(i == 1 && mpi_myrank != 0){
                MPI_Irecv(bot_ghost_row, GRID_SIZE, MPI_SHORT,mpi_myrank, mpi_myrank-1, MPI_COMM_WORLD, &request);
                MPI_Isend(universe, GRID_SIZE, MPI_SHORT,mpi_myrank+1, mpi_myrank+1, MPI_COMM_WORLD, &request2);
                MPI_Wait(&request, status);

            }
            else{
                info->top_ghost_row = NULL
            }
            int rc = pthread_create(&tid[i],NULL,game_of_life,(void*)info);

            if (rc != 0) {
                fprintf(stderr,"Main thread could not create child thread (%d)\n",rc);
                return EXIT_FAILURE;
            }
        }

        struct thread_args *info = (struct thread_args*) calloc(1, sizeof(struct thread_args));
        info->universe = universe;
        info->start_row = 0;
        if (mpi_myrank != 0){
            // TODO: Figure out ghost rows
            //What to do here and why
            MPI_Irecv(&top_ghost_row, ROWS_PER_THREAD, MPI_INT,mpi_myrank-1, mpi_myrank, MPI_COMM_WORLD, &request2);
            MPI_Wait(&request2, status2);
        }
        else
            info->top_ghost_row = NULL;
        info->bot_ghost_row = NULL;
        game_of_life((void*)info);


        // Join child threads with main thread
        for (int i = 1; i < NUM_THREADS; ++i) {
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
    struct thread_args args = *(struct thread_args*)arg;
    
}

void tick(){

}
