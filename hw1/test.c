#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <mpi.h>

typedef enum{
    true = 1 == 1,
    false = 1 == 0
}bool;

bool compare(float *a, float *b){
    if(*a > *b){
        float t = *a;
        *a = *b;
        *b = t;
        return true;
    }
    return false;
}

int open_file(int len, char *file_in, FILE **pfile_in, char *file_out, FILE **pfile_out, float **seq_in, float **seq_out){
    printf("open_file 0\n");
    // printf("open_file 1: %s\n", fileIn);
    *pfile_in = fopen("seq.in", "r");
    
    // printf("open_file 2: %s\n", fileOut);
    *pfile_out = fopen("seq.out", "w");
    
    // printf("open_file 3\n");

    if(*pfile_in == NULL || *pfile_out == NULL){
        // printf("open_file 4\n");
        return 0;
    }
    // printf("open_file 5\n");
    *seq_in = (float *) malloc(sizeof(float) * len);
    // printf("open_file 6\n");
    *seq_out = (float *) malloc(sizeof(float) * len);
    // printf("open_file 7\n");
    if(*seq_in == NULL || *seq_out == NULL){
        // printf("open_file 8\n");
        return 0;
    }
    // printf("open_file 9\n");
    for(int i = 0; i<len; i++){
        float t;
        // printf("open_file 10\n");
        fscanf(*pfile_in, "%f", &t);
        
        (*seq_in)[i] = t;
        (*seq_out)[i] = t;
        // printf("%f, %f\n", (*seq_in)[i], (*seq_out)[i]);
    }
    // printf("open_file 11\n");
    return 1;
}

bool swap(int rank, int start, int end, float *seq_out){
    // printf("0\n");
    int len = end - start + 1;
    bool is_swap_ret = false;
    // printf("1\n");
    if(len % 2 == 0){
        // printf("2\n");
        for(int i = start; i <= end; i+=2){
            // printf("3\n");
            bool is_swap = compare(seq_out+i, seq_out+i+1);
            // printf("4\n");
            if(is_swap){is_swap_ret = true;}
            printf("%f %f ", seq_out[i], seq_out[i+1]);
        }
        printf("Rank %d Swap Done\n", rank);
        return is_swap_ret;
    }   
}

bool odd_phase(int rank, int comm_size, int *start, int *end, int seq_len, float *seq_out){
    int pair_num = (seq_len - 1) / 2;
    int segment_len_longer = (pair_num / comm_size + 1) * 2;
    int segment_len_shorter = (pair_num / comm_size) * 2;

    int segment_len = segment_len_shorter; // For remain_pairs == 0
    int remain_pairs = pair_num % comm_size;
    
    if(remain_pairs != 0){
        if(rank < remain_pairs){
            segment_len = segment_len_longer;
        }
    }
    // printf("ODD segment: %d, pair_num: %d, segment_longer: %d, segment_shorter: %d\n", segment_len, pair_num, segment_len_longer, segment_len_shorter);
    
    int start_i = rank <= remain_pairs? rank * segment_len_longer + 1: remain_pairs * segment_len_longer + (rank - remain_pairs) * segment_len_shorter + 1;
    int end_i = start_i + segment_len - 1;

    // printf("ODD Rank %d, start: %d, End: %d in Comm_size=%d, Seq_len=%d\n", rank, start_i, end_i, comm_size, seq_len);

    (*start) = start_i;
    (*end) = end_i;
    // for(int i=0; i<10; i++){
    //     printf("%f ", seq_out[i]);
    // }
    // printf("\n");
    // return swap(rank, start, end, seq_out);
}

bool even_phase(int rank, int comm_size, int *start, int *end, int seq_len, float *seq_out){
    int pair_num = seq_len / 2;
    int segment_len_longer = (pair_num / comm_size + 1) * 2;
    int segment_len_shorter = (pair_num / comm_size) * 2;

    int segment_len = segment_len_shorter; // For remain_pairs == 0
    int remain_pairs = pair_num % comm_size;
    
    if(remain_pairs != 0){
        if(rank < remain_pairs){
            segment_len = segment_len_longer;
        }
    }
    // printf("EVEN segment: %d, pair_num: %d, segment_longer: %d, segment_shorter: %d\n", segment_len, pair_num, segment_len_longer, segment_len_shorter);
    
    int start_i = rank <= remain_pairs? rank * segment_len_longer : remain_pairs * segment_len_longer + (rank - remain_pairs) * segment_len_shorter;
    int end_i = start_i + segment_len - 1;

    // printf("EVEN Rank %d, start: %d, End: %d in Comm_size=%d, Seq_len=%d\n", rank, start_i, end_i, comm_size, seq_len);

    (*start) = start_i;
    (*end) = end_i;

    // return swap(rank, start_i, end_i, seq_out);
}



// Return is continue or not?
bool wait_sync(int rank, bool is_even_swap, bool is_odd_swap, int comm_size, MPI_Comm communicator){
    int collector = comm_size - 1;
    int *msg_arr = (int *) malloc(sizeof(int) * comm_size); // Move to Global
    // int msg_arr[10];
    // msg_arr = 0: Not yet or Continue
    //         = 1: Done, but swap pair during proccessing
    //         = 2: Done, No swap pair during proccessing
    //         = 3: Request Exit
    if(comm_size == 1){
        return is_even_swap || is_odd_swap;
    }else{
        if(rank == collector){
            bool is_sort_finish = true;
            for(int i=0; i<comm_size-1; i++){
                MPI_Recv(msg_arr + i, 1, MPI_INT, i, 0, communicator, MPI_STATUS_IGNORE);
                printf("Proc %d Recv msg: %d from Proc %d\n", rank, msg_arr[i], i);
                if(msg_arr[i] == 1){is_sort_finish = false;}
            }
            if(!is_sort_finish){
                for(int i=0; i<comm_size-1; i++){
                    msg_arr[i] = 0;
                    MPI_Send(msg_arr + i, 1, MPI_INT, i, 1, communicator);
                    printf("Proc %d Send msg: %d to Proc %d\n", rank, msg_arr[i], i);
                }
                return true;
            }else{
                for(int i=0; i<comm_size-1; i++){
                    msg_arr[i] = 3;
                    MPI_Send(msg_arr + i, 1, MPI_INT, i, 1, communicator);
                    printf("Proc %d Send msg: %d to Proc %d\n", rank, msg_arr[i], i);
                }
                return false;
            }
        }else{// Sender
            msg_arr[rank] = is_even_swap || is_odd_swap? 1:2;
            MPI_Send(msg_arr + rank, 1, MPI_INT, collector, 0, communicator);
            printf("Proc %d Send msg: %d to Proc %d\n", rank, msg_arr[rank], collector);
            MPI_Recv(msg_arr + rank, 1, MPI_INT, collector, 1, communicator, MPI_STATUS_IGNORE);
            printf("Proc %d Recv msg: %d from Proc %d\n", rank, msg_arr[rank], collector);
            if(msg_arr[rank] == 0){return true;}
            else if(msg_arr[rank] == 3){return false;}
        }
    }
}

void odd_allocator(int rank, int comm_size, int seq_len,int *start, int *end, float *seq_out, float **segment_p){
    // printf("0\n");
    if(rank == 0){
        // printf("1\n");
        for(int rank_i = 0; rank_i < comm_size; rank_i++){
            // printf("2\n");
            int start_i = -1;
            int end_i = -1;
            odd_phase(rank_i, comm_size, &start_i, &end_i, seq_len, seq_out);
            int segment_len = end_i - start_i + 1;
            // MPI_Send(&segment_len, 1, MPI_INT, rank_i, 5, MPI_COMM_WORLD);
            // printf("3\n");
            float *buf = (float *)malloc(sizeof(float) * segment_len);
            for(int i=0, j=(start_i); j<=(end_i); i++, j++){
                // printf("%f ", seq_out[i]);
                buf[i] = seq_out[j];
            }
            // printf("4\n");
            printf("Odd_Allocator Send segment: \n");
            for(int i=0; i<segment_len; i++){
                printf("%f ", buf[i]);
            }
            printf("\n");
            MPI_Send(buf, segment_len, MPI_FLOAT, rank_i, 8, MPI_COMM_WORLD);
            // free(buf);
        }
    }
    // printf("5\n");
    int start_i = -1, end_i = -1;
    odd_phase(rank, comm_size, &start_i, &end_i, seq_len, seq_out);
    int segment_len = end_i - start_i + 1;
    *start = start_i;
    *end = end_i;
    // printf("6\n");
    float *buf = (float *)malloc(sizeof(float) * segment_len);
    printf("Rank %d Odd_allocator start_i: %d, end_i: %d, segment_len: %d\n", rank, start_i, end_i, segment_len);
    MPI_Recv(buf, (segment_len), MPI_FLOAT, 0, 8, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // printf("Odd_Allocator Recv segment: \n");
    // for(int i=0; i<segment_len; i++){
    //     printf("%f ", buf[i]);
    // }
    // printf("\n"); 
    (*segment_p) = buf;
    // printf("Odd_Allocator segment_p: \n");
    // for(int i=0; i<segment_len; i++){
    //     printf("%f ", (*segment_p)[i]);
    // }
    // printf("\n"); 
}


void even_allocator(int rank, int comm_size, int seq_len,int *start, int *end, float *seq_out, float **segment_p){
    // printf("0\n");
    if(rank == 0){
        // printf("1\n");
        for(int rank_i = 0; rank_i < comm_size; rank_i++){
            // printf("2\n");
            int start_i = -1;
            int end_i = -1;
            even_phase(rank_i, comm_size, &start_i, &end_i, seq_len, seq_out);
            int segment_len = end_i - start_i + 1;
            // MPI_Send(&segment_len, 1, MPI_INT, rank_i, 5, MPI_COMM_WORLD);
            // printf("3\n");
            float *buf = (float *)malloc(sizeof(float) * segment_len);
            for(int i=0, j=(start_i); j<=(end_i); i++, j++){
                // printf("%f ", seq_out[i]);
                buf[i] = seq_out[j];
            }
            // printf("4\n");
            printf("Even_Allocator Send segment: \n");
            for(int i=0; i<segment_len; i++){
                printf("%f ", buf[i]);
            }
            printf("\n");
            MPI_Send(buf, segment_len, MPI_FLOAT, rank_i, 6, MPI_COMM_WORLD);
            // free(buf);
        }
    }
    // printf("5\n");
    int start_i = -1, end_i = -1;
    even_phase(rank, comm_size, &start_i, &end_i, seq_len, seq_out);
    int segment_len = end_i - start_i + 1;
    *start = start_i;
    *end = end_i;
    // printf("6\n");
    float *buf = (float *)malloc(sizeof(float) * segment_len);
    printf("Rank %d Even_allocator start_i: %d, end_i: %d, segment_len: %d\n", rank, start_i, end_i, segment_len);
    MPI_Recv(buf, (segment_len), MPI_FLOAT, 0, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // printf("Even_Allocator Recv segment: \n");
    // for(int i=0; i<segment_len; i++){
    //     printf("%f ", buf[i]);
    // }
    // printf("\n"); 
    (*segment_p) = buf;
    // printf("Even_Allocator segment_p: \n");
    // for(int i=0; i<segment_len; i++){
    //     printf("%f ", (*segment_p)[i]);
    // }
    // printf("\n");   
}
void odd_gather(int rank, int comm_size, int seq_len, float *seq_out, float *segment_p){
    
    int start_i = -1;
    int end_i = -1;
    odd_phase(rank, comm_size, &start_i, &end_i, seq_len, seq_out);
    int segment_len = end_i - start_i + 1;
    MPI_Send(segment_p, segment_len, MPI_FLOAT, 0, 9, MPI_COMM_WORLD);
    free(segment_p);
    
    if(rank == 0){
        for(int rank_i=0; rank_i<comm_size; rank_i++){
            int start_i = -1;
            int end_i = -1;
            odd_phase(rank_i, comm_size, &start_i, &end_i, seq_len, seq_out);
            int segment_len = end_i - start_i + 1;

            float *buf = (float *)malloc(sizeof(float) * segment_len);
            MPI_Recv(buf, segment_len, MPI_FLOAT, rank_i, 9, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for(int i=(start_i), j=0; i<=(end_i); i++, j++){
                seq_out[i] = buf[j];
            }
            free(buf);
        }
    }
}

void even_gather(int rank, int comm_size, int seq_len, float *seq_out, float *segment_p){
    
    int start_i = -1;
    int end_i = -1;
    even_phase(rank, comm_size, &start_i, &end_i, seq_len, seq_out);
    int segment_len = end_i - start_i + 1;
    MPI_Send(segment_p, segment_len, MPI_FLOAT, 0, 7, MPI_COMM_WORLD);
    free(segment_p);
    
    if(rank == 0){
        for(int rank_i=0; rank_i<comm_size; rank_i++){
            int start_i = -1;
            int end_i = -1;
            even_phase(rank_i, comm_size, &start_i, &end_i, seq_len, seq_out);
            int segment_len = end_i - start_i + 1;

            float *buf = (float *)malloc(sizeof(float) * segment_len);
            MPI_Recv(buf, segment_len, MPI_FLOAT, rank_i, 7, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for(int i=(start_i), j=0; i<=(end_i); i++, j++){
                seq_out[i] = buf[j];
            }
            free(buf);
        }
    }
}

void sort(int rank, int comm_size, int seq_len, float *seq_out){
    printf("Rank %d comm_size %d\n", rank, comm_size);
    // printf("Rank %d Sort 0\n", rank);
    bool is_even_swap = true;
    bool is_odd_swap = true;
    bool is_continue = true;
    // printf("Rank %d Sort 1\n", rank);

    while(is_continue){
        
        float *segment_p = NULL;
        int start = 0;
        int end = -1;
        even_allocator(rank, comm_size, seq_len, &start, &end, seq_out, &segment_p);
        int segment_len = end - start + 1;

        printf("Rank Even %d Sort segment: \n", rank);
        for(int i=0; i<segment_len; i++){
            printf("%f ", segment_p[i]);
        }
        printf("\n");
        printf("Rank %d Even Sort start: %d, end: %d, segment_len: %d\n", rank, start, end, sizeof(segment_p) / sizeof(float));
        is_even_swap = swap(rank, 0, segment_len-1, segment_p);
        even_gather(rank, comm_size, seq_len, seq_out, segment_p);

        // printf("Rank %d Sort 2\n", rank);
        wait_sync(rank, is_even_swap, is_odd_swap, comm_size, MPI_COMM_WORLD);
        // printf("Rank %d Sort 3\n", rank);
        if(rank == 0){
            printf("Rank %d Sync Even:\n", rank);
            for(int i=0; i<seq_len; i++){
                printf("%f ", seq_out[i]);
            }
            printf("\n");
        }

        // Reset Variable
        segment_p = NULL;
        start = 0;
        end = -1;
        segment_len = 0;

        // ODD PHASE
        odd_allocator(rank, comm_size, seq_len, &start, &end, seq_out, &segment_p);
        segment_len = end - start + 1;

        printf("Rank Odd %d Sort segment: \n", rank);
        for(int i=0; i<segment_len; i++){
            printf("%f ", segment_p[i]);
        }
        printf("\n");
        printf("Rank %d Odd Sort start: %d, end: %d, segment_len: %d\n", rank, start, end, sizeof(segment_p) / sizeof(float));
        is_odd_swap = swap(rank, 0, segment_len-1, segment_p);
        odd_gather(rank, comm_size, seq_len, seq_out, segment_p);

        // printf("Rank %d Sort 2\n", rank);
        is_continue = wait_sync(rank, is_even_swap, is_odd_swap, comm_size, MPI_COMM_WORLD);
        // printf("Rank %d Sort 3\n", rank);
        if(rank == 0){
            printf("Rank %d Sync Odd:\n", rank);
            for(int i=0; i<seq_len; i++){
                printf("%f ", seq_out[i]);
            }
            printf("\n");
            printf("Rank: %d, is_even_swap: %s, is_odd_swap: %s\n", rank, is_even_swap? "True":"False", is_odd_swap? "True":"False");
        }

        // wait_sync(rank, is_even_swap, is_odd_swap, comm_size, MPI_COMM_WORLD);
        // printf("Rank %d Sort 5\n", rank);
    }
    

    printf("Proc %d FFFFF\n", rank);
    MPI_Finalize();
}

int monitor(int *rank, int *size, int *comm_size, int *argc, char **argv[], int *seq_len, FILE **pfile_in, FILE **pfile_out, float **seq_in, float **seq_out){
    // comm_size should < seq_len/2
    // printf("Monitor 0\n");
    MPI_Init(argc, argv);
    // printf("Monitor 1\n");
    MPI_Comm_rank(MPI_COMM_WORLD, rank);
    MPI_Comm_size(MPI_COMM_WORLD, size);
    // printf("Monitor 2\n");
    int read_file_done = 0;
    *seq_len = 10;

    if(*rank == 0){
        // printf("Monitor 3\n");
        printf("%s %s %s\n", (*argv)[1], (*argv)[2], (*argv)[3]);
        // open_file(atoi((*argv)[1]), (*argv)[2], pfile_in, (*argv)[3], pfile_out, seq_in, seq_out);
        int len = 10;
        char in[7] = "seq.in";
        char out[8] = "seq.out";
        printf("in %s\n", in);
        printf("out %s\n", out);
        // open_file(len, in, pfile_in, out, pfile_out, seq_in, seq_out);
        open_file(atoi((*argv)[3]), (*argv)[4], pfile_in, (*argv)[5], pfile_out, seq_in, seq_out);
        // printf("Monitor 4\n");
        int pair_num = (*seq_len) / 2;
        *comm_size = (*size)<=pair_num? (*size):pair_num;
        // *seq_len = atoi((*argv)[1]);
        
        // printf("Monitor 5\n");
        read_file_done = 1;
        for(int i = 0; i<(*size); i++){
            MPI_Send(&read_file_done, 1, MPI_INT, i, 3, MPI_COMM_WORLD);
        }
        

        // for(int i = 0; i<(*size); i++){
        //     MPI_Send((*seq_out), (*seq_len), MPI_FLOAT, i, 4, MPI_COMM_WORLD);
        // }
        
    }else if((*rank) > 0){
        // printf("Monitor 6\n");
        MPI_Recv(&read_file_done, 1, MPI_INT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // printf("Monitor 7\n");
        // printf("*Seq_len: %d\n", (*seq_len));
        // *seq_out = (float *)malloc(sizeof(float) * (*seq_len));
        // MPI_Recv((*seq_out), (*seq_len), MPI_FLOAT, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        // printf("Monitor 8\n");

        int pair_num = (*seq_len) / 2;
        *comm_size = (*size)<=pair_num? (*size):pair_num;

        // printf("Print Sended Seq.out\n");
        // for(int i=0; i<(*seq_len); i++){
        //     printf("%f ", (*seq_out)[i]);
        // }
        // printf("\n");

        // printf("Monitor 9\n");
    }
    
    
    if(rank < comm_size){
        return 1;
    }else{
        return 0;
    }
}

int close_file(int rank, int seq_len, float *seq_out, FILE *pfile_in, FILE *pfile_out){
    if(rank == 0){
        for(int i=0; i<seq_len-1; i++){
            fprintf(pfile_out, "%f ", seq_out[i]);
        }
        fprintf(pfile_out, "%f", seq_out[seq_len-1]);
        fclose(pfile_out);
        fclose(pfile_in);
    }
    
}



bool get_rand_bool(){
    srand(time(NULL));
    return rand()%2 == 1? true:false;
}



int main(int argc, char* argv[]){
    int rank = -1;
    int size = -1;
    int comm_size = -1;
    int seq_len = -1;
    FILE *pfile_in = NULL;
    FILE *pfile_out = NULL;
    float *seq_in = NULL;
    float *seq_out = NULL;

    // printf("main 0\n");
    monitor(&rank, &size, &comm_size, &argc, &argv, &seq_len, &pfile_in, &pfile_out, &seq_in, &seq_out);
    // printf("main 1\n");
    sort(rank, comm_size, seq_len, seq_out);
    
    // printf("main 2\n");
    close_file(rank, seq_len, seq_out, pfile_in, pfile_out);
}