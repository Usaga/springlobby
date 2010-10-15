#ifndef __HOTKEYEXCEPTION_h__
#define __HOTKEYEXCEPTION_h__

#include <stdexcept>

#include <wx/string.h>
#include "../../utils/conversion.h"

class HotkeyException : public std::exception
{
public:
	HotkeyException( const wxString& message ) : m_message( message ) {}
#ifndef __WXMSW__
	virtual ~HotkeyException() throws {};
#endif

	const char* what() const { return "Use HotkeyException::getMessage() instead"; };
	const wxString& getMessage() const
	{
		return this->m_message;
	}

private:
	const wxString	m_message;
};

#endif