/******************************************************************************
 * Created by Claire Grady on 3 / 9 / 2022.
 * gauss.c
 * Program that demonstrates the use of MPI to perform a gaussian blur on a bmp
 *
 * Parameters:
 *    1. -np The number of processes
 *    2. The name of the bmp file to be processed
 *    3. The filename the processed bmp will be written to (will be created if it doesn't exist)
 *    4. The standard deviation to be used in the gaussian algorithm
 *
 * Build:
 *    mpicc -Wall -pedantic -lm -c -o gaussianLib.o segment.o 
 *    qdbmp.o gauss.c -o gauss 
 * Run Example:
 *    mpirun -np <number of processes> ./gauss <input filename> 
 *    <output filename> <standard deviation>
 * ***************************************************************************/
