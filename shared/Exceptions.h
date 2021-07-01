class Exception
{
protected:
	String message;

public:
	Exception () :
	  message()
	  {
	  };

	Exception (const String &message) :
	  message(message)
	  {
	  };

	String &Message ()
	{
		return message;
	}
};

class ExceptionIndexOutOfRange : public Exception
{
public:
	ExceptionIndexOutOfRange() :
	  Exception(String("The specified index is out of range"))
	  {
	  };

	ExceptionIndexOutOfRange(const String &variable) :
	  Exception(String("The variable \"").Concatenate(variable).Concatenate("\" is out of range"))
	  {
	  };
};

class ExceptionNotImplemented : public Exception
{
public:
	ExceptionNotImplemented() :
	  Exception(String("The specified function/method has not been implemented yet."))
	  {
	  };
};

class ExceptionNetworking : public Exception
{
public:
	ExceptionNetworking(const String &message) :
	  Exception(message)
	  {
	  };
};
