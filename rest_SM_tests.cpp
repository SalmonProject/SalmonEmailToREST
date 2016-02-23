#include <string>

#include "email_util.h"
#include "rest_SM_tests.h"

#include "rest_SM_to_sendqueue.h"

void testUnicode()
{
	SalmonEmail* test_email_2 = new SalmonEmail;
	test_email_2->random_string = std::string("test2test2test2test2test2test2test2test2test2test2");
	test_email_2->body = std::string("fredsalmondggfeykvrmelsjeUNICODEtest");
	test_email_2->email_addr = std::string("redacted@example.com");
	pthread_t dummy2;
	pthread_create(&dummy2, NULL, sendMessageToDir, (void*)test_email_2);
}

void testLongRequests()
{
	SalmonEmail* test_email_1 = new SalmonEmail;
	test_email_1->random_string = std::string("test1test1test1test1test1test1test1test1test1test1");
	test_email_1->body = std::string("needServer");
	test_email_1->email_addr = std::string("redacted@example.com");
	pthread_t dummy1;
	pthread_create(&dummy1, NULL, sendMessageToDir, (void*)test_email_1);
	
	SalmonEmail* test_email_2 = new SalmonEmail;
	test_email_2->random_string = std::string("test2test2test2test2test2test2test2test2test2test2");
	test_email_2->body = std::string("fredsalmondggfeykvrmelsjelongtest\n2");
	test_email_2->email_addr = std::string("redacted@example.com");
	pthread_t dummy2;
	pthread_create(&dummy2, NULL, sendMessageToDir, (void*)test_email_2);
	
	SalmonEmail* test_email_3 = new SalmonEmail;
	test_email_3->random_string = std::string("test3test3test3test3test3test3test3test3test3test3");
	test_email_3->body = std::string("needServer");
	test_email_3->email_addr = std::string("nobody@example.com");
	pthread_t dummy3;
	pthread_create(&dummy3, NULL, sendMessageToDir, (void*)test_email_3);
	
	SalmonEmail* test_email_4 = new SalmonEmail;
	test_email_4->random_string = std::string("test4test4test4test4test4test4test4test4test4test4");
	test_email_4->body = std::string("needServer");
	test_email_4->email_addr = std::string("nobody@example.com");
	pthread_t dummy4;
	pthread_create(&dummy4, NULL, sendMessageToDir, (void*)test_email_4);
	
	sleep(2);
	
	SalmonEmail* test_email_5 = new SalmonEmail;
	test_email_5->random_string = std::string("test5test5test5test5test5test5test5test5test5test5");
	test_email_5->body = std::string("fredsalmondggfeykvrmelsjelongtest\n5");
	test_email_5->email_addr = std::string("nobody@example.com");
	pthread_t dummy5;
	pthread_create(&dummy5, NULL, sendMessageToDir, (void*)test_email_5);
	
	SalmonEmail* test_email_6 = new SalmonEmail;
	test_email_6->random_string = std::string("test6test6test6test6test6test6test6test6test6test6");
	test_email_6->body = std::string("existingLogin");
	test_email_6->email_addr = std::string("redacted@example.com");
	pthread_t dummy6;
	pthread_create(&dummy6, NULL, sendMessageToDir, (void*)test_email_6);
	
	SalmonEmail* test_email_7 = new SalmonEmail;
	test_email_7->random_string = std::string("test7test7test7test7test7test7test7test7test7test7");
	test_email_7->body = std::string("existingLogin");
	test_email_7->email_addr = std::string("nobody@example.com");
	pthread_t dummy7;
	pthread_create(&dummy7, NULL, sendMessageToDir, (void*)test_email_7);
	
	sleep(1);
	SalmonEmail* test_email_8 = new SalmonEmail;
	test_email_8->random_string = std::string("test8test8test8test8test8test8test8test8test8test8");
	test_email_8->body = std::string("existingLogin");
	test_email_8->email_addr = std::string("redacted@example.com");
	pthread_t dummy8;
	pthread_create(&dummy8, NULL, sendMessageToDir, (void*)test_email_8);
}

void interactiveTest()
{
	while(true)
	{
		std::string thetowhom;
		std::string thers;
		std::string thebody;
		SalmonEmail* curRecvdMail = new SalmonEmail;
		getline(std::cin, curRecvdMail->email_addr);
		getline(std::cin, curRecvdMail->body);
		curRecvdMail->random_string = std::string("12345678901234567890123456789012345678901234567890");
		pthread_t dummyDontCare;
		pthread_create(&dummyDontCare, NULL, sendMessageToDir, (void*)curRecvdMail);
	}
}
