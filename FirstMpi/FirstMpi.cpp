#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <limits>
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

// دالة توزيع البيانات باستخدام الشجرة الثنائية
void scatter_recursive(int low, int high, vector<int>& current_data, vector<int>& local_data, int N, int size, int rank) {
    if (low == high) {
        local_data = current_data; // العملية تحتفظ بالبيانات بتاعتها
    }
    else {
        int mid = low + (high - low) / 2; // نص الشجرة
        if (rank == low && mid + 1 <= high) {
            // حساب عدد العناصر للنص اليمين
            int total_elements_right = 0;
            int base_size = N / size;
            int remainder = N % size;
            for (int r = mid + 1; r <= high; r++) {
                int current_size = base_size + (r < remainder ? 1 : 0);
                total_elements_right += current_size;
            }
            // إرسال النص اليمين للعملية mid+1
            vector<int> data_for_right(current_data.end() - total_elements_right, current_data.end());
            MPI_Send(data_for_right.data(), data_for_right.size(), MPI_INT, mid + 1, 0, MPI_COMM_WORLD);
            // الاحتفاظ بالنص الشمال
            current_data.resize(current_data.size() - total_elements_right);
        }
        else if (rank == mid + 1 && low <= mid) {
            // استقبال البيانات من العملية low
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
        // التكرار على الشجرة الفرعية
        if (rank <= mid) {
            scatter_recursive(low, mid, current_data, local_data, N, size, rank);
        }
        else {
            scatter_recursive(mid + 1, high, current_data, local_data, N, size, rank);
        }
    }
}

// دالة جمع النتايج باستخدام الشجرة الثنائية
void gather_recursive(int low, int high, int& result, int rank, int size) {
    if (low == high) {
        // لو وصلنا لعملية واحدة، النتيجة جاهزة
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
            // استقبال النتيجة من الشجرة اليمين
            int received_result;
            MPI_Recv(&received_result, 1, MPI_INT, mid + 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            // دمج النتيجة مع أقل مؤشر
            if (received_result != -1 && (result == -1 || received_result < result)) {
                result = received_result;
            }
        }
        else if (rank == mid + 1 && low <= mid) {
            // إرسال النتيجة للعملية low
            MPI_Send(&result, 1, MPI_INT, low, 0, MPI_COMM_WORLD);
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

    // نشر target لكل العمليات
    MPI_Bcast(&target, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // نشر عدد العناصر N
    int N;
    if (rank == 0) {
        N = data.size();
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // توزيع البيانات باستخدام الشجرة الثنائية
    vector<int> current_data;
    if (rank == 0) {
        current_data = data;
    }
    scatter_recursive(0, size - 1, current_data, local_data, N, size, rank);

    // حساب المؤشر العام للبداية
    int base_size = N / size;
    int remainder = N % size;
    int start_index;
    if (rank < remainder) {
        start_index = rank * (base_size + 1);
    }
    else {
        start_index = remainder * (base_size + 1) + (rank - remainder) * base_size;
    }

    // البحث المحلي
    search_start = MPI_Wtime();
    int local_result = -1;
    for (int i = 0; i < local_data.size(); i++) {
        if (local_data[i] == target) {
            int global_index = start_index + i;
            if (local_result == -1 || global_index < local_result) {
                local_result = global_index;
            }
        }
    }
    search_end = MPI_Wtime();

    // جمع النتايج باستخدام الشجرة الثنائية
    int global_result = local_result;
    gather_recursive(0, size - 1, global_result, rank, size);

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

    // Broadcast the input range to all processes
    MPI_Bcast(&start_range, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&end_range, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calculate local range
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

    // Check for prime numbers in local range
    compute_start = MPI_Wtime();
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
    compute_end = MPI_Wtime();

    // Gather counts first
    int local_count = local_primes.size();
    vector<int> counts(size);
    MPI_Gather(&local_count, 1, MPI_INT, counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Gather displacements
    vector<int> displs(size);
    int total_primes = 0;
    if (rank == 0) {
        displs[0] = 0;
        for (int i = 1; i < size; ++i) {
            displs[i] = displs[i - 1] + counts[i - 1];
        }
        total_primes = displs[size - 1] + counts[size - 1];
    }

    // Gather all primes to master
    vector<int> all_primes(total_primes);
    MPI_Gatherv(local_primes.data(), local_count, MPI_INT,
        all_primes.data(), counts.data(), displs.data(), MPI_INT,
        0, MPI_COMM_WORLD);

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

}

void radix_sort(int rank, int size) {

}

void sample_sort(int rank, int size) {

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