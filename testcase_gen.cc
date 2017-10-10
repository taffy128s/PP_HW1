#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cfloat>
#include <assert.h>
#include <mpi.h>
#include <ctime>
#define MAGIC_NUMBER 48

int main(int argc, char** argv) {
    assert(argc == 2);
    MPI_Init(&argc, &argv);
    MPI_Offset file_size;
    MPI_File wfh;
    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &wfh);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (size != 1) {
        MPI_Finalize();
        return 0;
    }
    srand (time(NULL));
    if (MAGIC_NUMBER < 1000) {
        float output_arr[MAGIC_NUMBER];
        for (int i = 0; i < MAGIC_NUMBER; i++) {
            output_arr[i] = (float) (rand() % MAGIC_NUMBER);
        }
        MPI_File_write_at(wfh, 0, output_arr, MAGIC_NUMBER, MPI_FLOAT, MPI_STATUS_IGNORE);
        MPI_Finalize();
        return 0;
    }
    int wrt_byte_idx = 0;
    float output_arr[MAGIC_NUMBER / 1000];
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < MAGIC_NUMBER / 1000; j++) {
            output_arr[j] = (float) (rand() % MAGIC_NUMBER);
        }
        MPI_File_write_at(wfh, wrt_byte_idx, output_arr, MAGIC_NUMBER / 1000, MPI_FLOAT, MPI_STATUS_IGNORE);
        wrt_byte_idx += (MAGIC_NUMBER / 1000 * sizeof(float));
    }
    MPI_Finalize();
    return 0;
}
