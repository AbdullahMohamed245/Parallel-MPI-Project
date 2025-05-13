#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <limits>
#include <cmath>
#include "mpi.h"

using namespace std;

string ValidFile(const string& prompt)
{
    string filename;
    bool fileExists = false;

    while (!fileExists) {
        cout << prompt;
        cin >> filename;

        ifstream file(filename);
        if (file.good()) {
            fileExists = true;
            file.close();
        }
        else {
            cout << "File not found. Please try again.\n";
        }
    }

    return filename;
}

int ValidInput(const string& prompt) {
    int n;
    while (true) {
        cout << prompt;
        cin >> n;

        if (cin.fail()) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input. Please enter a valid number.\n";
        }
        else if (n <= 0) {
            cout << "Number must be positive. Try again.\n";
        }
        else {
            break;
        }
    }
    return n;
}

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

void scatter_recursive(int low, int high, vector<int>& current_data, vector<int>& local_data, int N, int size, int rank) {
    if (low == high) {
        local_data = current_data;
    }
    else {
        int mid = low + (high - low) / 2;
        if (rank == low && mid + 1 <= high) {
            int total_elements_right = 0;
            int base_size = N / size;
            int remainder = N % size;
            for (int r = mid + 1; r <= high; r++) {
                int current_size = base_size + (r < remainder ? 1 : 0);
                total_elements_right += current_size;
            }
            vector<int> data_for_right(current_data.end() - total_elements_right, current_data.end());
            MPI_Send(data_for_right.data(), data_for_right.size(), MPI_INT, mid + 1, 0, MPI_COMM_WORLD);
            current_data.resize(current_data.size() - total_elements_right);
        }
        else if (rank == mid + 1 && low <= mid) {
            int total_elements_right = 0;
            int base_size = N / size;
            int remainder = N % size;
            for (int r = mid + 1; r <= high; r++) {
                int current_size = base_size + (r < remainder ? 1 : 0);
                total_elements_right += current_size;
            }
            current_data.resize(total_elements_right);
            MPI_Recv(current_data.data(), total_elements_right, MPI_INT, low, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        if (rank <= mid) {
            scatter_recursive(low, mid, current_data, local_data, N, size, rank);
        }
        else {
            scatter_recursive(mid + 1, high, current_data, local_data, N, size, rank);
        }
    }
}

void gather_recursive(int low, int high, int& result, int rank, int size) {
    if (low == high) {
    }
    else {
        int mid = low + (high - low) / 2;
        if (rank <= mid) {
            gather_recursive(low, mid, result, rank, size);
        }
        else {
            gather_recursive(mid + 1, high, result, rank, size);
        }
        if (rank == low && mid + 1 <= high) {
            int received_result;
            MPI_Recv(&received_result, 1, MPI_INT, mid + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (received_result != -1 && (result == -1 || received_result < result)) {
                result = received_result;
            }
        }
        else if (rank == mid + 1 && low <= mid) {
            MPI_Send(&result, 1, MPI_INT, low, 0, MPI_COMM_WORLD);
        }
    }
}

void gathervector_recursive(int low, int high, vector<int>& local_data, vector<int>& result, int rank, int size) {
    if (low == high) {
        result = local_data;
    }
    else {
        int mid = low + (high - low) / 2;
        if (rank <= mid) {
            gathervector_recursive(low, mid, local_data, result, rank, size);
        }
        else {
            gathervector_recursive(mid + 1, high, local_data, result, rank, size);
        }
        if (rank == low && mid + 1 <= high) {
            int right_size;
            MPI_Recv(&right_size, 1, MPI_INT, mid + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            vector<int> received_data(right_size);
            MPI_Recv(received_data.data(), right_size, MPI_INT, mid + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            result.insert(result.end(), received_data.begin(), received_data.end());
        }
        else if (rank == mid + 1 && low <= mid) {
            int local_size = local_data.size();
            MPI_Send(&local_size, 1, MPI_INT, low, 0, MPI_COMM_WORLD);
            MPI_Send(local_data.data(), local_size, MPI_INT, low, 0, MPI_COMM_WORLD);
        }
    }
}




void quick_search(int rank, int size) {
    double total_start_time = MPI_Wtime();

    int target;
    vector<int> data;
    vector<int> local_data;
    double file_read_start, file_read_end, search_start, search_end, input_time_start, input_time_end;

    if (rank == 0) {
        cout << "Quick Search Selected\n";
        cout << "------------------------------\n";
        string filename;
        input_time_start = MPI_Wtime();
        filename = ValidFile("Please enter the path to the input file: ");
        target = ValidInput("Enter Search Target: ");
        input_time_end = MPI_Wtime();

        cout << "Reading data from file...\n";
        file_read_start = MPI_Wtime();
        data = read_data_from_file(filename);
        file_read_end = MPI_Wtime();
    }

    search_start = MPI_Wtime();
    MPI_Bcast(&target, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int N;
    if (rank == 0) {
        N = data.size();
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> current_data;
    if (rank == 0) {
        current_data = data;
    }
    scatter_recursive(0, size - 1, current_data, local_data, N, size, rank);

    int base_size = N / size;
    int remainder = N % size;
    int start_index;
    if (rank < remainder) {
        start_index = rank * (base_size + 1);
    }
    else {
        start_index = remainder * (base_size + 1) + (rank - remainder) * base_size;
    }

    
    int local_result = -1;
    for (int i = 0; i < local_data.size(); i++) {
        if (local_data[i] == target) {
            int global_index = start_index + i;
            if (local_result == -1 || global_index < local_result) {
                local_result = global_index;
            }
        }
    }

    int global_result = local_result;
    gather_recursive(0, size - 1, global_result, rank, size);

    search_end = MPI_Wtime();


    if (rank == 0) {
        if (global_result != -1) {
            cout << "Result: Value " << target << " found at index " << global_result << endl;
        }
        else {
            cout << "Result: Value " << target << " not found\n";
        }

        double total_end_time = MPI_Wtime();
        double total_input = input_time_end - input_time_start;
        double total_duration = total_end_time - total_start_time - total_input;
        double file_read_duration = file_read_end - file_read_start;
        double search_time = search_end - search_start;

        cout << "\n========== Time ==========\n";
        cout << " File Reading Time      : " << file_read_duration << " seconds\n";
        cout << " Search Time (actual)   : " << search_time << " seconds\n";
        cout << " Total Execution Time    : " << total_duration << " seconds\n";
        cout << "==================================\n";
    }
}




void prime_number_finding(int rank, int size) {
    double total_start_time = MPI_Wtime();

    int start_range = 0, end_range = 0;
    vector<int> local_primes;
    double input_start, input_end, compute_start, compute_end;

    if (rank == 0) {
        cout << "Prime Number Finding Selected\n";
        cout << "----------------------------------\n";
        input_start = MPI_Wtime();
        start_range = ValidInput("Enter start of range: ");
        while (end_range <= start_range)
        {
            cout << "End of range MUST be grater than start of range...\n";
            end_range = ValidInput("Enter end of range: ");
        }
        input_end = MPI_Wtime();
    }

    compute_start = MPI_Wtime();


    MPI_Bcast(&start_range, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&end_range, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int total_numbers = end_range - start_range + 1;
    int base = total_numbers / size;
    int remainder = total_numbers % size;
    int local_start, local_end;

    if (rank < remainder) {
        local_start = start_range + rank * (base + 1);
        local_end = local_start + base;
    }
    else {
        local_start = start_range + rank * base + remainder;
        local_end = local_start + base - 1;
    }

    for (int i = max(2, local_start); i <= local_end; i++) {
        bool is_prime = true;
        for (int j = 2; j * j <= i; j++) {
            if (i % j == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) {
            local_primes.push_back(i);
        }
    }

    int local_count = local_primes.size();
    vector<int> counts(size);
    MPI_Gather(&local_count, 1, MPI_INT, counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> displs(size);
    int total_primes = 0;
    if (rank == 0) {
        displs[0] = 0;
        for (int i = 1; i < size; ++i) {
            displs[i] = displs[i - 1] + counts[i - 1];
        }
        total_primes = displs[size - 1] + counts[size - 1];
    }

    vector<int> all_primes(total_primes);
    MPI_Gatherv(local_primes.data(), local_count, MPI_INT,
        all_primes.data(), counts.data(), displs.data(), MPI_INT,
        0, MPI_COMM_WORLD);

    compute_end = MPI_Wtime();


    if (rank == 0) {
        double total_input = input_end - input_start;
        double total_compute = compute_end - compute_start;

        ofstream outfile("prime_output.txt");
        outfile << "Total Prime Numbers Found: " << all_primes.size() << "\n";
        outfile << "========== Prime Numbers ==========\n";
        for (int p : all_primes) {
            outfile << p << " ";
        }
        outfile.close();

        double total_end_time = MPI_Wtime();
        double total_duration = total_end_time - total_start_time - total_input;

        cout << "Prime numbers to prime_output.txt\n";
        cout << "\n\n========== Time ==========\n";
        cout << "Total Compute Time: " << total_compute << " seconds\n";
        cout << "Total Execution Time: " << total_duration << " seconds\n";
        cout << "==================================\n";
    }
}

void bitonic_sort(int rank, int size) {
    double total_start_time = MPI_Wtime();

    vector<int> data;
    vector<int> local_data;
    double file_read_start, file_read_end, sort_start, sort_end, input_time_start, input_time_end;

    if (rank == 0) {
        cout << "Bitonic Sort Selected\n";
        cout << "------------------------------\n";
        input_time_start = MPI_Wtime();
        string filename = ValidFile("Please enter the path to the input file: ");
        input_time_end = MPI_Wtime();

        cout << "Reading data from file...\n";
        file_read_start = MPI_Wtime();
        data = read_data_from_file(filename);
        file_read_end = MPI_Wtime();
    }
    sort_start = MPI_Wtime();

    int N;
    if (rank == 0) {
        N = data.size();
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> current_data;
    if (rank == 0) {
        current_data = data;
    }
    scatter_recursive(0, size - 1, current_data, local_data, N, size, rank);

    sort(local_data.begin(), local_data.end());

    bool is_power_of_two = (size & (size - 1)) == 0;
    if (!is_power_of_two) {
        if (rank == 0) {
            cout << "Error: Bitonic Sort requires the number of processes to be a power of 2\n";
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (int stage = 0; stage < log2(size); stage++) {
        for (int step = stage; step >= 0; step--) {
            int partner = rank ^ (1 << step);
            if (partner < size) {
                int local_size = local_data.size();
                int partner_size;
                MPI_Sendrecv(&local_size, 1, MPI_INT, partner, 0,
                    &partner_size, 1, MPI_INT, partner, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                vector<int> partner_data(partner_size);
                MPI_Sendrecv(local_data.data(), local_size, MPI_INT, partner, 0,
                    partner_data.data(), partner_size, MPI_INT, partner, 0,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                bool ascending = (rank >> (stage + 1)) % 2 == 0;
                vector<int> merged;
                merged.reserve(local_size + partner_size);

                if (ascending) {
                    size_t i = 0, j = 0;
                    while (i < local_size && j < partner_size) {
                        if (local_data[i] <= partner_data[j]) {
                            merged.push_back(local_data[i++]);
                        }
                        else {
                            merged.push_back(partner_data[j++]);
                        }
                    }
                    while (i < local_size) merged.push_back(local_data[i++]);
                    while (j < partner_size) merged.push_back(partner_data[j++]);
                }
                else {
                    size_t i = 0, j = 0;
                    while (i < local_size && j < partner_size) {
                        if (local_data[i] >= partner_data[j]) {
                            merged.push_back(local_data[i++]);
                        }
                        else {
                            merged.push_back(partner_data[j++]);
                        }
                    }
                    while (i < local_size) merged.push_back(local_data[i++]);
                    while (j < partner_size) merged.push_back(partner_data[j++]);
                }

                int half_size = merged.size() / 2;
                if (rank < partner) {
                    local_data.assign(merged.begin(), merged.begin() + half_size);
                }
                else {
                    local_data.assign(merged.begin() + half_size, merged.end());
                }
            }
        }
    }


    vector<int> all_data;
    if (rank == 0) {
        all_data.reserve(N);
    }
    gathervector_recursive(0, size - 1, local_data, all_data, rank, size);
    sort_end = MPI_Wtime();

    if (rank == 0) {
        vector<int> final_data;
        final_data.reserve(N);
        final_data = all_data;
        sort(final_data.begin(), final_data.end());

        ofstream outfile("bitonic_sort_output.txt");
        outfile << "Total Elements Sorted: " << final_data.size() << "\n";
        outfile << "========== Sorted Data ==========\n";
        for (int val : final_data) {
            outfile << val << " ";
        }
        outfile.close();

        double total_end_time = MPI_Wtime();
        double total_input = input_time_end - input_time_start;
        double total_duration = total_end_time - total_start_time - total_input;
        double file_read_duration = file_read_end - file_read_start;
        double sort_time = sort_end - sort_start;

        cout << "Sorted data written to bitonic_sort_output.txt\n";
        cout << "\n========== Time ==========\n";
        cout << " File Reading Time      : " << file_read_duration << " seconds\n";
        cout << " Sort Time (actual)     : " << sort_time << " seconds\n";
        cout << " Total Execution Time   : " << total_duration << " seconds\n";
        cout << "==================================\n";
    }
}

void radix_sort(int rank, int size) {
    double total_start_time = MPI_Wtime();

    vector<int> data;
    vector<int> local_data;
    double file_read_start, file_read_end, sort_start, sort_end, input_time_start, input_time_end;

    if (rank == 0) {
        cout << "Radix Sort Selected\n";
        cout << "------------------------------\n";
        input_time_start = MPI_Wtime();
        string filename = ValidFile("Please enter the path to the input file: ");
        input_time_end = MPI_Wtime();

        cout << "Reading data from file...\n";
        file_read_start = MPI_Wtime();
        data = read_data_from_file(filename);
        file_read_end = MPI_Wtime();
    }
    sort_start = MPI_Wtime();

    int N;
    if (rank == 0) {
        N = data.size();
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> current_data;
    if (rank == 0) {
        current_data = data;
    }
    scatter_recursive(0, size - 1, current_data, local_data, N, size, rank);

    int max_element = 0;
    for (int val : local_data) {
        max_element = max(max_element, abs(val));
    }
    int max_digits = 0;
    while (max_element > 0) {
        max_digits++;
        max_element /= 10;
    }

    for (int exp = 1; max_digits > 0; exp *= 10, max_digits--) {
        vector<int> output(local_data.size());
        vector<int> count(10, 0);

        for (int val : local_data) {
            int digit = (abs(val) / exp) % 10;
            count[digit]++;
        }

        for (int i = 1; i < 10; i++) {
            count[i] += count[i - 1];
        }

        for (int i = local_data.size() - 1; i >= 0; i--) {
            int digit = (abs(local_data[i]) / exp) % 10;
            output[count[digit] - 1] = local_data[i];
            count[digit]--;
        }

        local_data = output;
    }

    vector<int> sorted_local_data(local_data.size());
    int neg_pos = 0, pos_pos = local_data.size() - 1;
    for (int val : local_data) {
        if (val < 0) {
            sorted_local_data[neg_pos++] = val;
        }
        else {
            sorted_local_data[pos_pos--] = val;
        }
    }
    reverse(sorted_local_data.begin() + neg_pos, sorted_local_data.end());
    local_data = sorted_local_data;


    vector<int> all_data;
    if (rank == 0) {
        all_data.reserve(N);
    }
    gathervector_recursive(0, size - 1, local_data, all_data, rank, size);

    sort_end = MPI_Wtime();

    if (rank == 0) {
        vector<int> final_data;
        final_data.reserve(N);
        final_data = all_data;
        sort(final_data.begin(), final_data.end());

        ofstream outfile("radix_sort_output.txt");
        outfile << "Total Elements Sorted: " << final_data.size() << "\n";
        outfile << "========== Sorted Data ==========\n";
        for (int val : final_data) {
            outfile << val << " ";
        }
        outfile.close();

        double total_end_time = MPI_Wtime();
        double total_input = input_time_end - input_time_start;
        double total_duration = total_end_time - total_start_time - total_input;
        double file_read_duration = file_read_end - file_read_start;
        double sort_time = sort_end - sort_start;

        cout << "Sorted data written to radix_sort_output.txt\n";
        cout << "\n========== Time ==========\n";
        cout << " File Reading Time      : " << file_read_duration << " seconds\n";
        cout << " Sort Time (actual)     : " << sort_time << " seconds\n";
        cout << " Total Execution Time   : " << total_duration << " seconds\n";
        cout << "==================================\n";
    }
}

void sample_sort(int rank, int size) {
    double total_start_time = MPI_Wtime();
    vector<int> data;
    vector<int> local_data;
    double file_read_start, file_read_end, sort_start, sort_end, input_time_start, input_time_end;

    if (rank == 0) {
        cout << "Sample Sort Selected\n";
        cout << "------------------------------\n";
        string filename;
        input_time_start = MPI_Wtime();
        filename = ValidFile("Please enter the path to the input file: ");
        input_time_end = MPI_Wtime();

        cout << "Reading data from file...\n";
        file_read_start = MPI_Wtime();
        data = read_data_from_file(filename);
        file_read_end = MPI_Wtime();
    }

    sort_start = MPI_Wtime();

    int N;
    if (rank == 0) {
        N = data.size();
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> current_data;
    if (rank == 0) {
        current_data = data;
    }
    scatter_recursive(0, size - 1, current_data, local_data, N, size, rank);


    sort(local_data.begin(), local_data.end());

    int p = size;
    int m = local_data.size();
    vector<int> samples;
    if (m > 0) {
        int nsamples = min(p, m);
        samples.reserve(nsamples);
        for (int i = 0; i < nsamples; i++) {
            int idx = (i * m) / p;
            samples.push_back(local_data[idx]);
        }
    }

    int nsamples = samples.size();
    vector<int> sample_counts(size);
    MPI_Gather(&nsamples, 1, MPI_INT, sample_counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> sample_displs(size);
    int total_samples = 0;
    if (rank == 0) {
        sample_displs[0] = 0;
        for (int i = 0; i < size; i++) {
            total_samples += sample_counts[i];
            if (i > 0) sample_displs[i] = sample_displs[i - 1] + sample_counts[i - 1];
        }
    }

    vector<int> all_samples(total_samples);
    MPI_Gatherv(samples.data(), nsamples, MPI_INT,
        all_samples.data(), sample_counts.data(), sample_displs.data(),
        MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> splitters;
    if (rank == 0) {
        sort(all_samples.begin(), all_samples.end());
        int S = all_samples.size();
        if (S > 0) {
            for (int i = 0; i < p - 1; i++) {
                int idx = (i + 1) * S / p;
                splitters.push_back(all_samples[idx]);
            }
        }
    }

    int num_splitters = splitters.size();
    MPI_Bcast(&num_splitters, 1, MPI_INT, 0, MPI_COMM_WORLD);

    splitters.resize(num_splitters);
    MPI_Bcast(splitters.data(), num_splitters, MPI_INT, 0, MPI_COMM_WORLD);

    vector<vector<int>> buckets(p);
    for (int x : local_data) {
        auto iter = upper_bound(splitters.begin(), splitters.end(), x);
        int bucket = distance(splitters.begin(), iter);
        if (bucket == num_splitters) bucket = p - 1;
        buckets[bucket].push_back(x);
    }

    vector<int> sendcounts(p);
    for (int i = 0; i < p; i++) {
        sendcounts[i] = buckets[i].size();
    }

    vector<int> recvcounts(p);
    MPI_Alltoall(sendcounts.data(), 1, MPI_INT, recvcounts.data(), 1, MPI_INT, MPI_COMM_WORLD);

    int recv_total = 0;
    for (int c : recvcounts) recv_total += c;

    vector<int> sdispls(p), rdispls(p);
    sdispls[0] = 0;
    rdispls[0] = 0;
    for (int i = 1; i < p; i++) {
        sdispls[i] = sdispls[i - 1] + sendcounts[i - 1];
        rdispls[i] = rdispls[i - 1] + recvcounts[i - 1];
    }

    vector<int> sendbuf;
    for (int i = 0; i < p; i++) {
        sendbuf.insert(sendbuf.end(), buckets[i].begin(), buckets[i].end());
    }

    vector<int> recvbuf(recv_total);

    MPI_Alltoallv(sendbuf.data(), sendcounts.data(), sdispls.data(), MPI_INT,
        recvbuf.data(), recvcounts.data(), rdispls.data(), MPI_INT,
        MPI_COMM_WORLD);

    sort(recvbuf.begin(), recvbuf.end());

    vector<int> final_data;
    gathervector_recursive(0, size - 1, recvbuf, final_data, rank, size);

    sort_end = MPI_Wtime();

    if (rank == 0) {
        cout << "Sample Sort completed.\n";
        ofstream outfile("sample_sort_output.txt");
        outfile << "Total Elements Sorted: " << final_data.size() << "\n";
        outfile << "========== Sorted Data ==========\n";
        for (int val : final_data) {
            outfile << val << " ";
        }
        outfile.close();

        double total_end_time = MPI_Wtime();
        double total_input = input_time_end - input_time_start;
        double file_read_duration = file_read_end - file_read_start;
        double sort_time = sort_end - sort_start;
        double total_duration = total_end_time - total_start_time - total_input;

        cout << "\n========== Time ==========\n";
        cout << " File Reading Time      : " << file_read_duration << " seconds\n";
        cout << " Sorting Time           : " << sort_time << " seconds\n";
        cout << " Total Execution Time    : " << total_duration << " seconds\n";
        cout << "==================================\n";
    }
}

int main(int argc, char* argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        cout << "[INFO] Number of processes: " << size << endl;
    }

    char choice = 'Y';
    while (choice == 'Y' || choice == 'y')
    {
        int algorithm_choice = 10;

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
            while (algorithm_choice > 5 || algorithm_choice < 1)
            {
                cout << "Number MUST be from 1 TO 5 \n";
                algorithm_choice = ValidInput("Enter the number of the algorithm to run: ");
            }
        }

        MPI_Bcast(&algorithm_choice, 1, MPI_INT, 0, MPI_COMM_WORLD);

        switch (algorithm_choice) {
        case 1: quick_search(rank, size); break;
        case 2: prime_number_finding(rank, size); break;
        case 3: bitonic_sort(rank, size); break;
        case 4: radix_sort(rank, size); break;
        case 5: sample_sort(rank, size); break;
        default:
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