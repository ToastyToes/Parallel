#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <unistd.h>

#define NUM_MAX RAND_MAX

int mpi_rank, mpi_size;

struct Bucket {
	int* bucket;
	int size; //number of elements in the bucket
	int size_actual; //size of the bucket array
	int num;
};

void bucketSort(int array[], int array_size, int myrank, int mysize, int num_threads);
void* sortBucket(void* args);
void quickSort(int* bucket, int low, int high);
int partition(int* bucket, int low, int high);

void bucketSort(int array[], int array_size, int myrank, int mysize, int num_threads){
	//Init bucket list
	mpi_rank = myrank;
	mpi_size = mysize;
	int num_buckets = (num_threads) ? num_threads : 5;
	//int bucket_terminator = RAND_MAX+1;
	struct Bucket *bucket = (struct Bucket*) calloc(num_buckets, sizeof(struct Bucket));
	for (int n = 0; n < num_buckets; ++n){
		bucket[n].bucket = (int*) calloc(array_size/num_buckets, sizeof(int));
		bucket[n].size = 0;
		bucket[n].size_actual = array_size/num_buckets;
		bucket[n].num = n;
	}
	float size_mod = 1.2; //bucket size multiplier upon realloc
	
	//fill buckets
	int dest_bucket;
	for (int i = 0; i < array_size; ++i){
		dest_bucket = ((double)array[i]/NUM_MAX)*num_buckets;
		//bucket full, realloc
		if (bucket[dest_bucket].size >= bucket[dest_bucket].size_actual){ 
			int* temp = (int*) calloc(bucket[dest_bucket].size_actual*size_mod, sizeof(int));
			bucket[dest_bucket].size_actual *= size_mod;
			memcpy(temp, bucket[dest_bucket].bucket, bucket[dest_bucket].size);
			free(bucket[dest_bucket].bucket);
			bucket[dest_bucket].bucket = temp;
		}
		bucket[dest_bucket].bucket[bucket[dest_bucket].size] = array[i];
		bucket[dest_bucket].size += 1;

	}

	//use pthreads to sort the buckets
	pthread_t tid[num_threads];
	if (num_threads){
		for (int p = 0; p < num_buckets; ++p){
			pthread_create(&tid[p], NULL, sortBucket, (void*) &bucket[p]);
		}
	
		//pass sorted buckets to threads of rank 0 for combining and final sorting
		if (mpi_rank != 0){
			//MPI_Barrier(MPI_COMM_WORLD);
			MPI_Request request;
			//package and send bucket data
			for (int i = 0; i < num_buckets; ++i){
				MPI_Isend(&bucket[i].size, 1, MPI_INT, 0, (i+1)*mpi_rank+1, MPI_COMM_WORLD, &request);
			}
			//MPI_Barrier(MPI_COMM_WORLD);
			for (int i = 0; i < num_buckets; ++i){
				MPI_Isend(bucket[i].bucket, bucket[i].size, MPI_INT, 0, (i+1)*mpi_rank, MPI_COMM_WORLD, &request);
			}
		}
		for (int i = 0; i < num_threads; ++i){
			pthread_detach(tid[i]);
		}
	}
	else { //Main thread sorts over array and then sorts all buckets
		//sortBucket((void*) &bucket[n]);
		MPI_Request request;
		if (mpi_rank != 0){
			for (int i = 0; i < num_buckets; ++i){
				quickSort(bucket[i].bucket, 0, bucket[i].size-1);
				MPI_Isend(&bucket[i].size, 1, MPI_INT, 0, (i+1)*mpi_rank+1, MPI_COMM_WORLD, &request);
				MPI_Isend(bucket[i].bucket, bucket[i].size, MPI_INT, 0, (i+1)*mpi_rank, MPI_COMM_WORLD, &request);
				MPI_Barrier(MPI_COMM_WORLD);
			}
		}
		else {
			for (int i = 0; i < num_buckets; ++i){
				quickSort(bucket[i].bucket, 0, bucket[i].size-1);
				int size = 0;
				int sizes[mpi_size];
				sizes[0] = bucket[i].size;
				int* recv_bucket;
				MPI_Status status;
				for (int j = 1; j < mpi_size; ++j){
					int tmp_size;
					MPI_Irecv(&tmp_size, 1, MPI_INT, j, (i+1)*j+1, MPI_COMM_WORLD, &request);
					MPI_Wait(&request, &status);
					size += tmp_size;
					sizes[j] = tmp_size;
				}
				
				recv_bucket = (int*) calloc(size, sizeof(int));

				for (int j = 1; j < mpi_size; ++j){
					//tmp_bucket = (int*) calloc(sizes[j], sizeof(int));
					MPI_Irecv(recv_bucket+sizes[j-1], sizes[j], MPI_INT, j, (i+1)*j, MPI_COMM_WORLD, &request);
					MPI_Wait(&request, &status);
				}
				quickSort(recv_bucket, 0, size-1);
				free(bucket[i].bucket);
				bucket[i].bucket = recv_bucket;
				bucket[i].size = size;
				bucket[i].size_actual = size;
				MPI_Barrier(MPI_COMM_WORLD);
			}				
		
		}

	}
}

void* sortBucket(void* args){
	struct Bucket bucket = *(struct Bucket*)args;
	quickSort(bucket.bucket, 0, bucket.size-1);
	int rank = 1;
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	if (rank == 0){
		//MPI_Barrier(MPI_COMM_WORLD);
		//receive mpi_size buckets per thread
		int size = 0;
		int sizes[mpi_size];
		sizes[0] = bucket.size;
		int* recv_bucket;
		MPI_Request request;
		//start by getting lengths of buckets
		for (int j = 1; j < mpi_size; ++j){
			int tmp_size;
			MPI_Irecv(&tmp_size, 1, MPI_INT, j, (bucket.num+1)*j+1, MPI_COMM_WORLD, &request);
			size += tmp_size;
			sizes[j] = tmp_size;
		}
		recv_bucket = (int*) calloc(size, sizeof(int));
		//int* tmp_bucket;
		for (int j = 1; j < mpi_size; ++j){
			//tmp_bucket = (int*) calloc(sizes[j], sizeof(int));
			MPI_Irecv(recv_bucket+sizes[j-1], sizes[j], MPI_INT, j, (bucket.num+1)*j, MPI_COMM_WORLD, &request);
		}
		//recv_bucket now contains all buckets from other ranks
		quickSort(recv_bucket, 0, size-1);
		
		free(bucket.bucket);
		bucket.bucket = recv_bucket;
		bucket.size = size;
		bucket.size_actual = size;
	}
	return NULL;
}

void quickSort(int* bucket, int low, int high){
	if (low < high){
		int p = partition(bucket, low, high);
		quickSort(bucket, low, p-1);
		quickSort(bucket, p+1, high);
	}
}

int partition(int* bucket, int low, int high){
	int pivot = bucket[high];
	int i = low-1;
	int temp;
	for (int j = low; j < high; ++j){
		if (bucket[j] < pivot){
			i++;
			temp = bucket[i];
			memcpy(&bucket[i], &bucket[j], 1);
			//*bucket[i] = *bucket[j];
			bucket[j] = temp;
		}
	}
	temp = bucket[i+1];
	memcpy(&bucket[i+1], &bucket[high], 1);
	//*bucket[i+1] = *bucket[high];
	bucket[high] = temp;
	return i+1;
}

/*
int main(int argc, char *argv[]) {
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

	int array[100];
	for (int i = 0; i < 100; ++i){
		array[i] = 99-i;
	}
	
	bucketSort(array, 100, 4);
	
	//MPI_Barrier(MPI_COMM_WORLD);
	//MPI_Finalize();
	return 0;
}
*/

