#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <process.h>
#include <stdlib.h>
#include <sys/neutrino.h>

int main(int argc, char *argv[]) {
	std::cout << "[P3] Welcome to P3 process!" << std::endl;
	pid_t parrent_pid = getppid();

	int parrent_channel = atoi(argv[1]);

	int id_channel_p3 = ChannelCreate(0);
	if(id_channel_p3 == -1){
		std::cout << "[P3] Creating connect channel has done with error!" << std::endl;
		return 1;
	}

	char *smsg=(char*)"P3\0";
	char rmsg[200];

	int con_id = ConnectAttach(0,parrent_pid,parrent_channel,_NTO_SIDE_CHANNEL,0);
	if(con_id == -1){
		std::cout << "[P3] Connecting to MAIN channel has done with error!" << std::endl;
		return 1;
	}

	MsgSend(con_id,smsg,std::strlen(smsg)+1, rmsg,sizeof(rmsg));
	std::cout << "[P3] " << rmsg << std::endl;
	smsg = (char*)"P3 OK\0";
	MsgSend(con_id,smsg,std::strlen(smsg)+1, NULL, NULL);

	int result = ConnectDetach(con_id);
	if(result == -1){
		std::cout << "[P3] Closing connection has done with error!" << std::endl;
		return 1;
	}

	return 0;
}
