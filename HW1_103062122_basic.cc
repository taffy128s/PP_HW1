#include <cstdio>
#include <algorithm>
#include <assert.h>
#include <mpi.h>

int recvCompareAndSend(float *toCompare, int target) {
    float buf_left;
    MPI_Request req;
    MPI_Recv(&buf_left, 1, MPI_FLOAT, target, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (buf_left > *toCompare) {
        MPI_Isend(toCompare, 1, MPI_FLOAT, target, 0, MPI_COMM_WORLD, &req);
        *toCompare = buf_left;
        return 1;
    } else {
        MPI_Isend(&buf_left, 1, MPI_FLOAT, target, 0, MPI_COMM_WORLD, &req);
        return 0;
    }
}

int swapInAnInterval(float *buf, int start, int end) {
    int cont = 0;
    for (int i = start; i < end; i += 2) {
        if (buf[i + 1] < buf[i]) {
            std::swap(buf[i], buf[i + 1]);
            cont = 1;
        }
    }
    return cont;
}

int main(int argc, char** argv) {
    assert(argc == 4);
    MPI_Init(&argc, &argv);
    MPI_Offset file_size;
    MPI_File rfh, wfh;
    MPI_File_open(MPI_COMM_WORLD, argv[2], MPI_MODE_RDONLY, MPI_INFO_NULL, &rfh);
    MPI_File_open(MPI_COMM_WORLD, argv[3], MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &wfh);
    
    clock_t begin, end;
    double IO_time = 0, computing_time = 0, communication_time = 0;
    int total_floats;
    int input_size = atoi(argv[1]);
    if (input_size == 0) {
        MPI_File_get_size(rfh, &file_size);
        total_floats = file_size / sizeof(float);
    } else
        total_floats = input_size;
    int rank, size, buf_size, buf_size_last, cont, cont_all = 1;
    float buf_left;
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
            while (cont_all) {
                cont_all = 0;
                cont_all |= swapInAnInterval(buf, 0, buf_size - 1);
                cont_all |= swapInAnInterval(buf, 1, buf_size - 1);
            }
            end = clock();
            computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
            begin = clock();
            MPI_File_write_at(wfh, 0, buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
            end = clock();
            IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
        }
        printf("[%02d] Computing time: %.2f, IO time: %.2f, Communication time: %.2f, total time: %.2f\n", 
                rank, computing_time, IO_time, communication_time, computing_time + IO_time + communication_time);
        MPI_Finalize();
        return 0;
    }
    buf_size_last = buf_size + total_floats % size;
    MPI_Request req;
    if (rank == size - 1) { // last process
        buf = new float[buf_size_last];
        begin = clock();
        MPI_File_read_at(rfh, rank * buf_size * sizeof(float), buf, buf_size_last, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
        while (cont_all) {
            cont = 0;
            if (buf_size * (size - 1) % 2 == 1) { // we have an odd number of floats before
                begin = clock();
                cont |= swapInAnInterval(buf, 1, buf_size_last - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= recvCompareAndSend(&buf[0], size - 2);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= swapInAnInterval(buf, 0, buf_size_last - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
            } else {
                begin = clock();
                cont |= swapInAnInterval(buf, 0, buf_size_last - 1);
                cont |= swapInAnInterval(buf, 1, buf_size_last - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= recvCompareAndSend(&buf[0], size - 2);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
            }
            begin = clock();
            MPI_Allreduce(&cont, &cont_all, 1, MPI_INT, MPI_BOR, MPI_COMM_WORLD);
            end = clock();
            communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
        }
        begin = clock();
        MPI_File_write_at(wfh, rank * buf_size * sizeof(float), buf, buf_size_last, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
    } else if (rank == 0) { // first process
        buf = new float[buf_size];
        begin = clock();
        MPI_File_read_at(rfh, rank * buf_size * sizeof(float), buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
        while (cont_all) {
            cont = 0;
            if (buf_size % 2 == 1) { // buffer size is an odd number
                begin = clock();
                MPI_Isend(&buf[buf_size - 1], 1, MPI_FLOAT, 1, 0, MPI_COMM_WORLD, &req);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= swapInAnInterval(buf, 0, buf_size - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                MPI_Recv(&buf[buf_size - 1], 1, MPI_FLOAT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= swapInAnInterval(buf, 1, buf_size - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
            } else {
                begin = clock();
                cont |= swapInAnInterval(buf, 0, buf_size - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                MPI_Isend(&buf[buf_size - 1], 1, MPI_FLOAT, 1, 0, MPI_COMM_WORLD, &req);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= swapInAnInterval(buf, 1, buf_size - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                MPI_Recv(&buf[buf_size - 1], 1, MPI_FLOAT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
            }
            begin = clock();
            MPI_Allreduce(&cont, &cont_all, 1, MPI_INT, MPI_BOR, MPI_COMM_WORLD);
            end = clock();
            communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
        }
        begin = clock();
        MPI_File_write_at(wfh, rank * buf_size * sizeof(float), buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
    } else { // other processes
        buf = new float[buf_size];
        begin = clock();
        MPI_File_read_at(rfh, rank * buf_size * sizeof(float), buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
        while (cont_all) {
            cont = 0;
            if (buf_size * rank % 2 == 1) { // we have an odd number of floats before, and buffer size is also an odd number
                begin = clock();
                cont |= swapInAnInterval(buf, 1, buf_size - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= recvCompareAndSend(&buf[0], rank - 1);
                MPI_Isend(&buf[buf_size - 1], 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, &req);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                cont |= swapInAnInterval(buf, 0, buf_size - 1);
                end = clock();
                computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                begin = clock();
                MPI_Recv(&buf[buf_size - 1], 1, MPI_FLOAT, rank + 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                end = clock();
                communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
            } else {
                if (buf_size % 2 == 1) { // buffer size is an odd number
                    begin = clock();
                    MPI_Isend(&buf[buf_size - 1], 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, &req);
                    end = clock();
                    communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                    begin = clock();
                    cont |= swapInAnInterval(buf, 0, buf_size - 1);
                    end = clock();
                    computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                    begin = clock();
                    MPI_Recv(&buf[buf_size - 1], 1, MPI_FLOAT, rank + 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    end = clock();
                    communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                    begin = clock();
                    cont |= swapInAnInterval(buf, 1, buf_size - 1);
                    end = clock();
                    computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                    begin = clock();
                    cont |= recvCompareAndSend(&buf[0], rank - 1);
                    end = clock();
                    communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                } else {
                    begin = clock();
                    cont |= swapInAnInterval(buf, 0, buf_size - 1);
                    end = clock();
                    computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                    begin = clock();
                    MPI_Isend(&buf[buf_size - 1], 1, MPI_FLOAT, rank + 1, 0, MPI_COMM_WORLD, &req);
                    end = clock();
                    communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                    begin = clock();
                    cont |= swapInAnInterval(buf, 1, buf_size - 1);
                    end = clock();
                    computing_time += (double) (end - begin) / CLOCKS_PER_SEC;
                    begin = clock();
                    cont |= recvCompareAndSend(&buf[0], rank - 1);
                    MPI_Recv(&buf[buf_size - 1], 1, MPI_FLOAT, rank + 1, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    end = clock();
                    communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
                }
            }
            begin = clock();
            MPI_Allreduce(&cont, &cont_all, 1, MPI_INT, MPI_BOR, MPI_COMM_WORLD);
            end = clock();
            communication_time += (double) (end - begin) / CLOCKS_PER_SEC;
        }
        begin = clock();
        MPI_File_write_at(wfh, rank * buf_size * sizeof(float), buf, buf_size, MPI_FLOAT, MPI_STATUS_IGNORE);
        end = clock();
        IO_time += (double) (end - begin) / CLOCKS_PER_SEC;
    }
    printf("[%02d] Computing time: %.2f, IO time: %.2f, Communication time: %.2f, total time: %.2f\n", 
            rank, computing_time, IO_time, communication_time, computing_time + IO_time + communication_time);
    MPI_File_close(&rfh);
    MPI_File_close(&wfh);
    MPI_Finalize();
    return 0;
}
