#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <mpi.h>

typedef enum{
    true = 1 == 1,
    false = 1 == 0
}bool;

int main(int argc, char *argv[]){
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    printf("HI, Rank %d\n", rank);
    if(rank == 0){
        float arr[5] = {5.1, 3.3, 6.2, 8.2, 1.1};
        for(int i=1; i<size; i++){
            MPI_Send(arr, 5, MPI_FLOAT, i, 0, MPI_COMM_WORLD);
        }
    }else{
        float *arr = (float *)malloc(sizeof(float) * 5);
        MPI_Recv(arr, 5, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i=0; i<5; i++){
            printf("%f ", arr[i]);
        }
        printf("\n");
    }

    MPI_Finalize();
}