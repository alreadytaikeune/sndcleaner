#include <stdexcept>
#include <string>
class NotImplementedException : public std::logic_error
{
public:
	NotImplementedException(const std::string& message) 
        : std::logic_error(message) { };
    virtual char const * what() const throw(){ 
    	return "Function not yet implemented."; 
    }
} ;