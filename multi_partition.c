/*
Fabiano A. de Sá Filho
GRR: 20223831
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <string.h>

#include "chrono.h"
#include "verifica_particoes.h"

#define INPUT_SIZE 8000000 // 8M, conforme e-mail do professor
#define NTIMES 10 // Numero de vezes foi ajustado no script slurm. Dessa forma não há interferencia da cache
#define MAX_THREADS 64
#define MAX_TOTAL_ELEMENTS (500*1000*1000) // if each float takes 4 bytes
                                            // will have a maximum 500 million FLOAT elements
                                            // which fits in 2 GB of RAM

#define CACHE_SIZE (32*1024*1024) // Faz copias de pelo menos 32 Mb de dados
                                  // para garantir que a cache não interfira na medição


long long geraAleatorioLL() {
    int a = rand();  // Returns a pseudo-random integer
                //    between 0 and RAND_MAX.
    int b = rand();  // same as above
    long long v = (long long)a * 100 + b;
    return v;
}  

void preencheAleatorio(long long* array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        array[i] = geraAleatorioLL();
    }
}

void merge(long long* array, size_t left, size_t mid, size_t right) {
    size_t n1 = mid - left + 1; // Size of the left subarray
    size_t n2 = right - mid;    // Size of the right subarray

    // Create temporary arrays
    long long* L = (long long*)malloc(n1 * sizeof(long long));
    long long* R = (long long*)malloc(n2 * sizeof(long long));

    // Copy data to temporary arrays
    for (size_t i = 0; i < n1; i++)
        L[i] = array[left + i];
    for (size_t j = 0; j < n2; j++)
        R[j] = array[mid + 1 + j];

    // Merge the temporary arrays back into the original array
    size_t i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            array[k] = L[i];
            i++;
        } else {
            array[k] = R[j];
            j++;
        }
        k++;
    }

    // Copy any remaining elements from the left subarray
    while (i < n1) {
        array[k] = L[i];
        i++;
        k++;
    }

    // Copy any remaining elements from the right subarray
    while (j < n2) {
        array[k] = R[j];
        j++;
        k++;
    }

    // Free the temporary arrays
    free(L);
    free(R);
}

// Recursive function to implement Merge Sort
void mergeSort(long long* array, size_t left, size_t right) {
    if (left < right) {
        size_t mid = left + (right - left) / 2;

        // Sort the left and right halves
        mergeSort(array, left, mid);
        mergeSort(array, mid + 1, right);

        // Merge the sorted halves
        merge(array, left, mid, right);
    }
}

// Wrapper function for Merge Sort
void sortArray(long long* array, size_t size) {
    mergeSort(array, 0, size - 1);
}

void generatePartitionArray(long long* P, size_t np) {
    size_t partitionCount = np - 1;

    // Fill with random values
    for (size_t i = 0; i < partitionCount; i++) {
        P[i] = geraAleatorioLL();
    }

    // Set the last value to LLONG_MAX
    P[partitionCount] = LLONG_MAX;

    // Sort the array
    sortArray(P, np);
}

// Utilizei uma struct para passar os parametros para as threads
typedef struct {
    long long *Input;
    long long *P;
    long long *Output;
    unsigned int *Pos;
    int *faixa_count;
    int *faixa_indices;
    int n, np;
    int thread_id, num_threads;
    pthread_barrier_t *barrier;
} ThreadData;


// Função que vai ser passada para as threads
// Helper function for binary search
int binary_search(long long *P, int np, long long value) {
    int left = 0, right = np - 1;
    while (left < right) {
        int mid = (left + right) / 2;
        if (value < P[mid])
            right = mid;
        else
            left = mid + 1;
    }
    return left;
}

void *thread_func(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int n = data->n, np = data->np;
    long long *Input = data->Input, *P = data->P, *Output = data->Output;
    unsigned int *Pos = data->Pos;
    int *faixa_count = data->faixa_count, *faixa_indices = data->faixa_indices;

    // Divide input range for this thread
    int chunk_size = (n + data->num_threads - 1) / data->num_threads;
    int start = data->thread_id * chunk_size;
    int end = (start + chunk_size > n) ? n : start + chunk_size;

    // Local faixa_count to reduce contention
    int faixa_count_local[np];
    memset(faixa_count_local, 0, sizeof(int) * np);

    // First pass: Count elements in each faixa using binary search
    for (int i = start; i < end; i++) {
        int faixa = binary_search(P, np, Input[i]);
        faixa_count_local[faixa]++;
    }

    // Combine local counts into global faixa_count
    for (int i = 0; i < np; i++) {
        __sync_fetch_and_add(&faixa_count[i], faixa_count_local[i]);
    }

    pthread_barrier_wait(data->barrier);

    // Calculate faixa_indices in thread 0
    if (data->thread_id == 0) {
        Pos[0] = 0;
        for (int i = 1; i < np; i++) {
            Pos[i] = Pos[i - 1] + faixa_count[i - 1];
        }
        for (int i = 0; i < np; i++) {
            faixa_indices[i] = Pos[i];
        }
    }

    pthread_barrier_wait(data->barrier);

    // Second pass: Place elements into Output using binary search
    for (int i = start; i < end; i++) {
        int faixa = binary_search(P, np, Input[i]);
        int idx = __sync_fetch_and_add(&faixa_indices[faixa], 1);
        Output[idx] = Input[i];
    }

    return NULL;
}


void multi_partition_parallel(long long *Input, int n, long long *P, int np, 
                              long long *Output, unsigned int *Pos, int num_threads) {
    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    int faixa_count[np];
    int faixa_indices[np];
    pthread_barrier_t barrier;

    // Initialize faixa_count
    for (int i = 0; i < np; i++) {
        faixa_count[i] = 0;
    }
    pthread_barrier_init(&barrier, NULL, num_threads);

    // Cria o pool de threads
    for (int t = 0; t < num_threads; t++) {
        thread_data[t] = (ThreadData){
            .Input = Input,
            .P = P,
            .Output = Output,
            .Pos = Pos,
            .faixa_count = faixa_count,
            .faixa_indices = faixa_indices,
            .n = n,
            .np = np,
            .thread_id = t,
            .num_threads = num_threads,
            .barrier = &barrier
        };
        pthread_create(&threads[t], NULL, thread_func, &thread_data[t]);
    }

    // Espera todas as threads terminarem
    for (int t = 0; t < num_threads; t++) {
        pthread_join(threads[t], NULL);
    }

    pthread_barrier_destroy(&barrier);
}

int main(int argc, char* argv[]) {

    int nThreads;
    int nTotalElements; // O numero de elementos no Vetor P !!
    
    chronometer_t parallelReductionTime;
    
    if( argc != 3 ) {
         printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] ); 
         return 0;
    } else {
         nThreads = atoi( argv[2] );
         if( nThreads == 0 ) {
              printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] );
              printf( "<nThreads> can't be 0\n" );
              return 0;
         }     
         if( nThreads > MAX_THREADS ) {  
              printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] );
              printf( "<nThreads> must be less than %d\n", MAX_THREADS );
              return 0;
         }     
         nTotalElements = atoi( argv[1] ); 
         if( nTotalElements > MAX_TOTAL_ELEMENTS ) {  
              printf( "usage: %s <nTotalElements> <nThreads>\n" ,
                 argv[0] );
              printf( "<nTotalElements> must be up to %d\n", MAX_TOTAL_ELEMENTS );
              return 0;
         }     
    }

    srand(time(NULL));

    int n = INPUT_SIZE;
    int np = nTotalElements;

    // Tive que alocar dinamicamente pois estava tendo segmentation faults na minha máquina
    // Isso diminuiu a performance um pouco
    long long* P = (long long*)malloc(np * sizeof(long long));
    long long* Input = (long long*)malloc(n * sizeof(long long));
    long long* Output = (long long*)malloc(n * sizeof(long long));
    unsigned int* Pos = (unsigned int*)malloc(np * sizeof(long long));

    if (P == NULL || Input == NULL || Output == NULL || Pos == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return EXIT_FAILURE;
    }

    preencheAleatorio(Input, n);
    generatePartitionArray(P, np);

    // Faz vetor de copias
    size_t single_copy_size = (n * sizeof(long long)) + (np * sizeof(long long));
    long long num_copies = CACHE_SIZE / single_copy_size + 1;

    long long **Input_copies = (long long **)malloc(num_copies * sizeof(long long *));
    long long **P_copies = (long long **)malloc(num_copies * sizeof(long long *));

    // Copy data from the original arrays
    for (int i = 0; i < num_copies; i++) {
        Input_copies[i] = (long long *)malloc(n * sizeof(long long));
        P_copies[i] = (long long *)malloc(np * sizeof(long long));
        memcpy(Input_copies[i], Input, n * sizeof(long long));
        memcpy(P_copies[i], P, np * sizeof(long long));
    }

    // Começa a medir
    chrono_reset( &parallelReductionTime );
    chrono_start( &parallelReductionTime );

    printf( "will call multi_partition_parallel %d times\n", NTIMES );

    for( int i=0; i<NTIMES ; i++ ) {

        int copy_index = i % num_copies;
        multi_partition_parallel(Input_copies[copy_index], n, P_copies[copy_index], np, Output, Pos, nThreads);
    }

    chrono_stop( &parallelReductionTime );
    chrono_reportTime( &parallelReductionTime, "multi_partition" );
    

    // calcular e imprimir a VAZAO (numero de operacoes/s)
    double total_time_in_seconds = (double) chrono_gettotal( &parallelReductionTime ) /
                                      ((double)1000*1000*1000);
    printf( "total_time_in_seconds: %lf s\n", total_time_in_seconds );
                                  
    double OPS = ((double)nTotalElements*NTIMES)/total_time_in_seconds;
    double MEPS = OPS/1000000;
    printf( "Throughput: %lf MEP/s\n", MEPS );
    
    verifica_particoes(Input, n, P, np, Output, Pos);

/* PARA IMPRIMIR OS VETORES
    // Print Output and Pos
    printf("Vetor Output: ");
    for (int i = 0; i < n; i++) {
        printf("%lld ", Output[i]);
    }
    printf("\n");

    printf("Vetor Pos: ");
    for (int i = 0; i < np; i++) {
        printf("%d ", Pos[i]);
    }
    printf("\n");
*/

    for (int i = 0; i < num_copies; i++) {
        free(Input_copies[i]);
        free(P_copies[i]);
    }
    free(Input_copies);
    free(P_copies);
    free(P);
    free(Input);
    free(Output);
    free(Pos);
    return 0;
}
