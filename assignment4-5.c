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
int NUM_TICKS = 128;

struct thread_args{
    short (*universe)[];
    short (*new_universe)[];
    const short start_row;  //only way for thread to know its position inside universe
    short *ghost_row;       //only need one. if start_row = 0 then it's the main thread
                            // main thread ghost row is above, child thread ghost rows are below
};

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
    
// Example MPI startup and using CLCG4 RNG
    MPI_Init( &argc, &argv);
    MPI_Comm_size( MPI_COMM_WORLD, &mpi_commsize);
    MPI_Comm_rank( MPI_COMM_WORLD, &mpi_myrank);
    MPI_Request request, request2;
    MPI_Status status, status2;
    double start_time, finish_time;


    if (mpi_myrank == 0) {
        start_time = MPI_Wtime();
    }

    NUM_THREADS = atoi(argv[1]);
    THRESHOLD = atoi(argv[2]);

    NUM_RANKS = mpi_commsize;

    ROWS_PER_RANK = GRID_SIZE/NUM_RANKS;

    if (NUM_THREADS == 0) {
        fprintf(stderr, "Need to handle zero thread case\n");
    }
    ROWS_PER_THREAD = ROWS_PER_RANK/NUM_THREADS;



    num_ghost_rows = NUM_RANKS - 1;

    // This rank's rows of the grid
    short (*universe)[GRID_SIZE] = malloc(ROWS_PER_RANK*GRID_SIZE*sizeof(short));
    short (*new_universe)[GRID_SIZE] = malloc(ROWS_PER_RANK*GRID_SIZE*sizeof(short));
// Init 16,384 RNG streams - each rank has an independent stream
    InitDefault();

    
// Note, used the mpi_myrank to select which RNG stream to use.
// You must replace mpi_myrank with the right row being used.
// This just show you how to call the RNG.    
    // printf("Rank %d of %d has been started and a first Random Value of %lf\n", 
    //    mpi_myrank, mpi_commsize, GenVal(mpi_myrank));
    MPI_Barrier(MPI_COMM_WORLD);
    
// Insert your code
    if (NUM_THREADS) {

        //Initialize grid to random states
        for (int i = 0; i < ROWS_PER_RANK; ++i){
            for (int j = 0; j < GRID_SIZE; ++j){
                universe[i][j] = (GenVal(i) > .5) ? 1 : 0;
            }
        }
        //short *ghost_top = NULL, *ghost_bot = NULL;
        //send ghost rows
        // if (mpi_myrank != 0){
        //     //ghost_top = malloc(GRID_SIZE*sizeof(short));
        //     MPI_Isend(universe[0], GRID_SIZE, MPI_SHORT, mpi_myrank-1, mpi_myrank-1, MPI_COMM_WORLD, &request);
        // }
        // if (mpi_myrank != mpi_commsize-1){
        //     //ghost_bot = malloc(GRID_SIZE*sizeof(short));
        //     MPI_Isend(universe[ROWS_PER_RANK-1], GRID_SIZE, MPI_SHORT, mpi_myrank+1, mpi_myrank+1, MPI_COMM_WORLD, &request2);
        // }
        // Create thread array
        pthread_t tid[NUM_THREADS];

        // Create each thread and call thread function game_of_life
        for (int i = 1; i < NUM_THREADS; ++i) {
            //Create struct to pass arguments to threads
            struct thread_args *info = (struct thread_args*) calloc(1, sizeof(struct thread_args));//possible memory issue from multiple of same variable?
            info->universe = universe;
            info->new_universe = new_universe;
            *(short *)&info->start_row = i*ROWS_PER_THREAD;
            info->ghost_row = NULL;
            // printf("%d\n", i);
            if (i == NUM_THREADS-1 && mpi_myrank != mpi_commsize-1){
                short *ghost_row = (short *)calloc(GRID_SIZE,sizeof(short));
                MPI_Irecv(ghost_row, GRID_SIZE, MPI_SHORT,mpi_myrank+1, mpi_myrank, MPI_COMM_WORLD, &request);
                info->ghost_row = ghost_row;
            }
            else 
                info->ghost_row = NULL;
            
            int rc = pthread_create(&tid[i],NULL,game_of_life,(void*)info);

            if (rc != 0) {
                fprintf(stderr,"Main thread could not create child thread (%d)\n",rc);
                return EXIT_FAILURE;
            }
        }

        //Main thread runs the game too
        struct thread_args *info = (struct thread_args*) calloc(1, sizeof(struct thread_args));
        info->universe = universe;
        info->new_universe = new_universe;
        *(short *)&info->start_row = 0;
        // if (mpi_myrank != 0){
        //     short *ghost_row = (short *)calloc(GRID_SIZE,sizeof(short));
        //     MPI_Irecv(ghost_row, GRID_SIZE, MPI_INT, mpi_myrank-1, mpi_myrank, MPI_COMM_WORLD, &request2);
        //     info->ghost_row = ghost_row;
        // }
        // else
        //     info->ghost_row = NULL;
        
        game_of_life((void*)info);




        // Join child threads with main thread
        for (int i = 1; i < NUM_THREADS; ++i) {
                                              // ============================================
            // void **thread_info;                 // IDK how pthreads work do we need this part?
                // printf("here\n");
                // printf("here\n");

            pthread_join(tid[i],NULL);   // ============================================
                // printf("here\n");

            // free(thread_info);
        }// info->universe or info->new_universe should now contain the finished grid

    } else {
        // Corner case without pthreads
    }

// END -Perform a barrier and then leave MPI

    free(universe);
    free(new_universe);

    MPI_Barrier( MPI_COMM_WORLD );
    MPI_Finalize();

    return 0;
}

/***************************************************************************/
/* Other Functions - You write as part of the assignment********************/
/***************************************************************************/


/* Run the game of life by looping through the grid and modifying args->new_universe
 *  @param arg - void pointer pointing to a struct thread_args
 *      @param universe - the current grid
 *      @param new_universe - the not yet constructed next iteration of the universe
 *      @param start_row - the first row this thread has been assigned to compute
 *      @param ghost_row - NULL unless start_row is 0 or ROWS_PER_THREAD*(NUM_THREADS-1)
            otherwise is an array containing the row required by that thread
            UPDATE THIS FOR EACH ITERATION
 *  @return - can be left void because the main thread's struct will still contain a pointer 
        to the finished grid
 */
void* game_of_life(void *arg) {
    struct thread_args args = *(struct thread_args*)arg;
    short (*universe)[GRID_SIZE] = args.universe;
    short *ghost_row = args.ghost_row;

    int top = 0;
    int bot = 1;

    short *ghost_row_top, *ghost_row_bot;

    printf("I am PID %lu of rank %d\n", pthread_self(), mpi_myrank);

    //Code to communicate between ranks
    if (args.start_row == 0 && mpi_myrank == 0 && mpi_commsize > 1) {
    //     MPI_Request request;
    //     // MPI_Isend(universe[ROWS_PER_RANK-1],GRID_SIZE,MPI_SHORT,1,0,MPI_COMM_WORLD,&request);

        for (int i = 1; i < NUM_RANKS; ++i) {
            printf("PID recv %lu\n", pthread_self());

            MPI_Request request2,request3;
            MPI_Status status;

            short *ghost_row_top = (short*)malloc(sizeof(short)*GRID_SIZE);
            short *ghost_row_bot = (short*)malloc(sizeof(short)*GRID_SIZE);

            // MPI_Irecv(ghost_row_top,GRID_SIZE,MPI_SHORT,i,top,MPI_COMM_WORLD,&request2);
            // MPI_Wait(&request2,&status);

            // MPI_Irecv(ghost_row_bot,GRID_SIZE,MPI_SHORT,i,bot,MPI_COMM_WORLD,&request3);
            // MPI_Wait(&request3,&status);

        }


    } 
    // MPI_Barrier(MPI_COMM_WORLD);
    if (args.start_row != 0 && mpi_myrank != 0) {
        MPI_Request request;
        printf("PID sending %lu\n", pthread_self());
        // short buffer[GRID_SIZE];
        // for (int i = 0; i < GRID_SIZE; ++i) {
        //     // buffer[i] = universe[0][i];
        //     printf("%d", *universe[i]);
        // }
        MPI_Isend(universe[0],GRID_SIZE,MPI_SHORT,0,top,MPI_COMM_WORLD,&request);
        // MPI_Isend(universe[ROWS_PER_RANK-1],GRID_SIZE,MPI_SHORT,0,bot,MPI_COMM_WORLD,&request);

    }

    printf("PID leaving %lu\n", pthread_self());

    return NULL;
    //Stuff
    // for (int i = 0; i < NUM_TICKS; ++i) {

    // }
    
    //Clean up memory
    // if(args.ghost_row)
    //     free(args.ghost_row);
    // if (args.start_row != 0)
    //     free(arg);
}

