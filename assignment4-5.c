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
#include <unistd.h>

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
int GRID_SIZE = 1024;
int NUM_TICKS = 128;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t pt_barrier;
int thread_cnt;

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
int num_alive_neighbors(short** universe, int i, int j, short* top, short* bot);

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

    thread_cnt = -1;

    

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
        int bar = NUM_THREADS*NUM_RANKS;
        int rc = pthread_barrier_init(&pt_barrier,NULL,2);
        if (rc != 0) {
            fprintf(stderr,"pthread_barrier_init");
            exit(1);
        }

        // Create thread array
        pthread_t tid[NUM_THREADS];

        // Create each thread and call thread function game_of_life
        for (int i = 0; i < NUM_THREADS; ++i) {
            //Create struct to pass arguments to threads
            struct thread_args *info = (struct thread_args*) calloc(1, sizeof(struct thread_args));//possible memory issue from multiple of same variable?
            info->universe = universe;
            info->new_universe = new_universe;
            *(short *)&info->start_row = ROWS_PER_RANK*mpi_myrank + i*ROWS_PER_THREAD;
            info->ghost_row = NULL;
            
            int rc = pthread_create(&tid[i],NULL,game_of_life,(void*)info);

            if (rc != 0) {
                fprintf(stderr,"Main thread could not create child thread (%d)\n",rc);
                return EXIT_FAILURE;
            }
        }

        // Join child threads with main thread
        for (int i = 0; i < NUM_THREADS; ++i) {
                                              // ============================================
            pthread_join(tid[i],NULL);   // ============================================

        }// info->universe or info->new_universe should now contain the finished grid

        printf("Rank %d finished!\n",mpi_myrank);

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
    short (*univ)[GRID_SIZE] = args.universe;
    short *universe[ROWS_PER_RANK];
    for (int i = 0; i < ROWS_PER_RANK; ++i) {
        universe[i] = univ[i];
    }

    short (*new_univ)[GRID_SIZE] = args.new_universe;
    short *new_universe[ROWS_PER_RANK];
    for (int i = 0; i < ROWS_PER_RANK; ++i) {
        new_universe[i] = new_univ[i];
    }

    short *ghost_row = args.ghost_row;

    int top = 123;
    int bot = 200;

    short *ghost_row_top, *ghost_row_bot, *my_top, *my_bot;

    for (int i = 0; i < 1; ++i) {
        
        
        if (args.start_row == ROWS_PER_RANK*mpi_myrank) {
            printf("Rank %d, PID %lu\n", mpi_myrank,pthread_self());
            MPI_Request request,request2,request3,request4;
            MPI_Status status,status2;
            
            my_top = (short*)malloc(sizeof(short)*GRID_SIZE);
            my_bot = (short*)malloc(sizeof(short)*GRID_SIZE);

            if (mpi_myrank == 0) {
                // printf("%d, %d\n", universe[0][0],universe[ROWS_PER_RANK-1][0]);
                MPI_Isend(universe[0],GRID_SIZE,MPI_SHORT,NUM_RANKS-1,top,MPI_COMM_WORLD,&request);
            } else {
                // printf("%d, %d\n", universe[0][0],universe[ROWS_PER_RANK-1][0]);
                MPI_Isend(universe[0],GRID_SIZE,MPI_SHORT,mpi_myrank-1,top,MPI_COMM_WORLD,&request);
            }

            if (mpi_myrank == NUM_RANKS-1) {
                // printf("%d, %d\n", universe[0][0],universe[ROWS_PER_RANK-1][0]);
                MPI_Isend(universe[ROWS_PER_RANK-1],GRID_SIZE,MPI_SHORT,0,bot,MPI_COMM_WORLD,&request2);
            } else {
                // printf("%d, %d\n", universe[0][0],universe[ROWS_PER_RANK-1][0]);
                MPI_Isend(universe[ROWS_PER_RANK-1],GRID_SIZE,MPI_SHORT,mpi_myrank+1,bot,MPI_COMM_WORLD,&request2);
                
            }

            // MPI_Barrier(MPI_COMM_WORLD);

            if (mpi_myrank == 0) {
                MPI_Irecv(my_top,GRID_SIZE,MPI_SHORT,NUM_RANKS-1,bot,MPI_COMM_WORLD,&request3);
                MPI_Wait(&request3,&status);
                // printf("%d\n",my_top==NULL);
            } else {
                MPI_Irecv(my_top,GRID_SIZE,MPI_SHORT,mpi_myrank-1,bot,MPI_COMM_WORLD,&request3);
                MPI_Wait(&request3,&status);
                // printf("%d\n",my_top==NULL);

            }

            if (mpi_myrank == NUM_RANKS-1) {
                MPI_Irecv(my_bot,GRID_SIZE,MPI_SHORT,0,top,MPI_COMM_WORLD,&request4);
                MPI_Wait(&request4,&status2);
                // printf("%d\n",my_top==NULL);

            } else {
                MPI_Irecv(my_bot,GRID_SIZE,MPI_SHORT,mpi_myrank+1,top,MPI_COMM_WORLD,&request4);
                MPI_Wait(&request4,&status2);
                // printf("%d\n",my_top==NULL);

            }



        } 
        
        // printf("%d\n", NUM_THREADS);
        printf("PID %lu waiting here in rank %d\n",pthread_self(),mpi_myrank);
        MPI_Barrier(MPI_COMM_WORLD);
        int rc = pthread_barrier_wait(&pt_barrier);
        printf("PID %lu done waiting\n", pthread_self());

        // if (rc == 0) {
        //     // printf("Thread passed barrier: return value was 0\n");
        // } else if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
        //     // printf("Thread passed barrier: return value was PTHREAD_BARRIER_SERIAL_THREAD\n");
        // } else {
        //     fprintf(stderr, "pthread_barrier_wait\n");
        //     exit(1);
        // }
        pthread_mutex_lock(&mutex);
        if (my_bot == NULL || my_top == NULL) {
            fprintf(stderr, "ghost row is NULL for rank %d for PID %lu, %d %d\n",mpi_myrank,pthread_self(),my_bot==NULL,my_top==NULL);
            exit(1);
        }
        pthread_mutex_unlock(&mutex);

        // // Apply rules for this generation using previous generation
        pthread_mutex_lock(&mutex);
        thread_cnt++;
        printf("From Rank %d: thread range: %d - %d\n",mpi_myrank,(thread_cnt)*ROWS_PER_THREAD, (thread_cnt+1)*ROWS_PER_THREAD-1);
        printf("ROWS_PER_RANK: %d\n", ROWS_PER_RANK);
        for (int j = (thread_cnt)*ROWS_PER_THREAD; j < (thread_cnt+1)*ROWS_PER_THREAD-1; ++j) {
            // printf("%d\n",j);
            for (int k = 0; k < GRID_SIZE; ++k) {
                float rng_val = GenVal(j);
                if (rng_val > THRESHOLD) {
                    // int num_alive = 0;
                    int num_alive = num_alive_neighbors(universe,j,k,my_top,my_bot);
                    // if (universe[j][k] == ALIVE) {
                    //     if (num_alive < 2) {
                    //         new_universe[j][k] = DEAD;
                    //     } else if (num_alive > 1 && num_alive < 4) {
                    //         new_universe[j][k] = ALIVE;
                    //     } else if (num_alive > 3) {
                    //         new_universe[j][k] = DEAD;
                    //     } 
                    // } else {
                    //     if (num_alive == 3) {
                    //         new_universe[j][k] = ALIVE;
                    //     } else {
                    //         new_universe[j][k] = DEAD;
                    //     }
                    // }
                } else {
                    new_universe[j][k] = (GenVal(j) > .5) ? 1 : 0;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        
        // if (mpi_myrank != 0 && args.start_row == ROWS_PER_RANK*mpi_myrank) {
        //     for (int j = args.start_row; j < args.start_row + ROWS_PER_THREAD; ++j) {
        //         for (int k = 0; k < GRID_SIZE; ++k) {
        //             printf("Rank %d: %d %d %d\n", mpi_myrank, j, k, universe[j][k] );
        //             int num_alive = num_alive_neighbors(universe,j,k,my_top,my_bot);
        //         }
        //     }
        // }

        // if (args.start_row == ROWS_PER_RANK*mpi_myrank) {
        //     // printf("Rank %d reached here2\n", mpi_myrank);
        // }

        // Update universe at end of iteration
        for (int j = 0; j < ROWS_PER_RANK; ++j) {
            new_universe[j] = universe[j];
        }

        // if (args.start_row == ROWS_PER_RANK*mpi_myrank) {
        //     // printf("Rank %d reached here3\n", mpi_myrank);
        // }
        
    }

    // if (args.start_row == ROWS_PER_RANK*mpi_myrank) {
    //     // printf("Rank %d is returning\n", mpi_myrank);
    // }

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

// Get the number of alive neighbors
int num_alive_neighbors(short** universe, int i, int j, short *top, short *bot) {
    // printf("%d\n", universe[i][j]);
    // printf("%d\n", top==NULL);
    // if (i >= ROWS_PER_RANK || j >= GRID_SIZE) {
    //     printf("this is a problem %d %d\n", i,ROWS_PER_RANK);
    //     printf("%d\n", universe[i][j]);
    // }
    int num_alive = universe[i][j+1] + universe[i][j-1];

    // Account for ghost rows if needed
    if (i == 0) {
        // num_alive = num_alive + (int)top[j+1] + (int)top[j] + (int)top[j-1];
    } else {
        num_alive = num_alive + universe[i-1][j] + universe[i-1][j+1] + universe[i-1][j-1];
    }

    if (i==NUM_RANKS-1) {
        // num_alive = num_alive + bot[j+1] + bot[j] + bot[j-1];
    } else {
        num_alive = num_alive + universe[i+1][j] + universe[i+1][j+1] + universe[i+1][j-1];
    }

    // return num_alive;
    return 0;
     
}