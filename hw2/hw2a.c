#define PNG_NO_SETJMP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>
#include<assert.h>
#include<png.h>
#include<string.h>

typedef enum{
    true = 1==1,
    false = 1==0
}bool;

typedef struct task_data{
    int thread_id;
    int thread_num;
    int *image;
    int height;
    double upper;
    double lower;
    int width;
    double right;
    double left;
    int iters;
}Task_Data;

Task_Data* malloc_task_data(int thread_id, int thread_num, int *image, int height, double upper, double lower, int width, double right, double left, int iters){
    Task_Data *data = (Task_Data*)malloc(sizeof(Task_Data));
    data->thread_id = thread_id;
    data->thread_num = thread_num;
    data->image = image;
    data->height = height;
    data->upper = upper;
    data->lower = lower;
    data->width = width;
    data->right = right;
    data->left = left;
    data->iters = iters;
    return data;
}

void set_task_data(Task_Data *data_p, int thread_id, int thread_num, int *image, int height, double upper, double lower, int width, double right, double left, int iters){
    data_p->thread_id = thread_id;
    data_p->thread_num = thread_num;
    data_p->image = image;
    data_p->height = height;
    data_p->upper = upper;
    data_p->lower = lower;
    data_p->width = width;
    data_p->right = right;
    data_p->left = left;
    data_p->iters = iters;
    // return data_p;
}

Task_Data get_task_data(int thread_id, int thread_num, int *image, int height, double upper, double lower, int width, double right, double left, int iters){
    Task_Data data;
    data.thread_id = thread_id;
    data.thread_num = thread_num;
    data.image = image;
    data.height = height;
    data.upper = upper;
    data.lower = lower;
    data.width = width;
    data.right = right;
    data.left = left;
    data.iters = iters;
    return data;
}

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

int* malloc_image(int height, int width){
    int *image = (int*)malloc(width * height * sizeof(int));
    assert(image);
    return image;
}
int monitor(pthread_t **thread_array, Task_Data **task_data_array){
    // int thread_num = 5;

    cpu_set_t cpu_set;
    sched_getaffinity(0, sizeof(cpu_set), &cpu_set);
    int thread_num = CPU_COUNT(&cpu_set);
    (*thread_array) = (pthread_t*)malloc(sizeof(pthread_t) * thread_num);
    (*task_data_array) = (Task_Data*)malloc(sizeof(Task_Data) * thread_num);

    return thread_num;
}
void compute(int thread_id, int thread_num, int *image, int height, double upper, double lower, int width, double right, double left, int iters){
    for (int j = thread_id; j < height; j+=thread_num) {
        double y0 = j * ((upper - lower) / height) + lower;
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
            image[j * width + i] = repeats;
        }
    }
}

void *parallel_task(void *data){
    Task_Data *data_p = (Task_Data *)data;
    // printf("Hello, here is thread %d\n", data_p->thread_id);

    //Computing
    compute(data_p->thread_id, data_p->thread_num, data_p->image, data_p->height, data_p->upper, data_p->lower, data_p->width, data_p->right, data_p->left, data_p->iters);

    pthread_exit(NULL);
}



void dispatcher(int thread_num, int *image, pthread_t *thread_array, Task_Data *task_data_array, int height, double upper, double lower, int width, double right, double left, int iters){
    for(int thread_id=0; thread_id<thread_num; thread_id++){
        // Task_Data *data = malloc_task_data(thread_id, thread_num, image, height, upper, lower, width, right, left, iters);
         set_task_data(task_data_array + thread_id, thread_id, thread_num, image, height, upper, lower, width, right, left, iters);
         Task_Data *data = task_data_array + thread_id;
        // parallel_task((void*)data;

        // pthread_t *thread_p = (pthread_t*)malloc(sizeof(pthread_t));
        pthread_t *thread_p = thread_array + thread_id;
        pthread_create(thread_p, NULL, parallel_task, (void *)data);
    }
}

bool threads_join(int thread_num, pthread_t *thread_array){
    bool is_successful = true;
    for(int i=0; i<thread_num; i++){
        int ret = pthread_join(thread_array[i], NULL);
        if(ret != 0){
            is_successful = false;
            // printf("Thread %d exit unsuccessful\n");
        }
    }
    return is_successful;
}


int main(int argc, char *argv[]){
    // printf("Hello World\n");
    /* argument parsing */
    // assert(argc == 9);

    const char* filename = argv[4];
    int iters = strtol(argv[5], 0, 10);
    double left = strtod(argv[6], 0);
    double right = strtod(argv[7], 0);
    double lower = strtod(argv[8], 0);
    double upper = strtod(argv[9], 0);
    int width = strtol(argv[10], 0, 10);
    int height = strtol(argv[11], 0, 10);

    // const char* filename = argv[1];
    // int iters = strtol(argv[2], 0, 10);
    // double left = strtod(argv[3], 0);
    // double right = strtod(argv[4], 0);
    // double lower = strtod(argv[5], 0);
    // double upper = strtod(argv[6], 0);
    // int width = strtol(argv[7], 0, 10);
    // int height = strtol(argv[8], 0, 10);

    // const char* filename = NULL;
    // int iters = -1;
    // double left = -1.0;
    // double right = -1.0;
    // double lower = -1.0;
    // double upper = -1.0;
    // int width = -1;
    // int height = -1;
    // if(argc == 9){
    //     filename = argv[1];
    //     iters = strtol(argv[2], 0, 10);
    //     left = strtod(argv[3], 0);
    //     right = strtod(argv[4], 0);
    //     lower = strtod(argv[5], 0);
    //     upper = strtod(argv[6], 0);
    //     width = strtol(argv[7], 0, 10);
    //     height = strtol(argv[8], 0, 10);
    // }else if(argc == 12){
    //     filename = argv[4];
    //     iters = strtol(argv[5], 0, 10);
    //     left = strtod(argv[6], 0);
    //     right = strtod(argv[7], 0);
    //     lower = strtod(argv[8], 0);
    //     upper = strtod(argv[9], 0);
    //     width = strtol(argv[10], 0, 10);
    //     height = strtol(argv[11], 0, 10);
    // }

    /* allocate memory for image */
    int* image = (int*)malloc(width * height * sizeof(int));
    assert(image);

    // Parallelize
    pthread_t *thread_array = NULL;
    Task_Data *task_data_array = NULL;
    int thread_num = monitor(&thread_array, &task_data_array);
    dispatcher(thread_num, image, thread_array, task_data_array, height, upper, lower, width, right, left, iters);
    threads_join(thread_num, thread_array);

    /* draw and cleanup */
    write_png(filename, iters, width, height, image);
    free(image);
}