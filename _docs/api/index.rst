.. _php-can-server:

============
Server API
============

.. php:namespace:: Can

.. php:class:: Server
 
   Server class
 
.. php:method:: __construct(string $ip, int $port, string $log_format = "", resource $log_handler = STDOUT)
 
  Constructor. Instantiates new HTTP server.
 
  :param string $ip: IP to bind server to.
  :param int $port:  The port to listen on.
  :param string $log_format: The log format string (ELF). 
  :param resource $log_handler: File descriptor for the logging, used only if $log_format defined, default STDOUT.
  :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
  :throws: * :php:class:`ServerBindingException` If binding of the server to given IP and port fails.
  
  Supported fields for ELF:
        - ``cs-uri`` - client-to-server requested URI
        - ``cs-query`` - client-to-server query string
        - ``c-ip`` - client IP address
        - ``c-port`` - client port number
        - ``cs-method`` - client-to-server HTTP Method 
        - ``sc-status`` - server-to-client HTTP Status code in the response 
        - ``sc-bytes`` - number of bytes sent 
        - ``time`` - time in 24-hour format, the machine's timezone  
        - ``date`` - date in yyyy-mm-dd format
        - ``time-taken`` - Time taken for transaction to complete in seconds 
        - ``x-memusage`` - memory usage
        - ``x-error`` - detailed error information if any
        
Example:
  
.. code-block:: php

    <?php
    
    use \Can\Server;
    
    // Bind server to all available network interfaces on port 8080 without logging
    $server = new Server('0.0.0.0', 8080);
    
    // Bind server to all available network interfaces on port 8080
    // with logging to standard output with defined format
    $server = new Server('0.0.0.0',  8080, 
        "x-reqnum time c-ip cs-method cs-uri sc-status " . 
            "sc-bytes time-taken x-memusage x-error\n"
    );

    // Bind server to all available network interfaces on port 8080
    // with logging to defined logfile and log format
    $server = new Server('0.0.0.0',  8080, 
        "x-reqnum time c-ip cs-method cs-uri sc-status " . 
            "sc-bytes time-taken x-memusage x-error\n",
        fopen('/tmp/phpcan.log', 'w')
    );
    
    ?>
 
.. php:method:: start(Router $router)
 
  Starts the server.
  
  :param Router $router: :php:class:`Router` instance.
  :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
 
.. php:method:: stop()
 
  Stops the server.
  
  :throws: * :php:class:`InvalidOperationException` If server is not running.

============
Router API
============

.. php:namespace:: Can\Server

.. php:class:: Router

   Router class
    
.. php:method:: __construct(array $routes)

   Constructor. Instantiates new Server router.
   
   :param array $routes: array where each element is an instance of :php:class:`Route`.
   
   :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
   
.. php:method:: addRoute(Route $route)

   Adds new route to the router.
   
   :param Route $route: Instance of :php:class:`Route` to add.
   
   :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
   
=========
Route API
=========

.. php:namespace:: Can\Server

.. php:class:: Route

   Route class

.. php:const:: METHOD_GET
.. php:const:: METHOD_POST
.. php:const:: METHOD_PUT
.. php:const:: METHOD_DELETE
.. php:const:: METHOD_HEAD
.. php:const:: METHOD_OPTIONS
.. php:const:: METHOD_TRACE
.. php:const:: METHOD_CONNECT
.. php:const:: METHOD_PATCH
   
.. php:method:: __construct(string $uri, callable $handler, int $methods = Route::METHOD_GET)

    Constructor. Instantiates new Server route.

    :param string $uri: Static or dynamic URI path.
    :param callable $handler: Request handler.
    :param int $methods: HTTP request methods bitmask this route associated with, default Route::METHOD_GET.
    :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
    
.. php:method:: getUri(bool $as_regexp = false)

    Get URI path associated with this Route instance.
    
    :param bool $as_regexp: If set to ``true``, return value is a valid PCRE representation of the URI path - can be used as Nginx location definiton. (Only for dynamic routes)
    :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
    :returns: string
    
.. php:method:: getMethod(bool $as_regexp = false)

    Get HTTP request method associated with this Route instance.
    
    :param bool $as_regexp: If set to ``true``, return value is a valid PCRE representation of the method(s).
    :throws: * :php:class:`InvalidParametersException` If invalid parameters will be passed.
    :returns: string
    
.. php:method:: getHandler()

    Get request handler associated with this Route instance.
    
    :returns: callable