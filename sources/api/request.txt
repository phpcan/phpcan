.. _php-can-request:

===========================
``\Can\Server\Request`` API
===========================

.. php:namespace:: Can\Server

.. php:class:: Request

   Request class
   
.. php:attr:: method

    ``string`` HTTP request method
    
.. php:attr:: uri 

    ``string`` URI path.
    
.. php:attr:: query 

    ``string`` The query string.
    
.. php:attr:: protocol 

    ``string`` HTTP protocol used by client to communicate with the web server.
    
.. php:attr:: remote_addr 

    ``string`` Client's remote IP address.
    
.. php:attr:: remote_port 

    ``integer`` The port being used by client to communicate with the web server.
    
.. php:attr:: headers

    ``array`` Container with HTTP request headers 
    
.. php:attr:: cookies

    ``array`` Container with parsed cookies sent by the client
    
.. php:attr:: get 

    ``array`` Container with parsed GET parameters
    
.. php:attr:: post 

    ``array`` Container with parsed POST parameters

.. php:attr:: files 

    ``array`` Container with uploaded files information
    
.. php:attr:: time 

    ``float`` The timestamp of the start of the request.
    
.. php:method:: findRequestHeader(string $name)

    Try to find the value of the request header with the name $name. This will perform case-insensetive search
    through all request headers.
    
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    
    :returns: string
    
.. php:method:: findResponseHeader(string $name)

    Try to find the value of the response header with the name $name. This will perform case-insensetive search
    through all response headers.
    
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    
    :returns: string
    
.. php:method:: getRequestBody()

    Get raw content of the request body.
    
    :returns: string
    
.. php:method:: getResponseBody()

    Get content of the response body that will be sent to the client.
    
    :returns: string
    
.. php:method:: setResponseBody()

    Set content of the response body that will be sent to the client.
    
    :returns: string

.. php:method:: addResponseHeader(string $name, string $value)
    
    Add response header.
    
    :param string $name: The response header name.
    :param string $value: The response header value.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    :returns: boolean ``true`` on success, ``false`` on failure
    
.. php:method:: removeResponseHeader(string $name[, string $value])

    Remove response header. If value is provided the header will be only removed if the existing header value and given value are identical.
    
    :param string $name: The response header name to remove.
    :param string $value: The response header value filter.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    :returns: boolean ``true`` on success, ``false`` if header does not exist
    
.. php:method:: setResponseStatus(int $status)

    Set response status. 
    
    :param int $status: The HTTP response status of the valid range 100-599
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    
.. php:method:: redirect(string $location[, int $status = 302])
    
    Redirect client to the new location. Actually this method adds the ``Location`` response header with provided $location
    and sets HTTP response status to 302 (by default) or any provided status in a valid range 300-399
    
    :param string $location: The new location to redirect client to.
    :param int $status: The HTTP redirection response status of the valid range 300-399.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    
.. php:method:: setCookie(string $name [, string $value [, int $expire = 0 [, string $path = '/' [, string $domain [, bool $secure = false [, bool $httponly = false ]]]]]]))
    
    Set the cookie. Actually this method adds ``Set-Cookie`` response header with provided options.
    
    :param string $name: The name of the cookie.
    :param string $value: The value of the cookie.
    :param int $expire: The time the cookie expires. This is a Unix timestamp so is in number of seconds since the epoch.
    :param string $path: The path on the server in which the cookie will be available on.
    :param string $domain: The domain that the cookie is available to.
    :param bool $secure: Indicates that the cookie should only be transmitted over a secure HTTPS connection from the client.
    :param bool $httponly: When ``true`` the cookie will be made accessible only through the HTTP protocol.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
    :returns: boolean ``true`` on success, ``false`` on failure
    
.. php:method:: sendFile(string $filename[, string $root[, string $mimetype[, mixed $download[, int $chunksize=10240]]]])

    Serve file in a safe and convenient way. See :ref:`tutorial-routing-staticfiles` for detailed information.
    
    :param string $filename: Name of the file to send.
    :param string $root: Root directory where file $filename to be expected.
    :param string $mimetype: Add this mimetype instead of automatically guesed one.
    :param bool|string $download: Force download of the file. If ``true``, the filename will be determine automatically, if ``string``, the filename will be set to value of $download.
    :param int $chunksize: The size of the chunks if chunked transfer encoding is used (Serving of files with filesize >= $chunksize). Default value is 8192 bytes.

.. php:method:: sendResponseStart(int $status[, string $reason])

	Start sending a chunked encoded response to zhe client
	
	:param int $status: A valid HTTP status code (range 100-599).
	:param string $reason: The reason phrase. Optional.
	:throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
	:throws: * :php:class:`InvalidOperationException` If response already sent.
	
.. php:method:: sendResponseChunk(string $chunk)
	
	Send next chunk of data to the client.
	
	:param string $chunk: The chunk of data to send.
	:throws: * :php:class:`InvalidParametersException` If invalid parameters are passed to the method.
	:throws: * :php:class:`InvalidOperationException` If response already sent.
	
.. php:method:: sendResponseEnd()

	Finalize chunked encoded response.

	:throws: * :php:class:`InvalidOperationException` If response already sent.

    
    