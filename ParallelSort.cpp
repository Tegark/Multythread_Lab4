#include "ParallelSort.h"
#include "mpi.h"
#include <algorithm>
#include <iostream>
#include <fstream>

#define PIVOT_TAG 0
#define ARRAY_TAG 1
#define ARRAY_SIZE_TAG 2

ParallelSort::ParallelSort()
{
}

ParallelSort::~ParallelSort()
{
}

void ParallelSort::lessThan(int *mas, int &masSize, int *masNew, int &masNewSize, int *masSend, int &masSendSize, int pivot)
{
	
	for (int t = 0; t < masSize; ++t)
	{
		if (mas[t] <= pivot) 
		{
			masNew[masNewSize] = mas[t];
			++masNewSize;
		}
		else 
		{
			masSend[masSendSize] = mas[t];
			++masSendSize;
		}
	}
}

void ParallelSort::greaterThan(int *mas, int &masSize, int *masNew, int &masNewSize, int *masSend, int &masSendSize, int pivot)
{

	for (int t = 0; t < masSize; ++t)
	{
		if (mas[t] > pivot)
		{
			masNew[masNewSize] = mas[t];
			++masNewSize;
		}
		else
		{
			masSend[masSendSize] = mas[t];
			++masSendSize;
		}
	}
}

void ParallelSort::sort(int *mas, int masSize, int rootProc) 
{
	int procNum, procRank, masNum, size, procMasSize, procMasNewSize, procMasSendSize, procMasRecvSize;
	int *procMas, *procMasNew, *procMasSend;
	MPI_Comm_size(MPI_COMM_WORLD, &procNum);
	MPI_Comm_rank(MPI_COMM_WORLD, &procRank);

	if (procNum == 1) 
	{
		std::sort(mas, mas + masSize);
		return;
	}

	if (procRank == 0) 
	{
		size = masSize;
	}

	MPI_Bcast(&size, 1, MPI_INT, 0, MPI_COMM_WORLD);
	masNum = size / procNum;

	int resSize = masNum * procNum;
	int diff = size - resSize;

	// integer array (of length group size) specifying the number of elements to send to each processor
	int *scounts = new int[procNum];

	for (int i = 0; i < procNum - 1; ++i)
	{
		scounts[i] = masNum;
	}
		
	scounts[procNum - 1] = masNum + diff;
	
	// integer array (of length group size). Entry i specifies the displacement (relative to sendbuf from which to take the outgoing data to process i
	int *displs = new int[procNum];
	displs[0] = 0;

	for (int i = 1; i < procNum; ++i) 
	{
		displs[i] = displs[i - 1] + scounts[i - 1];
	}

	procMasSize = scounts[procRank];
	procMas = new int[procMasSize];

	// dividing initial array between processes  
	MPI_Scatterv(mas, scounts, displs, MPI_INT, procMas, procMasSize, MPI_INT, 0, MPI_COMM_WORLD);
	
	delete[] scounts;
	delete[] displs;
	
	// calculate amount of hypercube dimensions
	int countOfIterations = log2(procNum); 

	// main parallel logic
	// currentBit - bit that will be checked on current step
	// stepMainOfCube - step between main processes of each hypercube
	// pairProcess - pair process of current process
	// pivot - pivot element of current process (and current hypercube)
	int currentBit, stepMainOfCube, pairProcess, pivot;
	for (int i = countOfIterations - 1; i >= 0; --i)
	{
		currentBit = 1 << i;
		stepMainOfCube = 1 << (i + 1);
		bool isMainProc = false;
		// calculation of pair process with which
		// current process will work on this iteration
		pairProcess = procRank | currentBit;
		if (procRank == pairProcess)
		{
			pairProcess -= currentBit;
		}

		// finding main processes of each hypercube
		for (int j = 0; j < procNum; j += stepMainOfCube)
		{
			if (procRank == j) 
			{
				isMainProc = true;

				// counting the pivot element
				if (procMasSize != 0)
				{
					pivot = procMas[procMasSize / 2];
				}
				else
				{
					pivot = 0;
				}
					
				// sending it's pivot to another processes
				// of it's hypercube
				for (int t = procRank + 1; t < procRank + stepMainOfCube; ++t) 
				{
					MPI_Send(&pivot, 1, MPI_INT, t, PIVOT_TAG, MPI_COMM_WORLD);
				}
				break;
			}
		}

		// other processes recieve the pivot element from main process of hypercube
		if (!isMainProc)
		{
			MPI_Status status;
			MPI_Recv(&pivot, 1, MPI_INT, MPI_ANY_SOURCE, PIVOT_TAG, MPI_COMM_WORLD, &status);
		}

		// changing blocks of data with pair process
		// here we preaparing sending data and data,
		// that will remain on this process
		procMasNew = new int[procMasSize];
		procMasSend = new int[procMasSize];
		procMasNewSize = 0;
		procMasSendSize = 0;

		if (procRank < pairProcess) 
		{
			lessThan(procMas, procMasSize, procMasNew, procMasNewSize, procMasSend, procMasSendSize, pivot);
		}
		else 
		{
			greaterThan(procMas, procMasSize, procMasNew, procMasNewSize, procMasSend, procMasSendSize, pivot);
		}

		delete[] procMas; // don't need old array now

		// For resolving the occuring MPI_Send buffer problem (deadlock due to data sending by all processes )
		// at first, only one process of pair will send data and the other process will receive it
		// at second, the other process will do the same, after receivng data.
		if (procRank < pairProcess)
		{	
			MPI_Send(&procMasSendSize, 1, MPI_INT, pairProcess, ARRAY_SIZE_TAG, MPI_COMM_WORLD);
			MPI_Send(procMasSend, procMasSendSize, MPI_INT, pairProcess, ARRAY_TAG, MPI_COMM_WORLD);

			delete[] procMasSend;

			MPI_Status status;
			MPI_Recv(&procMasRecvSize, 1, MPI_INT, pairProcess, ARRAY_SIZE_TAG, MPI_COMM_WORLD, &status);

			// where old and new elements will be stored
			procMasSize = procMasNewSize + procMasRecvSize; // now need new size
			procMas = new int[procMasSize];
			memcpy(procMas, procMasNew, procMasNewSize * sizeof(int));

			delete[] procMasNew;

			MPI_Recv(procMas + procMasNewSize, procMasRecvSize, MPI_INT, pairProcess, ARRAY_TAG, MPI_COMM_WORLD, &status);
		}
		else
		{
			MPI_Status status;
			MPI_Recv(&procMasRecvSize, 1, MPI_INT, pairProcess, ARRAY_SIZE_TAG, MPI_COMM_WORLD, &status);

			// where old and new elements will be stored
			procMasSize = procMasNewSize + procMasRecvSize; // now need new size
			procMas = new int[procMasSize];
			memcpy(procMas, procMasNew, procMasNewSize * sizeof(int));

			delete[] procMasNew;

			MPI_Recv(procMas + procMasNewSize, procMasRecvSize, MPI_INT, pairProcess, ARRAY_TAG, MPI_COMM_WORLD, &status);

			MPI_Send(&procMasSendSize, 1, MPI_INT, pairProcess, ARRAY_SIZE_TAG, MPI_COMM_WORLD);
			MPI_Send(procMasSend, procMasSendSize, MPI_INT, pairProcess, ARRAY_TAG, MPI_COMM_WORLD);

			delete[] procMasSend;
		}

	}

	// sorting processes' part of array
	std::sort(procMas, procMas + procMasSize);
	// gathering data
	// to understand look at OpenMPI API, MPI_Gatherv example 5
	int *rcounts = new int[procNum];
	displs = new int[procNum];

	MPI_Gather(&procMasSize, 1, MPI_INT, rcounts, 1, MPI_INT, 0, MPI_COMM_WORLD);
	
	displs[0] = 0;

	for (int i = 1; i < procNum; ++i) 
	{
		displs[i] = displs[i - 1] + rcounts[i - 1];
	}

	MPI_Gatherv(procMas, procMasSize, MPI_INT, mas, rcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);

	delete[] procMas;
	delete[] rcounts;
	delete[] displs;
}