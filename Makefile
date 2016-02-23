normal:
	g++ --std=c++11 -pthread -g -o email_to_REST rest_SM_receiving.cpp rest_SM_sending.cpp rest_SM_to_sendqueue.cpp rest_send_mail.cpp email_util.cpp rest_SM_tests.cpp email_parsing.cpp -lvmime -lcurl
debug:
	g++ -pthread -g -o send_mail DEBUG_controlledsender.cpp
clean:
	rm -rf *o send_mail email_to_REST
