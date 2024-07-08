#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <pthread.h>
#include <ctype.h>

#define MAX_THREADS 9
#define MAP_ANONYMOUS 0x20

#pragma pack(push)
#pragma pack(1)

typedef struct {
    unsigned char id[2];
    unsigned int bmp_size;
    unsigned char unused[4];
    unsigned int data_offset;
} BMPHeader;

typedef struct {
    unsigned int header_size;
    unsigned int width;
    unsigned int height;
    unsigned short color_planes;
    unsigned short bits_per_pixel;
    unsigned int compression;
    unsigned int data_size;
    unsigned int pwidth;
    unsigned int pheight;
    unsigned int colors_count;
    unsigned int imp_colors_count;
} DIBHeader;

typedef struct {
    BMPHeader bhdr;
    DIBHeader dhdr;
    unsigned char* data;
} BMPFile;

typedef struct {
    int* shared_rgb_sum;
    BMPFile* bmp_file;
    int k;
} PT_ARGS;

#pragma pack(pop)



int ceiling(int a, int b) {
    return (int)(a/b) + 1;
}

int max_color(int r, int g, int b) {
    if (r >= g && r >= b) {
        return 0;
    }
    if (g > r && g >= b) {
        return 1;
    }
    if (b > r && b > g) {
        return 2;
    }
    return -1;
}

BMPFile* read_bmp(char* fname) {
    FILE* fp = fopen(fname, "rb");
    if (!fp) {
        printf("ERROR! The program hasn't read bmp-file '%s'\n", fname);
        exit(0);
    }

    BMPFile* bmp_file = (BMPFile*)malloc(sizeof(BMPFile));
    fread(&bmp_file->bhdr, sizeof(BMPHeader), 1, fp);
    fread(&bmp_file->dhdr, sizeof(DIBHeader), 1, fp);

    int row_bytesize_with_pad = ceiling(bmp_file->dhdr.bits_per_pixel*bmp_file->dhdr.width, 32)*4;

    bmp_file->data = (unsigned char*)malloc(row_bytesize_with_pad*bmp_file->dhdr.height);
    fseek(fp, bmp_file->bhdr.data_offset, SEEK_SET);
    fread(bmp_file->data, 1,
          row_bytesize_with_pad*bmp_file->dhdr.height, fp);

    fclose(fp);
    return bmp_file;
}

void free_bmp(BMPFile* bmp_file) {
    if (bmp_file) {
        if (bmp_file->data) {
            free(bmp_file->data);
        }
        free(bmp_file);
    }
}

pthread_mutex_t mutex;
pthread_mutexattr_t mutex_attr;

void* read_bmp_thread(void* arg) {
    PT_ARGS* p_arg = (PT_ARGS*)arg;
    int row_bytesize_with_pad = ceiling(p_arg->bmp_file->dhdr.bits_per_pixel*p_arg->bmp_file->dhdr.width, 32)*4;
    int res;
    int* rgb_sum = (int*)malloc(3*sizeof(int));
    rgb_sum[0] = 0;
    rgb_sum[1] = 0;
    rgb_sum[2] = 0;

    for (int i = 0; i < p_arg->bmp_file->dhdr.height; ++i) {
        for (int j = p_arg->k*p_arg->bmp_file->dhdr.width / MAX_THREADS; j < (p_arg->k+1)*p_arg->bmp_file->dhdr.width / MAX_THREADS; ++j) {
            res = max_color(p_arg->bmp_file->data[i*row_bytesize_with_pad + 3*j + 2],
                            p_arg->bmp_file->data[i*row_bytesize_with_pad + 3*j + 1],
                            p_arg->bmp_file->data[i*row_bytesize_with_pad + 3*j]);
            if (res == 0) {
                rgb_sum[0]++;
            }
            else if (res == 1) {
                rgb_sum[1]++;
            }
            else if (res == 2) {
                rgb_sum[2]++;
            }
        }
    }
    pthread_mutex_lock(&mutex);
        printf("Process %d:\nred_count: %d\ngreen_count: %d\nblue_count: %d\n\n",
            p_arg->k+1, rgb_sum[0], rgb_sum[1], rgb_sum[2]);
        p_arg->shared_rgb_sum[0] += rgb_sum[0];
        p_arg->shared_rgb_sum[1] += rgb_sum[1];
        p_arg->shared_rgb_sum[2] += rgb_sum[2];
    pthread_mutex_unlock(&mutex);
    free(rgb_sum);
    return NULL;
}

int main() {
    int* shared_memory = mmap(NULL, 3*sizeof(int), PROT_READ | PROT_WRITE,
                                MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    shared_memory[0] = 0;
    shared_memory[1] = 0;
    shared_memory[2] = 0;

    // Ниже содержится путь к bmp-файлу
    char* fname = "House.bmp";

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&mutex, &mutex_attr);

    pid_t pid[MAX_THREADS];
    BMPFile* bmpf = read_bmp(fname);

    PT_ARGS p_arg[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i) {
        pid[i] = fork();
        if (pid[i] == 0) {
            p_arg[i].shared_rgb_sum = shared_memory;
            p_arg[i].bmp_file = bmpf;
            p_arg[i].k = i;
            read_bmp_thread(&p_arg[i]);
            exit(EXIT_SUCCESS);
        }
    }

    
    
    for (int i = 0; i < MAX_THREADS; ++i) {
        waitpid(pid[i], NULL, 0);
    }

    printf("Global output:\nred_count: %d\ngreen_count: %d\nblue_count: %d\n\n",
            shared_memory[0], shared_memory[1], shared_memory[2]);
    free_bmp(bmpf);
    return 0;
}