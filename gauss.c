/******************************************************************************
 * Created by Claire Grady on 3 / 9 / 2022.
 * gauss.c
 * Program that demonstrates the use of MPI to perform a gaussian blur on a bmp
 *
 * Parameters:
 *    1. -np The number of processes
 *    2. The name of the bmp file to be processed
 *    3. The filename the processed bmp will be written to (will be created
 *       if it doesn't exist)
 *    4. The standard deviation to be used in the gaussian algorithm
 *
 * Build:
 *    mpicc -Wall -pedantic -c -o gaussianLib.o segment.o 
 *    qdbmp.o gauss.c -o gauss -lm
 * Run Example:
 *    mpirun -np <number of processes> ./gauss <input filename> 
 *    <output filename> <standard deviation>
 * ***************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "segment.h"
#include "mpi.h"
#include "qdbmp.h"
#include "gaussianLib.h"
#define BOSSMAN 0
#define K_DIM 3
#define BUFF 1000000
#define S_TAG 0
#define W_TAG 1
#define H_TAG 2
#define D_TAG 3
#define DATA_TAG 4
#define MAX_FILE_PATH 128
#define SLEEP 10000

/*****************************************************************************
 * Function: parse_args
 *
 * Function to parse command line arguments
 *
 * Parameters:
 *     1. The number of command line parameters
 *     2. Array of pointers to each command line parameter
 *     3. Name of the bmp to be processed
 *     4. Name of the output bmp file
 *     5. The standard deviation
 *
 * Returns: 0 on Success, -1 on Failure
 *
 ****************************************************************************/

int parse_args(int argc, char **argv, char *in_file, char *out_file, float *sd)
{
  if ((argc != 4) ||
      ((strcpy(in_file, argv[1])) == NULL) ||
      ((strcpy(out_file, argv[2])) == NULL) ||
      ((*sd = (float)atoi(argv[3])) <= 0)){
    fprintf(stderr,
            "Usage: %s <input file> <output file> standard deviation\n", argv[0]);
    return (-1);
  }
  return(0);
}

/*****************************************************************************
 * Function: collect_results
 *
 * Function to collect the process image data and reorganise the segments to 
 * remove the overlapping sections
 *
 * Parameters:
 *     1. The number of worker processes
 *     2. The BMP output file
 *     3. Depth of the BMP
 *     4. The bmp segment
 *     5. Size of the biggest segment
 *
 * Returns: 0 on Success
 *
 ****************************************************************************/

int collect_results(int slave, BMP *bmp_out, int depth, struct segment *head, int max_data_size)
{

  UCHAR **data;
  MPI_Status r_status;
  struct segment *seg;
  MPI_Request r_request[slave];
  int finished = 0;
  seg = head;

  //allocate space for the image data
  data = calloc(slave, sizeof(UCHAR *));
  for (int i = 0; i < slave; i++)
  {
    data[i] = malloc(max_data_size * sizeof(UCHAR));
  }

  /* Pool the receipt of all other ranks */
  //Receive the processed data from workers
  do
  {
    MPI_Irecv(data[seg->id - 1], seg->size, MPI_UNSIGNED_CHAR, seg->id, DATA_TAG,
              MPI_COMM_WORLD, &r_request[seg->id - 1]);
   
  } while ((seg = seg->next) != NULL);
  seg = head;

  do
  {
    int flag, index;
    //Check to see when the worker request has been received
    usleep(SLEEP);
    MPI_Testany(slave, r_request, &index, &flag, &r_status);
    
    //When the flag is true assign the index to seg.id for 0 indexing
    if (flag)
    {
      BMP *section;
      seg = head;
      do
      {
        if (seg->id - 1 == index)
          break;
      } while ((seg = seg->next));
    
      //Set the data into new bitmap section, remove overlapping sections
      section = BMP_Create(seg->w, seg->h, depth);
      BMP_SetData(section, data[seg->id - 1]);
      reorder_segments(seg, section, bmp_out);
      BMP_Free(section);

      finished++;
    }

  } while (finished < slave);

  free(data);


  return 0;
}

/*****************************************************************************
 * Function: bossman
 *
 * Function handle all of bossmans tasks
 *
 * Parameters:
 *     1. The number of worker processes
 *     2. The kernel dimension
 *     3. Name of the original bmp
 *     4. Name of the output bmp file
 *
 * Returns: 0 on Success
 *
 ****************************************************************************/

int bossman(int slave, int kernel_dim, char *in_file, char *out_file)
{

  USHORT depth;
  UINT width, height;
  BMP *bmp, *bmp_out;
  struct segment *head, *seg;
  int f_out, overlap, max_data_size;

  bmp_out = NULL;
  //Read the file in and calculation dimensions
  bmp = BMP_ReadFile(in_file);
  height = BMP_GetHeight(bmp);
  width = BMP_GetWidth(bmp);
  depth = BMP_GetDepth(bmp);

  //Open/create the output file
  if ((f_out = open(out_file, O_WRONLY | O_CREAT, 0666)) < 0)
  {
    fprintf(stderr, "There was a problem opening the output file");
    exit(EXIT_FAILURE);
  }
  // Create the image segments
  head = create_segments(bmp, height, width, depth, slave, kernel_dim, &overlap, &max_data_size);
  seg = head;

  //Check the segments were created properly
  if (seg == NULL)
  {
    BMP_Free(bmp);
    close(f_out);
    fprintf(stderr, "The current segment is empty\n");
    exit(EXIT_FAILURE);
  }

  UCHAR *data[slave];
  //Send the segment data to the workers
  do
  {
    data[seg->id - 1] = BMP_GetData(seg->bmp);
    MPI_Send(&seg->size, 1, MPI_UNSIGNED_LONG, seg->id, S_TAG, MPI_COMM_WORLD);
    MPI_Send(&seg->w, 1, MPI_UNSIGNED_LONG, seg->id, W_TAG, MPI_COMM_WORLD);
    MPI_Send(&seg->h, 1, MPI_UNSIGNED_LONG, seg->id, H_TAG, MPI_COMM_WORLD);
    MPI_Send(&depth, 1, MPI_UNSIGNED_SHORT, seg->id, D_TAG, MPI_COMM_WORLD);
    MPI_Send(data[seg->id - 1], seg->size, MPI_UNSIGNED_CHAR, seg->id,
             DATA_TAG, MPI_COMM_WORLD);

  } while ((seg = seg->next) != NULL);

  //Collect the results from the workers
  if (collect_results(slave, bmp, depth, head, max_data_size) != EXIT_SUCCESS)
  {
    fprintf(stderr, "There was a problem receiving the results");
    exit(EXIT_FAILURE);
  }
  //Write the new bmp and check for errors
  BMP_WriteFile(bmp, f_out);
  if (close(f_out) < 0)
  {
    fprintf(stderr, "There was a problem closing the file");
    exit(EXIT_FAILURE);
  }
  //Free the bmp
  BMP_Free(bmp_out);

  return 0;
}

/*****************************************************************************
 * Function: main
 *
 * Main program the handles the MPI initialisation, creates the required 
 * number of processes, generates the kernel, handles the worker tasks,
 * frees the new BMP and finalizes MPI
 *
 * Returns: 0 on Success, exits on Failure
 *
 ****************************************************************************/

int main(int argc, char **argv) {

  int np, taskid, error;
  int slave, kernel_dim, origin;
  float sd;  
  char in_file[MAX_FILE_PATH], out_file[MAX_FILE_PATH];
  float kernel_max, colour_max;

  UINT size, width, height;
  USHORT depth;
  UCHAR *data;
  BMP *bmp, *new_bmp;
  MPI_Status status;
  
  //Initialising MPI
  if ((error = MPI_Init(&argc, &argv)) != MPI_SUCCESS)
  {
    fprintf(stderr, "Failed to initialize MPI\n");
    return error;
  }
  // Initialising MPI Communicator Rank
  if ((error = MPI_Comm_rank(MPI_COMM_WORLD, &taskid)) != MPI_SUCCESS)
  {
    fprintf(stderr, "Failed to initialise MPI communicator group\n");
    return error;
  }
  ////Initialising MPI Communicator Size
  if ((error = MPI_Comm_size(MPI_COMM_WORLD, &np)) != MPI_SUCCESS)
  {
    fprintf(stderr, "Failed to initialise MPI communicator size\n");
    return error;
  }
  slave = np - 1;

  //Parse command line arguments
  if (parse_args(argc, argv, in_file, out_file, &sd) < 0)
  {
    printf("There was a problem parsing the command line arguments");
    exit(EXIT_FAILURE);
  }
  
  //Calculate kernel dimensions and generate kernel
  kernel_dim = (2 * (K_DIM* sd)) + 1;
  origin = K_DIM * sd;
  float **kernel = (float **)malloc(kernel_dim * sizeof(float *));
  for (int i = 0; i < kernel_dim; i++)
  {
    kernel[i] = (float *)malloc(kernel_dim * sizeof(float));
  }
  generateGaussianKernel(kernel, kernel_dim, sd, origin, &kernel_max, &colour_max);

   //Bossman call to perform its tasks
   if (taskid == BOSSMAN) {
    if (bossman(slave, kernel_dim, in_file, out_file) != EXIT_SUCCESS) {
      printf("There was a problem calling the master");
      exit(EXIT_FAILURE);
    }
    //Worker tasks
  } else {
    MPI_Recv(&size, 1, MPI_UNSIGNED_LONG, BOSSMAN, S_TAG,
             MPI_COMM_WORLD, &status);
    MPI_Recv(&width, 1, MPI_UNSIGNED_LONG, BOSSMAN, W_TAG,
             MPI_COMM_WORLD, &status);
    MPI_Recv(&height, 1, MPI_UNSIGNED_LONG, BOSSMAN, H_TAG,
             MPI_COMM_WORLD, &status);
    MPI_Recv(&depth, 1, MPI_UNSIGNED_SHORT, BOSSMAN, D_TAG,
             MPI_COMM_WORLD, &status);
    data = calloc(size, sizeof(UCHAR));
    MPI_Recv(data, size, MPI_UNSIGNED_CHAR, BOSSMAN, DATA_TAG,
             MPI_COMM_WORLD, &status);

    //Create the original and new bmp
    bmp = BMP_Create(width, height, depth);
    BMP_SetData(bmp, data);
    new_bmp = BMP_Create(width, height, depth);

    //Workers process the data and send results back to master
    applyConvolution(kernel, kernel_dim, origin, colour_max, bmp, new_bmp);
    data = BMP_GetData(new_bmp);
    MPI_Send(data, size, MPI_UNSIGNED_CHAR, BOSSMAN, DATA_TAG,
             MPI_COMM_WORLD);
    
    //Free the new bmp from memory
    if (new_bmp != NULL)
      BMP_Free(new_bmp);
  }

  //Finalize MPI
  MPI_Finalize();

}
