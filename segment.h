#include "qdbmp.h"

typedef struct segment
{
  int id;                   /* Sequence number of tile */
  int imaxy, iminy;         /* Reference pixels on source image */
  int bottom_extra, top_extra;   /* Margin excluded from tile mapping */
  UINT h, w;                /* Height and width of tile (pixels) */
  UINT size;                /* Size of tile data in bytes */
  BMP *bmp;                
  int flag;            /* Flag to indicate if the tile was processed */
  struct segment *next; /* Next tile in the linked list (or NULL) */
} SEGMENT;

struct segment *create_segments(BMP *src, UINT height, UINT width, UINT depth, int num, int kern_size,
                                int *overlap, int *max_data_size);

void reorder_segments(struct segment *tile, BMP *bmp, BMP *bmp_out);


