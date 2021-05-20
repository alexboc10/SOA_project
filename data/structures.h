#include "constants.h"

/* This structure represents a TAG service, a TST
   entry. It is an element of a double linked list.*/
struct tag_service {
   int key; /* TAG service unique ID */
   int perm; /* TAG service permission */
   struct tag_service *prev;
   struct tag_service *next;
};
