#include "common.h"

JOB_QUEUE *job_list;
int fd;
int server_id;

void error_and_die(char* msg){
	printf("%s\n", msg);
	exit(0);
}

void sanity_check(int argc, char *argv[]){
	if (argc != 1){
		printf(ANSI_COLOR_RED "Incorrect number of arguments" ANSI_COLOR_RESET "\n");
		printf(ANSI_COLOR_RED "Usage: ./server <number of slots>" ANSI_COLOR_RESET "\n");	
		exit(1);
	}
}

void require_number_of_slots(int *size){
	printf("This is your first print server\n");
	printf("Please initialize the number slots you want in the buffer\n");
	char size_str[11];
	fgets(size_str, 11, stdin);
	int len = strlen(size_str);
	if (len > 11){
		*size = MAX_SLOTS;
		return;
	}
	size_str[len] = '\0';
	*size = atoi(size_str);
	if ((*size <= 0) || (*size > MAX_SLOTS))
		error_and_die("Input invalid. Number of slots should range from 1 to 999999999");
}

void setup_shared_mem(){
	printf("Initializing shared memory region" ANSI_COLOR_RESET "\n");
	fd =  shm_open("/myshm", O_CREAT | O_RDWR, 0666);
	if (fd < 0)
		error_and_die(ANSI_COLOR_RED"shm_open" ANSI_COLOR_RESET "\n");
	ftruncate(fd, sizeof(JOB_QUEUE) + (sizeof(JOB)*10));
}

void attach_share_mem(){
	job_list = (JOB_QUEUE *)mmap(0, sizeof(JOB_QUEUE) + (sizeof(JOB)*10), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (job_list == MAP_FAILED)
		error_and_die(ANSI_COLOR_RED"mmap failed in attach_share_mem" ANSI_COLOR_RESET "\n");
}

void init_semaphore(int size){
	sem_init(&job_list->mutex, 1, 1);
	sem_init(&job_list->full, 1, 0);
	sem_init(&job_list->empty, 1, size);
	job_list->s_id = 0;
	job_list->c_id = 0;
	job_list->start = 0;
	job_list->end = 0;
	job_list->size = size;
	job_list->current_size = 0;
}

void connect_shared_memory(int size){
	fd = shm_open("/myshm", O_RDWR, 0666);	
	if (fd < 0){
		require_number_of_slots(&size);
		setup_shared_mem();
		attach_share_mem();
		init_semaphore(size);
	}else{
		attach_share_mem(size);
	}
}

void assign_server_id(){
	job_list->s_id = job_list->s_id + 1;
	server_id = job_list->s_id;
	printf(ANSI_COLOR_YELLOW "buffer size is %d" ANSI_COLOR_RESET "\n", job_list->size);
	printf(ANSI_COLOR_YELLOW "server id: %d" ANSI_COLOR_RESET "\n", server_id);
}

void take_a_job(JOB *job){
	// printf("waiting for full\n");
	sem_wait(&job_list->full);
    // printf("waiting for mutex\n");
	sem_wait(&job_list->mutex);
	*job = job_list->jobs[job_list->end];
	job_list->end = (job_list->end+1) % (job_list->size);
	job_list->current_size = job_list->current_size-1;
}

void print_a_msg(JOB *job){
	printf("Printer %d starts printing %d pages from client %d\n", server_id, (*job).duration, (*job).source);
	sem_post(&job_list->mutex);
	sem_post(&job_list->empty);
}
		
void go_sleep(JOB *job){
	sleep((*job).duration);
	printf("Printer %d finishes printing %d pages from client %d\n", server_id, (*job).duration, (*job).source);
	if (job_list->current_size == 0)
		printf(ANSI_COLOR_YELLOW "No request in buffer, Printer sleeps" ANSI_COLOR_RESET "\n");
}

int main(int argc, char *argv[]){
	int size;
	JOB job;

	sanity_check(argc, argv);
	connect_shared_memory(size);
	assign_server_id();

	// main loop
	while(1){
		take_a_job(&job);
		print_a_msg(&job);
		go_sleep(&job);
	}
	return 0;
}