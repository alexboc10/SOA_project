#include "constants.h"

/* This structure represents a TAG service, a TST entry. */
struct tag_service {
   int key; /* If existing, it's the accessing key */
   int owner; /* The user who created the service, if requested */
   int open; /* The service can be open or closed, still existing */
};
