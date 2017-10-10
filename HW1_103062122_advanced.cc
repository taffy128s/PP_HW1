#include <cstdio>
#include <cfloat>
#include <algorithm>
#include <assert.h>
#include <mpi.h>

int main(int argc, char** argv) {
    assert(argc == 4);
    MPI_Init(&argc, &argv);
    MPI_Offset file_size;
    MPI_File rfh, wfh;
    MPI_File_open(MPI_COMM_WORLD, argv[2], MPI_MODE_RDONLY, MPI_INFO_NULL, &rfh);
    MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &wfh);
    
    clock_t begin, end;
    double IO_time = 0, computing_time = 0;
    int total_floats;
    int input_size = atoi(argv[1]);
    if (input_size == 0) {
        MPI_File_get_size(rfh, &file_size);
        total_floats = file_size / sizeof(float);
    } else
        total_floats = input_size;
    int rank, size, buf_size, buf_size_last;
    float *buf;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    buf_size = total_floats / size;
    if (size == 1 || buf_size == 0) { // only one process, or number of process > number of floats
        if (rank == 0) {
            buf_size = total_floats;
            buf = new float[buf_size];
            begin = clock();
            MPI_File_read_at(rfh, 0, buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
            end = clock();
            IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
            begin = clock();
            std::sort(buf, buf + buf_size);
            end = clock();
            computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
            begin = clock();
            MPI_File_write_at(wfh, 0, buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
            end = clock();
            IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
        }
        printf("[%02d] Computing time: %.2f, IO time: %.2f, total time: %.2f\n", rank, computing_time, IO_time, computing_time + IO_time);
        MPI_Finalize();
        return 0;
    }
    buf_size_last = buf_size + total_floats % size;
    MPI_Request req;
    buf = new float[total_floats];
    begin = clock();
    MPI_File_read_at(rfh, 0, buf, total_floats, MPI_FLOAT, MPI_STATUS_IGNORE);
    end = clock();
    IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
    if (rank == 0) {
        begin = clock();
        std::nth_element(buf, buf + buf_size, buf + total_floats);
        std::sort(buf, buf + buf_size);
        end = clock();
        computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
        begin = clock();
        MPI_File_write_at(wfh, 0, buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
    } else if (rank == size - 1) {
        int start_idx = rank * buf_size;
        begin = clock();
        std::nth_element(buf, buf + start_idx, buf + total_floats);
        std::sort(buf + start_idx, buf + total_floats);
        end = clock();
        computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
        begin = clock();
        MPI_File_write_at(wfh, start_idx * sizeof(float), buf + start_idx, buf_size_last, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
    } else {
        int start_idx = rank * buf_size, end_idx = start_idx + buf_size;
        begin = clock();
        std::nth_element(buf, buf + start_idx, buf + total_floats);
        std::nth_element(buf + start_idx + 1, buf + end_idx, buf + total_floats);
        std::sort(buf + start_idx, buf + end_idx);
        end = clock();
        computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
        begin = clock();
        MPI_File_write_at(wfh, start_idx * sizeof(float), buf + start_idx, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
    }
    printf("[%02d] Computing time: %.2f, IO time: %.2f, total time: %.2f\n", rank, computing_time, IO_time, computing_time + IO_time);
    MPI_File_close(&rfh);
    MPI_File_close(&wfh);
    MPI_Finalize();
    return 0;
}
