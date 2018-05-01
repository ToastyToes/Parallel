#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

#define INF INT_MAX
#ifdef BGQ
#include <hwi/include/bqc/A2_inlines.h>
#else
#define GetTimeBase MPI_Wtime
#endif

MPI_Datatype Build_block(int n, int local_n);
void scatter_matrix(int local_matrix[], int n, int local_n, MPI_Datatype block_t, int rank, MPI_Comm comm);
void Dijkstra(int matrix[], int local_dist[], int local_pred[], int n, int local_n, int rank, MPI_Comm comm);
void init_matrix(int matrix[], int local_dist[], int local_pred[], int known[], int local_n, int rank);
int find_min(int local_dist[], int local_known[], int local_n, int my_rank, MPI_Comm comm);

int main(int argc, char* argv[]) {
    int *local_matrix, *local_dist, *local_pred;
    int n, local_n, size, rank;
    double unsigned long long start_cycles, end_cycles;
    MPI_Comm comm;
    MPI_Datatype block_t;

    MPI_Init(&argc, &argv);
    comm = MPI_COMM_WORLD;
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);
    
    n = 10000;
    local_n = n/size;
    local_matrix = malloc(n*local_n*sizeof(int));
    local_dist = malloc(n*local_n*sizeof(int));
    local_pred = malloc(n*local_n*sizeof(int));
    
    block_t = Build_block(n, local_n);
    
    scatter_matrix(local_matrix, n, local_n, block_t, rank, comm);
    if(rank == 0){
    	printf("Scattered, starting Dijkstra\n");
   	    start_cycles = GetTimeBase();

    }
    Dijkstra(local_matrix, local_dist, local_pred, n, local_n, rank, comm);
    if(rank == 0){
    	end_cycles = GetTimeBase();
        double time_in_secs = ((double)(end_cycles - start_cycles));
    	printf("Finished Dijkstra in %lf seconds\n", time_in_secs);
    }
    
    //Print_dists(local_dist, n, local_n, rank, comm);
    //Print_paths(local_pred, n, local_n, rank, comm);
    
    free(local_matrix);
    free(local_dist);
    free(local_pred);

    MPI_Type_free(&block_t);
    
    MPI_Finalize();
    return 0;
}  /* main */

MPI_Datatype Build_block(int n, int local_n) {
    MPI_Aint lb, extent;
    MPI_Datatype temp_t;
    MPI_Datatype first_t;
    MPI_Datatype block_t;
    
    MPI_Type_contiguous(local_n, MPI_INT, &temp_t);
    MPI_Type_get_extent(temp_t, &lb, &extent);
    
    MPI_Type_vector(n, local_n, n, MPI_INT, &first_t);
    MPI_Type_create_resized(first_t, lb, extent, &block_t);
    MPI_Type_commit(&block_t);
    
    MPI_Type_free(&temp_t);
    MPI_Type_free(&first_t);
    
    return block_t;
} 

void scatter_matrix(int local_matrix[], int n, int local_n, MPI_Datatype block_t, int rank, MPI_Comm comm) {
    int* matrix = NULL;
    int i, j;
    srand(time(NULL));

    if (rank == 0) {
        matrix = malloc(n*n*sizeof(int));
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++)
                matrix[i*n + j] = rand()%100;
    }
    
    MPI_Scatter(matrix, 1, block_t, local_matrix, n*local_n, MPI_INT, 0, comm);
    
    if (rank == 0) 
    	free(matrix);
}

void Dijkstra(int matrix[], int local_dist[], int local_pred[], int n, int local_n, int rank, MPI_Comm comm) {
    int i, pos, *known, new_dist;
    int min_dist, local_vert;
                      
    known = malloc(local_n*sizeof(int));
    
    init_matrix(matrix, local_dist, local_pred, known, local_n, rank);
  
    for (i = 1; i < n; i++) {
        min_dist = find_min(local_dist, known, local_n, rank, comm);
        
        int min[2], glbl_min[2];
        int g_min_dist;

        if (min_dist < INF) {
            min[0] = local_dist[min_dist];
            min[1] = min_dist + rank*local_n;
        } else {
            min[0] = INF;
            min[1] = INF;
        }
        
        MPI_Allreduce(min, glbl_min, 1, MPI_2INT, MPI_MINLOC, comm);
        pos = glbl_min[1];
        g_min_dist = glbl_min[0];

        if (pos/local_n == rank) {
            min_dist = pos % local_n;
            known[min_dist] = 1;
        }

        for (local_vert = 0; local_vert < local_n; local_vert++)
           if (!known[local_vert]) {
               new_dist = g_min_dist + matrix[pos*local_n + local_vert];
               if (new_dist < local_dist[local_vert]) {
                   local_dist[local_vert] = new_dist;
                   local_pred[local_vert] = pos;
                }
            }
    }  
    free(known);
} 

void init_matrix(int matrix[], int local_dist[], int local_pred[], int known[], int local_n, int rank) { 

   for (int vert = 0; vert < local_n; vert++) {
      local_dist[vert] = matrix[0*local_n + vert];
      local_pred[vert] = 0;
      known[vert] = 0;
   }
   
   if (rank == 0) {
      known[0] = 1;
    } 
}

int find_min(int local_dist[], int local_known[], int local_n, int my_rank, MPI_Comm comm) {
    int local_vert, local_u;
    int local_min_dist = INF;
    
    local_u = INF;
    for (local_vert = 0; local_vert < local_n; local_vert++)
        if (!local_known[local_vert])
            if (local_dist[local_vert] < local_min_dist) {
                local_u = local_vert;
                local_min_dist = local_dist[local_vert];
            }

    return local_u;
} 
