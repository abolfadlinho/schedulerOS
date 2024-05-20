//OS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MEMORY_SIZE 60
#define MAX_STRING_LENGTH 30
#define MAX_INSTRUCTIONS_PER_PROCESS 9
#define VARIABLES_PER_PROCESS 3
#define VARIABLE_DATA "x" 

typedef struct {
    int pid;
    char state[20]; 
    int priority;
    int pc;
    int mem_lower_bound; 
    int mem_upper_bound;
    int maxpc; 
} PCB;

typedef struct {
    char variableName[MAX_STRING_LENGTH];
    char data[MAX_STRING_LENGTH];
} MemoryWord;

typedef struct queue_node {
    PCB* pcb;
    struct queue_node* next;
} QueueNode;

void print_memory();
void create_process(char* program_file);
int allocate_memory_for_process(PCB* process, const char* program_file);
void schedule_processes(int arrival1, int arrival2, int arrival3);
void execute_instruction(PCB* process,bool isLastQ);
void semWait(char* resource,PCB* process,bool isLastQ);
void semSignal(char* resource,PCB* process);
void print_queues();
void print_memory();
void enqueue(PCB* process, int priority);
PCB* dequeue(int priority);
void enqueueB(int iof,PCB* process);
PCB* dequeueB(int iof);

MemoryWord memory[MEMORY_SIZE];
int number_of_programs = 0;
bool file_mutex = false;
bool input_mutex = false;
bool output_mutex = false;
int quantum = 1;

QueueNode* ready_queues[4] = {NULL}; //array of pointers to the head of each queue
QueueNode* general_blocked_queue[3] = {NULL}; //0-> input blocked, 1-> output blocked, 2-> file blocked

//Start
typedef struct QueueNodeG {
    int data;
    struct QueueNodeG* next;
} QueueNodeG;

// Define the Queue structure
typedef struct QueueG {
    QueueNodeG* front;
    QueueNodeG* rear;
} QueueG;

// Function to initialize the queue
void initializeQueue(QueueG* queue) {
    queue->front = NULL;
    queue->rear = NULL;
}

// Function to enqueue an integer into the queue
void enqueueG(QueueG* queue, int value) {
    QueueNodeG* newNode = (QueueNodeG*)malloc(sizeof(QueueNodeG));
    if (!newNode) {
        printf("Memory allocation error\n");
        return;
    }
    newNode->data = value;
    newNode->next = NULL;

    if (queue->rear == NULL) {
        queue->front = newNode;
        queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }
}

// Function to remove a specific integer from the queue
bool removeSpecific(QueueG* queue, int value) {
    if (queue->front == NULL) {
        return false; // Queue is empty
    }

    QueueNodeG* current = queue->front;
    QueueNodeG* previous = NULL;

    while (current != NULL) {
        if (current->data == value) {
            if (previous == NULL) { // Removing the front node
                queue->front = current->next;
                if (queue->front == NULL) {
                    queue->rear = NULL; // Queue is now empty
                }
            } else {
                previous->next = current->next;
                if (current->next == NULL) {
                    queue->rear = previous; // Removing the rear node
                }
            }
            free(current);
            return true; // Successfully removed the node
        }
        previous = current;
        current = current->next;
    }
    return false; // Value not found in the queue
}

// Function to print the queue (for debugging purposes)
void printQueueG(QueueG* queue) {
    QueueNodeG* current = queue->front;
    printf("The General Blocked Queue: ");
    while (current != NULL) {
        printf("Process %d, ", current->data);
        current = current->next;
    }
    printf(" End of The General Blocked Queue.\n");
}
//End
QueueG gq;


//QUEUE FUNCTIONS
bool are_queues_empty() {
    // Check if ready_queues are empty
    for (int i = 0; i < 4; i++) {
        if (ready_queues[i] != NULL) {
            return false; // If any queue is not empty, return false
        }
    }

    // Check if general_blocked_queue is empty for all types of blocking
    for (int i = 0; i < 3; i++) {
        if (general_blocked_queue[i] != NULL) {
            return false; // If any queue is not empty, return false
        }
    }

    // If all queues are empty, return true
    return true;
}

void enqueueB(int iof,PCB* process) { //iof is wether input,output or file
    QueueNode* new_node = (QueueNode*)malloc(sizeof(QueueNode));
    new_node->pcb = process;
    new_node->next = NULL;

    if (general_blocked_queue[iof] == NULL) { 
        general_blocked_queue[iof] = new_node;
    } else {
        QueueNode* current = general_blocked_queue[iof]; 
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
    enqueueG(&gq,process->pid);
}

PCB* dequeueB(int iof) { //dequeues the process with highest priority in that resource's blocked queue
    if (general_blocked_queue[iof] == NULL) {
        return NULL;
    } else {
        QueueNode* current_node = general_blocked_queue[iof];
        QueueNode* highest_priority_node = current_node;
        PCB* highest_priority_process = highest_priority_node->pcb;
        while (current_node != NULL) {
            if (current_node->pcb->priority > highest_priority_process->priority) {
                highest_priority_node = current_node;
                highest_priority_process = current_node->pcb;
            }
            current_node = current_node->next;
        }

        if (highest_priority_node == general_blocked_queue[iof]) {
            general_blocked_queue[iof] = highest_priority_node->next;
        } else {
            QueueNode* prev_node = general_blocked_queue[iof];
            while (prev_node->next != highest_priority_node) {
                prev_node = prev_node->next;
            }
            prev_node->next = highest_priority_node->next;
        }

        highest_priority_node->next = NULL;

        strcpy(highest_priority_process->state, "Ready");
        strcpy(memory[59-(highest_priority_process->pid)*7 - 1].data,"Ready");
        enqueue(highest_priority_process, highest_priority_process->priority);
        removeSpecific(&gq,highest_priority_process->pid);
        return highest_priority_process;
    }
}

void enqueue(PCB* process, int priority) { //remember input priority here starts from 1 not 0
    QueueNode* new_node = (QueueNode*)malloc(sizeof(QueueNode));
    new_node->pcb = process;
    new_node->next = NULL;

    if (ready_queues[priority - 1] == NULL) { //if priority level is empty then set to begining
        ready_queues[priority - 1] = new_node;
    } else {
        QueueNode* current = ready_queues[priority - 1]; //if priority level is already full then make last node.next = this pcb
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
    char str[30];
    sprintf(str,"%d",process->priority);
    strcpy(memory[59-(process->pid)*7 - 2].data,str);
}

PCB* dequeue(int priority) { //remember input priority here starts from 1 not 0
    if(priority > 4 || priority < 1) {
        printf("ERROR IN DEQUEUE: priority entered is invalid");
        return NULL;
    }
    if (ready_queues[priority - 1] == NULL) {
        return NULL; //queue is empty
    } else {
        QueueNode* dequeued_node = ready_queues[priority - 1];
        PCB* dequeued_process = dequeued_node->pcb;
        ready_queues[priority - 1] = dequeued_node->next; //head of queue now points at .next
        free(dequeued_node); //important for laptop
        return dequeued_process;
    }
}

void create_process(char* program_file) {
    PCB* process = (PCB*) malloc(sizeof(PCB)); //must be malloc
    process->pid = number_of_programs++;
    strcpy(process->state, "Ready");
    process->priority = 1; 
    process->pc = 0;
    process->maxpc = allocate_memory_for_process(process,program_file);
    enqueue(process,1);
    char str[30]; //assign pcb to memory
    sprintf(str,"%d",process->pid);
    strcpy(memory[59-(process->pid)*7].variableName,"pid");
    strcpy(memory[59-(process->pid)*7].data,str);
    strcpy(memory[59-(process->pid)*7 - 1].variableName,"state");
    strcpy(memory[59-(process->pid)*7 - 1].data,process->state);
    strcpy(memory[59-(process->pid)*7 - 2].variableName,"priority");
    sprintf(str,"%d",process->priority);
    strcpy(memory[59-(process->pid)*7 - 2].data,str);
    strcpy(memory[59-(process->pid)*7 - 3].variableName,"pc");
    sprintf(str,"%d",process->pc);
    strcpy(memory[59-(process->pid)*7 - 3].data,str);
    strcpy(memory[59-(process->pid)*7 - 4].variableName,"maxpc");
    sprintf(str,"%d",process->maxpc);
    strcpy(memory[59-(process->pid)*7 - 4].data,str);
    strcpy(memory[59-(process->pid)*7 - 5].variableName,"memory_lower_bound");
    sprintf(str,"%d",process->mem_lower_bound);
    strcpy(memory[59-(process->pid)*7 - 5].data,str);
    strcpy(memory[59-(process->pid)*7 - 6].variableName,"memory_upper_bound");
    sprintf(str,"%d",process->mem_upper_bound);
    strcpy(memory[59-(process->pid)*7 - 6].data,str);
}

//multilevel feedback scheduling algorithm
void schedule_processes(int arrival1, int arrival2, int arrival3) {
    PCB* current_process;
    int counter = 0;
    if(counter == arrival1) {
        printf("Arrival of Program %d\n",1);
        create_process("Program_1.txt");
    }
    if(counter == arrival2) {
        printf("Arrival of Program %d\n",2);
        create_process("Program_2.txt");
    }
    if(counter == arrival3) {
        printf("Arrival of Program %d\n",3);
        create_process("Program_3.txt");
    }
    print_queues();
    while (!are_queues_empty()) { //while exists any remaining programs
        if (ready_queues[0] != NULL) { //if priority level is not empty
            current_process = dequeue(1); //1 because input priority starts from 1 in dequeue
            printf("\nClock cycle: %d\n\n",counter);
            printf("Executing Process %d with Quantum = %d/1 ...\n", current_process->pid,1);
            execute_instruction(current_process,true);
            printf("Process %d state: %s\n", current_process->pid, current_process->state);
            if((strcmp(current_process->state, "Terminated") != 0) && (strcmp(current_process->state, "Blocked") != 0)) { //if program is not finished/blocked then lessen the priority level
                current_process->priority = 2;
                enqueue(current_process,2); //remember 2 because priority starts from 1 not 0
            }
            counter++;
            printf("\nMemory after executing:\n");
            print_memory();
            if(counter == arrival1) {
                printf("Arrival of Program %d\n",1);
                create_process("Program_1.txt");
            }
            if(counter == arrival2) {
                printf("Arrival of Program %d\n",2);
                create_process("Program_2.txt");
            }
            if(counter == arrival3) {
                printf("Arrival of Program %d\n",3);
                create_process("Program_3.txt");
            }
        } else if(ready_queues[1] != NULL) {
            current_process = dequeue(2); //same repeated but change the priority only for next 3 if conditions
            for(int i = 0; i < 2;i++) {
                printf("\n_______________________________________________________________\n");
                printf("\nClock cycle: %d\n\n",counter);
                printf("Executing Process %d with Quantum = %d/2 ...\n", current_process->pid,i+1);
                execute_instruction(current_process,(i==1));
                printf("Process %d state: %s\n", current_process->pid, current_process->state);
                counter++;
                printf("\nMemory after executing:\n");
                print_memory();
                if(counter == arrival1) {
                    printf("Arrival of Program %d\n",1);
                    create_process("Program_1.txt");
                }
                if(counter == arrival2) {
                    printf("Arrival of Program %d\n",2);
                    create_process("Program_2.txt");
                }
                if(counter == arrival3) {
                    printf("Arrival of Program %d\n",3);
                    create_process("Program_3.txt");
                }
                if (strcmp(current_process->state, "Terminated") == 0 || strcmp(current_process->state, "Blocked") == 0) {
                    break; //exit the loop if process is terminated or blocked
                }
            }
            if((strcmp(current_process->state, "Terminated") != 0) && (strcmp(current_process->state, "Blocked") != 0)) { 
                current_process->priority = 3;
                enqueue(current_process,3); 
            }
        } else if(ready_queues[2] != NULL) {
            current_process = dequeue(3); 
            for(int i = 0; i<4;i++) {
                printf("\n_______________________________________________________________\n");
                printf("\nClock cycle: %d\n\n",counter);
                printf("Executing Process %d with Quantum = %d/4 ...\n", current_process->pid,i+1);
                execute_instruction(current_process,(i==3));
                printf("Process %d state: %s\n", current_process->pid, current_process->state);
                counter++;
                printf("\nMemory after executing:\n");
                print_memory();
                if(counter == arrival1) {
                    printf("Arrival of Program %d\n",1);
                    create_process("Program_1.txt");
                }
                if(counter == arrival2) {
                    printf("Arrival of Program %d\n",2);
                    create_process("Program_2.txt");
                }
                if(counter == arrival3) {
                    printf("Arrival of Program %d\n",3);
                    create_process("Program_3.txt");
                }
                if (strcmp(current_process->state, "Terminated") == 0 || strcmp(current_process->state, "Blocked") == 0) {
                    break; //exit the loop if process is terminated or blocked
                }
            }
            if((strcmp(current_process->state, "Terminated") != 0) && (strcmp(current_process->state, "Blocked") != 0)) {
                current_process->priority = 4;
                enqueue(current_process,4); 
            }
        } else if(ready_queues[3] != NULL) {
            current_process = dequeue(4);
            for(int i = 0; i< 8;i++) {
                printf("\n_______________________________________________________________\n");
                printf("\nClock cycle: %d\n\n",counter);
                printf("Executing Process %d with Quantum = %d/8 ...\n", current_process->pid,i+1);
                execute_instruction(current_process,(i==7));
                printf("Process %d state: %s\n", current_process->pid, current_process->state);
                counter++;
                printf("\nMemory after executing:\n");
                print_memory();
                if(counter == arrival1) {
                    printf("Arrival of Program %d\n",1);
                    create_process("Program_1.txt");
                }
                if(counter == arrival2) {
                    printf("Arrival of Program %d\n",2);
                    create_process("Program_2.txt");
                }
                if(counter == arrival3) {
                    printf("Arrival of Program %d\n",3);
                    create_process("Program_3.txt");
                }
                if (strcmp(current_process->state, "Terminated") == 0 || strcmp(current_process->state, "Blocked") == 0) {
                    break; //exit the loop if process is terminated or blocked
                }
            }
            if((strcmp(current_process->state, "Terminated") != 0) && (strcmp(current_process->state, "Blocked") != 0)) { 
                enqueue(current_process,4); //remember priority cannot be any less than 4
            }
        }
        if((!input_mutex) && (general_blocked_queue[0] != NULL)) { //if a mutex was freed and there was a process blocked because of it then unblock
            dequeueB(0);
        }
        if((!output_mutex) && (general_blocked_queue[1] != NULL)) {
            dequeueB(1);
        }
        if((!file_mutex) && (general_blocked_queue[2] != NULL)) {
            dequeueB(2);
        }
        printf("\nExecution done, queues after executing:\n");
        print_queues();
    }
    free(current_process); //important for laptop
}

char* read_file(const char* filename) {
    FILE *file;
    char *buffer;
    long file_size;

    file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open file %s for reading\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = (char*)malloc((file_size + 1) * sizeof(char));
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    fread(buffer, sizeof(char), file_size, file);
    buffer[file_size] = '\0';

    fclose(file);

    return buffer;
}


void execute_instruction(PCB* process,bool isLastQ) { //assumptions: 1.variable names are of one character only
    char* original = memory[process->mem_lower_bound + (process->pc)].data;
    strcat(original,"\0"); //important since .data can return characters without end of string character
    printf("Instruction being executed: %s\n",original);

    char instruction[100];

    strcpy(instruction,original); //must copy to other pointer as to protect memory
    
    char* first_token = "";

    char* token = strtok(instruction," "); 

    first_token = strdup(token);

    if (strcmp(first_token, "semWait") == 0) {
        if(instruction[8] == 'f') {
            if(!file_mutex) {
                process->pc++; 
                char str[30];
                sprintf(str,"%d",process->pc);
                strcpy(memory[59-(process->pid)*7 - 3].data,str);
            }
            semWait("file",process,isLastQ); 
        } else if(instruction[12] == 'O') {
            if(!output_mutex) {
                process->pc++;
                char str[30];
                sprintf(str,"%d",process->pc);
                strcpy(memory[59-(process->pid)*7 - 3].data,str);
                }
            semWait("useroutput",process,isLastQ);
        } else if(instruction[12] == 'I'){
            if(!input_mutex) {
                process->pc++; 
                char str[30];
                sprintf(str,"%d",process->pc);
                strcpy(memory[59-(process->pid)*7 - 3].data,str);
            }
            semWait("userInput",process,isLastQ);
        }
    } else if (strcmp(first_token, "semSignal") == 0) {
        if(instruction[10] == 'f') {
            semSignal("file",process);
        } else if(instruction[14] == 'O') {
            semSignal("userOutput",process);
        } else if(instruction[14] == 'I'){
            semSignal("userInput",process);
        }
        process->pc++; 
        char str[30];
        sprintf(str,"%d",process->pc);
        strcpy(memory[59-(process->pid)*7 - 3].data,str);

    } else if (strcmp(first_token, "assign") == 0) {
        if(instruction[9] == 'i') { //input case
            printf("Please enter a value: ");
            char str[2];
            str[0] = instruction[7];
            str[1] = '\0';
            if(strcmp(memory[process->mem_upper_bound - 2].data,"x") == 0) {
                scanf("%s", memory[process->mem_upper_bound - 2].data);
                strcpy(memory[process->mem_upper_bound - 2].variableName, str);
            } else if(strcmp(memory[process->mem_upper_bound - 1].data,"x") == 0) {
                scanf("%s", memory[process->mem_upper_bound - 1].data);
                strcpy(memory[process->mem_upper_bound - 1].variableName, str);
            } else if(strcmp(memory[process->mem_upper_bound ].data,"x") == 0) {
                scanf("%s", memory[process->mem_upper_bound ].data);
                strcpy(memory[process->mem_upper_bound ].variableName, str);
            } else {
                printf("\n\nVARIABLE MEMORY IS FULL\n\n");
            }
        } else {
            //handle readfile case
            char str[2];
            str[1] = '\0';
            str[0] = instruction[18];
            char* file_name;
            if(strcmp(memory[process->mem_upper_bound - 2].variableName,str) == 0) {
                file_name = memory[process->mem_upper_bound - 2].data;
            } else if(strcmp(memory[process->mem_upper_bound - 1].variableName,str) == 0) {
                file_name = memory[process->mem_upper_bound - 1].data;
            } else if(strcmp(memory[process->mem_upper_bound ].variableName,str) == 0) {
                file_name = memory[process->mem_upper_bound ].data;
            } else {
                file_name = "blank.txt";
                printf("\n\nINVALID VARIABLE NAME\n\n");
            }

            char* data = read_file(file_name);
            str[0] = instruction[7];
            if(strcmp(memory[process->mem_upper_bound - 2].data,"x") == 0) {
                strcpy(memory[process->mem_upper_bound - 2].variableName, str);
                strcpy(memory[process->mem_upper_bound - 2].data, data);
            } else if(strcmp(memory[process->mem_upper_bound - 1].data,"x") == 0) {
                strcpy(memory[process->mem_upper_bound - 1].variableName, str);
                strcpy(memory[process->mem_upper_bound - 1].data, data);
            } else if(strcmp(memory[process->mem_upper_bound ].data,"x") == 0) {
                strcpy(memory[process->mem_upper_bound ].variableName, str);
                strcpy(memory[process->mem_upper_bound ].data, data);
            } else {
                printf("\n\nVARIABLE MEMORY IS FULL\n\n");
            }
        }
        process->pc++;
        char str[30];
        sprintf(str,"%d",process->pc);
        strcpy(memory[59-(process->pid)*7 - 3].data,str);
    } else if (strcmp(first_token, "printFromTo") == 0) {
        int from = 0;
        int to = -1;
        char str[2];
        str[0] = instruction[12];
        str[1] = '\0';
        if (strcmp(memory[process->mem_upper_bound - 2].variableName,str) == 0) {
            from = atoi(memory[process->mem_upper_bound - 2].data);
        } else if (strcmp(memory[process->mem_upper_bound - 1].variableName,str) == 0) {
            from = atoi(memory[process->mem_upper_bound - 1].data);
        } else if (strcmp(memory[process->mem_upper_bound ].variableName,str) == 0) {
            from = atoi(memory[process->mem_upper_bound ].data);
        } else {
            from = 0;
            printf("\n\nINVALID FIRST VARIABLE NAME\n\n");
        }
        str[0] = instruction[14];
        if (strcmp(memory[process->mem_upper_bound - 2].variableName,str) == 0) {
            to = atoi(memory[process->mem_upper_bound - 2].data);
        } else if (strcmp(memory[process->mem_upper_bound - 1].variableName,str) == 0) {
            to = atoi(memory[process->mem_upper_bound - 1].data);
        } else if (strcmp(memory[process->mem_upper_bound ].variableName,str) == 0) {
            to = atoi(memory[process->mem_upper_bound ].data);
        } else {
            to = 10;
            printf("\n\nINVALID SECOND VARIABLE NAME\n\n");
        }
        while(from <= to) {
            printf("%d,",from++);
        }
        printf("\n");
        process->pc++;
        char str1[30];
        sprintf(str1,"%d",process->pc);
        strcpy(memory[59-(process->pid)*7 - 3].data,str1);
    } else if (strcmp(first_token, "writeFile") == 0) {
        char* file_name;
        char* data;
        char str[2];
        str[0] = instruction[10]; //first variable
        str[1] = '\0';
        if (strcmp(memory[process->mem_upper_bound - 2].variableName,str) == 0) {
            file_name = memory[process->mem_upper_bound - 2].data;
        } else if (strcmp(memory[process->mem_upper_bound - 1].variableName,str) == 0) {
            file_name = memory[process->mem_upper_bound - 1].data;
        } else if (strcmp(memory[process->mem_upper_bound ].variableName,str) == 0) {
            file_name = memory[process->mem_upper_bound ].data;
        } else {
            file_name = "blank.txt";
            printf("\n\nINVALID FIRST VARIABLE NAME\n\n");
        }
        str[0] = instruction[12];  //second variable
        printf("%d",process->mem_upper_bound);

        if (strcmp(memory[process->mem_upper_bound - 2].variableName,str) == 0) {
            data = memory[process->mem_upper_bound - 2].data;
        } else if (strcmp(memory[process->mem_upper_bound - 1].variableName,str) == 0) {
            data = memory[process->mem_upper_bound - 1].data;
        } else if (strcmp(memory[process->mem_upper_bound ].variableName,str) == 0) {
            data = memory[process->mem_upper_bound ].data;
        } else {
            data = "blank";
            printf("\n\nINVALID FIRST VARIABLE NAME\n\n");
        }
        FILE *file = fopen(file_name, "w");
        fprintf(file, "%s",data);
        fclose(file);
        process->pc++;
        char str1[30];
        sprintf(str1,"%d",process->pc);
        strcpy(memory[59-(process->pid)*7 - 3].data,str1);
    } else if(strcmp(first_token, "print") == 0) {
        char str[2];
        str[0] = instruction[6];
        str[1] = '\0';
        if (strcmp(memory[process->mem_upper_bound - 2].variableName,str) == 0) {
            printf("%s\n",memory[process->mem_upper_bound - 2].data);
        } else if (strcmp(memory[process->mem_upper_bound - 1].variableName,str) == 0) {
            printf("%s\n",memory[process->mem_upper_bound - 1].data);
        } else if (strcmp(memory[process->mem_upper_bound ].variableName,str) == 0) {
            printf("%s\n",memory[process->mem_upper_bound ].data);
        } else {
            printf("\n\nINVALID VARIABLE NAME\n\n");
        }
        process->pc++;
        char str1[30];
        sprintf(str1,"%d",process->pc);
        strcpy(memory[59-(process->pid)*7 - 3].data,str1);
    }
    if(process->pc == process->maxpc) {
        strcpy(process->state,"Terminated");
        strcpy(memory[59-(process->pid)*7 - 1].data,"Terminated");
    }
}

void semWait(char* resource, PCB* process, bool isLastQ) { //remember iof order input = 0, output = 1, file = 2
    if (strcmp(resource, "userInput") == 0) {
        if(!input_mutex) {
            input_mutex = true;
        } else {
            printf("Process %d was blocked \n", process->pid);
            strcpy(process->state,"Blocked");
            strcpy(memory[59-(process->pid)*7 - 1].data,"Blocked");
            if(isLastQ && (strcmp(process->state, "Blocked") == 0)) {
                char str[30];
                if(process->priority != 4) {
                    process->priority++;
                }
                int newPriority = (process->priority);
                sprintf(str,"%d",newPriority);
                strcpy(memory[59-(process->pid)*7 - 2].data,str);
            }
            enqueueB(0,process); //if it is busy then block
        }
    } else if (strcmp(resource, "userOutput") == 0) {
        if(!output_mutex) {
            output_mutex = true;
        } else {
            printf("Process %d was blocked \n", process->pid);
            strcpy(process->state,"Blocked");
            strcpy(memory[59-(process->pid)*7 - 1].data,"Blocked");
            if(isLastQ && (strcmp(process->state, "Blocked") == 0)) {
                char str[30];
                if(process->priority != 4) {
                    process->priority++;
                }                
                int newPriority = (process->priority);
                sprintf(str,"%d",newPriority);
                strcpy(memory[59-(process->pid)*7 - 2].data,str);
            }
            enqueueB(1,process); //if it is busy then block
        }
    } else if(strcmp(resource,"file") == 0) {
        if(!file_mutex) {
            file_mutex = true;
        } else {
            printf("Process %d was blocked \n", process->pid);
            strcpy(process->state,"Blocked");
            strcpy(memory[59-(process->pid)*7 - 1].data,"Blocked");
            if(isLastQ && (strcmp(process->state, "Blocked") == 0)) {
                char str[30];
                if(process->priority != 4) { //cannot increment more than 4
                    process->priority++;
                }
                int newPriority = (process->priority); //if a process is blocked when it is in it's last quantum then it should increment priority
                sprintf(str,"%d",newPriority);
                strcpy(memory[59-(process->pid)*7 - 2].data,str);
            }
            enqueueB(2,process); //if it is busy then block
        }
    }
}

void semSignal(char* resource, PCB* process) { //code given in tutorial
    if (strcmp(resource, "userInput") == 0) {
        input_mutex = false;
    } else if (strcmp(resource, "userOutput") == 0) {
        output_mutex = false;
    } else if(strcmp(resource,"file") == 0) {
        file_mutex = false;
    }
}

void print_ready_queue() {
    printf("Ready Queues:\n");
    for (int i = 0; i < 4; i++) {
        printf("    Priority Level %d: ", i + 1);
        QueueNode* temp_queue = ready_queues[i];//temp queue for printing only
        if (temp_queue == NULL) {
            printf("Empty\n");
        } else {
            while (temp_queue != NULL) {
                printf("Process %d, ", temp_queue->pcb->pid);
                temp_queue = temp_queue->next;
            }
            printf("\n");
        }
    }
}

void print_blocked_queue(QueueNode* blocked_queue) {
    if (blocked_queue == NULL) {
        printf("Empty\n");
    } else {
        QueueNode* temp_queue = blocked_queue;
        while (temp_queue != NULL) {
            printf("Process %d, ", temp_queue->pcb->pid);
            temp_queue = temp_queue->next;
        }
        printf("\n");
    }
}

void print_blocked_queues() {
    printf("General Blocked Queues:\n");

    printf("    Blocked for Input: ");
    print_blocked_queue(general_blocked_queue[0]);
    
    printf("    Blocked for Output: ");
    print_blocked_queue(general_blocked_queue[1]);

    printf("    Blocked for File: ");
    print_blocked_queue(general_blocked_queue[2]);
}

void print_queues() {
    print_ready_queue();
    print_blocked_queues();
    printQueueG(&gq);
    //the general blocked queue contains all what is in each resource's blocked queue but in order of what was blocked first
}

int allocate_memory_for_process(PCB* process, const char* program_file) {
    int start_index = -1;
    int maxpc = 0;
    for (int i = 0; i < MEMORY_SIZE; i++) { //as if it is a malloc but sets words.data to ""
        if (strcmp(memory[i].data, "") == 0) {
            start_index = i;
            break;
        }
    }
    if (start_index == -1 || start_index + MAX_INSTRUCTIONS_PER_PROCESS + VARIABLES_PER_PROCESS > MEMORY_SIZE) { //CHANGED?
        printf("Error: Insufficient memory or unable to allocate for process %d\n", process->pid);
        return maxpc;
    }

    FILE* file = fopen(program_file, "r");
    if (file == NULL) {
        printf("Error: Unable to open program file %s\n", program_file);
        return maxpc;
    }

    char instruction[MAX_STRING_LENGTH];

    int countWhile = 0;
    while (fgets(instruction, sizeof(instruction), file) != NULL) { //populate instructions into memory from txt file
        char* newline = strchr(instruction, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }
        sprintf(memory[start_index + countWhile++].data, "%s", instruction);
        maxpc++;
    }

    for (int i = 0; i < VARIABLES_PER_PROCESS; i++) { //Set data of variables to an arbitrary value at beginning so we can detect empty variables
        sprintf(memory[start_index + maxpc + i].data, "%s", VARIABLE_DATA);
    }

    fclose(file);

    process->mem_lower_bound = start_index;
    process->mem_upper_bound = start_index + maxpc + VARIABLES_PER_PROCESS - 1; //maxpc because maxpc = count(instructions)
    return maxpc;
}

void print_memory() {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        printf("Memory word %d: %s -> %s\n", i, memory[i].variableName,memory[i].data);
    }
}

int main() {

    int arrival1;
    int arrival2;
    int arrival3;

    printf("Enter arrival time of Program_1.txt: ");
    scanf("%d",&arrival1);
    printf("Enter arrival time of Program_2.txt: ");
    scanf("%d",&arrival2);
    printf("Enter arrival time of Program_3.txt: ");
    scanf("%d",&arrival3);

    initializeQueue(&gq); //Initializing the general blocked queue

    print_queues(); //shows that general blocked queue is empty at start

    schedule_processes(arrival1,arrival2,arrival3); //Arrival program 1, arrival program 2, arrival program 3

    return 0;
}