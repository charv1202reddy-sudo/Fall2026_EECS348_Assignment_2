/* =============================================================
 * Program: EECS 348 Assignment 2 - CEO Email Priority Queue
 * -------------------------------------------------------------
 * Description:
 *   Implements a priority queue for a CEO's email inbox using a
 *   from-scratch MaxHeap (list/array-based implementation).
 *   Emails are prioritized first by sender category (Boss >
 *   Subordinate > Peer > ImportantPerson > OtherPerson), and
 *   within the same category, by newest date first.
 *
 *   The program reads commands from a text file:
 *     EMAIL <category>,<subject>,<date>  - add an email
 *     NEXT                               - show highest-priority email
 *     READ                               - remove highest-priority email
 *     COUNT                              - show number of unread emails
 *
 * Inputs:
 *   argv[1] - path to a text file containing the commands above.
 *
 * Output:
 *   Terminal output per command, as described above.
 *
 * Collaborators: None
 * Other sources:
 *   - Initial code drafted by ChatGPT and Google Gemini (see
 *     GenAI analysis PDF for prompts and raw output).
 *
 * Author: Charvi Reddy Konudula
 * Creation date: September 17, 2026
 * Revision date: September 17, 2026
 * Revisions:
 *   - Added missing "Next email:" header to NEXT command output
 *     (both original GenAI versions omitted it).
 *   - Added a warning message for malformed EMAIL lines instead
 *     of silently ignoring them.
 *   - Converted heapify_down from recursive to iterative to
 *     remove function-call overhead on every sift-down.
 * ============================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Starting capacity of the heap's backing array. It doubles
 * automatically whenever it fills up, so this is just a
 * reasonable starting point, not a hard limit. */
#define INITIAL_CAPACITY 16

/* -------------------------------------------------------------
 * PriorityCategory
 * A larger enum value means a HIGHER priority (read sooner).
 * Boss = 5 is highest, OtherPerson = 1 is lowest.
 * ------------------------------------------------------------- */
typedef enum {
    OTHER_PERSON = 1,
    IMPORTANT_PERSON = 2,
    PEER = 3,
    SUBORDINATE = 4,
    BOSS = 5
} PriorityCategory;

/* -------------------------------------------------------------
 * Email
 * Holds everything the program needs to know about one email:
 * its priority category, the raw text fields for display, and
 * the date broken into integers so dates can be compared
 * numerically instead of as strings.
 * ------------------------------------------------------------- */
typedef struct {
    PriorityCategory category; /* numeric priority, derived from sender_str */
    char sender_str[32];       /* original sender category string, for display */
    char subject[256];         /* email subject line (may contain spaces) */
    char date_str[11];         /* original "MM-DD-YYYY" string, for display */
    int year;                  /* parsed year, for numeric comparison */
    int month;                 /* parsed month, for numeric comparison */
    int day;                   /* parsed day, for numeric comparison */
} Email;

/* -------------------------------------------------------------
 * MaxHeap
 * A list-based (array-based) binary max-heap. "data" is a
 * dynamically-allocated array that doubles in size whenever it
 * runs out of room, so the heap can grow to hold any number of
 * emails without a hardcoded cap.
 * ------------------------------------------------------------- */
typedef struct {
    Email *data;     /* dynamic array backing the heap */
    int size;        /* number of emails currently stored */
    int capacity;    /* allocated size of "data" */
} MaxHeap;

/* -------------------------------------------------------------
 * parse_category
 * Converts a sender-category string (e.g. "Boss") into its
 * matching PriorityCategory enum value. Any unrecognized string
 * is treated as OtherPerson (lowest priority) as a safe default.
 * ------------------------------------------------------------- */
PriorityCategory parse_category(const char *cat) {
    if (strcmp(cat, "Boss") == 0) return BOSS;
    if (strcmp(cat, "Subordinate") == 0) return SUBORDINATE;
    if (strcmp(cat, "Peer") == 0) return PEER;
    if (strcmp(cat, "ImportantPerson") == 0) return IMPORTANT_PERSON;
    return OTHER_PERSON;
}

/* -------------------------------------------------------------
 * parse_date
 * Splits a "MM-DD-YYYY" string into three integers so dates can
 * later be compared numerically (newer = larger year, then
 * larger month, then larger day).
 * ------------------------------------------------------------- */
void parse_date(const char *date_str, int *m, int *d, int *y) {
    sscanf(date_str, "%d-%d-%d", m, d, y);
}

/* -------------------------------------------------------------
 * compare_emails
 * Returns a positive number if email "a" should be read BEFORE
 * email "b" (i.e., a has higher priority), negative if "a"
 * should be read after "b", and 0 if they tie exactly.
 *
 * Comparison order:
 *   1. Sender category (Boss beats Subordinate beats Peer, etc.)
 *   2. If categories match, the newer date wins (year, then
 *      month, then day).
 * ------------------------------------------------------------- */
int compare_emails(const Email *a, const Email *b) {
    if (a->category != b->category) {
        return a->category - b->category; /* bigger enum value = higher priority */
    }
    if (a->year != b->year) return a->year - b->year;   /* newer year wins */
    if (a->month != b->month) return a->month - b->month; /* newer month wins */
    return a->day - b->day;                              /* newer day wins */
}

/* -------------------------------------------------------------
 * create_heap
 * Allocates a new, empty MaxHeap with room for INITIAL_CAPACITY
 * emails. Caller is responsible for calling free_heap() later.
 * ------------------------------------------------------------- */
MaxHeap* create_heap() {
    MaxHeap *heap = (MaxHeap*)malloc(sizeof(MaxHeap));   /* allocate the heap struct itself */
    heap->capacity = INITIAL_CAPACITY;                   /* start with room for 16 emails */
    heap->size = 0;                                      /* heap starts empty */
    heap->data = (Email*)malloc(heap->capacity * sizeof(Email)); /* allocate backing array */
    return heap;
}

/* -------------------------------------------------------------
 * free_heap
 * Releases all memory owned by the heap. Safe to call with a
 * NULL pointer.
 * ------------------------------------------------------------- */
void free_heap(MaxHeap *heap) {
    if (heap) {
        free(heap->data); /* free the backing array first */
        free(heap);        /* then free the struct itself */
    }
}

/* -------------------------------------------------------------
 * swap
 * Exchanges the contents of two Email slots in the heap array.
 * Used by both sift-up and sift-down during heap maintenance.
 * ------------------------------------------------------------- */
void swap(Email *a, Email *b) {
    Email temp = *a; /* stash a's value */
    *a = *b;          /* copy b into a's slot */
    *b = temp;        /* copy the stashed value into b's slot */
}

/* -------------------------------------------------------------
 * heapify_up (sift-up)
 * Called right after inserting a new email at the end of the
 * array. Repeatedly swaps the new email with its parent until
 * the parent has equal or higher priority, restoring the heap
 * property from the bottom up.
 * ------------------------------------------------------------- */
void heapify_up(MaxHeap *heap, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2; /* index of this node's parent */
        if (compare_emails(&heap->data[index], &heap->data[parent]) > 0) {
            swap(&heap->data[index], &heap->data[parent]); /* child outranks parent, so swap up */
            index = parent; /* continue checking from the new position */
        } else {
            break; /* heap property satisfied, stop */
        }
    }
}

/* -------------------------------------------------------------
 * heapify_down (sift-down)
 * Called after removing the root (highest-priority email) and
 * moving the last element into its place. Repeatedly swaps that
 * element down with its highest-priority child until the heap
 * property is restored. Written iteratively (a while-loop
 * instead of recursion) to avoid function-call overhead on
 * every step, since this runs on every READ/extract.
 * ------------------------------------------------------------- */
void heapify_down(MaxHeap *heap, int index) {
    while (1) {
        int largest = index;              /* assume current node is largest for now */
        int left = 2 * index + 1;         /* index of left child */
        int right = 2 * index + 2;        /* index of right child */

        /* If the left child exists and outranks the current largest, update largest. */
        if (left < heap->size && compare_emails(&heap->data[left], &heap->data[largest]) > 0) {
            largest = left;
        }
        /* If the right child exists and outranks the current largest, update largest. */
        if (right < heap->size && compare_emails(&heap->data[right], &heap->data[largest]) > 0) {
            largest = right;
        }

        if (largest == index) {
            break; /* current node already outranks both children; heap property restored */
        }

        swap(&heap->data[index], &heap->data[largest]); /* move the higher-priority child up */
        index = largest; /* continue sifting down from the child's old position */
    }
}

/* -------------------------------------------------------------
 * heap_insert
 * Adds a new email to the heap. Grows the backing array
 * (doubling its capacity) if it's currently full, so there is
 * no hardcoded limit on the number of emails the inbox can hold.
 * ------------------------------------------------------------- */
void heap_insert(MaxHeap *heap, Email email) {
    if (heap->size == heap->capacity) {
        heap->capacity *= 2; /* double the capacity */
        heap->data = (Email*)realloc(heap->data, heap->capacity * sizeof(Email)); /* grow the array */
    }

    heap->data[heap->size] = email;  /* place new email at the next free slot */
    heapify_up(heap, heap->size);    /* restore heap order by sifting it up */
    heap->size++;                    /* one more email is now stored */
}

/* -------------------------------------------------------------
 * heap_extract_max
 * Removes the highest-priority email (the root) from the heap
 * and copies it into *result. Returns 1 on success, 0 if the
 * heap was empty (result is left untouched in that case).
 * ------------------------------------------------------------- */
int heap_extract_max(MaxHeap *heap, Email *result) {
    if (heap->size <= 0) return 0; /* nothing to extract */

    *result = heap->data[0];                     /* save the root (highest priority) */
    heap->data[0] = heap->data[heap->size - 1];   /* move the last element to the root */
    heap->size--;                                 /* shrink the heap by one */

    if (heap->size > 0) {
        heapify_down(heap, 0); /* restore heap order from the root down */
    }

    return 1;
}

/* -------------------------------------------------------------
 * heap_peek_max
 * Copies the highest-priority email into *result WITHOUT
 * removing it from the heap. Returns 1 on success, 0 if the
 * heap is empty.
 * ------------------------------------------------------------- */
int heap_peek_max(const MaxHeap *heap, Email *result) {
    if (heap->size <= 0) return 0; /* nothing to peek at */
    *result = heap->data[0];        /* root is always the highest priority */
    return 1;
}

/* -------------------------------------------------------------
 * process_command
 * Parses one line of input and performs the corresponding
 * action (EMAIL / NEXT / READ / COUNT) on the heap.
 * ------------------------------------------------------------- */
void process_command(MaxHeap *heap, char *line) {
    line[strcspn(line, "\r\n")] = '\0'; /* strip trailing newline/carriage return */

    if (strlen(line) == 0) return; /* ignore blank lines */

    if (strncmp(line, "EMAIL ", 6) == 0) {
        char *payload = line + 6; /* skip past "EMAIL " to the actual fields */
        Email email;

        /* Split "<category>,<subject>,<date>" on commas.
         * NOTE: this assumes the subject itself contains no
         * commas, per the assignment's stated assumptions. */
        char *cat_tok = strtok(payload, ",");
        char *subj_tok = strtok(NULL, ",");
        char *date_tok = strtok(NULL, ",");

        if (cat_tok && subj_tok && date_tok) {
            /* Copy each field into the Email struct, leaving room for the null terminator. */
            strncpy(email.sender_str, cat_tok, sizeof(email.sender_str) - 1);
            strncpy(email.subject, subj_tok, sizeof(email.subject) - 1);
            strncpy(email.date_str, date_tok, sizeof(email.date_str) - 1);

            /* Guarantee null-termination even if the source string was too long. */
            email.sender_str[sizeof(email.sender_str) - 1] = '\0';
            email.subject[sizeof(email.subject) - 1] = '\0';
            email.date_str[sizeof(email.date_str) - 1] = '\0';

            email.category = parse_category(email.sender_str);              /* derive numeric priority */
            parse_date(email.date_str, &email.month, &email.day, &email.year); /* derive numeric date */

            heap_insert(heap, email); /* add the fully-built email to the heap */
        } else {
            /* Malformed EMAIL line (missing a field) - warn instead of
             * silently dropping it, so problems are visible during testing. */
            fprintf(stderr, "Warning: malformed EMAIL line ignored: %s\n", line);
        }
    } else if (strcmp(line, "NEXT") == 0) {
        Email top;
        if (heap_peek_max(heap, &top)) {
            /* "Next email:" header restored to match the assignment's
             * required sample output (both original GenAI versions omitted it). */
            printf("Next email:\n");
            printf("Sender: %s\n", top.sender_str);
            printf("Subject: %s\n", top.subject);
            printf("Date: %s\n", top.date_str);
        } else {
            printf("There are no emails in the queue.\n");
        }
    } else if (strcmp(line, "READ") == 0) {
        Email top;
        if (!heap_extract_max(heap, &top)) {
            printf("There are no emails to read.\n");
        }
        /* On success, intentionally print nothing: READ silently
         * removes the top email per the assignment spec. */
    } else if (strcmp(line, "COUNT") == 0) {
        printf("There are %d emails to read.\n", heap->size);
    }
}

/* -------------------------------------------------------------
 * main
 * Opens the input file named on the command line, reads it one
 * line at a time, and processes each line as a command until
 * end-of-file, then cleans up.
 * ------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input_file.txt>\n", argv[0]); /* remind the user how to run the program */
        return 1;
    }

    FILE *file = fopen(argv[1], "r"); /* open the command file for reading */
    if (!file) {
        perror("Error opening file"); /* print a system error message if the open failed */
        return 1;
    }

    MaxHeap *inbox = create_heap(); /* start with an empty inbox */
    char line[512];                  /* buffer for one line of input */

    while (fgets(line, sizeof(line), file)) { /* read the file one line at a time */
        process_command(inbox, line);          /* handle whatever command that line contains */
    }

    fclose(file);       /* done reading the file */
    free_heap(inbox);   /* release all heap memory before exiting */
    return 0;
}
