#include "intersect.h"
#include <stdio.h>

intersect_t A = {"A", "Bailey Ln and University St."};
intersect_t B = {"B", "University St. and North Parkway"};
intersect_t C = {"C", "North Parkway and N. McLean Blvd."};
intersect_t D = {"D", "North Parkway and Stonewall St."};
intersect_t E = {"E", "North Parkway and N. Watkins St."};
intersect_t F = {"F", "N. Watkins St. and Galloway Ave."};
intersect_t G = {"G", "Stonewall St. and Galloway Ave."};
intersect_t H = {"H", "Snowden Ave. and N. McLean Blvd."};
intersect_t I = {"I", "University St. and Jackson"};
intersect_t J = {"J", "Jackson Ave. and N. Evergreen St."};
intersect_t K = {"K", "Jackson Ave. and N. Watkins St."};
intersect_t L = {"L", "North Parkway and N. Evergreen St."};

void display_intersect(intersect_t *intersect) {
  printf("%s = %s\n", intersect->id, intersect->desc);
}
