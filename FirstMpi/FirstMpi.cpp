#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include "mpi.h"

using namespace std;

// Reading Data
vector<int> read_data_from_file(const string& filename) {
    ifstream file(filename);
    vector<int> data;
    if (!file.is_open()) {
        cerr << "Error: Unable to open file " << filename << endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    int value;
    while (file >> value) {
        data.push_back(value);
    }
    file.close();
    return data;
}

// Distributig Data
void distribute_data(const vector<int>& data, int size, int rank, vector<int>& local_data, int& local_size) {
    if (rank == 0) {
        int total_size = data.size();
        int base_size = total_size / size;
        int remainder = total_size % size;
        int start = 0;
        for (int i = 0; i < size; i++) {
            int current_size = base_size + (i < remainder ? 1 : 0);
            if (i == rank) {
                local_data.assign(data.begin() + start, data.begin() + start + current_size);
                local_size = current_size;
            }
            else {
                MPI_Send(&data[start], current_size, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
            start += current_size;
        }
    }
    else {
        MPI_Status status;
        int count;
        MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &count);
        local_data.resize(count);
        MPI_Recv(local_data.data(), count, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        local_size = count;
    }
}


void quick_search(int rank, int size) {
    double total_start_time = MPI_Wtime(); 

    int target;
    vector<int> data;
    vector<int> local_data;
    int local_size;
    double file_read_start, file_read_end ,search_start, search_end, input_time_start , input_time_end;
   

    if (rank == 0) {
        cout << "Quick Search Selected\n";
        cout << "------------------------------\n";
        string filename;
        cout << "Please enter the path to the input file: ";
        input_time_start = MPI_Wtime();
        cin >> filename;
        cout << "Enter Search Target: ";
        cin >> target;
        input_time_end = MPI_Wtime();

        cout << "Reading data from file...\n";
        file_read_start = MPI_Wtime();
        data = read_data_from_file(filename);
        file_read_end = MPI_Wtime();
    }

    MPI_Bcast(&target, 1, MPI_INT, 0, MPI_COMM_WORLD);

    distribute_data(data, size, rank, local_data, local_size);

    int start_index = 0;
    vector<int> displacements(size, 0);
    if (rank == 0) {
        int total_size = data.size();
        int base_size = total_size / size;
        int remainder = total_size % size;
        int offset = 0;
        for (int i = 0; i < size; ++i) {
            displacements[i] = offset;
            int current_size = base_size + (i < remainder ? 1 : 0);
            offset += current_size;
        }
    }

    MPI_Scatter(displacements.data(), 1, MPI_INT, &start_index, 1, MPI_INT, 0, MPI_COMM_WORLD);

    search_start = MPI_Wtime();
    int local_result = -1;
    for (int i = 0; i < local_size; i++) {
        if (local_data[i] == target) {
            local_result = i;
            break;
        }
    }
    search_end = MPI_Wtime();

    int global_result = (local_result != -1) ? start_index + local_result : -1;

    vector<int> all_results(size);
    vector<double> all_search_times(size);
    MPI_Gather(&global_result, 1, MPI_INT, all_results.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    double local_search_duration = search_end - search_start;
    MPI_Gather(&local_search_duration, 1, MPI_DOUBLE, all_search_times.data(), 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        int final_result = -1;
        int found_by = -1;
        double search_time = 0.0;
        for (int i = 0; i < size; i++) {
            if (all_results[i] != -1) {
                final_result = all_results[i];
                found_by = i;
                search_time = all_search_times[i];
                break;
            }
        }

        if (final_result != -1) {
            cout << "[Process " << found_by << "] Found value " << target << " at index " << final_result << endl;
            cout << "Result: Value " << target << " found at index " << final_result << endl;
        }
        else {
            cout << "Result: Value " << target << " not found\n";
        }

        double total_end_time = MPI_Wtime();
        double total_input = input_time_end - input_time_start;
        double total_duration = total_end_time - total_start_time - total_input;
        double file_read_duration = file_read_end - file_read_start;

        cout << "\n========== Time ==========\n";
        cout << " File Reading Time      : " << file_read_duration << " seconds\n";
        cout << " Search Time (actual)   : " << search_time << " seconds\n";
        cout << " Total Execution Time    : " << total_duration << " seconds\n";
        cout << "==================================\n";
    }
}

void prime_number_finding(int rank, int size) {
    double start_time = MPI_Wtime();
    if (rank == 0) {
        cout << "Prime Number Finding Selected\n";
        cout << "------------------------------\n";
        cout << "This function is under development.\n";
        double end_time = MPI_Wtime();
        cout << "Execution time: " << (end_time - start_time) << " seconds\n";
    }
}

void bitonic_sort(int rank, int size) {
    double start_time = MPI_Wtime();
    if (rank == 0) {
        cout << "Bitonic Sort Selected\n";
        cout << "------------------------------\n";
        cout << "This function is under development.\n";
        double end_time = MPI_Wtime();
        cout << "Execution time: " << (end_time - start_time) << " seconds\n";
    }
}

void radix_sort(int rank, int size) {
    double start_time = MPI_Wtime();
    if (rank == 0) {
        cout << "Radix Sort Selected\n";
        cout << "------------------------------\n";
        cout << "This function is under development.\n";
        double end_time = MPI_Wtime();
        cout << "Execution time: " << (end_time - start_time) << " seconds\n";
    }
}

void sample_sort(int rank, int size) {
    double start_time = MPI_Wtime();
    if (rank == 0) {
        cout << "Sample Sort Selected\n";
        cout << "------------------------------\n";
        cout << "This function is under development.\n";
        double end_time = MPI_Wtime();
        cout << "Execution time: " << (end_time - start_time) << " seconds\n";
    }
}



int main(int argc, char* argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        std::cout << "[INFO] Number of processes: " << size << std::endl;
    }

    char choice = 'Y';
    while (choice == 'Y' || choice == 'y') {
        int algorithm_choice;

        if (rank == 0) {
            cout << "===============================================\n";
            cout << "Welcome to Parallel Algorithm Simulation with MPI\n";
            cout << "===============================================\n";
            cout << "Please choose an algorithm to execute:\n";
            cout << "01 - Quick Search\n";
            cout << "02 - Prime Number Finding\n";
            cout << "03 - Bitonic Sort\n";
            cout << "04 - Radix Sort\n";
            cout << "05 - Sample Sort\n";
            cout << "Enter the number of the algorithm to run: ";
            cin >> algorithm_choice;
        }

        MPI_Bcast(&algorithm_choice, 1, MPI_INT, 0, MPI_COMM_WORLD);

        switch (algorithm_choice) {
        case 1: quick_search(rank, size); break;
        case 2: prime_number_finding(rank, size); break;
        case 3: bitonic_sort(rank, size); break;
        case 4: radix_sort(rank, size); break;
        case 5: sample_sort(rank, size); break;
        default:
            if (rank == 0) cout << "Invalid choice!\n";
            break;
        }

        if (rank == 0) {
            cout << "Want to try another algorithm? (Y/N): ";
            cin >> choice;
        }

        MPI_Bcast(&choice, 1, MPI_CHAR, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
