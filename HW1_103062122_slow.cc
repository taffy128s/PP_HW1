#include <cstdio>
#include <algorithm>
#include <assert.h>
#include <cfloat>
#include <mpi.h>

int main(int argc, char** argv) {
    assert(argc == 4);
    MPI_Init(&argc, &argv);
    MPI_Offset file_size;
    MPI_File rfh, wfh;
    MPI_File_open(MPI_COMM_WORLD, argv[2], MPI_MODE_RDONLY, MPI_INFO_NULL, &rfh);
    MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &wfh);
    
    int total_floats;
    int input_size = atoi(argv[1]);
    if (input_size == 0) {
        MPI_File_get_size(rfh, &file_size);
        total_floats = file_size / sizeof(float);
    } else {
        total_floats = input_size;
    }
    int rank, size, buf_size, buf_size_last, done, done_all = 0, interior_idx = 0, global_idx = 0, min_process;
    float *buf, min;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    buf_size = total_floats / size;
    if (size == 1 || buf_size == 0) { // only one process, or number of process > number of floats
        if (rank == 0) {
            buf_size = total_floats;
            buf = new float[buf_size];
            MPI_File_read_at(rfh, 0, buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
            std::sort(buf, buf + buf_size);
            MPI_File_write_at(wfh, 0, buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        }
        MPI_Finalize();
        return 0;
    }
    buf_size_last = buf_size + total_floats % size;
    MPI_Request req;
    if (rank == size - 1) { // last process
        buf = new float[buf_size_last];
        MPI_File_read_at(rfh, rank * buf_size * sizeof(float), buf, buf_size_last, MPI_FLOAT, MPI_STATUS_IGNORE);
        std::sort(buf, buf + buf_size_last);
        while (!done_all) {
            if (interior_idx == buf_size_last) {
                done = 1;
                MPI_Recv(&min, 1, MPI_FLOAT, size - 2, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&min_process, 1, MPI_INT, size - 2, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Bcast (&min, 1, MPI_FLOAT, size - 1, MPI_COMM_WORLD);
                MPI_Bcast (&min_process, 1, MPI_INT, size - 1, MPI_COMM_WORLD);
            } else {
                done = 0;
                MPI_Recv(&min, 1, MPI_FLOAT, size - 2, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&min_process, 1, MPI_INT, size - 2, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                //printf("rank: %d received\n", rank);
                if (buf[interior_idx] <= min) {
                    min_process = rank;
                    min = buf[interior_idx++];
                    MPI_File_write_at(wfh, global_idx * sizeof(float), &min, 1, MPI_FLOAT, MPI_STATUS_IGNORE);
                }
                MPI_Bcast(&min, 1, MPI_FLOAT, size - 1, MPI_COMM_WORLD);
                MPI_Bcast(&min_process, 1, MPI_INT, size - 1, MPI_COMM_WORLD);
                //printf("rank: %d has broadcast\n", rank);
            }
            MPI_Allreduce(&done, &done_all, 1, MPI_INT, MPI_BAND, MPI_COMM_WORLD);
            global_idx++;
        }
    } else if (rank == 0) { // first process
        buf = new float[buf_size];
        MPI_File_read_at(rfh, rank * buf_size * sizeof(float), buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        std::sort(buf, buf + buf_size);
        while (!done_all) {
            if (interior_idx == buf_size) {
                done = 1;
                min = FLT_MAX;
                min_process = -1;
                MPI_Send(&min, 1, MPI_FLOAT, 1, 0, MPI_COMM_WORLD);
                MPI_Send(&min_process, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
                MPI_Bcast(&min, 1, MPI_FLOAT, size - 1, MPI_COMM_WORLD);
                MPI_Bcast(&min_process, 1, MPI_INT, size - 1, MPI_COMM_WORLD);
            } else {
                done = 0;
                min = buf[interior_idx];
                min_process = 0;
                MPI_Send(&min, 1, MPI_FLOAT, 1, 0, MPI_COMM_WORLD);
                MPI_Send(&min_process, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
                //printf("rank: %d sent\n", rank);
                MPI_Bcast(&min, 1, MPI_FLOAT, size - 1, MPI_COMM_WORLD);
                MPI_Bcast(&min_process, 1, MPI_INT, size - 1, MPI_COMM_WORLD);
                //printf("rank: %d received broadcast\n", rank);
                if (min_process == 0) {
                    MPI_File_write_at(wfh, global_idx * sizeof(float), &min, 1, MPI_FLOAT, MPI_STATUS_IGNORE);
                    interior_idx++;
                }
            }
            MPI_Allreduce(&done, &done_all, 1, MPI_INT, MPI_BAND, MPI_COMM_WORLD);
            global_idx++;
        }
    } else { // other processes
        buf = new float[buf_size];
        MPI_File_read_at(rfh, rank * buf_size * sizeof(float), buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        std::sort(buf, buf + buf_size);
        while (!done_all) {
            if (interior_idx == buf_size) {
                done = 1;
                MPI_Recv(&min, 1, MPI_FLOAT, rank - 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&min_process, 1, MPI_INT, rank - 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(&min, 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD);
                MPI_Send(&min_process, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
                MPI_Bcast(&min, 1, MPI_FLOAT, size - 1, MPI_COMM_WORLD);
                MPI_Bcast(&min_process, 1, MPI_INT, size - 1, MPI_COMM_WORLD);
            } else {
                done = 0;
                MPI_Recv(&min, 1, MPI_FLOAT, rank - 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(&min_process, 1, MPI_INT, rank - 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                //printf("rank: %d received\n", rank);
                if (buf[interior_idx] <= min) {
                    min_process = rank;
                    min = buf[interior_idx];
                }
                MPI_Send(&min, 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD);
                MPI_Send(&min_process, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
                //printf("rank: %d sent\n", rank);
                MPI_Bcast(&min, 1, MPI_FLOAT, size - 1, MPI_COMM_WORLD);
                MPI_Bcast(&min_process, 1, MPI_INT, size - 1, MPI_COMM_WORLD);
                //printf("rank: %d received broadcast\n", rank);
                if (min_process == rank) {
                    MPI_File_write_at(wfh, global_idx * sizeof(float), &min, 1, MPI_FLOAT, MPI_STATUS_IGNORE);
                    interior_idx++;
                }
            }
            MPI_Allreduce(&done, &done_all, 1, MPI_INT, MPI_BAND, MPI_COMM_WORLD);
            global_idx++;
        }
    }
    MPI_Finalize();
    return 0;
}
