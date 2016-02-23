#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <iostream>
#include <iomanip>


#include <curl/curl.h>

#include "email_util.h"
#include "rest_SM_tests.h"

void *receiving(void*);
void* sending(void*);

int main()
{
	vmime::platform::setHandler<vmime::platforms::posix::posixHandler>();
	
	curl_global_init(CURL_GLOBAL_ALL);

	int ret;

	pthread_t inputing;
	pthread_t send;
	pthread_t receive;
	
	pthread_mutex_init(&outbound_message_queue_mutex, 0);
	
	ret = pthread_create(&send, NULL, sending, NULL);
	if(ret != 0)
		std::cerr << "Error creating receiving thread" << std::endl;

	
	
//==================TEST==========================	
#include "test_control.inc"
#ifdef REST_SEND_MAIL_INTERACTIVE_DEBUG
	std::cerr << "***NOW STARTING INTERACTIVE DEBUGGER!\n" <<
			"For every email you want to simulate sending,\n"<<
			"first enter the email address on one line,\n"<<
			"and then the message body on the next." << std::endl;
	interactiveTest();
#endif //#ifdef REST_SEND_MAIL_INTERACTIVE_DEBUG
#ifdef REST_SEND_MAIL_LONGREQUESTS_TEST
	std::cerr << "Long concurrent requests test..." << std::endl;
	testLongRequests();
#endif //#ifdef REST_SEND_MAIL_LONGREQUESTS_TEST
#ifdef REST_SEND_MAIL_UNICODE_TEST
	std::cerr << "Unicode test..." << std::endl;
	testUnicode();
#endif //#ifdef REST_SEND_MAIL_UNICODE_TEST
//==================TEST==========================
	
	
	
	ret = pthread_create(&receive, NULL, receiving, NULL);
	if(ret != 0)
		std::cerr << "Error creating receiving thread" << std::endl;
	std::cerr << "All send_mail (NON-TEST) threads now started." << std::endl;
	ret = pthread_join(receive, NULL);
	std::cerr << "Error! Receiving thread ended..." << std::endl;
	
	
	ret = pthread_join(send, NULL);
	std::cerr << "Error! Sending thread ended..." << std::endl;
	
	curl_global_cleanup();
	return 0;
}

