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


void write_file_trend(char* _value, char* _time);// Записать значение функции и время в файл
// обработчик сигналов от процесса P1
void signal_handler(int signo, siginfo_t *info, void *over);
// функция завершения всех процессов
void finish_all(union sigval arg);

volatile bool start_timer = false;// Флаг Готов ли процесс запустить таймер

volatile bool start_read = false;// Готов ли процесс начать чтение

FILE* file;//файл, в который будут записываться значения функций

int main(int argc, char *argv[]) {
	std::cout << "[P2] START!" << std::endl;
	// имя именованной памяти, которую P2 будет пытаться присоединять
	char* name = (char*)"/sharemap";
	// создание набора сигналов, состоящего из сигнала SIGUSR1
	// данный сигнал приходт от P1, который будет сообщать
	// о запуске таймера и начале чтения из именованной памяти
	sigset_t signalSet;
	sigemptyset(&signalSet);
	sigaddset(&signalSet, SIGUSR1);
	// указание для этого набора диспозиции пользовательскому сигналу -
	// назначение функции обработки сигнала signal_handler
	struct sigaction act;
	act.sa_flags = 0;
	act.sa_mask = signalSet;
	act.sa_sigaction = &signal_handler;
	sigaction(SIGUSR1, &act, NULL);
	// создание набора сигналов, состоящего из сигнала SIGUSR2
	// данный сигнал будет приходить от таймера текущего процесса,
	// что будет сообщать о возможности продолжения считывания из памяти
	sigset_t timer_read_set;
	sigemptyset(&timer_read_set);
	sigaddset(&timer_read_set, SIGUSR2);
	// указание размера именнованной памяти
	size_t shared_mem_len = 8;
	//присоединение именованной памяти и возвращение ее дескриптора
	int fd = shm_open("/sharemap", O_RDONLY, 0);
	if (fd == -1) {
		std::cout << "[P2] Connecting sharemap has done with error!" << std::endl;
		return 1;
	}
	// установка размера именованной памяти
	ftruncate(fd, shared_mem_len);
	//отображение им.памяти в адресное пространство процесса
	char* addr = (char*)mmap(0, shared_mem_len, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		std::cout << "[P2] mmap has done with error!" << std::endl;
		return 1;
	}
	//запуск процесса P1 на базе модуля (Module2)
	int pid_p1 = spawnl(P_NOWAIT, "Module2", "Module2", name, NULL);
	if (pid_p1 == -1) {
		std::cout << "[P2] Running subprocess P1 has done with error!" << std::endl;
		return 1;
	}
	// Создание таймера для связи с реальным временем для чтения из им.памяти
	// структура для типа уведомления таймера
	struct sigevent timer_read_event;
	// инициализация уведомления типа "Сигнал"
	SIGEV_SIGNAL_INIT(&timer_read_event, SIGUSR2);
	// id таймера
	timer_t timer_read_id;
	// создание таймера
	if (timer_create(CLOCK_REALTIME, &timer_read_event, &timer_read_id) == -1)
	{
		std::cout << "[P2] Creating period timer has done with error!" << std::endl;
		return 1;
	}
    // структура для указания свойств таймера
	struct itimerspec timer_read_prop;
	// указание свойств таймера (переодический)
	timer_read_prop.it_value.tv_sec = 0;
	timer_read_prop.it_value.tv_nsec = 400000000;
	timer_read_prop.it_interval.tv_sec = 0;
	timer_read_prop.it_interval.tv_nsec = 400000000;
	// создание таймера для связи с реальным временем для остановки процессов P1 и P2
	// структура для типа уведомления таймера
	struct sigevent timer_stop_event;
	// инициализация уведомления типа "Создать нить"
	SIGEV_THREAD_INIT(&timer_stop_event, finish_all, pid_p1, NULL);
	// id таймера
	timer_t timer_stop_id;
	// создание таймера
	if (timer_create(CLOCK_REALTIME, &timer_stop_event, &timer_stop_id) == -1) {
		std::cout << "Creating stop timer has done with error!" << std::endl;
		return 1;
	}
	// структура для указания свойств таймера
	struct itimerspec timer_stop_prop;
	// указание свойств таймера (однократный)
	timer_stop_prop.it_value.tv_sec = 107;
	timer_stop_prop.it_value.tv_nsec = 0;
	timer_stop_prop.it_interval.tv_sec = 0;
	timer_stop_prop.it_interval.tv_nsec = 0;
	// открытие файла тренда для записи
	file = fopen("/home/trend.txt", "wt");
	// буффер для записи значения функции
	char buff_func_res[shared_mem_len];
	// буффер для записи значения текущей метки времени
	char buff_time[24];
	// текущая метка времени
	double time = 0;
	// ожидание сигнала от P1 для запуска таймера
	while (!start_timer)
		pause();
	// запуск таймера на считывание значений из именованной память
	timer_settime(timer_read_id, 0, &timer_read_prop, NULL);
	// запуск таймера на завершение всех процессов
	timer_settime(timer_stop_id, 0, &timer_stop_prop, NULL);
	// запуск цикла считывания
	while (true) {
		// Ожидаем сигнал для чтения первого значения
		while (!start_read)
			pause();
		// Копируем результат функции из именованной памяти в буфер
		strcpy(buff_func_res, addr);
		// преобразование времени к строке
		sprintf(buff_time, "%f", time);
		// запись в файл тренда
		write_file_trend(buff_func_res, buff_time);
		// ожидание сигнала от таймера
		// номер сигнала
		int signo;
		sigwait(&timer_read_set, &signo);
		// инкремент времени
		std::cout << "[P2] Чтение результата функции: " << buff_func_res << " Текущее время: " << buff_time << std::endl;
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

