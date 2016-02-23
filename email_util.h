#ifndef _SALMON_SENDMAIL_EMAIL_UTIL_H_ 
#define _SALMON_SENDMAIL_EMAIL_UTIL_H_

#include <vmime/vmime.hpp>
#include <vmime/constants.hpp>
#include <vmime/platforms/posix/posixHandler.hpp>

#include <queue>

#include "tracer.hpp"
#include "timeoutHandler.hpp"

class SalmonEmail
{
public:
	std::string email_addr;
	std::string body;
	std::string random_string;
	bool is_mobileconfig;
	bool is_manual;
	
	SalmonEmail(const std::string& email, const std::string& message_body, const std::string& rand_id, bool mobileconfig, bool manual)
	{
		email_addr = email;
		body = message_body;
		random_string = rand_id;
		is_mobileconfig = mobileconfig;
		is_manual = manual;
	}
	SalmonEmail(){}
};


std::ostream& operator<<(std::ostream& os, const vmime::exception& e);
vmime::shared_ptr<vmime::security::cert::X509Certificate> loadX509CertificateFromFile(const std::string& path);


class PlaceholderNoVer : public vmime::security::cert::defaultCertificateVerifier
{
	void verify(vmime::shared_ptr <vmime::security::cert::certificateChain> chain, const vmime::string& hostname)	{return;}
};


extern pthread_mutex_t outbound_message_queue_mutex;
extern std::queue<SalmonEmail> outbound_message_queue;

const std::string IMAP_urlString = "imaps://REDACTED";
const std::string SMTP_urlString = "smtp://REDACTED";
const std::string SMTP_username = "REDACTED@example.com";
const std::string IMAP_username = "REDACTED";


//The password! Does not belong in the repository!
#include "SALMON_EMAIL_PASSWORD.inc"
#ifndef _SALMON_EMAIL_PASSWORD_VAR_DEFINED_
#define _SALMON_EMAIL_PASSWORD_VAR_DEFINED_

#warning USING FAKE PASSWORD JUST SO WE CAN COMPILE! PROGRAM WONT BE ABLE TO DO EMAIL STUFF!
const std::string password = "fake";

#endif //_SALMON_EMAIL_PASSWORD_VAR_DEFINED_



#endif //_SALMON_SENDMAIL_EMAIL_UTIL_H_
