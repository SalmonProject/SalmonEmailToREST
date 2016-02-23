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

#include <curl/curl.h>

#include "email_util.h"


//http://stackoverflow.com/questions/3589936/c-urlencode-library-unicode-capable,
//modified to replace some "clever" code and remove boost dependency
namespace {
	
std::string encodeChar(std::string::value_type v)
{
	if (isalnum(v))
		return std::string()+v;

	std::ostringstream enc;
	enc << '%' << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << int(static_cast<unsigned char>(v));
	return enc.str();
}

std::string encodeURLComponent(const std::string& url)
{
	std::string encoded_component;
	
	for(std::string::const_iterator start = url.begin(); start != url.end(); ++start)
		encoded_component += encodeChar(*start);

	return encoded_component;
}

std::string encodeURL(const std::string& url)
{
	// Find the start of the query string
	std::string::const_iterator start = std::find(url.begin(), url.end(), '?');
	std::string domain_part = std::string(url.begin(), start+1);

	// If there isn't one there's nothing to do!
	if (start == url.end())
		return url;

	// store the modified query string
	std::string qstr;
	
	for(; start != url.end(); ++start)
		qstr += encodeChar(*start);

	return domain_part + qstr;
}

//from http://curl.haxx.se/libcurl/c/getinmemory.html
struct MemoryStruct
{
	char *memory;
	size_t size;
};

//NOTE: this one DOES guarantee a null terminated result.
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL)
	{
		std::cerr << "not enough memory (realloc returned NULL)" << std::endl;
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

std::string ughDoEscapes(char* instr)
{
	std::string return_me = "";
	
	if(!instr || !*instr)
		return return_me;
	
	char* cur = instr;
	//if(*cur == '"') handle beginning ",    if this is a    "hi im a \n \n string!!!"    type of string.
	//	cur++;
	//if(!*instr)
	//	return return_me;
	
	char* cur_backslash;
	while((cur_backslash = strchr(cur, '\\')) != 0)
	{
		*cur_backslash = 0;
		return_me += cur;
		switch(*(cur_backslash+1))
		{
			case 'n':
				return_me += "\n";
			break;
			case 'r':
				return_me += "\r";
			break;
			case 0:
				return return_me;
			break;
			case '\'':
				return_me += "'";
			break;
			case '"':
				return_me += "\"";
			break;
			case '?':
				return_me += "?";
			break;
			case '\\':
				return_me += "\\";
			case 't':
				return_me += "\t";
			break;
			case 'v':
				return_me += "\v";
			break;
			default:
				return_me += *(cur_backslash+1);
		}
		cur = cur_backslash + 2;
		if(*cur == 0)
			return return_me;
	}
	return_me += cur;
	return return_me;
}

SalmonEmail parseRESTResponse(const struct MemoryStruct& chunk)
{
	SalmonEmail return_me;
	
	//std::cerr << "\n\nDEBUG: HERE IS THE WHOLE REST RESPONSE (chunk.memory):\n" << chunk.memory << std::endl;
	
	//HACK HACK HACK stupid hack: vibe is refusing to leave \n's and \r\n's as actual characters...
	//it's actually printing \n (like, an actual backslash, then an actual n) into the HTTP response.
	//SO... do some basic escaping logic. wow also... it begins and ends with "".
	std::string cpp_actually_escaped = ughDoEscapes(chunk.memory);
	char* actually_escaped = new char[cpp_actually_escaped.length() + 1];
	std::strcpy(actually_escaped, cpp_actually_escaped.c_str());
	char* return_email = actually_escaped;
	
	bool appears_quoted = false;
	if(*return_email == '"')  //HACK ugggggh (see HACK HACK HACK above)
	{
		return_email++;
		appears_quoted = true;
	}
	
	char *firstLinebreak, *returnRS, *endRS, *returnBody;
	if((firstLinebreak = strchr(return_email, '\n')) == 0)
	{
		std::cerr << "dir REST sent over a one line reply: " << return_email << std::endl;
		return_me.email_addr = return_me.body = return_me.random_string = "";
		delete actually_escaped;
		return return_me;
	}
	if(*(firstLinebreak - 1) == '\r')
		*(firstLinebreak - 1) = 0;
	else
		*firstLinebreak = 0;
	
	returnRS = firstLinebreak + 1;
	if((endRS = strchr(returnRS, '\n')) == 0)
	{
		std::cerr << "dir REST sent over a two line reply: " << return_email << "\n" << returnRS << std::endl;
		return_me.email_addr = return_me.body = return_me.random_string = "";
		delete actually_escaped;
		return return_me;
	}
	if(*(endRS - 1) == '\r')
		*(endRS - 1) = 0;
	else
		*endRS = 0;
	returnBody = endRS + 1;
	return_me.body = "";
	char* body_cur = returnBody;
	bool moreLinesLeft = true;
	do
	{
		char* nextBreak = strchr(body_cur, '\n');
		if(nextBreak == 0)
			moreLinesLeft = false;
		else
			*nextBreak = 0;
		return_me.body += body_cur;
		return_me.body += "\r\n";
		body_cur = nextBreak + 1;
	} while(moreLinesLeft);
	
	//if the whoooole response started with a ", assume that a " at the end matches that one and is not wanted
	if(appears_quoted)
	{
		//HACK ugggggh (see HACK HACK HACK above)
		int hackTrim = return_me.body.length() - 1;
		for(; 
		    hackTrim >= 0 && (return_me.body[hackTrim] == '\n' || return_me.body[hackTrim] == '\r');
			hackTrim--
		){}
		if(return_me.body[hackTrim] == '"')
			return_me.body.erase(hackTrim);
	}
	
	return_me.email_addr = std::string(return_email);
	return_me.random_string = std::string(returnRS);
	
	delete actually_escaped;
	return return_me;
}

SalmonEmail REST_with_dir(std::string fromWhom, std::string random_string, std::string body)
{
	SalmonEmail return_me;
	return_me.email_addr = "";
	
	CURL *curl_handle;
	CURLcode res;

	struct MemoryStruct chunk;

	chunk.memory = (char*)malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */ 

	curl_handle = curl_easy_init();
	
	//Need this if we don't want our program to die after a few minutes with "Alarm clock"!
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	
	//@queryParam("emailAddr", "addr") @queryParam("random_string", "rand") @queryParam("theBody", "body")
	std::string getThis = std::string("http://127.0.0.1:9004/email?addr=") + encodeURLComponent(fromWhom) + std::string("&rand=") + random_string + std::string("&body=") + encodeURLComponent(body);
	
	curl_easy_setopt(curl_handle, CURLOPT_URL, getThis.c_str());
	//send all data to this function
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	//everytime the callback function is called, we want our "chunk" output location to be available to it
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	//get it!
	res = curl_easy_perform(curl_handle);

	if(res != CURLE_OK)
	{
		std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
		curl_easy_cleanup(curl_handle);
		free(chunk.memory);
		return_me.email_addr = return_me.body = return_me.random_string = "";
		return return_me;
	}
	
	//chunk.memory now points to a memory block of size chunk.size, containing the remote file.
	return_me = parseRESTResponse(chunk);
	curl_easy_cleanup(curl_handle);
	free(chunk.memory);
	
	std::string trimmed_body;
	if(return_me.body.length() <= 60 || strstr(return_me.body.c_str(), "RESPONSE") && strstr(return_me.body.c_str(), "TEST"))
		trimmed_body = return_me.body;
	else
		trimmed_body = return_me.body.substr(0,60);
	std::cerr << "yay! correctly returning:\n  to addr: " << return_me.email_addr << "\n  RS: " << return_me.random_string << "\n  body: " << trimmed_body << std::endl;
	return return_me;
}

} //anonymous namespace

void *sendMessageToDir(void* cur_recvd_mail_p)
{
	pthread_detach(pthread_self());
	
	SalmonEmail* cur_recvd_mail = (SalmonEmail*)cur_recvd_mail_p;
	std::cerr << "Got an email from " << cur_recvd_mail->email_addr << ", now RESTing to dir..." << std::endl;
	
	SalmonEmail the_reply = REST_with_dir(cur_recvd_mail->email_addr, cur_recvd_mail->random_string, cur_recvd_mail->body);
	
	if(the_reply.email_addr == "") //indicates REST call to dir server failed.
		return 0;
	
	the_reply.is_mobileconfig = false;
	if(strstr(the_reply.body.c_str(), "http://www.apple.com/DTDs/PropertyList"))
		the_reply.is_mobileconfig = true;
	
	std::cerr << "Queueing dir's reply to " << cur_recvd_mail->email_addr << std::endl;
	delete cur_recvd_mail;
	
	//=============================================
	//BEGIN outbound_message_queue accessing critical section
	pthread_mutex_lock(&outbound_message_queue_mutex);
	outbound_message_queue.push(the_reply);
	pthread_mutex_unlock(&outbound_message_queue_mutex);
	//END outbound_message_queue accessing critical section
	//=============================================
	
	return 0;
}
