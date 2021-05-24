#include "constants.h"

/* This structure represents a TAG service, a TST
   entry. It has a system visibility */
struct tag_service {
   int key; /* If existing, it's the accessing key */
   int owner; /* The user who created the service, if requested */
};

/* This structure represents a open tag service, with 
   single-thread visibility */
struct open_tag {
   int tag_id; /* Tag service id, visible for the specific thread */
   struct tag_service *service;
};
