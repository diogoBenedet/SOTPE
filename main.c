#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <string.h>

#define NUM_THREADS 5
#define MAX_ID_LENGTH 20
#define LOG_FILE "logs.txt"
#define IDS_FILE "lista_ids.txt"

// Estrutura para passar dados para as threads
typedef struct {
    int thread_id;
    FILE *log_file;
    pthread_mutex_t *log_mutex;
    pthread_mutex_t *ids_mutex;
    long *current_id_index;
    long total_ids;
    long *ids;
} thread_data_t;

// Função para simular a chamada à API
char* simulate_api_call(long id) {
    char* json_response = (char*)malloc(200 * sizeof(char));
    if (json_response == NULL) {
        perror("Erro ao alocar memória para JSON");
        return NULL;
    }
    // Simula um valor aleatório para o campo 'valor'
    int random_value = rand() % 1000;
    snprintf(json_response, 200, "{\"id\":%ld,\"status\":\"ok\",\"valor\":{\"nome\":\"valor_%d\"}}", id, random_value);
    return json_response;
}

// Função executada por cada thread
void *process_ids(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    long id_to_process;
    char timestamp[30];
    time_t rawtime;
    struct tm *info;

    while (1) {
        pthread_mutex_lock(data->ids_mutex);
        if (*(data->current_id_index) >= data->total_ids) {
            pthread_mutex_unlock(data->ids_mutex);
            break; // Todos os IDs foram processados
        }
        id_to_process = data->ids[*(data->current_id_index)];
        (*(data->current_id_index))++;
        pthread_mutex_unlock(data->ids_mutex);

        // Simula o processamento do ID
        char* api_response = simulate_api_call(id_to_process);
        if (api_response == NULL) {
            fprintf(stderr, "Thread %d: Erro ao simular API para ID %ld\n", data->thread_id, id_to_process);
            continue;
        }

        // Obtém o timestamp atual
        time(&rawtime);
        info = localtime(&rawtime);
        strftime(timestamp, 30, "%Y-%m-%d %H:%M:%S", info);

        // Escreve no arquivo de log
        pthread_mutex_lock(data->log_mutex);
        fprintf(data->log_file, "%s, Thread %d, ID %ld, Resposta %s\n", timestamp, data->thread_id, id_to_process, api_response);
        pthread_mutex_unlock(data->log_mutex);

        free(api_response);
    }
    pthread_exit(NULL);
}

// Processo P1
void p1_process() {
    printf("P1: Iniciando o processo P1.\n");

    FILE *ids_file = fopen(IDS_FILE, "r");
    if (ids_file == NULL) {
        perror("P1: Erro ao abrir lista_ids.txt");
        exit(EXIT_FAILURE);
    }

    // Conta o número total de IDs
    long total_ids = 0;
    char line[MAX_ID_LENGTH];
    while (fgets(line, sizeof(line), ids_file) != NULL) {
        total_ids++;
    }
    rewind(ids_file); // Volta para o início do arquivo

    // Aloca memória para armazenar todos os IDs
    long *ids = (long*)malloc(total_ids * sizeof(long));
    if (ids == NULL) {
        perror("P1: Erro ao alocar memória para IDs");
        fclose(ids_file);
        exit(EXIT_FAILURE);
    }

    // Lê todos os IDs para a memória
    for (long i = 0; i < total_ids; i++) {
        if (fgets(line, sizeof(line), ids_file) != NULL) {
            ids[i] = atol(line);
        } else {
            fprintf(stderr, "P1: Erro ao ler ID do arquivo na posição %ld.\n", i);
            total_ids = i; // Ajusta o total de IDs se houver erro de leitura
            break;
        }
    }
    fclose(ids_file);

    FILE *log_file = fopen(LOG_FILE, "w");
    if (log_file == NULL) {
        perror("P1: Erro ao abrir logs.txt");
        free(ids);
        exit(EXIT_FAILURE);
    }

    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t ids_mutex = PTHREAD_MUTEX_INITIALIZER;
    long current_id_index = 0;

    srand(time(NULL)); // Inicializa o gerador de números aleatórios

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i + 1;
        thread_data[i].log_file = log_file;
        thread_data[i].log_mutex = &log_mutex;
        thread_data[i].ids_mutex = &ids_mutex;
        thread_data[i].current_id_index = &current_id_index;
        thread_data[i].total_ids = total_ids;
        thread_data[i].ids = ids;
        if (pthread_create(&threads[i], NULL, process_ids, (void *)&thread_data[i]) != 0) {
            perror("P1: Erro ao criar thread");
            // Lidar com erro de criação de thread, talvez cancelar as já criadas
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    fclose(log_file);
    free(ids);
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&ids_mutex);

    printf("P1: Processo P1 finalizado. %ld IDs processados.\n", total_ids);
    exit(EXIT_SUCCESS);
}

// Processo P0
int main() {
    printf("P0: Iniciando o processo P0.\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("P0: Erro ao criar processo filho");
        return EXIT_FAILURE;
    } else if (pid == 0) {
        // Processo filho (P1)
        p1_process();
    } else {
        // Processo pai (P0)
        int status;
        printf("P0: Aguardando a finalização do processo P1 (PID: %d).\n", pid);
        if (waitpid(pid, &status, 0) == -1) {
            perror("P0: Erro ao aguardar P1");
            return EXIT_FAILURE;
        }

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == EXIT_SUCCESS) {
                printf("P0: Processo P1 finalizado com sucesso.\n");
                // Conferir o arquivo de log (opcional, pode ser feito manualmente ou com script)
                printf("P0: Verifique o arquivo '%s' para os logs.\n", LOG_FILE);
            } else {
                fprintf(stderr, "P0: Processo P1 finalizado com erro. Código de saída: %d\n", WEXITSTATUS(status));
            }
        } else {
            fprintf(stderr, "P0: Processo P1 não finalizou normalmente.\n");
        }
    }

    printf("P0: Processo P0 finalizado.\n");
    return EXIT_SUCCESS;
}

