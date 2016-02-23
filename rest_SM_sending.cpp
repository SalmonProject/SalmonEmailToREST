#include <atomic>

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
#include <time.h>
#include <iostream>
#include <map>
#include <fstream>
#include <vector>
#include <time.h>
#include <iomanip>


#include "email_util.h"

namespace {

std::atomic_int atomic_attachment_counter(1);
void getNextAttachmentName(char* here)
{
	int use_counter = atomic_attachment_counter.fetch_add(1);
	strcpy(here, "temp_attach");
	char* num_goes_here = here + strlen(here);
	sprintf(num_goes_here, "%d", use_counter);
	strcat(num_goes_here, ".txt");
}

bool trySendMessage(const vmime::shared_ptr <vmime::net::transport> &SMTP_transport,
				const SalmonEmail &message_to_send)
{
	try
	{
		vmime::shared_ptr <vmime::message> msg;
		vmime::messageBuilder mb;

		std::string subject_string(
			message_to_send.is_manual 
			? ("salmon response to manually sent client message (ID "
				+message_to_send.random_string+")")
			: ("salmon backend - does not need to be seen by humans. Unique identifier "
				+message_to_send.random_string));
		mb.setSubject(vmime::text(subject_string));
		mb.setExpeditor(vmime::mailbox(SMTP_username));
		mb.getRecipients().appendAddress(vmime::make_shared <vmime::mailbox>(message_to_send.email_addr));

		//generate a temp attach file name (with an atomically incrementing seq #)
		char full_attachment_path[300];
		if(getcwd(full_attachment_path, 300)!=0)
		{
			char curAttachmentName[100];
			getNextAttachmentName(curAttachmentName);
			strcat(full_attachment_path, curAttachmentName);
		}
		else
		{
			std::cerr << "lol wut couldn't getcwd() ???" << std::endl;
			return false;
		}

		//open + write that temporary attachment file
		FILE* attach_file = fopen(full_attachment_path,"w");
		fwrite(message_to_send.body.c_str(), 1, strlen(message_to_send.body.c_str()), attach_file);
		fclose(attach_file);

		//tell vmime about that attachment file with whatever voodoo vmime wants
		vmime::shared_ptr <vmime::fileAttachment> att = 
		vmime::make_shared <vmime::fileAttachment>
		(
			full_attachment_path,
			vmime::mediaType("text"),
			vmime::text("file")
		);
		
		std::string attachment_filename;
		if(message_to_send.is_mobileconfig)
			attachment_filename = "Salmon.mobileconfig";
		else if(message_to_send.is_manual)
			attachment_filename = (message_to_send.random_string + ".salmonattach");
		else
			attachment_filename = "attach.txt";
		att->getFileInfo().setFilename(attachment_filename);
		mb.appendAttachment(att);
		msg = mb.construct();
		SMTP_transport->send(msg);
		std::cerr << "Successfully sent dir's reply email to " << message_to_send.email_addr 
		<< std::endl;
		unlink(full_attachment_path);
	}
	catch(vmime::exceptions::open_file_error& e)
	{
		std::cerr << std::endl;
		std::cerr << "vmime file exception when constructing/sending a message: " << e << std::endl;
		return false;
	}
	catch(vmime::exception& e)
	{
		std::cerr << std::endl;
		std::cerr << "vmime exception when constructing/sending a message: " << e << std::endl;
		SMTP_transport->disconnect();
		SMTP_transport->connect();
		return false;
	}
	catch(std::exception& e)
	{
		std::cerr << std::endl;
		std::cerr << "std::exception when constructing/sending a message: " << e.what() << std::endl;
		return false;
	}
	return true;
}

bool initVmimeSMTP(const vmime::shared_ptr <vmime::net::session>	&sender_session,
			    vmime::utility::url						*url,
			    vmime::shared_ptr <vmime::net::transport>	*SMTP_transport)
{
	try
	{
		url->setProtocol("smtp");
		url->setUsername(SMTP_username); //defined in email_util.h
		url->setPassword(password); //defined in email_util.h

		*SMTP_transport = sender_session->getTransport(*url);

#if VMIME_HAVE_TLS_SUPPORT
		(*SMTP_transport)->setProperty("connection.tls", true);
		(*SMTP_transport)->setTimeoutHandlerFactory(vmime::make_shared <timeoutHandlerFactory>());
		//NOTE NOTE would now need to theVerifierGuy->setX509RootCAs(trustedCerts) if we had ANY sort of
		//verification... see old vmime_dir_server folder for the version where this is still around 
		//(including the interactive verifier class, and some ACTUAL certs to use!)
		vmime::shared_ptr<PlaceholderNoVer> theVerifierGuy = vmime::make_shared <PlaceholderNoVer>();
		(*SMTP_transport)->setCertificateVerifier(theVerifierGuy);
#else
#error "NOPE VMIME NEEDS TO HAVE TLS SUPPORT! It's non-negotiable."
#endif // VMIME_HAVE_TLS_SUPPORT

	}
	catch(vmime::exception& e)
	{
		std::cerr << std::endl;
		std::cerr << "vmime exception from sending init: " << e << std::endl;
		sleep(5);
		return false;
	}
	catch(std::exception& e)
	{
		std::cerr << std::endl;
		std::cerr << "std::exception from sending init: " << e.what() << std::endl;
		sleep(5);
		return false;
	}
	return true;
}

} //anonymous namespace

void* sending(void*)
{
	vmime::shared_ptr <vmime::net::session> sender_session = vmime::make_shared <vmime::net::session>();
	vmime::shared_ptr <vmime::net::transport> SMTP_transport;
	vmime::utility::url url(SMTP_urlString); //defined in email_util.h
	
	bool sending_init_succeeded = false;
	for(int try_init = 0; !sending_init_succeeded && try_init < 4; try_init++)
		sending_init_succeeded = initVmimeSMTP(sender_session, &url, &SMTP_transport);
	
	if(!sending_init_succeeded)
	{
		std::cerr << "FATAL! Could not initialize the sending thread. Goodbye." << std::endl;
		exit(2);
	}
	
	while(true)
	{
		//==============================================
		//BEGIN outbound_message_queue accessing critical section
		pthread_mutex_lock(&outbound_message_queue_mutex);
		
		if(outbound_message_queue.empty())
		{
			pthread_mutex_unlock(&outbound_message_queue_mutex);
			sleep(1);
			continue;
		}

		std::queue<SalmonEmail> cur_messages;
		while(!outbound_message_queue.empty())
		{
			SalmonEmail temp_message = outbound_message_queue.front();
			outbound_message_queue.pop();
			cur_messages.push(temp_message);
		}
		
		pthread_mutex_unlock(&outbound_message_queue_mutex);
		//END outbound_message_queue accessing critical section
		//=============================================
		
		
		SMTP_transport->connect();
		
		while(!cur_messages.empty())
		{
			SalmonEmail message_to_send;
			message_to_send = cur_messages.front();
			cur_messages.pop();
			
			if(!trySendMessage(SMTP_transport, message_to_send))
				std::cerr<<"Giving up on sending reply to "<<message_to_send.email_addr<<std::endl;
		}
		
		SMTP_transport->disconnect();
		sleep(1);
	}
}

