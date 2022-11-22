#include <stdlib.h>
#include "qdbmp.h"
#include "segment.h"


/*****************************************************************************
 * Function: create_segments
 *
 * Function to split the bmp into horizontal segments.
 *
 * Parameters:
 *     1. The original bmp
 *     2. Height of the bmp
 *     3. Width of the bmp
 *     4. Depth of the bmp
 *     5. Number of processes/segments
 *     6. Size of the kernel
 *     7. Overlap across segments
 *     8. Size of the largest segment
 *
 * Returns: Segments on Success or exits on Failure
 *
 ****************************************************************************/
struct segment *create_segments(BMP *bmp, UINT height, UINT width, UINT depth, int num, int kern_size,
  int *overlap, int *max_size) {
  UCHAR r, g, b;
  struct segment *head;
  UINT i, x, y, pixel_y;
  UINT segment_height;
  head = NULL;

  //Calculate the height of each segment
  segment_height = (height / num - 1);
  if (segment_height < kern_size)
  {
    //Check to make sure the segment height is larger than the kernel
    fprintf(stderr, "The segment height must be larger than the kernel");
    return head;
  }

  //Calculate the overlap needed. 
  *overlap = (kern_size - 1) / 2;

  // Create segments
  for (i = 0; i < num; i++) {
    struct segment *seg;

    seg = malloc(sizeof(*seg));
    if (seg == NULL) break;

    seg->next = head;
    head = seg;

    //Assign segment dimensions
    seg->bottom_extra = i == 0 ? 0 : *overlap;
    seg->top_extra = i < num - 1 ? *overlap : 0;
    seg->id = i + 1;
    seg->imaxy = (segment_height * (i + 1)) + seg->top_extra;
    seg->iminy = (segment_height * i) - seg->bottom_extra;

    //if there are extras, assign them to the last segment
    if (height - seg->imaxy < segment_height)
    {
      seg->imaxy = height;
      seg->top_extra = 0;
    }

    seg->h = seg->imaxy - seg->iminy;
    seg->w = width;

    //Create a bmp for the segment
    seg->bmp = BMP_Create(width, seg->h, depth);
    if (seg->bmp == NULL) 
      fprintf(stderr, "There was a problem creating the segment");
    seg->size = BMP_GetDataSize(seg->bmp);
    if (*max_size < seg->size) *max_size = seg->size;

    //Get the original pixel values and set them for the new segment bmp
    for (pixel_y = 0, y = seg->iminy; y < seg->imaxy; y++, pixel_y++) {
      for (x = 0; x < width; x++) {
        BMP_GetPixelRGB(bmp, x, y, &r, &g, &b);
        BMP_SetPixelRGB(seg->bmp, x, pixel_y, r, g, b);
      }
    }
  }
  if (head != NULL)
    return head;
  else
    exit(EXIT_FAILURE);

}

/*****************************************************************************
 * Function: reorder_segments
 *
 * Function to set only the pixels that process is responsible for and
 * not the overlapping sections
 *
 * Parameters:
 *     1. The segment
 *     2. The original bmp
 *     3. The output bmp
 *
 ****************************************************************************/
void reorder_segments(struct segment *seg, BMP *bmp, BMP *bmp_out) {

  int x, y, pixel_y;
  UCHAR r, g, b;

  // Get the original pixel values and set them for the new segment bmp
  //but this time without the overlapping sections
  for (y = 0 + seg->bottom_extra, pixel_y = seg->iminy + y;
      pixel_y < seg->imaxy - seg->top_extra; y++, pixel_y++) {
    for (x = 0; x < seg->w; x++) {

      BMP_GetPixelRGB(bmp, x, y, &r, &g, &b);
      BMP_SetPixelRGB(bmp_out, x, pixel_y, r, g, b);

    }
  }
}


