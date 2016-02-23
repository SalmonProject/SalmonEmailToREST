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
#include <fstream>
#include <vector>
#include <iomanip>

#include "email_util.h"
#include "email_parsing.h"
#include "rest_SM_to_sendqueue.h"

namespace {

bool initVmimeIMAP(vmime::shared_ptr <vmime::net::store>	*init_this_store,
			    vmime::shared_ptr <vmime::net::folder>	*init_this_folder,
			    const std::string					&with_this_username,
			    const std::string					&with_this_password,
			    const std::string					&at_this_server_url)
{
	try
	{
		vmime::utility::url url(at_this_server_url);
		url.setPort(993);
		url.setProtocol("imaps");
		url.setUsername(with_this_username);
		url.setPassword(with_this_password);

		(*init_this_store) = vmime::make_shared <vmime::net::session>()->getStore(url);		
		
#if VMIME_HAVE_TLS_SUPPORT
		(*init_this_store)->setProperty("connection.tls", true);
		(*init_this_store)->setTimeoutHandlerFactory(vmime::make_shared <timeoutHandlerFactory>());
		(*init_this_store)->setCertificateVerifier(vmime::make_shared <PlaceholderNoVer>());
#else
#error "NOPE VMIME NEEDS TO HAVE TLS SUPPORT! It's non-negotiable."
#endif // VMIME_HAVE_TLS_SUPPORT

		// Trace communication between client and server
		vmime::shared_ptr <std::ostringstream> traceStream = vmime::make_shared <std::ostringstream>();
		(*init_this_store)->setTracerFactory(vmime::make_shared <myTracerFactory>(traceStream));
		
		(*init_this_store)->connect();
		
		*init_this_folder = (*init_this_store)->getDefaultFolder();
		(*init_this_folder)->open(vmime::net::folder::MODE_READ_WRITE);
	}
	catch(vmime::exception& e)
	{
		std::cerr << std::endl;
		std::cerr << "vmime::exception during recv init: " << e << std::endl;
		sleep(5);
		return false;
	}
	catch(std::exception& e)
	{
		std::cerr << std::endl;
		std::cerr << "std::exception during recv init: " << e.what() << std::endl;
		sleep(5);
		return false;
	}
	return true;
}

} //anonymous namespace

void *receiving(void*)
{
	vmime::shared_ptr <vmime::net::store> the_store;
	vmime::shared_ptr <vmime::net::folder> default_folder;
	
	bool recvInitSucceeded = false;
	for(int tryInit = 0; !recvInitSucceeded && tryInit < 4; tryInit++)
		recvInitSucceeded = initVmimeIMAP(&the_store, &default_folder,
								    IMAP_username,password,IMAP_urlString);
	
	if(!recvInitSucceeded)
	{
		std::cerr << "FATAL! Could not initialize the receiving thread. Goodbye." << std::endl;
		exit(1);
	}
	else
		std::cerr << "Successfully initialized the receiving thread!" << std::endl;
	
	while(true)
	{
		try
		{
			//we delete emails after we have parsed all of the currently available ones. 
			//delete_num holds the indices of the emails we will at the end tell vmime to delete.
			std::vector <int> delete_num;
			
			std::vector< vmime::shared_ptr <vmime::net::message> > 
				allMessages = default_folder->getMessages(vmime::net::messageSet::byUID(1, "*"));
			
			for(int msg_array_ind = allMessages.size() - 1; msg_array_ind >= 0; msg_array_ind--)
			{
				int email_folder_ind = msg_array_ind + 1;
				
				//do the vmime stuff to actually get our hands on this email's data
				//(fetchMessage has vmime get the actual data, enabling us to call the various
				// wacky getParsedMessage(), Subject(), generare() stuff.)
				default_folder->fetchMessage(allMessages[msg_array_ind],
									    vmime::net::fetchAttributes::FULL_HEADER);
				SalmonEmail* cur_recvd_mail = prepareRESTQueryFromEmail(allMessages[msg_array_ind]);
				
				if(cur_recvd_mail)
				{
					//we now have our message: pass it to the dir via REST (in new thread) and move on
					pthread_t dummyDontCare;
					pthread_create(&dummyDontCare, NULL, sendMessageToDir, (void*)cur_recvd_mail);
				}
				delete_num.push_back(email_folder_ind);
			}
			
			if(!delete_num.empty())
			{
				default_folder->deleteMessages(vmime::net::messageSet::byNumber(delete_num));
				default_folder->expunge();
				delete_num.clear();
			}
		}
		catch(vmime::exception& e)
		{
			std::cerr << std::endl;
			std::cerr << "receiving thread vmime exception: " << e << std::endl;
			sleep(1);
			default_folder->close(true);
			the_store->disconnect();
			the_store->connect();
			default_folder = the_store->getDefaultFolder();
		}
		catch(std::exception& e)
		{
			std::cerr << std::endl;
			std::cerr << "receiving thread std::exception: " << e.what() << std::endl;
		}
		sleep(2);
	}
}

