#include <stdio.h>
#include <process.h>
#include <errno.h>
#include <iostream.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <sys/siginfo.h>
#include <pthread.h>


void write_file_trend(char* _value, char* _time);// �������� �������� ������� � ����� � ����
// ���������� �������� �� �������� P1
void signal_handler(int signo, siginfo_t *info, void *over);
// ������� ���������� ���� ���������
void finish_all(union sigval arg);

volatile bool start_timer = false;// ���� ����� �� ������� ��������� ������

volatile bool start_read = false;// ����� �� ������� ������ ������

FILE* file;//����, � ������� ����� ������������ �������� �������

int main(int argc, char *argv[]) {
	std::cout << "[P2] START!" << std::endl;
	// ��� ����������� ������, ������� P2 ����� �������� ������������
	char* name = (char*)"/sharemap";
	// �������� ������ ��������, ���������� �� ������� SIGUSR1
	// ������ ������ ������� �� P1, ������� ����� ��������
	// � ������� ������� � ������ ������ �� ����������� ������
	sigset_t signalSet;
	sigemptyset(&signalSet);
	sigaddset(&signalSet, SIGUSR1);
	// �������� ��� ����� ������ ���������� ����������������� ������� -
	// ���������� ������� ��������� ������� signal_handler
	struct sigaction act;
	act.sa_flags = 0;
	act.sa_mask = signalSet;
	act.sa_sigaction = &signal_handler;
	sigaction(SIGUSR1, &act, NULL);
	// �������� ������ ��������, ���������� �� ������� SIGUSR2
	// ������ ������ ����� ��������� �� ������� �������� ��������,
	// ��� ����� �������� � ����������� ����������� ���������� �� ������
	sigset_t timer_read_set;
	sigemptyset(&timer_read_set);
	sigaddset(&timer_read_set, SIGUSR2);
	// �������� ������� ������������ ������
	size_t shared_mem_len = 8;
	//������������� ����������� ������ � ����������� �� �����������
	int fd = shm_open("/sharemap", O_RDONLY, 0);
	if (fd == -1) {
		std::cout << "[P2] Connecting sharemap has done with error!" << std::endl;
		return 1;
	}
	// ��������� ������� ����������� ������
	ftruncate(fd, shared_mem_len);
	//����������� ��.������ � �������� ������������ ��������
	char* addr = (char*)mmap(0, shared_mem_len, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		std::cout << "[P2] mmap has done with error!" << std::endl;
		return 1;
	}
	//������ �������� P1 �� ���� ������ (Module2)
	int pid_p1 = spawnl(P_NOWAIT, "Module2", "Module2", name, NULL);
	if (pid_p1 == -1) {
		std::cout << "[P2] Running subprocess P1 has done with error!" << std::endl;
		return 1;
	}
	// �������� ������� ��� ����� � �������� �������� ��� ������ �� ��.������
	// ��������� ��� ���� ����������� �������
	struct sigevent timer_read_event;
	// ������������� ����������� ���� "������"
	SIGEV_SIGNAL_INIT(&timer_read_event, SIGUSR2);
	// id �������
	timer_t timer_read_id;
	// �������� �������
	if (timer_create(CLOCK_REALTIME, &timer_read_event, &timer_read_id) == -1)
	{
		std::cout << "[P2] Creating period timer has done with error!" << std::endl;
		return 1;
	}
    // ��������� ��� �������� ������� �������
	struct itimerspec timer_read_prop;
	// �������� ������� ������� (�������������)
	timer_read_prop.it_value.tv_sec = 0;
	timer_read_prop.it_value.tv_nsec = 400000000;
	timer_read_prop.it_interval.tv_sec = 0;
	timer_read_prop.it_interval.tv_nsec = 400000000;
	// �������� ������� ��� ����� � �������� �������� ��� ��������� ��������� P1 � P2
	// ��������� ��� ���� ����������� �������
	struct sigevent timer_stop_event;
	// ������������� ����������� ���� "������� ����"
	SIGEV_THREAD_INIT(&timer_stop_event, finish_all, pid_p1, NULL);
	// id �������
	timer_t timer_stop_id;
	// �������� �������
	if (timer_create(CLOCK_REALTIME, &timer_stop_event, &timer_stop_id) == -1) {
		std::cout << "Creating stop timer has done with error!" << std::endl;
		return 1;
	}
	// ��������� ��� �������� ������� �������
	struct itimerspec timer_stop_prop;
	// �������� ������� ������� (�����������)
	timer_stop_prop.it_value.tv_sec = 107;
	timer_stop_prop.it_value.tv_nsec = 0;
	timer_stop_prop.it_interval.tv_sec = 0;
	timer_stop_prop.it_interval.tv_nsec = 0;
	// �������� ����� ������ ��� ������
	file = fopen("/home/trend.txt", "wt");
	// ������ ��� ������ �������� �������
	char buff_func_res[shared_mem_len];
	// ������ ��� ������ �������� ������� ����� �������
	char buff_time[24];
	// ������� ����� �������
	double time = 0;
	// �������� ������� �� P1 ��� ������� �������
	while (!start_timer)
		pause();
	// ������ ������� �� ���������� �������� �� ����������� ������
	timer_settime(timer_read_id, 0, &timer_read_prop, NULL);
	// ������ ������� �� ���������� ���� ���������
	timer_settime(timer_stop_id, 0, &timer_stop_prop, NULL);
	// ������ ����� ����������
	while (true) {
		// ������� ������ ��� ������ ������� ��������
		while (!start_read)
			pause();
		// �������� ��������� ������� �� ����������� ������ � �����
		strcpy(buff_func_res, addr);
		// �������������� ������� � ������
		sprintf(buff_time, "%f", time);
		// ������ � ���� ������
		write_file_trend(buff_func_res, buff_time);
		// �������� ������� �� �������
		// ����� �������
		int signo;
		sigwait(&timer_read_set, &signo);
		// ��������� �������
		std::cout << "[P2] ������ ���������� �������: " << buff_func_res << " ������� �����: " << buff_time << std::endl;
		time += 0.4;
	}

	return 0;
}

void write_file_trend(char* _value, char* _time) {
	char text[124] = "";

	strcat(text, _value);
	strcat(text, "\t");
	strcat(text, _time);
	strcat(text, "\n");

	fputs(text, file);
}

void finish_all(union sigval arg)
{
	std::cout << "finishing all processes..." << std::endl;

	fclose(file);

	kill(arg.sival_int, SIGKILL);
	kill(getpid(), SIGKILL);
}

void signal_handler(int signo, siginfo_t *info, void *over)
{
	if (info->si_code == -5)
	{
		std::cout << "timer start" << std::endl;
		start_timer = true;
		return;
	}
	if (info->si_code == -6)
	{
		std::cout << "timer start read" << std::endl;
		start_read = true;
		return;
	}
}

