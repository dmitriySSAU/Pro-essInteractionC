#include <cstdlib>
#include <iostream>
#include <stddef.h>
#include <process.h>
#include <sys/neutrino.h>


int main(int argc, char *argv[]) {
	std::cout << "[MAIN] Welcome to MAIN process!" << std::endl;

	int id_my_chn = ChannelCreate(0);
	if(id_my_chn == -1){
		std::cout << "[MAIN] Creating connect channel has done with error!" << std::endl;
		return 1;
	}

	char buff[12];
	itoa(id_my_chn, buff, 10);

	int result = spawnl(P_NOWAIT, "p2", "p2", buff, NULL);
	if(result == -1){
		std::cout << "[MAIN] Running subprocess P2 has done with error!" << std::endl;
		return 1;
	}

	result = spawnl(P_NOWAIT, "p3", "p3", buff, NULL);
	if(result == -1){
		std::cout << "[MAIN] Running subprocess P3 has done with error!" << std::endl;
		return 1;
	}

	char message[126];
	int count_rcv_msg = 0;

	while(true)
	{
		int rcvid=MsgReceive(id_my_chn,message,sizeof(message),NULL);

		if (std::strcmp(message, "P2") == 0)
			std::strcpy(message,"p1 send message to p2");
		else if (std::strcmp(message, "P3") == 0)
			std::strcpy(message,"p1 send message to p3");
		else
		{
			++count_rcv_msg;
			std::cout << "[MAIN] " << message << std::endl;
			if (count_rcv_msg == 2)
				break;
			else continue;
		}
		MsgReply(rcvid,1,message,sizeof(message));
	}

	result = ChannelDestroy(id_my_chn);
	if(result == -1){
		std::cout << "[MAIN] Destruction connect channel has done with error!" << std::endl;
		return 1;
	}

	return 0;
}
