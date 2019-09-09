#define PNG_NO_SETJMP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <png.h>
#include <string.h>
#include <assert.h>
#include <mpi.h>
#include <omp.h>

typedef enum{
    true = 1 == 1,
    false = 1 == 0
}bool;

void write_png(const char* filename, int iters, int width, int height, const int* buffer) {
    // printf("write_png 0\n");
    FILE* fp = fopen(filename, "wb");
    assert(fp);
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr);
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_filter(png_ptr, 0, PNG_NO_FILTERS);
    png_write_info(png_ptr, info_ptr);
    png_set_compression_level(png_ptr, 1);
    size_t row_size = 3 * width * sizeof(png_byte);
    png_bytep row = (png_bytep)malloc(row_size);
    for (int y = 0; y < height; ++y) {
        memset(row, 0, row_size);
        for (int x = 0; x < width; ++x) {
            int p = buffer[(height - 1 - y) * width + x];
            png_bytep color = row + x * 3;
            if (p != iters) {
                if (p & 16) {
                    color[0] = 240;
                    color[1] = color[2] = p % 16 * 16;
                } else {
                    color[0] = p % 16 * 16;
                }
            }
            // printf("Image H%d w%d\n", y, x);
        }
        png_write_row(png_ptr, row);
    }
    free(row);
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

int monitor(int *collector){

    return ;
}

void distribute(int rank, int size, int *comm_size_p, int *collector, int height, int width, int *start, int *end, int *mem_segment_len){
    if(size >= height){
        (*comm_size_p) = height;
        (*collector) = height - 1;
        if(rank < height){
            (*start) = rank;
            (*end) = rank;
            (*mem_segment_len) = width;
        }
    }
    else{
        int comm_size = size;
        (*comm_size_p) = comm_size;
        (*collector) = comm_size - 1;

        int remain = height % comm_size;
        int shorter_segment = height / comm_size;
        int longer_segment = height / comm_size + 1;
        int segment = rank < remain? longer_segment : shorter_segment;
        (*start) = rank <= remain? longer_segment * rank : longer_segment * remain + shorter_segment * (rank - remain);
        (*end) = (*start) + segment - 1;
        (*mem_segment_len) = segment * width;
    }
    printf("Rank %d size %d comm_size %d collector %d start %d end %d\n", rank, size, (*comm_size_p), (*collector), (*start), (*end));
}

void gather(int rank, int comm_size, int mem_segment_len, int *image, int *buf, int collector){
    if(rank > 0){
        printf("Rank %d gather 0\n", rank);
        int temp_mem_segment_len = mem_segment_len;
        MPI_Send(&temp_mem_segment_len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        printf("Rank %d send temp_mem_segment_len\n", rank);
        MPI_Send(buf, mem_segment_len, MPI_INT, 0, 1, MPI_COMM_WORLD);
        printf("Rank %d send buff mem_segment\n", rank);
        // free(buf);
    }
    
    // for(int i=0; i<mem_segment_len; i++){
    //     printf("%d ", )
    // }
    
    if(rank == 0){
        int accum_idx = 0;
        for(int j=0; j<mem_segment_len; j++){
            image[accum_idx] = buf[j];
            // printf("Rank %d image[%d]=temp_buff[%d] < %d\n", i, accum_idx, j, buff_mem_segment_len);
            accum_idx++;
        }
        // free(buf);
        for(int i = 1; i < comm_size; i++){
            int buff_mem_segment_len;
            printf("Rank %d wait for rank %d mem_segment_len\n", rank, i);
            MPI_Recv(&buff_mem_segment_len, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int *temp_buf = (int*)malloc(sizeof(int) * buff_mem_segment_len);

            printf("Rank %d wait for rank %d temp_buf\n", rank, i);
            MPI_Recv(temp_buf, buff_mem_segment_len, MPI_INT, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for(int j=0; j<buff_mem_segment_len; j++){
                image[accum_idx] = temp_buf[j];
                // printf("Rank %d image[%d]=temp_buff[%d] < %d\n", i, accum_idx, j, buff_mem_segment_len);
                accum_idx++;
            }
            free(temp_buf);
        }
    }
}

void compute(int rank, int start, int end, int mem_segment_len, int **buf, int height, double upper, double lower, int width, double right, double left, int iters){
    // int segment_len = end - start + 1;
    (*buf) = (int*)malloc(sizeof(int) * mem_segment_len);
    
    for (int j = start, k = 0; j <= end; ++j, ++k) {
        double y0 = j * ((upper - lower) / height) + lower;
        #pragma omp parallel for
        for (int i = 0; i < width; ++i) {
            double x0 = i * ((right - left) / width) + left;

            int repeats = 0;
            double x = 0;
            double y = 0;
            double length_squared = 0;
            while (repeats < iters && length_squared < 4) {
                double temp = x * x - y * y + x0;
                y = 2 * x * y + y0;
                x = temp;
                length_squared = x * x + y * y;
                ++repeats;
            }
            (*buf)[k * width + i] = repeats;
            // printf("Rank %d buff_idx %d < %d mem_segment_len\n", rank, k*width+i, mem_segment_len);
        }
    }
}

void proc_task(int rank, int size, int *image, int height, double upper, double lower, int width, double right, double left, int iters){
    int start = -1;
    int end = -1;
    int mem_segment_len = -1;
    int comm_size = -1;
    int collector = -1;
    int *buf = NULL;
    distribute(rank, size, &comm_size, &collector, height, width, &start, &end, &mem_segment_len);
    printf("Rank %d comm_size %d mem_segment_len  %d colletor %d start %d end %d\n", rank, comm_size, mem_segment_len, collector, start, end);
    if(start != -1){
        compute(rank, start, end, mem_segment_len, &buf, height, upper, lower, width, right, left, iters);
        printf("Rank %d compute End\n", rank);
        gather(rank, comm_size, mem_segment_len, image, buf, collector);
        printf("Rank %d gather End\n", rank);
        // free(buf);
    }
    
}

int main(int argc, char *argv[]){
    // const char* filename = argv[4];
    // int iters = strtol(argv[5], 0, 10);
    // double left = strtod(argv[6], 0);
    // double right = strtod(argv[7], 0);
    // double lower = strtod(argv[8], 0);
    // double upper = strtod(argv[9], 0);
    // int width = strtol(argv[10], 0, 10);
    // int height = strtol(argv[11], 0, 10);

    const char* filename = NULL;
    int iters = -1;
    double left = -1.0;
    double right = -1.0;
    double lower = -1.0;
    double upper = -1.0;
    int width = -1;
    int height = -1;
    if(argc == 9){
        filename = argv[1];
        iters = strtol(argv[2], 0, 10);
        left = strtod(argv[3], 0);
        right = strtod(argv[4], 0);
        lower = strtod(argv[5], 0);
        upper = strtod(argv[6], 0);
        width = strtol(argv[7], 0, 10);
        height = strtol(argv[8], 0, 10);
    }else if(argc == 12){
        filename = argv[4];
        iters = strtol(argv[5], 0, 10);
        left = strtod(argv[6], 0);
        right = strtod(argv[7], 0);
        lower = strtod(argv[8], 0);
        upper = strtod(argv[9], 0);
        width = strtol(argv[10], 0, 10);
        height = strtol(argv[11], 0, 10);
    }

    int rank = -1, size = -1;
    int *buf = NULL;
    int* image = NULL;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // comm_size = size;
    if(rank == 0){
        image = (int*)malloc(width * height * sizeof(int));
        printf("Image size: %d\n", width * height);
        assert(image);
    }

    printf("Hi, Here is rank %d\n", rank);
    proc_task(rank, size, image, height, upper, lower, width, right, left, iters);
    printf("Rank %d proc_task End\n", rank);

    if(rank == 0){
        write_png(filename, iters, width, height, image);
        free(image);
    }
    MPI_Finalize();
}