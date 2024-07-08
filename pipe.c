#include <sys/wait.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#define MAX_THREADS 9

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


int* read_bmp_thread(BMPFile* bmp_file, int k) {
    int row_bytesize_with_pad = ceiling(bmp_file->dhdr.bits_per_pixel*bmp_file->dhdr.width, 32)*4;
    int res;
    int* rgb_sum = (int*)malloc(3*sizeof(int));
    rgb_sum[0] = 0;
    rgb_sum[1] = 0;
    rgb_sum[2] = 0;

    for (int i = 0; i < bmp_file->dhdr.height; ++i) {
        for (int j = k*bmp_file->dhdr.width / MAX_THREADS; j < (k+1)*bmp_file->dhdr.width / MAX_THREADS; ++j) {
            res = max_color(bmp_file->data[i*row_bytesize_with_pad + 3*j + 2],
                            bmp_file->data[i*row_bytesize_with_pad + 3*j + 1],
                            bmp_file->data[i*row_bytesize_with_pad + 3*j]);
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
    printf("Process %d:\nred_count: %d\ngreen_count: %d\nblue_count: %d\n\n",
            k+1, rgb_sum[0], rgb_sum[1], rgb_sum[2]);
    return rgb_sum;

    
}

int main() {
    int rgb_sl[3];
    int rgb_gl[3] = {0, 0, 0};
    int pipefd[MAX_THREADS][2];
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe creation failed");
            exit(EXIT_FAILURE);
        }
    }

    // Ниже содержится путь к bmp-файлу
    char* fname = "House.bmp";

    pid_t pid[MAX_THREADS];
    BMPFile* bmpf = read_bmp(fname);
    for (int i = 0; i < MAX_THREADS; ++i) {
        pid[i] = fork();
        if (pid[i] == 0) {
            close(pipefd[i][0]);
            int* rgb_sum = read_bmp_thread(bmpf, i);
            write(pipefd[i][1], rgb_sum, 3*sizeof(int));
            free(rgb_sum);
            close(pipefd[i][1]);
            exit(EXIT_SUCCESS);
        }
        else if (pid[i] > 0) {
            close(pipefd[i][1]);
            read(pipefd[i][0], rgb_sl, 3*sizeof(int));
            close(pipefd[i][0]);
            rgb_gl[0] += rgb_sl[0];
            rgb_gl[1] += rgb_sl[1];
            rgb_gl[2] += rgb_sl[2];
        }
        else if (pid[i] == -1) {
            perror("process creation failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < MAX_THREADS; ++i) {
        waitpid(pid[i], NULL, 0);
    }

    printf("Global output:\nred_count: %d\ngreen_count: %d\nblue_count: %d\n\n",
            rgb_gl[0], rgb_gl[1], rgb_gl[2]);
    free_bmp(bmpf);
    return 0;
}
