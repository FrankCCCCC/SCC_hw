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

void distribute(int rank, int size, int *comm_size_p, int *collector, int height, int *start, int *end){
    if(size >= height){
        (*comm_size_p) = height;
        (*collector) = height - 1;
        if(rank < height){
            (*start) = rank + 1;
            (*end) = rank + 1;
        }
    }
    eles{
        int comm_size = size;
        (*comm_size_p) = comm_size;
        (*collector) = comm_size - 1;

        int remain = height % comm_size;
        int shorter_segment = height / comm_size;
        int longer_segment = height / comm_size + 1;
        int segment = rank < remain? longer_segment : shorter_segment;
        (*start) = rank <= remain? longer_segment * rank : longer_segment * remain + shorter_segment * (rank - remain);
        (*end) = (*start) + segment - 1;
    }
}

void proc_task(int rank, int size, int *image, int height, double upper, double lower, int width, double right, double left, int iters){
    int start = -1;
    int end = -1;
    int comm_size = -1;
    int collector = -1;
    distribute(rank, size, &comm_size, &collector, height, &start, &end);

}

void compute(){

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

    int* image = NULL;
    assert(image);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // comm_size = size;
    if(rank == 0){
        image = (int*)malloc(width * height * sizeof(int));
        assert(image);
    }

    printf("Hi, Here is rank %d\n", rank);

    if(rank == 0){
        write_png(filename, iters, width, height, image);
        free(image);
    }
    MPI_Finalize();
}