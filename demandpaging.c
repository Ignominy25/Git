#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constants
#define PAGE_SIZE 4096  // 4 KB
#define TOTAL_FRAMES 16384  // 64 MB / 4 KB
#define USER_FRAMES 12288   // 48 MB / 4 KB
#define PAGE_TABLE_SIZE 2048
#define ESSENTIAL_PAGES 10
#define VALID_BIT_MASK 0x8000  // Most significant bit
#define MAX_PROCESSES 500  // Maximum number of processes
#define MAX_SEARCHES 100   // Maximum searches per process

// Process state structure
typedef struct {
    unsigned short page_table[PAGE_TABLE_SIZE];
    int array_size;
    int search_indices[MAX_SEARCHES];
    int num_searches;
    int current_search;
    int frames_allocated;
    int is_active;
} Process;

// Queue structure for swap management
typedef struct {
    int items[MAX_PROCESSES];
    int front;
    int rear;
} SwapQueue;

// System state structure
typedef struct {
    int free_frames[TOTAL_FRAMES];
    int num_free_frames;
    Process processes[MAX_PROCESSES];
    int num_processes;
    SwapQueue swap_queue;
    int page_accesses;
    int page_faults;
    int num_swaps;
    int min_active_processes;
} SystemState;

// Function prototypes
void initQueue(SwapQueue *q);
int queueIsEmpty(SwapQueue *q);
void enqueue(SwapQueue *q, int process_id);
int dequeue(SwapQueue *q);
int get_active_process_count(SystemState *system);
void swap_out_process(SystemState *system, int process_id);
void swap_in_process(SystemState *system, int process_id);
int handle_page_fault(SystemState *system, int process_id, int page_num);
void simulate_binary_search(SystemState *system, int process_id);
void print_statistics(SystemState *system);

// Queue operations implementation
void initQueue(SwapQueue *q) {
    q->front = q->rear = -1;
}

int queueIsEmpty(SwapQueue *q) {
    return q->front == -1;
}

void enqueue(SwapQueue *q, int process_id) {
    if (q->front == -1) {
        q->front = q->rear = 0;
    } else {
        q->rear = (q->rear + 1) % MAX_PROCESSES;
    }
    q->items[q->rear] = process_id;
}

int dequeue(SwapQueue *q) {
    if (queueIsEmpty(q)) return -1;
    
    int item = q->items[q->front];
    if (q->front == q->rear) {
        q->front = q->rear = -1;
    } else {
        q->front = (q->front + 1) % MAX_PROCESSES;
    }
    return item;
}

// Helper function implementation
int get_active_process_count(SystemState *system) {
    int count = 0;
    for (int i = 0; i < system->num_processes; i++) {
        if (system->processes[i].is_active) count++;
    }
    return count;
}

// Main system functions
void initialize_system(SystemState *system, const char *input_file) {
    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        fprintf(stderr, "Error opening input file\n");
        exit(1);
    }

    // Initialize system state
    memset(system, 0, sizeof(SystemState));
    initQueue(&system->swap_queue);
    
    // Initialize free frames
    system->num_free_frames = USER_FRAMES;
    for (int i = 0; i < USER_FRAMES; i++) {
        system->free_frames[i] = i;
    }
    
    // Read number of processes and searches per process
    int num_searches;
    if (fscanf(fp, "%d %d", &system->num_processes, &num_searches) != 2) {
        fprintf(stderr, "Error reading process count and search count\n");
        fclose(fp);
        exit(1);
    }

    if (system->num_processes <= 0 || system->num_processes > MAX_PROCESSES ||
        num_searches <= 0 || num_searches > MAX_SEARCHES) {
        fprintf(stderr, "Invalid number of processes or searches\n");
        fclose(fp);
        exit(1);
    }
    
    // Initialize each process
    for (int i = 0; i < system->num_processes; i++) {
        Process *p = &system->processes[i];
        memset(p, 0, sizeof(Process));
        
        p->num_searches = num_searches;
        
        // Read array size
        if (fscanf(fp, "%d", &p->array_size) != 1) {
            fprintf(stderr, "Error reading array size for process %d\n", i);
            fclose(fp);
            exit(1);
        }
        
        // Read search indices
        for (int j = 0; j < p->num_searches; j++) {
            if (fscanf(fp, "%d", &p->search_indices[j]) != 1) {
                fprintf(stderr, "Error reading search index %d for process %d\n", j, i);
                fclose(fp);
                exit(1);
            }
        }
        
        // Initialize page table and allocate essential frames
        for (int j = 0; j < ESSENTIAL_PAGES && system->num_free_frames > 0; j++) {
            p->page_table[j] = system->free_frames[--system->num_free_frames] | VALID_BIT_MASK;
            p->frames_allocated++;
        }
        
        p->current_search = 0;
        p->is_active = 1;
    }
    
    fclose(fp);
    system->min_active_processes = system->num_processes;
    
    printf("+++ Simulation data read from file\n");
    printf("+++ Kernel data initialized\n");
}

void swap_out_process(SystemState *system, int process_id) {
    Process *p = &system->processes[process_id];
    
    // Only swap out if process is active
    if (!p->is_active) return;
    
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        if (p->page_table[i] & VALID_BIT_MASK) {
            system->free_frames[system->num_free_frames++] = p->page_table[i] & ~VALID_BIT_MASK;
            p->page_table[i] = 0;
        }
    }
    
    p->frames_allocated = 0;
    p->is_active = 0;
    enqueue(&system->swap_queue, process_id);
    system->num_swaps++;
    
    int active_count = get_active_process_count(system);
    if (active_count < system->min_active_processes) {
        system->min_active_processes = active_count;
    }
    
    printf("+++ Swapping out process %3d [%3d active processes]\n", 
           process_id, active_count);
}

int handle_page_fault(SystemState *system, int process_id, int page_num) {
    Process *p = &system->processes[process_id];
    
    if (system->num_free_frames > 0) {
        p->page_table[page_num] = system->free_frames[--system->num_free_frames] | VALID_BIT_MASK;
        p->frames_allocated++;
        return 1;
    }
    
    swap_out_process(system, process_id);
    return 0;
}


void swap_in_process(SystemState *system, int process_id) {
    Process *p = &system->processes[process_id];
    
    // Don't swap in if already active
    if (p->is_active) return;
    
    for (int i = 0; i < ESSENTIAL_PAGES && system->num_free_frames > 0; i++) {
        p->page_table[i] = system->free_frames[--system->num_free_frames] | VALID_BIT_MASK;
        p->frames_allocated++;
    }
    
    p->is_active = 1;
    system->num_swaps++;
    
    printf("+++ Swapping in process %3d [%3d active processes]\n", 
           process_id, system->min_active_processes);
}

void simulate_binary_search(SystemState *system, int process_id) {
    Process *p = &system->processes[process_id];
    if (!p->is_active || p->current_search >= p->num_searches) return;
    
    int search_key = p->search_indices[p->current_search];
    
#ifdef VERBOSE
    printf("\tSearch %d by Process %d\n", p->current_search + 1, process_id);
#endif
    
    int L = 0;
    int R = p->array_size - 1;
    
    while (L < R) {
        int M = (L + R) / 2;
        int page_num = (M * 4) / PAGE_SIZE + ESSENTIAL_PAGES;
        system->page_accesses++;
        
        if (!(p->page_table[page_num] & VALID_BIT_MASK)) {
            system->page_faults++;
            if (!handle_page_fault(system, process_id, page_num)) {
                return;
            }
        }
        
        if (search_key <= M) {
            R = M;
        } else {
            L = M + 1;
        }
    }
    
    p->current_search++;
    
    if (p->current_search >= p->num_searches) {
        // Process finished all searches
        for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
            if (p->page_table[i] & VALID_BIT_MASK) {
                system->free_frames[system->num_free_frames++] = p->page_table[i] & ~VALID_BIT_MASK;
                p->page_table[i] = 0;
            }
        }
        p->frames_allocated = 0;
        
        // Try to swap in processes from queue
        while (!queueIsEmpty(&system->swap_queue) && system->num_free_frames >= ESSENTIAL_PAGES) {
            int next_process = dequeue(&system->swap_queue);
            if (next_process != -1 && !system->processes[next_process].is_active) {
                swap_in_process(system, next_process);
            }
        }
    }
}

void print_statistics(SystemState *system) {
    printf("+++ Page access summary\n");
    printf("\tTotal number of page accesses  = %7d\n", system->page_accesses);
    printf("\tTotal number of page faults    = %7d\n", system->page_faults);
    printf("\tTotal number of swaps          = %7d\n", system->num_swaps / 2);
    printf("\tDegree of multiprogramming     = %7d\n", system->min_active_processes);
}

int main() {
    SystemState system;
    initialize_system(&system, "search.txt");
    
    int active_process = 0;
    while (1) {
        int all_done = 1;
        
        for (int i = 0; i < system.num_processes; i++) {
            if (system.processes[i].current_search < system.processes[i].num_searches) {
                all_done = 0;
                break;
            }
        }
        
        if (all_done) break;
        
        simulate_binary_search(&system, active_process);
        active_process = (active_process + 1) % system.num_processes;
    }
    
    print_statistics(&system);
    return 0;
}
