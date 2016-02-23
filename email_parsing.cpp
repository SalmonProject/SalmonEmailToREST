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

namespace{

//TODO ok found out how to do it in vmime (with a bunch of convoluted bs,
//of course), so probably eventually TRIM!
//this really ought to be done by asking vmime for the actual
//From field in a programmatic way. of course, i just spent a few minutes
//trying to do that most simple, fundamental, every-program-would-need-it
//of tasks, and got nowhere, because lol vmime. SO!
std::string whole_headerGetFrom(const char* whole_header)
{
	//ah... one extension or w/e, which yahoo uses, i guess lets them
	//list the fields they saw in the email or something. anyways, the result
	//is an extra From: long before the actual From: field. some ways to be
	//more robust:
	//first, skip to X-Yahoo-SMTP: if present, which will hopefully
	//skip all of it. then, skip to just past the last Received:. No guarantee
	//there won't be that same extension on that last one, i'm guessing, but
	//it helps, and works for the example i have here. finally... last line
	//of defense, just find the very last From: and hope it's the right one.
	const char* start_from_here = strstr(whole_header, "X-Yahoo-SMTP:");
	if(!start_from_here)
	{
		const char* temp_received = strstr(whole_header, "Received:");
		const char* last_received;
		while(temp_received)
		{
			temp_received++;
			last_received = temp_received;
			temp_received = strstr(temp_received, "Received:");
		}
		//ensure there is still a From: after the last Received:
		if(strstr(last_received, "From:"))
			start_from_here = last_received;
	}
	//ensure there is still a From: after X-Yahoo-SMTP:
	else if(!strstr(start_from_here, "From:"))
		start_from_here = 0;
	const char* start_from_where = (start_from_here ? start_from_here : whole_header);
	const char* temp_from = strstr(start_from_where, "From:");
	while(temp_from)
	{
		start_from_here = temp_from;
		temp_from = strstr(temp_from+1, "From:");
	}
	
	if(!start_from_here)
	{
		std::cerr << "GOT MESSAGE WITHOUT From: FIELD?!" << std::endl;
		std::cerr << whole_header << std::endl;
		return std::string("");//goto DeleteAndContinue;
	}
	
	//extract the sender (From: field) from the whole raw header.
	//it might come as From: sender@yahoo.com   or  
	//From: "Fred Douglas" <sender@gmail.com>, so handle both cases.
	
	//NOTE due to the new logic above, buffer_ptr is now guaranteed to
	//start on a From: 
	//char* buffer_ptr = strstr(start_from_here, "From:");
	const char* buffer_ptr = start_from_here;
	char sender_temp[300];
	memset(sender_temp, 0, 300);
	strncpy(sender_temp, buffer_ptr, 299);
	if(strchr(sender_temp, '\r'))
		*strchr(sender_temp, '\r') = 0;
	if(strchr(sender_temp, '\n'))
		*strchr(sender_temp, '\n') = 0;

	char* sender = strstr(sender_temp, "<");
	if(sender != NULL)
	{
		sender++;
		if(strstr(sender, ">"))
			*strstr(sender, ">") = 0;
		return std::string(sender);
	}
	else
	{
		//NOTE: don't need to check for : because strstr above is on "From:"
		sender = strchr(sender_temp, ':')+1;
		if(*sender == ' ')
			sender++;
		//the \r or \n at end of line has already been overwritten with 0,
		//and the email address should end just before there, so we can print now.
		return std::string(sender);
	}
}

std::string random_stringFromBody(const std::string& theBody)
{
	//BODY FORMAT:
	//Hello, do not reply to this email. No person reads mail that this address receives.
	//This message's unique identifier:djhsdjfy7643465gi746fgio58f74hi47wtb6gh4v9i (50 characters)
	std::cerr << "checking for uniqueID...";
	if(!strstr(theBody.c_str(), "Unique identifier "))
	{
		std::cerr << "The message didn't have the right uniqueID format. It's probably spam." << std::endl;
		if(strstr(theBody.c_str(), "www.linkedin.com"))
			std::cerr << "Yup, it's linkedin spam..." << std::endl;
		return std::string("");
	}
	char random_string[100];
	strncpy(random_string, strstr(theBody.c_str(), "Unique identifier ") + 
						strlen("Unique identifier "), 50);
	return std::string(random_string);
}

std::string messageBodyFromAttachments(const std::vector <vmime::shared_ptr <const vmime::attachment> >& atts)
{
	//sender and random string, but no message...
	if(atts.size() <= 0)									
	{
		std::cerr << "got mail with correct random string, but no attachments... ignoring." << std::endl;
		return std::string("");
	}
	
	for(std::vector <vmime::shared_ptr <const vmime::attachment> >::const_iterator
	        it = atts.begin() ; it != atts.end() ; ++it)
	{
		vmime::shared_ptr <const vmime::attachment> att = *it;

		// Get attachment size
		vmime::size_t size = 0;

		if(att->getData()->isEncoded())
			size = att->getData()->getEncoding().getEncoder()->getDecodedSize(att->getData()->getLength());
		else
			size = att->getData()->getLength();

		std::stringstream attachStream;
		vmime::utility::outputStreamAdapter attach_out(attachStream);
		att->getData()->extract(attach_out);

		return attachStream.str(); //should only be one
	}
}


//If it came as "Fred Douglas" <emailaddr@example.com>, return emailaddr@example.com
std::string emailFromMiniparse(vmime::shared_ptr <vmime::message> parsedMsg)
{
	std::string raw_from = parsedMsg->getHeader()->From()->getValue()->generate();
	char* strip_angles_buffer = new char[raw_from.length()+1];
	strcpy(strip_angles_buffer, raw_from.c_str());
	if(strchr(strip_angles_buffer, '<') && strchr(strip_angles_buffer, '>'))
	{
		*strchr(strip_angles_buffer, '>') = 0;
		std::string ret_string = std::string(strchr(strip_angles_buffer, '<')+1);
		delete strip_angles_buffer;
		return ret_string;
	}
	else
	{
		delete strip_angles_buffer;
		return raw_from;
	}
}

//Can you believe this isn't built into vmime? (I can after working with it so much...)
std::string emailBodyVmimeparse(vmime::shared_ptr <vmime::message> parsedMsg)
{
	vmime::messageParser parser(parsedMsg);
	
	
	const std::vector < vmime::shared_ptr < const vmime::textPart > > text_parts = 
		parser.getTextPartList();
	
	//ugh... I actually cannot figure out to get vmime to say whether a textPart is
	//plain or HTML (see below about getType()). Well... it appears multi-part MIME usually
	//does plain text first, so take the first part unless it has HTML-looking things,
	//and the second part doesn't.
	std::vector <std::string> ret_candidates;
	for(int i=0;i<text_parts.size();i++)
	{
		//returns "text" for both plain and html, vmime why can't you do anything right
		//std::cerr << "type: " << text_parts[i]->getType().getType() << std::endl;
		std::stringstream bodyStream;
		vmime::utility::outputStreamAdapter out(bodyStream);
		vmime::shared_ptr <const vmime::contentHandler> cth = text_parts[i]->getText();
		cth->extract(out);
		
		ret_candidates.push_back(bodyStream.str());
	}
	if(ret_candidates.size() == 0)
		return "ERROR NO TEXT";
	if(ret_candidates.size() == 1)
		return ret_candidates[0];
	if(	 (strstr(ret_candidates[0].c_str(), "text/html") || strstr(ret_candidates[0].c_str(), "<html>")) &&
		!(strstr(ret_candidates[1].c_str(), "text/html") || strstr(ret_candidates[1].c_str(), "<html>")))
		return ret_candidates[1];
	return ret_candidates[0];
}


//Given a vmime-parsed vmime::message representing a manually sent email, 
//return a new'd SalmonEmail containing that email's info
SalmonEmail* RESTQueryFromManualEmail(vmime::shared_ptr <vmime::message> parsedMsg)
{
	std::string subject_field = parsedMsg->getHeader()->Subject()->getValue()->generate();
	std::string email_id_string = std::string(strstr(subject_field.c_str(), "ID=") + 3);
	
	
	std::string whole_header = parsedMsg->getHeader()->generate();
	std::string email_addr;
	
	//extract the sender TODO make this oooh_from_field if that works
	//TODO TRIM if("" == (email_addr = whole_headerGetFrom(whole_header.c_str())))
	if("" == (email_addr = emailFromMiniparse(parsedMsg)))
		return 0;//goto DeleteAndContinue;
	
	std::stringstream bodyStream;
	vmime::utility::outputStreamAdapter out(bodyStream);
	
	vmime::shared_ptr <vmime::body> bdy = parsedMsg->getBody();
	vmime::shared_ptr <const vmime::contentHandler> cth = bdy->getContents();
	cth->extract(out);
	
	std::string body_text = emailBodyVmimeparse(parsedMsg);
	
	std::cerr << "manual email body text: " << body_text << std::endl;
	
	//TRIM SalmonEmail* cur_recvd_mail = new SalmonEmail;
	//TRIM cur_recvd_mail->email_addr = email_addr;
	//TRIM cur_recvd_mail->body = body_text;
	//TRIM cur_recvd_mail->random_string = email_id_string;
	//TRIM cur_recvd_mail->is_mobileconfig = false;
	//TRIM return cur_recvd_mail;
	return new SalmonEmail(	email_addr, //the person's email address
						body_text, //the message contents
						email_id_string, //the message's random id
						false, //attachment/message is not a mobileconfig file
						true //this IS a manually-sent-by-user email
					);
}

//Given a vmime-parsed vmime::message representing a normally (automatically) sent email, 
//return a new'd SalmonEmail containing that email's info
SalmonEmail* RESTQueryFromNormalEmail(vmime::shared_ptr <vmime::message> parsedMsg)
{
	std::string whole_header = parsedMsg->getHeader()->generate();
	//std::cerr << "DEBUG HERE IS THE WHOLE HEADER: " << whole_header << std::endl;
	
	//extract the sender TODO make this oooh_from_field if that works
	SalmonEmail* cur_recvd_mail = new SalmonEmail;
	//TODO TRIM if("" == (cur_recvd_mail->email_addr = whole_headerGetFrom(whole_header.c_str())))
	if("" == (cur_recvd_mail->email_addr = emailFromMiniparse(parsedMsg)))
		return 0;//goto DeleteAndContinue; TODO TRIM
	
	
	//extract the body to check that it matches the expected format, 
	//and to get the random string from it
	{
		/* TODO TRIM (dumps WHOLE body, plain + html)
		std::stringstream bodyStream;
		vmime::utility::outputStreamAdapter out(bodyStream);
		
		vmime::shared_ptr <vmime::body> bdy = parsedMsg->getBody();
		vmime::shared_ptr <const vmime::contentHandler> cth = bdy->getContents();
		cth->extract(out);
		
		std::string whole_body = std::string(bodyStream.str());
		*/
		
		std::string body_text = emailBodyVmimeparse(parsedMsg);
		
		//drop any message that is just the school email server saying error
		if(strstr(cur_recvd_mail->email_addr.c_str(), "MAILER-DAEMON") &&
			strstr(body_text.c_str(), "Delivery has failed"))
		{
			std::cerr << "This is a mailer-daemon bounce back; ignoring." << std::endl;
			return 0;//goto DeleteAndContinue;
		}

		cur_recvd_mail->random_string = random_stringFromBody(body_text);
		if(cur_recvd_mail->random_string == "")
			return 0;//goto DeleteAndContinue;
	}
	
	
	//finally, get the message body: the contents of the first attached file.
	cur_recvd_mail->body = messageBodyFromAttachments(
		vmime::attachmentHelper::findAttachmentsInMessage(parsedMsg));
	if(cur_recvd_mail->body == "")
		return 0;//goto DeleteAndContinue;
	
	cur_recvd_mail->is_manual = false;
	cur_recvd_mail->is_mobileconfig = false;
	
	return cur_recvd_mail;
}

} //anonymous namespace

//curMsg should be a fetch()d but not parsed message (maybe the parsing doesn't matter,
//I don't know). Returns a new'd SalmonEmail struct ready to go into the REST query queue.
SalmonEmail* prepareRESTQueryFromEmail(vmime::shared_ptr <vmime::net::message> curMsg)
{
	vmime::shared_ptr <vmime::message> parsedMsg = curMsg->getParsedMessage();
	
	//These two appear equivalent:
	std::string subject_field = parsedMsg->getHeader()->Subject()->getValue()->generate();
	//vmime::messageParser parser(parsedMsg);
	//std::string subject_field = parser.getSubject().getWholeBuffer();
	
	std::string oooh_from_field = emailFromMiniparse(parsedMsg);
	std::cerr << "oooh you actually can get from... " << oooh_from_field << std::endl;
	
	std::string plain_text_body = emailBodyVmimeparse(parsedMsg);
	std::cerr << "plain text body yuck vmime: " << plain_text_body << std::endl;
	
	//check whether this is a manually sent, "msgInBody" email or not
	if(strstr(subject_field.c_str(), "salmon msgInBody"))
		return RESTQueryFromManualEmail(parsedMsg);
	else
		return RESTQueryFromNormalEmail(parsedMsg);
}
