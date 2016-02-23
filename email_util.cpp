#include <fstream>

#include "email_util.h"

pthread_mutex_t outbound_message_queue_mutex;
std::queue<SalmonEmail> outbound_message_queue;

// Exception helper
std::ostream& operator<<(std::ostream& os, const vmime::exception& e)
{
	os << "* vmime::exceptions::" << e.name() << std::endl;
	os << "    what = " << e.what() << std::endl;

	// More information for special exceptions
	if(dynamic_cast <const vmime::exceptions::command_error*>(&e))
	{
		const vmime::exceptions::command_error& cee =
		    dynamic_cast <const vmime::exceptions::command_error&>(e);

		os << "    command = " << cee.command() << std::endl;
		os << "    response = " << cee.response() << std::endl;
	}

	if(dynamic_cast <const vmime::exceptions::invalid_response*>(&e))
	{
		const vmime::exceptions::invalid_response& ir =
		    dynamic_cast <const vmime::exceptions::invalid_response&>(e);

		os << "    response = " << ir.response() << std::endl;
	}

	if(dynamic_cast <const vmime::exceptions::connection_greeting_error*>(&e))
	{
		const vmime::exceptions::connection_greeting_error& cgee =
		    dynamic_cast <const vmime::exceptions::connection_greeting_error&>(e);

		os << "    response = " << cgee.response() << std::endl;
	}

	if(dynamic_cast <const vmime::exceptions::authentication_error*>(&e))
	{
		const vmime::exceptions::authentication_error& aee =
		    dynamic_cast <const vmime::exceptions::authentication_error&>(e);

		os << "    response = " << aee.response() << std::endl;
	}

	if(dynamic_cast <const vmime::exceptions::filesystem_exception*>(&e))
	{
		const vmime::exceptions::filesystem_exception& fse =
		    dynamic_cast <const vmime::exceptions::filesystem_exception&>(e);

		os << "    path = " << vmime::platform::getHandler()->
		   getFileSystemFactory()->pathToString(fse.path()) << std::endl;
	}

	if(e.other() != NULL)
		os << *e.other();

	return os;
}


vmime::shared_ptr<vmime::security::cert::X509Certificate> loadX509CertificateFromFile(const std::string& path)
{
	std::ifstream cert_file;
	cert_file.open(path.c_str(), std::ios::in | std::ios::binary) ;
	if(!cert_file)
		fprintf(stderr, "Failed to load %s\n", path.c_str());
	vmime::utility::inputStreamAdapter is(cert_file) ;
	
	// Try DER format
	return vmime::security::cert::X509Certificate::import(is);
}
