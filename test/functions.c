#include "header.h"
#include <omp.h>

struct Parms parms = {0.1, 0.1};

/**************************************************************************
* subroutine update
****************************************************************************/
void Independent_Update(float** A, float** B, int size)
{
	int i, j;
#ifdef __OMP__
	int thread_count = (size < MAXTHREADS) ? size : MAXTHREADS;
	// Create size threads with openmp or max
	#pragma omp parallel for num_threads( thread_count ) collapse(2)
#endif
	// Calcalate inside data
	for (i = 1; i < size-1; i++)
	{
		for (j = 1; j < size-1; j++)
		{
			B[i][j] = A[i][j] + 
				parms.cx * ( A[i + 1][j] + A[i-1][j] - 2.0 * A[i][j] ) + 
				parms.cy * ( A[i][j + 1] + A[i][j-1] - 2.0 * A[i][j] );
		}
	}
}

void Dependent_Update(float** A, float** B, int size, float** Row)
{
	int i, j;
	float T1, T2;
#ifdef __OMP__
	int thread_count = (size < MAXTHREADS) ? size : MAXTHREADS;
	// Create size threads with openmp or max
	#pragma omp parallel for num_threads( thread_count ) private(T1) private(T2) collapse(2)
#endif
	// Calculate the edges of array
	// 2 for instructions for better split to threads
	for (i = 0; i < size; i++)
	{
		for (j = 0; j < 4; j++)
		{
			if (j == 0)	// South Neighbor. Same format for the below if statement
			{
				T1 = (i != size-1) ? A[size-1][i + 1] : Row[RIGHT][size-1];   // If you are at the right edge of neighbor take data from East
				T2 = (i != 0) ? A[size-1][i-1] : Row[LEFT][size-1];
				B[size-1][i] = A[size-1][i] + 								  // Calculate formula
					parms.cx * ( Row[DOWN][i] + A[size-2][i] - 2.0 * A[size-1][i] ) + 
					parms.cy * ( T1 + T2 - 2.0 * A[size-1][i] );
			}
			else if(j == 1) // North Neighbor
			{
				T1 = (i != size-1) ? A[0][i + 1] : Row[RIGHT][0];
				T2 = (i != 0) ? A[0][i-1] : Row[LEFT][0];
				B[0][i] = A[0][i] + 
					parms.cx * ( A[1][i] + Row[UP][i] - 2.0 * A[0][i] ) + 
					parms.cy * ( T1 + T2 - 2.0 * A[0][i] );
			}
			else if(j == 2) // West Neighbor
			{
				T1 = (i != size-1) ? A[i + 1][0] : Row[DOWN][0];
				T2 = (i != 0) ? A[i-1][0] : Row[UP][0];
				B[i][0] = A[i][0] + 
					parms.cx * ( T1 + T2 - 2.0 * A[i][0] ) + 
					parms.cy * ( A[i][1] + Row[LEFT][i] - 2.0 * A[i][0] );
			}
			else	// East Neighbor
			{
				T1 = (i != size-1) ? A[i + 1][size-1] : Row[DOWN][size-1];
				T2 = (i != 0) ? A[i-1][size-1] : Row[UP][size-1];
				B[i][size-1] = A[i][size-1] + 
					parms.cx * ( T1 + T2 - 2.0 * A[i][size-1] ) + 
					parms.cy * ( Row[RIGHT][i] + A[i][size-2] - 2.0 * A[i][size-1] );
			}
		}
	}
}

inline float diffa(float** A, float** B, int size)
{
	float diff, sum = 0;
	int i, j;
#ifdef __OMP__
	int thread_count = (size < MAXTHREADS) ? size : MAXTHREADS;
	// Create size threads with openmp or max
	#pragma omp parallel for num_threads( thread_count ) reduction(+:sum) private(diff) collapse(2)
#endif
	for (i = 0; i < size; i++)
	{
		for (j = 0; j < size; j++)
		{
			diff = B[i][j]-A[i][j];
			sum += (diff*diff);
		}
	}
	return sum;
}

/*****************************************************************************
* subroutine inidat   - Initialize Array
*****************************************************************************/
inline void inidat(int size, float** u)
{
	int ix, iy;

	for (ix = 0; ix < size; ix++)
	{
		for (iy = 0; iy < size; iy++)
		{
			u[ix][iy] = (ix + 1) * ((size + 2) - ix - 2) * (iy + 1) * ((size + 2) - iy - 2);
		}
	}
}

/**************************************************************************
* subroutine prtdat - Print the results
**************************************************************************/
inline void prtdat(int size, float** u, char *fnam)
{
	int ix, iy;
	FILE *fp;
	fp = fopen(fnam, "w");
	for (ix = 0; ix < size; ix++)
	{
		for (iy = 0; iy < size-1; iy++)
		{
			fprintf(fp, "%6.1f ", u[ix][iy]);
		}
		fprintf(fp, "%6.1f\n", u[ix][iy]);
	}
	fclose(fp);
}

// Create 2D array with sequential memory positions
inline float** SeqAllocate(int size) {
	float* sequence = malloc(size*size*sizeof(float));
	float** matrix = malloc(size*sizeof(float *));
	int i;
	for (i = 0; i < size; i++)
		matrix[i] = &(sequence[i*size]);

	return matrix;
}

// Free 2D array with sequential memory positions
inline void SeqFree(float** memory_ptr)
{
	memory_ptr[0][2] = -3;
	free(memory_ptr[0]);
	free(memory_ptr);
}

// Free worker structures
inline void finalize(Finalize* fin)
{
	MPI_Wait(fin->request, MPI_STATUS_IGNORE);
	SeqFree(fin->A);
	free(fin->request);
	free(fin);
}
