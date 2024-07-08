#include <stdio.h>
#include <stdlib.h>

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

    unsigned int bytes_per_pixel = bmp_file->dhdr.bits_per_pixel / 8;
    int row_bytesize_with_pad = ceiling(bmp_file->dhdr.bits_per_pixel*bmp_file->dhdr.width, 32)*4;
    int row_bytesize = bytes_per_pixel*bmp_file->dhdr.width;
    int row_padding = row_bytesize_with_pad - bytes_per_pixel*bmp_file->dhdr.width;

    unsigned char* data_with_pad = (unsigned char*)malloc(row_bytesize_with_pad*bmp_file->dhdr.height);
    
    fseek(fp, bmp_file->bhdr.data_offset, SEEK_SET);
    fread(data_with_pad, 1,
          row_bytesize_with_pad*bmp_file->dhdr.height, fp);

    int i = 0;
    int j;
    int r_sum = 0;
    int g_sum = 0;
    int b_sum = 0;
    int res = -1;

    while (i < bmp_file->dhdr.height) {
        j = -1;
        while (j < row_bytesize-1) {
            j += 3;
            res = max_color(data_with_pad[i*row_bytesize_with_pad + j],
                            data_with_pad[i*row_bytesize_with_pad + j-1],
                            data_with_pad[i*row_bytesize_with_pad + j-2]);
            if (res == 0) {
                r_sum++;
            }
            else if (res == 1) {
                g_sum++;
            }
            else if (res == 2) {
                b_sum++;
            }
        }
        i++;
    }
    printf("count_red: %d\ncount_green: %d\ncount_blue: %d\n", r_sum, g_sum, b_sum);
    fclose(fp);
    bmp_file->data = data_with_pad;
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

int main(int argc, char* argv[]) {
    char* fname = "House.bmp";
    BMPFile* bmpf = read_bmp(fname);
    printf("\n%d", bmpf->dhdr.bits_per_pixel);
    free_bmp(bmpf);
    return 0;
}
