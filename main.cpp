#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <ctime>
#include "mpi.h"
#include "ParallelSort.h"

using namespace std;

int main(int argc, char* argv[]) 
{
	cout << "Programm is being executed" << endl;
	
	if (argc != 2) 
	{
		cout << "Name of input file needed" << endl;
		return -1;
	}

	MPI_Init(&argc, &argv);

	int procRank, procNum;

	MPI_Comm_rank(MPI_COMM_WORLD, &procRank);
	MPI_Comm_size(MPI_COMM_WORLD, &procNum);

	int *arr = nullptr;
	int arrSize;
	double start, finish;

	if (procRank == 0) 
	{
		cout << "Reading File..." << endl;
		
		ifstream input(argv[1]);

		input >> arrSize;

		arr = new int[arrSize];

		for (int i = 0; i < arrSize; ++i)
		{
			input >> arr[i];
		}

		input.close();
		cout << "File was read successfully!";
	}

	ofstream out("results.txt");
	double interval, avgTime;
	avgTime = 0;

	for (int i = 0; i < 10; ++i) 
	{
		std::random_shuffle(arr, arr + arrSize);

		start = MPI_Wtime();

		ParallelSort::sort(arr, arrSize, 0);

		finish = MPI_Wtime();

		interval = finish - start;

		avgTime += interval;

		if (procRank == 0)
		{
			out << interval << endl;
		}
			
	}

	cout << "Array has been sorted.";

	avgTime /= 10;

	if (procRank == 0)
	{
		out << avgTime << endl;
	}
	
	out << endl;

	for (int i = 0; i < arrSize; ++i)
	{
		out << arr[i] << ' ';
	}

	out.close();

	delete[] arr;
	MPI_Finalize();
	return 0;
}