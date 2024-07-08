#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

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

typedef struct {
    BMPFile* bmp_file;
    int k;
} MYDATA, * PMYDATA;

#pragma pack(pop)

int ceiling(int a, int b) {
    return (int)(a / b) + 1;
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

    int row_bytesize_with_pad = ceiling(bmp_file->dhdr.bits_per_pixel * bmp_file->dhdr.width, 32) * 4;

    bmp_file->data = (unsigned char*)malloc(row_bytesize_with_pad * bmp_file->dhdr.height);
    fseek(fp, bmp_file->bhdr.data_offset, SEEK_SET);
    fread(bmp_file->data, 1,
        row_bytesize_with_pad * bmp_file->dhdr.height, fp);
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

int rgb_sum_global[3] = { 0, 0, 0 };

DWORD WINAPI read_bmp_thread(LPVOID lpParam) {
    PMYDATA pattr = (PMYDATA)lpParam;
    int row_bytesize_with_pad = ceiling(pattr->bmp_file->dhdr.bits_per_pixel * pattr->bmp_file->dhdr.width, 32) * 4;
    int res;
    int* rgb_sum = (int*)malloc(3 * sizeof(int));
    rgb_sum[0] = 0;
    rgb_sum[1] = 0;
    rgb_sum[2] = 0;

    for (int i = 0; i < pattr->bmp_file->dhdr.height; ++i) {
        for (int j = pattr->k * pattr->bmp_file->dhdr.width / MAX_THREADS; j < (pattr->k + 1) * pattr->bmp_file->dhdr.width / MAX_THREADS; ++j) {
            res = max_color(pattr->bmp_file->data[i * row_bytesize_with_pad + 3 * j + 2],
                pattr->bmp_file->data[i * row_bytesize_with_pad + 3 * j + 1],
                pattr->bmp_file->data[i * row_bytesize_with_pad + 3 * j]);
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
        pattr->k + 1, rgb_sum[0], rgb_sum[1], rgb_sum[2]);

    rgb_sum_global[0] += rgb_sum[0];
    rgb_sum_global[1] += rgb_sum[1];
    rgb_sum_global[2] += rgb_sum[2];

    free(rgb_sum);
    
    return 0;
}

int main() {
    PMYDATA pDataArray[MAX_THREADS];
    HANDLE hThreadArray[MAX_THREADS];

    char* fname = "House.bmp";

    BMPFile* bmpf = read_bmp(fname);

    for (int i = 0; i < MAX_THREADS; ++i) {
        pDataArray[i] = (PMYDATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(MYDATA));

        if (pDataArray[i] == NULL) {
            ExitProcess(2);
        }

        pDataArray[i]->bmp_file = bmpf;
        pDataArray[i]->k = i;

        hThreadArray[i] = CreateThread(
            NULL,
            0,
            read_bmp_thread,
            pDataArray[i],
            0,
            NULL);


        if (hThreadArray[i] == NULL) {
            ExitProcess(3);
        }
    }

    WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);

    for (int i = 0; i < MAX_THREADS; ++i) {
        CloseHandle(hThreadArray[i]);
        if (pDataArray[i] != NULL) {
            HeapFree(GetProcessHeap(), 0, pDataArray[i]);
            pDataArray[i] = NULL;
        }
    }

    printf("Global output:\nred_count: %d\ngreen_count: %d\nblue_count: %d\n\n",
        rgb_sum_global[0], rgb_sum_global[1], rgb_sum_global[2]);
    free_bmp(bmpf);

    return 0;
}
